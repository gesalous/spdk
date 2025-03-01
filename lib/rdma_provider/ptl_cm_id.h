#ifndef PTL_CM_ID_H
#define PTL_CM_ID_H
#include "lib/rdma_provider/portals_log.h"
#include "lib/rdma_provider/ptl_context.h"
#include "lib/rdma_provider/ptl_cq.h"
#include "lib/rdma_provider/ptl_pd.h"
#include "lib/rdma_provider/ptl_qp.h"
#include "ptl_object_types.h"
#include "spdk/util.h"
#include <rdma/rdma_cma.h>
#include <stdint.h>
#include <sys/socket.h>

struct ptl_cm_id {
	ptl_obj_type_e object_type;
	struct rdma_cm_ptl_event_channel *ptl_channel;
	struct sockaddr src_addr;
	struct sockaddr dest_addr;
	struct rdma_cm_id fake_cm_id;
	/*As in verbs, each ptl_cm_id associates with a single ptl_pd*/
	struct ptl_pd *ptl_pd;
	struct ptl_qp *ptl_qp;
	struct ptl_cq *send_queue;
	struct ptl_cq *recv_queue;
	struct ptl_context *ptl_context;
	//needed for connection setup and shit
	const void *fake_data;
};



struct ptl_cm_id *ptl_cm_id_create(struct rdma_cm_ptl_event_channel * event_channel, void *context);

static inline struct ptl_cm_id *ptl_cm_id_get(struct rdma_cm_id *id)
{
	struct ptl_cm_id *ptl_id =
		SPDK_CONTAINEROF(id, struct ptl_cm_id, fake_cm_id);
	if (PTL_CM_ID != ptl_id->object_type) {
		SPDK_PTL_FATAL("Corrupted PTL ID");
	}
	return ptl_id;
}

void ptl_cm_id_create_event(struct ptl_cm_id *ptl_id,
			    struct rdma_cm_id *id,
			    enum rdma_cm_event_type event_type);

static inline struct sockaddr *
rdma_cm_ptl_id_get_src_addr(struct ptl_cm_id *ptl_id)
{
	return &ptl_id->src_addr;
}

void ptl_cm_id_set_fake_data(struct ptl_cm_id *ptl_id, const void *fake_data);

static inline void ptl_cm_id_set_ptl_qp(struct ptl_cm_id *ptl_id, struct ptl_qp *ptl_qp)
{
	if (ptl_id->ptl_qp) {
		SPDK_PTL_FATAL("PTL QP already set");
	}
	ptl_id->ptl_qp = ptl_qp;
	ptl_id->fake_cm_id.qp = ptl_qp_get_ibv_qp(ptl_qp);
}

static inline void ptl_cm_id_set_send_queue(struct ptl_cm_id *ptl_id, struct ptl_cq *send_queue)
{
	if (ptl_id->send_queue) {
		SPDK_PTL_FATAL("Send queue already set");
	}
	ptl_id->send_queue = send_queue;
	ptl_id->fake_cm_id.send_cq = ptl_cq_get_ibv_cq(send_queue);
}

static inline void ptl_cm_id_set_recv_queue(struct ptl_cm_id *ptl_id, struct ptl_cq *recv_queue)
{
	if (ptl_id->recv_queue) {
		SPDK_PTL_FATAL("Recv queue already set");
	}
	ptl_id->recv_queue = recv_queue;
	ptl_id->fake_cm_id.recv_cq = ptl_cq_get_ibv_cq(recv_queue);
}

static inline void ptl_cm_id_set_ptl_pd(struct ptl_cm_id *ptl_id, struct ptl_pd *ptl_pd)
{
	if (ptl_id->ptl_pd) {
		SPDK_PTL_FATAL("PTL PD already set");
	}
	ptl_id->ptl_pd = ptl_pd;
	ptl_id->fake_cm_id.pd = ptl_pd_get_ibv_pd(ptl_pd);
	/*set also context as in the verbs case*/
	ptl_id->ptl_context = ptl_pd_get_cnxt(ptl_pd);
	ptl_id->fake_cm_id.context = ptl_cnxt_get_ibv_context(ptl_pd_get_cnxt(ptl_pd));
}
#endif
