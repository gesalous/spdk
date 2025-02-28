#include "ptl_cm_id.h"
#include "deque.h"
#include "lib/rdma_provider/ptl_object_types.h"
#include "portals_log.h"
#include "ptl_context.h"
#include "rdma_cm_ptl_event_channel.h"
#include "spdk_ptl_macros.h"
#include <assert.h>
#include <stdlib.h>

struct ptl_cm_id *ptl_cm_id_create(struct rdma_cm_ptl_event_channel *ptl_channel, void *context)
{
	struct ptl_context * ptl_context;
	struct ptl_cm_id * ptl_id = calloc(1UL, sizeof(*ptl_id));
	if (NULL == ptl_id) {
		SPDK_PTL_FATAL("Failed to allocate memory");
	}
	ptl_id->object_type = PTL_CM_ID;
	/*o mpampas sas*/
	ptl_id->ptl_channel = ptl_channel;
	ptl_id->fake_cm_id.context = context;
	ptl_context = ptl_cnxt_get();
	ptl_id->fake_cm_id.verbs = ptl_cnxt_get_ibv_context(ptl_context);
	SPDK_PTL_DEBUG("=====> SUCCESSFULLY created PTL_ID");
	return ptl_id;
}

void ptl_cm_id_create_event(struct ptl_cm_id *ptl_id,
			    struct rdma_cm_id *id,
			    enum rdma_cm_event_type event_type)
{

	struct rdma_cm_event *fake_event;
	/*Create a fake event*/
	fake_event = calloc(1UL, sizeof(struct rdma_cm_event));
	if (!fake_event) {
		SPDK_PTL_FATAL("No memory!");
	}
	fake_event->id = id;
	assert(id->context);
	fake_event->status = 0;
	fake_event->event = event_type;
	fake_event->param.conn.private_data = ptl_id->fake_data;

	rdma_cm_ptl_event_channel_lock_event_deque(ptl_id->ptl_channel);
	if (false ==
	    deque_push_front(ptl_id->ptl_channel->events_deque, fake_event)) {
		SPDK_PTL_FATAL("Failed to queue fake event");
	}
	SPDK_PTL_DEBUG(" ********* Added event of type: %d", event_type);
	rdma_cm_ptl_event_channel_unlock_event_deque(ptl_id->ptl_channel);
	/*wake up guys waiting*/
	// if (-1 == sem_post(&ptl_id->ptl_channel->sem)) {
	//   perror("sem_post failed REASON:");
	//   SPDK_PTL_FATAL("Sorry sem_post failed bye!");
	// }
}

void ptl_cm_id_set_fake_data(struct ptl_cm_id *ptl_id,
				  const void *fake_data)
{
	ptl_id->fake_data = fake_data;
}
