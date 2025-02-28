#include "ptl_qp.h"
#include "lib/rdma_provider/portals_log.h"
#include "lib/rdma_provider/ptl_cm_id.h"
#include "lib/rdma_provider/ptl_cq.h"
#include "lib/rdma_provider/ptl_pd.h"
#include "portals_log.h"
#include "ptl_cm_id.h"
#include <stdlib.h>

struct ptl_qp *ptl_qp_create(struct ptl_pd *ptl_pd, struct ptl_cq *send_queue,
			     struct ptl_cq *receive_queue)
{
	assert(ptl_pd);
	assert(send_queue);
	assert(receive_queue);

	struct ptl_qp *ptl_qp = calloc(1UL, sizeof(*ptl_qp));
	if (NULL == ptl_qp) {
		SPDK_PTL_FATAL("Failed to allocate memory for portals queue pair");
	}
	ptl_qp->object_type = PTL_QP;
	/*pd related staff*/
	ptl_qp->ptl_pd = ptl_pd;
	ptl_qp->fake_qp.pd = ptl_pd_get_ibv_pd(ptl_pd);
	/*cq related staff*/
	ptl_qp->send_cq = send_queue;
	ptl_qp->fake_qp.send_cq = ptl_cq_get_ibv_cq(send_queue);
	ptl_qp->recv_cq = receive_queue;
	ptl_qp->fake_qp.recv_cq = ptl_cq_get_ibv_cq(receive_queue);
	return ptl_qp;
}
