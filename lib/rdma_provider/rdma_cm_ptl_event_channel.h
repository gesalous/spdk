#ifndef RDMA_CM_PTL_EVENT_CHANNEL
#define RDMA_CM_PTL_EVENT_CHANNEL
#include "ptl_log.h"
#include "spdk_ptl_macros.h"
#include <pthread.h>
#include <rdma/rdma_cma.h>
#include <spdk/util.h>
#include <stdint.h>
#define RDMA_CM_PTL_EVENT_CHANNEL_MAGIC_NUMBER 270883UL

struct rdma_cm_ptl_event_channel {
	uint64_t magic_number;
	struct dlist *open_fake_connections;
	struct deque *events_deque;
	pthread_mutex_t events_deque_lock;
	struct rdma_event_channel fake_channel;
#if RDMA_CM_PTL_BLOCKING_CHANNEL
	sem_t sem;
#endif
};

static inline struct rdma_cm_ptl_event_channel *
rdma_cm_ptl_event_channel_get(struct rdma_event_channel *channel)
{
	struct rdma_cm_ptl_event_channel *ptl_channel = SPDK_CONTAINEROF(
			channel, struct rdma_cm_ptl_event_channel, fake_channel);
	if (ptl_channel->magic_number !=
	    RDMA_CM_PTL_EVENT_CHANNEL_MAGIC_NUMBER) {
		SPDK_PTL_FATAL(
			"Corrupted PORTALS channel value is %lu instead of %lu",
			ptl_channel->magic_number,
			RDMA_CM_PTL_EVENT_CHANNEL_MAGIC_NUMBER);
	}
	return ptl_channel;
}

static inline void rdma_cm_ptl_event_channel_lock_event_deque(
	struct rdma_cm_ptl_event_channel *ptl_channel)
{
	RDMA_CM_LOCK(&ptl_channel->events_deque_lock);
}

static inline void rdma_cm_ptl_event_channel_unlock_event_deque(
	struct rdma_cm_ptl_event_channel *ptl_channel)
{
	RDMA_CM_UNLOCK(&ptl_channel->events_deque_lock);
}
#endif
