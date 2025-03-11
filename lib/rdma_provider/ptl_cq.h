#ifndef PTL_CQ_H
#define PTL_CQ_H
#include "lib/rdma_provider/portals_log.h"
#include "ptl_object_types.h"
#include "spdk/util.h"
#include <infiniband/verbs.h>
#include <portals4.h>
#include <pthread.h>
#include <stdint.h>
#define PTL_CQ_SIZE 4096
struct ptl_context;

struct ptl_cq {
	ptl_obj_type_e object_type;
	struct ibv_cq fake_ibv_cq;
	struct ptl_context *ptl_context;
	ptl_handle_eq_t eq_handle;
	void *cq_context;
	pthread_mutex_t lock;
	bool initialized;
};

struct ptl_cq *ptl_cq_get_instance(void *cq_context);

static inline ptl_handle_eq_t ptl_cq_get_queue(struct ptl_cq *ptl_cq)
{
	if (false == ptl_cq->initialized) {
		SPDK_PTL_FATAL("Uninitialized event queue");
	}
	return ptl_cq->eq_handle;
}

static inline struct ptl_cq *ptl_cq_get_from_ibv_cq(struct ibv_cq *ibv_cq)
{
	struct ptl_cq *ptl_cq = SPDK_CONTAINEROF(ibv_cq, struct ptl_cq, fake_ibv_cq);
	if (PTL_CQ != ptl_cq->object_type) {
		SPDK_PTL_FATAL("Corrupted ptl_cq");
	}
	return ptl_cq;
}

static inline struct ibv_cq *ptl_cq_get_ibv_cq(struct ptl_cq *ptl_cq)
{
	return &ptl_cq->fake_ibv_cq;
}

#endif
