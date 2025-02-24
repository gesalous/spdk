#include "deque.h"
#include "lib/rdma_provider/dlist.h"
#include "portals_log.h"
#include "ptl_context.h"
#include "rdma_cm_ptl_event_channel.h"
#include "rdma_cm_ptl_id.h"
#include "spdk/util.h"
#include "spdk_ptl_macros.h"
#include <dlfcn.h>
#include <infiniband/verbs.h>
#include <portals4.h>
#include <rdma/rdma_cma.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>
#define RDMA_CM_PTL_EQ_SIZE 16
#define RDMA_CM_PTL_BLOCKING_CHANNEL 0


struct rdma_event_channel *
rdma_create_event_channel(void)
{
	SPDK_PTL_DEBUG("RDMACM: Intercepted rdma_create_event_channel()");
	struct rdma_cm_ptl_event_channel *ptl_channel = calloc(1UL, sizeof(*ptl_channel));
	if (!ptl_channel) {
		SPDK_PTL_DEBUG("RDMACM: Allocation of memory failed");
		return NULL;
	}
	ptl_channel->magic_number = RDMA_CM_PTL_EVENT_CHANNEL_MAGIC_NUMBER;
#if RDMA_CM_PTL_BLOCKING_CHANNEL
	if (sem_init(&ptl_channel->sem, 0, 0) == -1) {
		perror("sem init failed reason:");
		SPDK_PTL_FATAL("sem init failed");
	}
#endif
	ptl_channel->open_fake_connections = dlist_create(NULL, NULL);
	RDMA_CM_LOCK_INIT(&ptl_channel->events_deque_lock);
	ptl_channel->events_deque = deque_create(NULL);

	struct rdma_event_channel *channel = &ptl_channel->fake_channel;

	channel->fd = eventfd(0, EFD_NONBLOCK);
	if (channel->fd < 0) {
		free(ptl_channel);
		SPDK_PTL_DEBUG("RDMACM: eventfd failed. Reason follows:");
		perror("Reason:");
		return NULL;
	}
	return channel;
}

struct ibv_context **rdma_get_devices(int *num_devices)
{
	struct ibv_context **devices;
	struct ptl_context *cnxt;

	SPDK_PTL_DEBUG("RDMACM: Intercepted rdma_get_devices");
	SPDK_PTL_DEBUG("RDMACM: Calling PtlInit()");
	cnxt = ptl_cnxt_get();

	devices = calloc(1UL, 2 * sizeof(struct ibv_context *));
	if (NULL == devices) {
		SPDK_PTL_FATAL("RDMACM: Failed to allocate memory for device list");
	}

	devices[0] = ptl_cnxt_get_ibv_context(cnxt);
	devices[0]->async_fd = eventfd(0, EFD_NONBLOCK);
	if (devices[0]->async_fd < 0) {
		SPDK_PTL_FATAL("RDMACM: Failed to create async fd");
	}
	SPDK_PTL_DEBUG("RDMACM: Created this async_fd thing");
	devices[0]->device = calloc(1UL, sizeof(struct ibv_device));
	if (NULL == devices[0]->device) {
		SPDK_PTL_FATAL("RDMACM: No memory");
	}
	// Set device name and other attributes
	strcpy(devices[0]->device->name, "portals_device");
	strcpy(devices[0]->device->dev_name, "bxi0");
	strcpy(devices[0]->device->dev_path, "/dev/portals0");
	if (num_devices) {
		*num_devices = 1;
	}
	devices[1] = NULL;
	SPDK_PTL_DEBUG("RDMACM: Initialization DONE with portals Initialization, encapsulated portals_context inside ibv_context");
	return devices;
}

/* Subset of libverbs that Nida implements so nvmf target can boot*/
int ibv_query_device(struct ibv_context *context,
		     struct ibv_device_attr *device_attr)
{

	// Zero out the structure first
	memset(device_attr, 0, sizeof(struct ibv_device_attr));

	device_attr->vendor_id = 0x02c9;       //Fake Mellanox id
	device_attr->vendor_part_id = 0x1017;  // Fake Mellanox part id
	device_attr->max_qp = 256;             // Number of QPs you'll support
	device_attr->max_cq = 256;             // Number of CQs
	device_attr->max_mr = 256;             // Number of Memory Regions
	device_attr->max_pd = 256;             // Number of Protection Domains
	device_attr->max_qp_wr = 4096;         // Max Work Requests per QP
	device_attr->max_cqe = 4096;           // Max CQ entries
	device_attr->max_mr_size = UINT64_MAX; // Max size of Memory Region
	device_attr->max_sge = 32;             // Max Scatter/Gather Elements
	device_attr->max_sge_rd = 32;          // Max SGE for RDMA read
	device_attr->max_qp_rd_atom = 16;      // Max outstanding RDMA reads
	device_attr->max_qp_init_rd_atom = 16; // Initial RDMA read resources
	device_attr->max_srq = 256;            // Max Shared Receive Queues
	device_attr->max_srq_wr = 4096;        // Max SRQ work requests
	device_attr->max_srq_sge = 32;         // Max SGE for SRQ

	// Set capabilities flags
	device_attr->device_cap_flags =
		IBV_DEVICE_RESIZE_MAX_WR |  // Support QP/CQ resize
		IBV_DEVICE_BAD_PKEY_CNTR |  // Support bad pkey counter
		IBV_DEVICE_BAD_QKEY_CNTR |  // Support bad qkey counter
		IBV_DEVICE_RAW_MULTI |      // Support raw packet QP
		IBV_DEVICE_AUTO_PATH_MIG |  // Support auto path migration
		IBV_DEVICE_CHANGE_PHY_PORT; // Support changing physical port
	SPDK_PTL_DEBUG("IBVPTL: Trapped ibv_query_device *FILLED* it with reasonable values...DONE");
	return 0;
}

struct ibv_pd *ibv_alloc_pd(struct ibv_context *context)
{
	struct ptl_context *cnxt = ptl_cnxt_get_from_ibcnxt(context);
	SPDK_PTL_DEBUG("IBVPTL: OK trapped ibv_alloc_pd sending dummy pd portals "
		       "does not need it");
	return ptl_cnxt_get_ibv_pd(cnxt);
}

struct ibv_context *ibv_open_device(struct ibv_device *device)
{
	SPDK_PTL_FATAL("UNIMPLEMENTED");
}

struct ibv_cq *ibv_create_cq(struct ibv_context *context, int cqe,
			     void *cq_context, struct ibv_comp_channel *channel,
			     int comp_vector)
{

	ptl_handle_eq_t eq_handle;
	struct ptl_context *ptl_context = ptl_cnxt_get_from_ibcnxt(context);
	SPDK_PTL_DEBUG("IBVPTL: Ok trapped ibv_create_cq time to create the event queue in portals");

	int ret = PtlEQAlloc(ptl_cnxt_get_ni_handle(ptl_context), RDMA_CM_PTL_EQ_SIZE, &eq_handle);
	if (ret != PTL_OK) {
		SPDK_PTL_FATAL("PtlEQAlloc failed with error code %d", ret);
	}
	ptl_cnxt_set_eq(ptl_context, eq_handle);
	SPDK_PTL_DEBUG("Ok set up event queue for PORTALS :-)");

	return ptl_cnxt_get_fake_ibv_cq(ptl_context);
}

// Caution! Due to inlining of ibv_poll_cq SPDK_PTL overrides it also in
// ptl_context
// int ibv_poll_cq(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc) {
//     SPDK_PTL_DEBUG("Intercepted ibv_poll_cq");
//     SPDK_PTL_FATAL("UNIMPLEMENTED");
//     return -1;
// }

int rdma_create_id(struct rdma_event_channel *channel, struct rdma_cm_id **id,
		   void *context, enum rdma_port_space ps)
{
	struct rdma_cm_ptl_id *ptl_id;
	struct ptl_context *ptl_cnxt;
	struct rdma_cm_ptl_event_channel *ptl_channel;
  ptl_channel = rdma_cm_ptl_event_channel_get(channel);
	//XXX TODO XXX move this code fragment into rdma_cm_ptl_id.c
	ptl_id = calloc(1UL, sizeof(*ptl_id));
	/*o mpampas sas*/
	ptl_id->ptl_channel = ptl_channel;
	ptl_id->fake_cm_id.context = context;
	ptl_cnxt = ptl_cnxt_get();
	ptl_id->fake_cm_id.verbs = ptl_cnxt_get_ibv_context(ptl_cnxt);
	ptl_id->magic_number = RDMA_CM_PTL_ID_MAGIG_NUMBER;
	/*Caution wiring need it, it is accessed later*/
	ptl_id->fake_cm_id.qp = &ptl_id->fake_qp;
	*id = &ptl_id->fake_cm_id;
	dlist_append(ptl_channel->open_fake_connections, ptl_id);
	SPDK_PTL_DEBUG("Trapped create cm id FAKED it, waking up possible guys for the event");
#if RDMA_CM_PTL_BLOCKING_CHANNEL
	if (sem_post(&ptl_channel->sem) == -1) {
		perror("sem_post failed REASON:");
		SPDK_PTL_FATAL("Sorry sem_post failed bye!");
	}
#endif
	return 0;
}

int rdma_bind_addr(struct rdma_cm_id *id, struct sockaddr *addr)
{
	SPDK_PTL_DEBUG("Trapped rdma_bind_addr FAKED it");
	struct rdma_cm_ptl_id *ptl_id = rdma_cm_ptl_id_get(id);

	return 0;
}

int rdma_listen(struct rdma_cm_id *id, int backlog)
{
	SPDK_PTL_DEBUG("Trapped rdma listen, FAKED IT");
	struct rdma_cm_ptl_id *ptl_id = rdma_cm_ptl_id_get(id);
	return 0;
}

int rdma_get_cm_event(struct rdma_event_channel *channel,
		      struct rdma_cm_event **event)
{

	SPDK_PTL_DEBUG(" --------> Going to take an event...");
	struct rdma_cm_ptl_event_channel *ptl_channel;
	struct rdma_cm_event *fake_event;
  ptl_channel = rdma_cm_ptl_event_channel_get(channel);
#if RDMA_CM_PTL_BLOCKING_CHANNEL
get_event:
#endif
	fake_event = NULL;
	RDMA_CM_LOCK(&ptl_channel->events_deque_lock);
	fake_event = deque_pop_front(ptl_channel->events_deque);
	RDMA_CM_UNLOCK(&ptl_channel->events_deque_lock);

	// if(fake_event)
	//   break;
	// SPDK_PTL_DEBUG("No event going to sleep...");
	// sem_wait(&ptl_channel->sem);
	// SPDK_PTL_DEBUG("Got something woke up");
	*event = fake_event;

	if (fake_event) {
		SPDK_PTL_DEBUG(" -------> OK got event!");
		return 0;
	}
#if RDMA_CM_PTL_BLOCKING_CHANNEL
	sem_wait(&ptl_channel->sem);
	goto get_event;
#else
	SPDK_PTL_DEBUG("Got nothing shit EAGAIN!");
#endif
	return EAGAIN;
}

int rdma_ack_cm_event(struct rdma_cm_event *event)
{
	SPDK_PTL_DEBUG("ACK CM event");
	free(event);
	return 0;
}

static void rdma_cm_print_addr_info(const char *prefix, struct sockaddr *addr)
{
	char ip_str[INET6_ADDRSTRLEN] = {0};
	uint16_t port = 0;

	if (!addr) {
		SPDK_PTL_INFO("%s: NULL address", prefix);
		return;
	}

	switch (addr->sa_family) {
	case AF_INET: {
		struct sockaddr_in *ipv4 = (struct sockaddr_in *)addr;
		inet_ntop(AF_INET, &(ipv4->sin_addr), ip_str, INET6_ADDRSTRLEN);
		port = ntohs(ipv4->sin_port);
		SPDK_PTL_DEBUG("%s: IPv4 %s:%u", prefix, ip_str, port);
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)addr;
		inet_ntop(AF_INET6, &(ipv6->sin6_addr), ip_str, INET6_ADDRSTRLEN);
		port = ntohs(ipv6->sin6_port);
		SPDK_PTL_DEBUG("%s: IPv6 %s:%u", prefix, ip_str, port);
		break;
	}
	default:
		SPDK_PTL_DEBUG("%s: Unknown address family %d", prefix, addr->sa_family);
	}
}




static struct sockaddr *rdma_cm_find_matching_local_ip(struct sockaddr *dst_addr)
{
	struct ifaddrs *ifaddr, *ifa;
	struct sockaddr *result = NULL;

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs, Reason");
		SPDK_PTL_FATAL("Sorry");
		return NULL;
	}

	struct sockaddr_in *dst = (struct sockaddr_in *)dst_addr;

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL) { continue; }
		if (ifa->ifa_addr->sa_family != AF_INET) { continue; }
		if (ifa->ifa_flags & IFF_LOOPBACK) { continue; }

		struct sockaddr_in *src = (struct sockaddr_in *)ifa->ifa_addr;
		struct sockaddr_in *netmask = (struct sockaddr_in *)ifa->ifa_netmask;

		if ((src->sin_addr.s_addr & netmask->sin_addr.s_addr) ==
		    (dst->sin_addr.s_addr & netmask->sin_addr.s_addr)) {

			// Allocate and copy the matching address
			result = calloc(1UL, sizeof(struct sockaddr));
			if (result == NULL) {
				goto exit;
			}
			memcpy(result, ifa->ifa_addr, sizeof(struct sockaddr));
			goto exit;
		}
	}
exit:
	freeifaddrs(ifaddr);
	return result;
}

int rdma_resolve_addr(struct rdma_cm_id *id, struct sockaddr *src_addr,
		      struct sockaddr *dst_addr, int timeout_ms)
{

	struct rdma_cm_ptl_id *ptl_id = rdma_cm_ptl_id_get(id);
	ptl_id->dest_addr = *dst_addr;
	struct sockaddr *resolve_src_addr =
		src_addr ? src_addr : rdma_cm_find_matching_local_ip(dst_addr);

	if (NULL == resolve_src_addr) {
		rdma_cm_print_addr_info("DESTINATION IS:", dst_addr);
		SPDK_PTL_FATAL("Could not find a local ip to match destination address");
	}

	ptl_id->src_addr = *resolve_src_addr;

	if (NULL == src_addr) {
		free(resolve_src_addr);
	}

	SPDK_PTL_DEBUG("----------->   Resolved src addr   <--------------");
	rdma_cm_print_addr_info("SOURCE_ADDR = ", &ptl_id->src_addr);
	rdma_cm_ptl_id_create_event(ptl_id, id, RDMA_CM_EVENT_ADDR_RESOLVED);
	SPDK_PTL_DEBUG("Ok stored dst addr and generated fake "
		       "RDMA_CM_EVENT_ADDR_RESOLVED event");
	return 0;
}

int rdma_resolve_route(struct rdma_cm_id *id, int timeout_ms)
{
	SPDK_PTL_DEBUG("RESOLVE ROUTE");
	// rdma_print_addr_info("SOURCE", src_addr);
	// rdma_print_addr_info("DESTINATION", dst_addr);
	struct rdma_cm_ptl_id *ptl_id = rdma_cm_ptl_id_get(id);
	rdma_cm_ptl_id_create_event(ptl_id, id, RDMA_CM_EVENT_ROUTE_RESOLVED);
	return 0;
}

int rdma_create_qp(struct rdma_cm_id *id, struct ibv_pd *pd,
		   struct ibv_qp_init_attr *qp_init_attr)
{
	struct rdma_cm_ptl_id *ptl_id = rdma_cm_ptl_id_get(id);
	ptl_id->qp_init_attr = *qp_init_attr;
	ptl_id->fake_qp.pd = ptl_cnxt_get_ibv_pd(ptl_cnxt_get());
	SPDK_PTL_DEBUG("Initialized only the pd of the fake qp");
	return 0;
}

int rdma_connect(struct rdma_cm_id *id, struct rdma_conn_param *conn_param)
{
	struct rdma_cm_ptl_id *ptl_id = rdma_cm_ptl_id_get(id);
  rdma_cm_ptl_id_set_fake_data(ptl_id, conn_param->private_data);
	rdma_cm_ptl_id_create_event(ptl_id, id, RDMA_CM_EVENT_CONNECT_RESPONSE);
	SPDK_PTL_DEBUG("FAKED successfull connection");
	return 0;
}

int rdma_set_option(struct rdma_cm_id *id, int level, int optname, void *optval,
		    size_t optlen)
{
	SPDK_PTL_INFO("Ignoring (for) now options XXX TODO XXX");
	return 0;
}

