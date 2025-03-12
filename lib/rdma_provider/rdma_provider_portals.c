/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright (C) 2020 Intel Corporation. All rights reserved.
 *   Copyright (c) Mellanox Technologies LTD. All rights reserved.
 *   Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */
#include "lib/rdma_provider/ptl_cq.h"
#include "lib/rdma_provider/ptl_pd.h"
#include "lib/rdma_provider/ptl_qp.h"
#include "portals4.h"
#include "ptl_log.h"
#include "ptl_cm_id.h"
#include "ptl_context.h"
#include "ptl_cq.h"
#include "spdk/likely.h"
#include "spdk/log.h"
#include "spdk/stdinc.h"
#include "spdk/string.h"
#include "spdk/util.h"
#include "spdk_internal/rdma_provider.h"
#include "spdk_internal/rdma_utils.h"
#include <rdma/rdma_cma.h>
#include <stdint.h>
#define SPDK_PTL_IGNORE 0xffffffff
#define SPDK_PTL_MATCH 1
#define SPDK_PTL_SRV_ME_OPTS                                                                                                 \
	PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_MAY_ALIGN | PTL_ME_IS_ACCESSIBLE | PTL_ME_MANAGE_LOCAL | \
		PTL_ME_NO_TRUNCATE | PTL_LE_USE_ONCE
#define SPDK_PTL_IOVEC_SIZE 2
//from common.c staff
#define SPDK_PTL_PROVIDER_SRQ_MAGIC_NUMBER 27081983UL
#define SPDK_PTL_PROVIDER_QP_MAGIC_NUMBER 19082018UL
#define SPDK_PTL_CHECK_SRQ(X) \
    if ((X)->magic_number != SPDK_PTL_PROVIDER_SRQ_MAGIC_NUMBER) { \
        SPDK_PTL_FATAL("Corrupted PORTALS SRQ"); \
    }


struct spdk_portals_provider_srq {
	uint64_t magic_number;
	struct spdk_rdma_provider_srq fake_srq;
	struct ptl_context *ptl_context;
};

struct spdk_portals_provider_qp {
	uint64_t magic_number;
	struct ptl_context *ptl_context;
	struct ptl_cm_id *ptl_id;
	struct spdk_rdma_provider_qp fake_spdk_rdma_qp;
};

struct spdk_rdma_provider_srq *
spdk_rdma_provider_srq_create(struct spdk_rdma_provider_srq_init_attr *init_attr)
{
	assert(init_attr);
	assert(init_attr->pd);
	struct ptl_context * ptl_context;
	struct spdk_portals_provider_srq *portals_srq;
	struct spdk_rdma_provider_srq * rdma_srq;

	ptl_context = ptl_cnxt_get_from_ibvpd(init_attr->pd);
	SPDK_PTL_DEBUG("Ok got portals context from ibv_pd!");
	portals_srq = calloc(1UL, sizeof(*portals_srq));
	if (!portals_srq) {
		SPDK_PTL_FATAL("Can't allocate memory for SRQ handle\n");
	}
	portals_srq->magic_number = SPDK_PTL_PROVIDER_SRQ_MAGIC_NUMBER;
	portals_srq->ptl_context = ptl_context;
	rdma_srq = &portals_srq->fake_srq;


	if (init_attr->stats) {
		rdma_srq->stats = init_attr->stats;
		rdma_srq->shared_stats = true;
	} else {
		rdma_srq->stats = calloc(1UL, sizeof(*rdma_srq->stats));
		if (!rdma_srq->stats) {
			SPDK_PTL_FATAL("SRQ statistics memory allocation failed");
			free(rdma_srq);
			return NULL;
		}
	}

	// rdma_srq->srq = ibv_create_srq(init_attr->pd, &init_attr->srq_init_attr);
	rdma_srq->srq = NULL;/*On purpose*/
	// if (!rdma_srq->srq) {
	// 	if (!init_attr->stats) {
	// 		free(rdma_srq->stats);
	// 	}
	// 	SPDK_ERRLOG("Unable to create SRQ, errno %d (%s)\n", errno, spdk_strerror(errno));
	// 	free(rdma_srq);
	// 	return NULL;
	// }
	SPDK_PTL_DEBUG("Ok emulated the RDMA_SRQ creation with PORTALS!");
	return rdma_srq;
}

int
spdk_rdma_provider_srq_destroy(struct spdk_rdma_provider_srq *rdma_srq)
{
	if (!rdma_srq) {
		return 0;
	}

	assert(rdma_srq->srq);
	SPDK_PTL_FATAL("UNIMPLEMENTED");
	free(rdma_srq);
	return -1;
}

static inline bool
rdma_queue_recv_wrs(struct spdk_rdma_provider_recv_wr_list *recv_wrs, struct ibv_recv_wr *first,
		    struct spdk_rdma_provider_wr_stats *recv_stats)
{
	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return false;
}

bool spdk_rdma_provider_srq_queue_recv_wrs(
	struct spdk_rdma_provider_srq *rdma_srq, struct ibv_recv_wr *first)
{
	struct ibv_recv_wr *last;
	struct spdk_rdma_provider_wr_stats *recv_stats;
	struct spdk_rdma_provider_recv_wr_list *recv_wrs;
	bool ret;

	assert(rdma_srq->stats);
	recv_stats = rdma_srq->stats;
	uint64_t diff = recv_stats->num_submitted_wrs;
	recv_stats->num_submitted_wrs++;
	last = first;
	while (last->next != NULL) {
		last = last->next;
		recv_stats->num_submitted_wrs++;
	}

	recv_wrs = &rdma_srq->recv_wrs;

	if (recv_wrs->first == NULL) {
		recv_wrs->first = first;
		recv_wrs->last = last;
		ret = true;
	} else {
		recv_wrs->last->next = first;
		recv_wrs->last = last;
		ret = false;
	}
	// SPDK_PTL_DEBUG("Done exact the same steps as in IBV case total "
	// 	       "submitted wrs: %lu current: %lu",
	// 	       recv_stats->num_submitted_wrs,
	// 	       recv_stats->num_submitted_wrs - diff);
	return ret;
}


/**
 * @brief Cleans up the ibv_recv_wr descriptors. In the vanilla implementation hardware handles the cleanup process. However,
 * since SPDK_PTL translates the ibv_recv_wr into PTLMEAppend entries, SPDK_PTL needs to do the cleanup.
 */
// static bool spdk_ptl_clean_up_recv_wr_desc(struct ibv_recv_wr *recv_wrs) {
//   return true;
// }


int
spdk_rdma_provider_srq_flush_recv_wrs(struct spdk_rdma_provider_srq *rdma_srq,
				      struct ibv_recv_wr **bad_wr)
{
	ptl_handle_ni_t nic;
	ptl_le_t le;
	ptl_handle_le_t le_handle;
	struct ptl_context_le_metadata *le_meta = calloc(1UL, sizeof(*le_meta));
	int ret;

	if (spdk_unlikely(rdma_srq->recv_wrs.first == NULL)) {
		return 0;
	}
	//Now it's time to append the entries in portals
	for (struct ibv_recv_wr *wr = rdma_srq->recv_wrs.first; wr != NULL;
	     wr = wr->next) {
		struct spdk_portals_provider_srq *portals_srq =
			SPDK_CONTAINEROF(rdma_srq, struct spdk_portals_provider_srq, fake_srq);
		SPDK_PTL_CHECK_SRQ(portals_srq);
		nic = ptl_cnxt_get_ni_handle(portals_srq->ptl_context);

		if (wr->num_sge != SPDK_PTL_IOVEC_SIZE) {
			SPDK_PTL_FATAL("IOVECTOR too small size is: %d needs %d", SPDK_PTL_IOVEC_SIZE, wr->num_sge);
		}
		// SPDK_PTL_DEBUG("Num of sges are %d", wr->num_sge);
		for (int i = 0; i < wr->num_sge; i++) {
			le_meta->io_vector[i].iov_base = (ptl_addr_t)wr->sg_list[i].addr;
			le_meta->io_vector[i].iov_len = wr->sg_list[i].length;
			// SPDK_PTL_DEBUG("iovector[%d] = : Address = %p, Length = %lu\n",
			// 	       i, le_meta->io_vector[i].iov_base, le_meta->io_vector[i].iov_len);
		}

		/*Initialize the list matching entry*/
		memset(&le, 0, sizeof(ptl_le_t));
		le.ignore_bits = SPDK_PTL_IGNORE;
		le.match_bits = SPDK_PTL_MATCH;
		le.match_id.phys.nid = PTL_NID_ANY;
		le.match_id.phys.pid = PTL_PID_ANY;
		le.min_free = 0;
		le.start = le_meta->io_vector;
		le.length = wr->num_sge;
		le.ct_handle = PTL_CT_NONE;
		le.uid = PTL_UID_ANY;
		le.options = SPDK_PTL_SRV_ME_OPTS | PTL_IOVEC;
		le_meta->wr_id = wr->wr_id;
		// Append the memory entry
		ret = PtlLEAppend(
			      nic,                    // Network interface handle
			      ptl_cnxt_get_portal_index(portals_srq->ptl_context), //Portals table index
			      &le,                    // List entry
			      PTL_PRIORITY_LIST,      // List type (PRIORITY or OVERFLOW)
			      (void *)le_meta,               // User pointer (can be used to store wr_id)
			      &le_handle              // Returned handle
		      );
		if (PTL_OK != ret) {
			SPDK_PTL_FATAL("Failed to append memory entry start addr: %p length %lu code is: %d", le.start,
				       le.length, ret);
		}

	}
	SPDK_PTL_DEBUG("Ok append the iovector in Portals");
	SPDK_PTL_DEBUG("UNIMPLEMENTED do the cleanup? XXX TODO XXX!");
	// rc = ibv_post_srq_recv(rdma_srq->srq, rdma_srq->recv_wrs.first, bad_wr);
	rdma_srq->recv_wrs.first = NULL;
	rdma_srq->stats->doorbell_updates++;
	return 0;
}

bool spdk_rdma_provider_qp_queue_recv_wrs(
	struct spdk_rdma_provider_qp *spdk_rdma_qp, struct ibv_recv_wr *first)
{

	assert(spdk_rdma_qp);
	assert(first);
	SPDK_PTL_DEBUG("DOING SAME STAFF AS VERBS");
	struct spdk_rdma_provider_wr_stats *recv_stats =
			&spdk_rdma_qp->stats->recv;
	struct spdk_rdma_provider_recv_wr_list *recv_wrs =
			&spdk_rdma_qp->recv_wrs;
	struct ibv_recv_wr *last;

	recv_stats->num_submitted_wrs++;
	last = first;
	while (last->next != NULL) {
		last = last->next;
		recv_stats->num_submitted_wrs++;
	}

	if (recv_wrs->first == NULL) {
		recv_wrs->first = first;
		recv_wrs->last = last;
		return true;
	} else {
		recv_wrs->last->next = first;
		recv_wrs->last = last;
		return false;
	}
}

int
spdk_rdma_provider_qp_flush_recv_wrs(struct spdk_rdma_provider_qp *spdk_rdma_qp,
				     struct ibv_recv_wr **bad_wr)
{
	SPDK_PTL_DEBUG("PORTALS REGISTERING THE BUFFERS FOR THE *SINGLE* QUEUE PAIR CASE (NOT SRQ)");
	struct spdk_portals_provider_qp *portals_qp;
	ptl_handle_ni_t nic;
	ptl_pt_index_t pt_index;
	ptl_le_t le;
	ptl_handle_le_t le_handle;
	struct ptl_context_le_metadata *le_meta = calloc(1UL, sizeof(*le_meta));
	int ret;


	/* gesalous start */
	portals_qp = SPDK_CONTAINEROF(spdk_rdma_qp, struct spdk_portals_provider_qp, fake_spdk_rdma_qp);
	if (SPDK_PTL_PROVIDER_QP_MAGIC_NUMBER != portals_qp->magic_number) {
		SPDK_PTL_FATAL("Corrupted Portals QP!");
	}

	if (spdk_unlikely(spdk_rdma_qp->recv_wrs.first == NULL)) {
		SPDK_PTL_DEBUG("Nothing to register for receive?");
		// raise(SIGINT);
		return 0;
	}
	//Now it's time to append the entries in portals
	for (struct ibv_recv_wr *wr = spdk_rdma_qp->recv_wrs.first; wr != NULL;
	     wr = wr->next) {
		nic = ptl_cnxt_get_ni_handle(portals_qp->ptl_context);
		pt_index = ptl_cnxt_get_portal_index(portals_qp->ptl_context);

		if (wr->num_sge > SPDK_PTL_IOVEC_SIZE) {
			SPDK_PTL_FATAL("io_vector too small size: %d needs %d", SPDK_PTL_IOVEC_SIZE, wr->num_sge);
		}

		SPDK_PTL_DEBUG("Num sges is %d", wr->num_sge);
		for (int i = 0; i < wr->num_sge; i++) {
			le_meta->io_vector[i].iov_base = (ptl_addr_t) wr->sg_list[i].addr;
			le_meta->io_vector[i].iov_len = wr->sg_list[i].length;
			SPDK_PTL_DEBUG("SGE %d: Address = %p, Length = %lu\n",
				       i, le_meta->io_vector[i].iov_base, le_meta->io_vector[i].iov_len);
		}
		// Setup the list entry
		// Initialize the matching entry
		memset(&le, 0, sizeof(ptl_le_t));
		le.ignore_bits = SPDK_PTL_IGNORE;
		le.match_bits = SPDK_PTL_MATCH;
		le.match_id.phys.nid = PTL_NID_ANY;
		le.match_id.phys.pid = PTL_PID_ANY;
		le.min_free = 0;
		le.start = le_meta->io_vector;
		le.length = wr->num_sge;
		le.ct_handle = PTL_CT_NONE;
		le.uid = PTL_UID_ANY;
		le.options = SPDK_PTL_SRV_ME_OPTS | PTL_IOVEC;
		le_meta->wr_id = wr->wr_id;

		// Append the memory entry
		ret = PtlLEAppend(
			      nic,                    // Network interface handle
			      pt_index,               // Portal table index
			      &le,                    // List entry
			      PTL_PRIORITY_LIST,      // List type (PRIORITY or OVERFLOW)
			      le_meta,               // User pointer (can be used to store wr_id)
			      &le_handle              // Returned handle
		      );
		if (PTL_OK != ret) {
			SPDK_PTL_FATAL("Failed to append memory entry");
		}

	}
	SPDK_PTL_DEBUG("Ok append the memory entries in Portals");

	/* gesalous end */
	spdk_rdma_qp->recv_wrs.first = NULL;
	spdk_rdma_qp->stats->recv.doorbell_updates++;

	return 0;
}
// common end

struct spdk_rdma_provider_qp *
spdk_rdma_provider_qp_create(struct rdma_cm_id *cm_id,
			     struct spdk_rdma_provider_qp_init_attr *qp_attr)
{
	SPDK_PTL_DEBUG("DOING BASICALLY THE SAME STAFF as the original");
	struct spdk_portals_provider_qp *spdk_portals_qp;
	struct spdk_rdma_provider_qp *spdk_rdma_qp;
	int rc;
	struct ibv_qp_init_attr attr = {.qp_context = qp_attr->qp_context,
						.send_cq = qp_attr->send_cq,
						.recv_cq = qp_attr->recv_cq,
						.srq = qp_attr->srq,
						.cap = qp_attr->cap,
						.qp_type = IBV_QPT_RC
	};

	if (qp_attr->domain_transfer) {
		SPDK_PTL_FATAL(
			"PORTALS provider doesn't support memory domain transfer functionality");
		return NULL;
	}

	spdk_portals_qp = calloc(1UL, sizeof(*spdk_portals_qp));
	if (!spdk_portals_qp) {
		SPDK_ERRLOG("qp memory allocation failed");
	}

	spdk_portals_qp->ptl_context = ptl_cnxt_get();
	spdk_portals_qp->ptl_id = ptl_cm_id_get(cm_id);
	spdk_portals_qp->magic_number =  SPDK_PTL_PROVIDER_QP_MAGIC_NUMBER;

	spdk_rdma_qp = &spdk_portals_qp->fake_spdk_rdma_qp;


	if (qp_attr->stats) {
		spdk_rdma_qp->stats = qp_attr->stats;
		spdk_rdma_qp->shared_stats = true;
	} else {
		spdk_rdma_qp->stats = calloc(1UL, sizeof(*spdk_rdma_qp->stats));
		if (!spdk_rdma_qp->stats) {
			SPDK_ERRLOG("qp statistics memory allocation failed\n");
			free(spdk_portals_qp);
			return NULL;
		}
	}

	rc = rdma_create_qp(cm_id, qp_attr->pd, &attr);
	if (rc) {
		SPDK_ERRLOG("Failed to create qp, rc %d, errno %s (%d)\n", rc,
			    spdk_strerror(errno), errno);
		free(spdk_portals_qp);
		return NULL;
	}
	spdk_rdma_qp->qp = cm_id->qp;
	spdk_rdma_qp->cm_id = cm_id;
	spdk_rdma_qp->domain = spdk_rdma_utils_get_memory_domain(qp_attr->pd);
	if (!spdk_rdma_qp->domain) {
		spdk_rdma_provider_qp_destroy(spdk_rdma_qp);
		return NULL;
	}

	qp_attr->cap = attr.cap;
	return spdk_rdma_qp;
}

int
spdk_rdma_provider_qp_accept(struct spdk_rdma_provider_qp *spdk_rdma_qp,
			     struct rdma_conn_param *conn_param)
{
	assert(spdk_rdma_qp != NULL);
	assert(spdk_rdma_qp->cm_id != NULL);
  struct ptl_cm_id *ptl_id = ptl_cm_id_get(spdk_rdma_qp->cm_id);
  SPDK_PTL_DEBUG("At accept got a valid ptl_id queue pair id: %d",ptl_id->fake_cm_id.qp->qp_num);
	return rdma_accept(spdk_rdma_qp->cm_id, conn_param);
}

int spdk_rdma_provider_qp_complete_connect(
	struct spdk_rdma_provider_qp *spdk_rdma_qp)
{
	struct rdma_cm_event *fake_event;
	struct ptl_cm_id *ptl_id = ptl_cm_id_get(spdk_rdma_qp->cm_id);
	/* Nothing to be done for Portals */
	SPDK_PTL_DEBUG("CREATE FAKE RDMA_CM_EVENT_ESTABLISHED event");
	fake_event = ptl_cm_id_create_event(ptl_id,
					    spdk_rdma_qp->cm_id,
					    RDMA_CM_EVENT_ESTABLISHED, NULL, 0);
	ptl_cm_id_add_event(ptl_id, fake_event);

	return 0;
}

void
spdk_rdma_provider_qp_destroy(struct spdk_rdma_provider_qp *spdk_rdma_qp)
{
	assert(spdk_rdma_qp != NULL);
	SPDK_PTL_FATAL("UNIMPLEMENTED");
	free(spdk_rdma_qp);
}

int
spdk_rdma_provider_qp_disconnect(struct spdk_rdma_provider_qp *spdk_rdma_qp)
{
	assert(spdk_rdma_qp != NULL);

	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return 0;
}

bool
spdk_rdma_provider_qp_queue_send_wrs(struct spdk_rdma_provider_qp *spdk_rdma_qp,
				     struct ibv_send_wr *first)
{
	SPDK_PTL_DEBUG("Enqueueing SEND WRS request as in the VANILLA CASE for Portals");
	struct ibv_send_wr *last;

	assert(spdk_rdma_qp);
	assert(first);

	spdk_rdma_qp->stats->send.num_submitted_wrs++;
	last = first;
	while (last->next != NULL) {
		last = last->next;
		spdk_rdma_qp->stats->send.num_submitted_wrs++;
	}

	if (spdk_rdma_qp->send_wrs.first == NULL) {
		spdk_rdma_qp->send_wrs.first = first;
		spdk_rdma_qp->send_wrs.last = last;
		return true;
	} else {
		spdk_rdma_qp->send_wrs.last->next = first;
		spdk_rdma_qp->send_wrs.last = last;
		return false;
	}
}

int
spdk_rdma_provider_qp_flush_send_wrs(struct spdk_rdma_provider_qp *spdk_rdma_qp,
				     struct ibv_send_wr **bad_wr)
{

	assert(spdk_rdma_qp);
	assert(bad_wr);
	int rc;

	struct ptl_qp *ptl_qp = ptl_qp_get_from_ibv_qp(spdk_rdma_qp->qp);
	struct ptl_pd *ptl_pd = ptl_qp_get_pd(ptl_qp);
	struct ptl_context *ptl_context = ptl_cnxt_get();
	struct ptl_mem_desc ptl_mem_desc;
	ptl_process_t target = {.phys.nid = 1, .phys.pid = 16};
	uint64_t local_offset;


	if (spdk_unlikely(NULL == spdk_rdma_qp->send_wrs.first)) {
		SPDK_PTL_DEBUG("Nothing to SEND");
		// raise(SIGINT);
		return 0;
	}

	SPDK_PTL_DEBUG("======> INFO about the send list of NVMe commands");
	for (struct ibv_send_wr *wr = spdk_rdma_qp->send_wrs.first; wr != NULL; wr = wr->next) {
		if (wr->num_sge != 1) {
			SPDK_PTL_FATAL("Num sges > 1 are under development, sorry");
		}

		for (int i = 0; i < wr->num_sge; i++) {
			ptl_mem_desc = ptl_pd_get_mem_desc(ptl_pd, wr->sg_list[i].addr, wr->sg_list[i].length);

			if (false == ptl_mem_desc.is_valid) {
				SPDK_PTL_FATAL("MEM desc not found!");
			}
			local_offset = wr->sg_list[i].addr - (uint64_t)ptl_mem_desc.mem_desc.start;
			SPDK_PTL_DEBUG("=====> SGE[%d]: Address = 0x%lx, Length = %u bytes local offset: %lu\n",
				       i,
				       wr->sg_list[i].addr,
				       wr->sg_list[i].length, local_offset);



			rc = PtlPut(ptl_mem_desc.mem_handle,
				    local_offset,           // local offset
				    wr->sg_list[i].length,         // length
				    PTL_ACK_REQ,                   // ack request
				    target,       // target process
				    ptl_cnxt_get_portal_index(ptl_context),        // portal table index
				    0,      // match bits
				    0,      // remote offset, don't care let target decide
				    (void *)wr->wr_id,             // user ptr
				    0);                            // priority

			if (rc != PTL_OK) {
				SPDK_PTL_FATAL("PtlPut failed with rc: %d", rc);
			}
			// raise(SIGINT);

			// ptl_ct_event_t ct_event;
			// rc = PtlCTWait(ptl_mem_desc.mem_desc.ct_handle, 1, &ct_event);
			// struct ptl_cq *ptl_cq = ptl_cq_get_instance(NULL);
			//  again:;
			// ptl_event_t event;
			// rc = PtlEQWait(ptl_cq_get_queue(ptl_cq), &event);
			// if (PTL_OK != rc) {
			// 	SPDK_PTL_FATAL("PtlEQWait failed code: %d", rc);
			// }
			// SPDK_PTL_DEBUG("Ok got event rc is %d event type %d!", rc, event.type);
			//    if(event.type != PTL_EVENT_SEND && event.type != PTL_EVENT_ACK){
			//      SPDK_PTL_FATAL("Unexpected event type: %d",event.type);
			//    }
			//    if(event.type != PTL_EVENT_ACK)
			//      goto again;
		}

	}
	SPDK_PTL_DEBUG("======>  Done with the send list spin loop and shit busy wait");
	// rc = ibv_post_send(spdk_rdma_qp->qp, spdk_rdma_qp->send_wrs.first, bad_wr);

	spdk_rdma_qp->send_wrs.first = NULL;
	spdk_rdma_qp->stats->send.doorbell_updates++;
  sleep(10);
	return 0;

}

bool
spdk_rdma_provider_accel_sequence_supported(void)
{
	SPDK_PTL_DEBUG("No accel sequence supported in PORTALS");
	return false;
}

