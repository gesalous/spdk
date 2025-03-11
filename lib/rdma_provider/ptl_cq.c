#include "ptl_cq.h"
#include "lib/rdma_provider/portals_log.h"
#include "lib/rdma_provider/ptl_context.h"
#include "lib/rdma_provider/ptl_object_types.h"
#include "portals4.h"
#include "spdk_ptl_macros.h"
#define SPDK_PORTALS_ID 0


struct ptl_cq *ptl_cq_get_instance(void *cq_context)
{
	static struct ptl_cq event_queue = {.lock = PTHREAD_MUTEX_INITIALIZER,
						    .object_type = PTL_CQ
	};

	struct ptl_cq *ptl_cq;
	int ret;
	RDMA_CM_LOCK(&event_queue.lock);
	if (event_queue.initialized) {
		goto exit;
	}


	event_queue.initialized = true;
	ptl_cq = &event_queue;
	ptl_cq->ptl_context = ptl_cnxt_get();
	ptl_cq->fake_ibv_cq.context = ptl_cnxt_get_ibv_context(ptl_cq->ptl_context);
	ptl_handle_ni_t nic = ptl_cnxt_get_ni_handle(ptl_cq->ptl_context);
	ret = PtlEQAlloc(nic, PTL_CQ_SIZE, &ptl_cq->eq_handle);
	if (ret != PTL_OK) {
		SPDK_PTL_FATAL("PtlEQAlloc failed with error code %d", ret);
	}
	ret =
		PtlPTAlloc(ptl_cnxt_get_ni_handle(ptl_cq->ptl_context), 0,
			   ptl_cq->eq_handle, SPDK_PORTALS_ID, &ptl_cq->ptl_context->portals_idx);
	if (ret != PTL_OK) {
		SPDK_PTL_FATAL("PtlPTAlloc failed");
	}
	SPDK_PTL_DEBUG("Allocated portals index: %u", ptl_cq->ptl_context->portals_idx);

	ptl_cq->cq_context = cq_context;

	SPDK_PTL_DEBUG("Initialized event queue! %p", ptl_cq);
exit:
	RDMA_CM_UNLOCK(&event_queue.lock);
	return ptl_cq;
}

