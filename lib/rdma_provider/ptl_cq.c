#include "ptl_cq.h"
#include "ptl_config.h"
#include "ptl_context.h"
#include "ptl_log.h"
#include "ptl_macros.h"
#include "ptl_object_types.h"
#include <portals4.h>
#include <stdint.h>

struct ptl_cq *ptl_cq_get_instance(void *cq_context)
{
	static struct ptl_cq event_queue = {.lock = PTHREAD_MUTEX_INITIALIZER,
						    .object_type = PTL_CQ
	};
	struct ptl_cq *ptl_cq;
	int ret;
	RDMA_CM_LOCK(&event_queue.lock);
	if (event_queue.initialized) {
		ptl_cq = &event_queue;
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
			   ptl_cq->eq_handle, PTL_PT_INDEX, &ptl_cq->ptl_context->portals_idx_send_recv);
	if (ret != PTL_OK) {
		SPDK_PTL_FATAL("PtlPTAlloc failed for SEND/RECV PORTALS INDEX");
	}
	SPDK_PTL_DEBUG("Allocated portals index: %u for *ALL* (send/recv/rma) operations",
		       ptl_cq->ptl_context->portals_idx_send_recv);

	ptl_cq->cq_context = cq_context;

	SPDK_PTL_DEBUG("Initialized event queue! %p", ptl_cq);
exit:
	RDMA_CM_UNLOCK(&event_queue.lock);
	return ptl_cq;
}

