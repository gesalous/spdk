#include "ptl_context.h"
#include "lib/rdma_provider/ptl_config.h"
#include "ptl_cq.h"
#include "ptl_log.h"
#include "ptl_object_types.h"
#include "ptl_pd.h"
#include "ptl_uuid.h"
#include <assert.h>
#include <infiniband/verbs.h>
#include <portals4.h>
#include <pthread.h>
#include <spdk/util.h>
#include <stdbool.h>
#include <stdint.h>


extern volatile int is_target;

struct ptl_cnxt_mem_handle {
	void *vaddr;
	size_t size;
	ptl_md_t mem_handle;
	bool inuse;
};

static struct ptl_context ptl_context;
typedef bool (*process_event)(ptl_event_t event, struct ibv_wc *wc);



static bool ptl_cnxt_process_get(ptl_event_t event, struct ibv_wc *wc)
{
	SPDK_PTL_DEBUG("Someone performed an RDMA READ from me, let's ignore it for now");
	return false;
}

static bool ptl_cnxt_process_get_overflow(ptl_event_t event, struct ibv_wc *wc)
{

	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return true;
}

static bool ptl_cnxt_process_put(ptl_event_t event, struct ibv_wc *wc)
{
	struct ptl_context_recv_op *le_recv_op;

	if (NULL == event.user_ptr) {
		SPDK_PTL_DEBUG("PtlPut (recv) has a null context, I should have got an RDMA_WRITE");
		return false;
	}
	/*Note: Receive operations from the srq is of type PTL_LE_METADATA
	 * (Target). For the initiator is the plain wr_id. We use the receive
	 * length which is 16 bytes for the receive operations of the initiator
	 * and 64 for the target. For now we do this just to avoid additional
	 * calloc and free operations re-think about it.
	 * */
	le_recv_op = event.user_ptr;
  if(le_recv_op->obj_type != PTL_LE_RECV_OP){
    SPDK_PTL_FATAL("Corrupted recv op");
  }

	SPDK_PTL_DEBUG("Got a PtlPut it's a RECEIVE!");
	uint64_t match_bits = event.match_bits;
	int initiator_qp_num =  ptl_uuid_get_initiator_qp_num(match_bits);
	int target_qp_num = ptl_uuid_get_target_qp_num(match_bits);

	memset(wc, 0x00, sizeof(*wc));

	if (event.ni_fail_type != PTL_NI_OK) {
		SPDK_PTL_FATAL("Operation failed");
	}
	wc->status =
		event.ni_fail_type == PTL_NI_OK ? IBV_WC_SUCCESS : IBV_WC_LOC_PROT_ERR;
	wc->opcode = IBV_WC_RECV;

	wc->byte_len = event.mlength;
	if (wc->byte_len != 64 && wc->byte_len != 16) {
		SPDK_PTL_FATAL("Wrong size, should have been either 64 B (NVMe command "
			       "size) or 16 B (NVMe response) size it is: %u",
			       wc->byte_len);
	}

	wc->wr_id = le_recv_op->wr_id;


	wc->qp_num = is_target ? target_qp_num : initiator_qp_num;
	wc->src_qp = is_target ? initiator_qp_num : target_qp_num;
	SPDK_PTL_DEBUG("CP server the recv operation (match_bits = %lu) is "
		       "between the pair initiator_qp_num = %d target_qp_num = "
		       "%d wc->qp_num: %d is target? %s size: %lu B",
		       match_bits, initiator_qp_num, target_qp_num, wc->qp_num,
		       is_target ? "YES" : "NO", event.mlength);

	free(le_recv_op);
	return true;
}

static bool ptl_cnxt_process_put_overflow(ptl_event_t event, struct ibv_wc *wc)
{

	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return true;
}


static bool ptl_cnxt_process_atomic(ptl_event_t event, struct ibv_wc *wc)
{

	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return true;
}


static bool ptl_cnxt_process_atomic_overflow(ptl_event_t event, struct ibv_wc *wc)
{

	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return true;
}


static bool ptl_cnxt_process_fetch_atomic(ptl_event_t event, struct ibv_wc *wc)
{

	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return true;
}

static bool ptl_cnxt_process_fetch_atomic_overflow(ptl_event_t event, struct ibv_wc *wc)
{

	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return true;
}

static bool ptl_cnxt_process_reply(ptl_event_t event, struct ibv_wc *wc)
{
	SPDK_PTL_DEBUG("Got a PTL_EVENT_REPLY even (RDMA read done). Number of bytes received: %lu. Filling wc with code %d event type: %d",
		       event.mlength, event.ni_fail_type, event.type);

	int initiator_qp_num =  ptl_uuid_get_initiator_qp_num(event.match_bits);
	int target_qp_num = ptl_uuid_get_target_qp_num(event.match_bits);
	SPDK_PTL_DEBUG("CP server the recv operation (match_bits = %lu) is between the pair initiator_qp_num = %d target_qp_num = %d",
		       event.match_bits, initiator_qp_num, target_qp_num);
	memset(wc, 0x00, sizeof(*wc));

	if (event.ni_fail_type != PTL_NI_OK) {
		SPDK_PTL_FATAL("Operation failed with code: %d", event.ni_fail_type);
	}

	wc->status =
		event.ni_fail_type == PTL_NI_OK ? IBV_WC_SUCCESS : IBV_WC_LOC_PROT_ERR;
	wc->opcode = IBV_WC_RDMA_READ;
	wc->wr_id = (uint64_t)event.user_ptr;
	wc->byte_len = event.mlength;
	wc->qp_num = is_target ? target_qp_num : initiator_qp_num;
	wc->src_qp = is_target ? initiator_qp_num : target_qp_num;
	return true;
}

static bool ptl_cnxt_process_send(ptl_event_t event, struct ibv_wc *wc)
{
	return false;
}

static bool ptl_cnxt_process_ack(ptl_event_t event, struct ibv_wc *wc)
{
	struct ptl_context_send_op *send_op;

	if (NULL == event.user_ptr) {
		SPDK_PTL_DEBUG("PtlPut without context? App does not want any signal");
		return false;
	}

	send_op = event.user_ptr;
	if (send_op->obj_type != PTL_LE_SEND_OP) {
		SPDK_PTL_FATAL("Corrupted object type this is not a PTL_LE_SEND_OP");
	}

	if (send_op->qp_num == 0) {
		SPDK_PTL_FATAL("Nida does not assign 0 fake qp numbers");
	}

	SPDK_PTL_DEBUG("Got a PTL_EVENT_ACK event filling wc with code %d event type: %d from local qp num: %d",
		       event.ni_fail_type, event.type, send_op->qp_num);

	memset(wc, 0x00, sizeof(*wc));

	if (event.ni_fail_type != PTL_NI_OK) {
		SPDK_PTL_FATAL("Operation failed");
	}
	wc->status =
		event.ni_fail_type == PTL_NI_OK ? IBV_WC_SUCCESS : IBV_WC_LOC_PROT_ERR;
	wc->opcode = IBV_WC_SEND;
	wc->wr_id = send_op->wr_id;
	wc->byte_len = event.mlength;
	wc->qp_num = send_op->qp_num;
	wc->src_qp = 0;//TOOO
	free(send_op);
	return true;
}

static bool ptl_cnxt_process_bt_disabled(ptl_event_t event, struct ibv_wc *wc)
{

	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return true;
}

static bool ptl_cnxt_process_auto_unlink(ptl_event_t event, struct ibv_wc *wc)
{
	SPDK_PTL_DEBUG("Got an UNLINK_EVENT! A receive buffer has been consumed from a prior PtlPut recv operation, ignore Portals internal");
	if (NULL == event.user_ptr) {
		SPDK_PTL_FATAL("Unlink event must have an associated user context");
	}

	return false;
}

static bool ptl_cnxt_process_auto_free(ptl_event_t event, struct ibv_wc *wc)
{

	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return true;
}

static bool ptl_cnxt_process_search(ptl_event_t event, struct ibv_wc *wc)
{

	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return true;
}


static bool ptl_cnxt_process_link(ptl_event_t event, struct ibv_wc *wc)
{

	SPDK_PTL_DEBUG("PROCESS LINK EVENT OK go on PORTALS internal");
	return false;
}

static process_event handler[16] = {
	ptl_cnxt_process_get,          ptl_cnxt_process_get_overflow,
	ptl_cnxt_process_put,          ptl_cnxt_process_put_overflow,
	ptl_cnxt_process_atomic,       ptl_cnxt_process_atomic_overflow,
	ptl_cnxt_process_fetch_atomic, ptl_cnxt_process_fetch_atomic_overflow,
	ptl_cnxt_process_reply,        ptl_cnxt_process_send,
	ptl_cnxt_process_ack,          ptl_cnxt_process_bt_disabled,
	ptl_cnxt_process_auto_unlink,  ptl_cnxt_process_auto_free,
	ptl_cnxt_process_search,       ptl_cnxt_process_link
};

// static const char *ptl_event_kind_to_str(ptl_event_kind_t event_kind)
// {
// 	switch (event_kind) {
// 	case PTL_EVENT_GET:
// 		return "PTL_EVENT_GET";
// 	case PTL_EVENT_GET_OVERFLOW:
// 		return "PTL_EVENT_GET_OVERFLOW";
// 	case PTL_EVENT_PUT:
// 		return "PTL_EVENT_PUT";
// 	case PTL_EVENT_PUT_OVERFLOW:
// 		return "PTL_EVENT_PUT_OVERFLOW";
// 	case PTL_EVENT_ATOMIC:
// 		return "PTL_EVENT_ATOMIC";
// 	case PTL_EVENT_ATOMIC_OVERFLOW:
// 		return "PTL_EVENT_ATOMIC_OVERFLOW";
// 	case PTL_EVENT_FETCH_ATOMIC:
// 		return "PTL_EVENT_FETCH_ATOMIC";
// 	case PTL_EVENT_FETCH_ATOMIC_OVERFLOW:
// 		return "PTL_EVENT_FETCH_ATOMIC_OVERFLOW";
// 	case PTL_EVENT_REPLY:
// 		return "PTL_EVENT_REPLY";
// 	case PTL_EVENT_SEND:
// 		return "PTL_EVENT_SEND";
// 	case PTL_EVENT_ACK:
// 		return "PTL_EVENT_ACK";
// 	case PTL_EVENT_PT_DISABLED:
// 		return "PTL_EVENT_PT_DISABLED";
// 	case PTL_EVENT_AUTO_UNLINK:
// 		return "PTL_EVENT_AUTO_UNLINK";
// 	case PTL_EVENT_AUTO_FREE:
// 		return "PTL_EVENT_AUTO_FREE";
// 	case PTL_EVENT_SEARCH:
// 		return "PTL_EVENT_SEARCH";
// 	case PTL_EVENT_LINK:
// 		return "PTL_EVENT_LINK";
// 	default:
// 		return "UNKNOWN_EVENT";
// 	}
// }

static int ptl_cnxt_poll_cq(struct ibv_cq *ibv_cq, int num_entries,
			    struct ibv_wc *wc)
{
	static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
	ptl_event_t event;
	int ret;
	int events_processed = 0;


	pthread_mutex_lock(&g_lock);
	struct ptl_cq *ptl_cq = ptl_cq_get_from_ibv_cq(ibv_cq);


	while (events_processed < num_entries) {
		ret = PtlEQGet(ptl_cq_get_queue(ptl_cq), &event);
		if (ret == PTL_OK) {
			if (false == handler[event.type](event, &wc[events_processed])) {
				continue;
			}
			++events_processed;

		} else if (ret == PTL_EQ_EMPTY) {
			// SPDK_PTL_DEBUG("No events ok COOL");
			break;
		} else if (ret == PTL_EQ_DROPPED) {
			SPDK_PTL_DEBUG("Ok queue overflow break");
			break;
		} else {
			SPDK_PTL_FATAL("PtlEQGet failed with error code %d", ret);
		}
	}
	pthread_mutex_unlock(&g_lock);
	return events_processed;
}

struct ptl_context *ptl_cnxt_get(void)
{
	static pthread_mutex_t cnxt_lock = PTHREAD_MUTEX_INITIALIZER;
	// ptl_ni_limits_t desired;
	ptl_ni_limits_t actual;
	int ret;
	const char *srv_pid;
	const char *srv_nid;
	pthread_mutex_lock(&cnxt_lock);
	if (ptl_context.initialized) {
		goto exit;
	}

	ptl_context.object_type = PTL_CONTEXT;
	ptl_context.portals_idx_send_recv = PTL_PT_INDEX;
	SPDK_PTL_DEBUG("Calling PtlInit()");
	ret = PtlInit();
	if (ret != PTL_OK) {
		SPDK_PTL_FATAL("PtlInit failed");
	}


	srv_pid = getenv("SERVER_PID");

	if (NULL == srv_pid) {
		SPDK_PTL_FATAL("Sorry you need to set SERVER_PID env variable");
	}
	srv_nid = getenv("SERVER_NID");

	if (NULL == srv_nid) {
		SPDK_PTL_FATAL("Sorry you need to set SERVER_NID env variable");
	}
	/*XXX TODO XXX Check for errors and staff*/
	ptl_context.pid = atoi(srv_pid);
	ptl_context.nid = atoi(srv_nid);

	// memset(&desired, 0, sizeof(ptl_ni_limits_t));

	// desired.max_waw_ordered_size = 4096UL;
	// desired.max_war_ordered_size = 4096UL;

	// desired.features = PTL_TOTAL_DATA_ORDERING;

	ret = PtlNIInit((int)atoi(srv_nid), PTL_NI_MATCHING | PTL_NI_PHYSICAL,
			(int)atoi(srv_pid), NULL, &actual, &ptl_context.ni_handle);

	if (ret != PTL_OK) {
		SPDK_PTL_FATAL("RDMACM: PtlNIInit failed");
	}

	// Check if PTL_TOTAL_DATA_ORDERING is supported
	// if (actual.features & PTL_TOTAL_DATA_ORDERING) {
	// 	SPDK_PTL_DEBUG("Total data ordering is enabled");
	// } else {
	// 	SPDK_PTL_FATAL("Total data ordering is not supported by this implementation. Cannot support NVMe-OF properties");
	// }

	SPDK_PTL_DEBUG("Actual max_waw_ordered_size: %zu bytes", actual.max_waw_ordered_size);
	SPDK_PTL_DEBUG("Actual max_war_ordered_size: %zu bytes", actual.max_war_ordered_size);

	ptl_context.fake_ibv_cnxt.ops.poll_cq = ptl_cnxt_poll_cq;
	ptl_context.fake_cq.context = &ptl_context.fake_ibv_cnxt;

	SPDK_PTL_DEBUG("SUCCESSFULLY create and initialized PORTALS context");
	ptl_context.initialized = true;
exit:
	pthread_mutex_unlock(&cnxt_lock);
	return &ptl_context;
}

struct ibv_context *ptl_cnxt_get_ibv_context(struct ptl_context *cnxt)
{
	return &cnxt->fake_ibv_cnxt;
}

struct ptl_context *ptl_cnxt_get_from_ibcnxt(struct ibv_context *ib_cnxt)
{
	struct ptl_context *cnxt =
		SPDK_CONTAINEROF(ib_cnxt, struct ptl_context, fake_ibv_cnxt);
	if (PTL_CONTEXT != cnxt->object_type) {
		SPDK_PTL_FATAL("Corrupted portals context, magic number does not match");
	}
	return cnxt;
}

struct ptl_context *ptl_cnxt_get_from_ibvpd(struct ibv_pd *ib_pd)
{
	struct ptl_pd *ptl_pd = ptl_pd_get_from_ibv_pd(ib_pd);
	return ptl_pd_get_cnxt(ptl_pd);
}

ptl_pt_index_t ptl_cnxt_get_portal_index(struct ptl_context *cnxt)
{
	return cnxt->portals_idx_send_recv;
}

ptl_handle_ni_t ptl_cnxt_get_ni_handle(struct ptl_context *cnxt)
{
	if (false == cnxt->initialized) {
		SPDK_PTL_FATAL("Context is not initialized!");
	}
	return cnxt->ni_handle;
}

