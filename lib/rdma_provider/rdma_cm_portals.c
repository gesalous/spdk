#include "deque.h"
#include "dlist.h"
#include "lib/rdma_provider/ptl_object_types.h"
#include "ptl_cm_id.h"
#include "ptl_config.h"
#include "ptl_connection.h"
#include "ptl_context.h"
#include "ptl_cq.h"
#include "ptl_log.h"
#include "ptl_macros.h"
#include "ptl_pd.h"
#include "ptl_qp.h"
#include "ptl_uuid.h"
#include "rdma_cm_ptl_event_channel.h"
#include "spdk/util.h"
#include <asm-generic/errno-base.h>
#include <dlfcn.h>
#include <infiniband/verbs.h>
#include <portals4.h>
#include <pthread.h>
#include <rdma/rdma_cma.h>
#include <semaphore.h>
#include <spdk/nvmf_spec.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#define RDMA_PTL_MAX_CONNECTIONS 128UL

#define RDMA_PTL_IGNORE 0xffffffff

#define RDMA_PTL_MATCH 1

#define RDMA_PTL_SRV_ME_OPTS                                                                                                 \
	PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_MAY_ALIGN | PTL_ME_IS_ACCESSIBLE | PTL_ME_MANAGE_LOCAL | \
		PTL_ME_NO_TRUNCATE | PTL_LE_USE_ONCE



#define PTL_CP_SERVER_LOCK(X) do { \
    int ret = pthread_mutex_lock(X); \
    if (ret != 0) { \
        SPDK_PTL_FATAL("pthread_mutex_lock failed: %s", strerror(ret)); \
    } \
} while(0)

#define PTL_CP_SERVER_UNLOCK(X) do { \
    int ret = pthread_mutex_unlock(X); \
    if (ret != 0) { \
        SPDK_PTL_FATAL("pthread_mutex_lock failed: %s", strerror(ret)); \
    } \
} while(0)



static struct sockaddr *rdma_cm_find_matching_local_ip(struct sockaddr *address,
		struct sockaddr *result)
{
	struct ifaddrs *ifaddr, *ifa;

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs, Reason");
		SPDK_PTL_FATAL("Sorry");
		return NULL;
	}

	struct sockaddr_in *dst = (struct sockaddr_in *)address;

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL) { continue; }
		if (ifa->ifa_addr->sa_family != AF_INET) { continue; }
		if (ifa->ifa_flags & IFF_LOOPBACK) { continue; }

		struct sockaddr_in *src = (struct sockaddr_in *)ifa->ifa_addr;
		struct sockaddr_in *netmask = (struct sockaddr_in *)ifa->ifa_netmask;

		if ((src->sin_addr.s_addr & netmask->sin_addr.s_addr) ==
		    (dst->sin_addr.s_addr & netmask->sin_addr.s_addr)) {

			memcpy(result, ifa->ifa_addr, sizeof(struct sockaddr));
			goto exit;
		}
	}
exit:
	freeifaddrs(ifaddr);
	return result;
}

/**
 * Used for faking connection setup
 */
static struct spdk_nvmf_rdma_request_private_data private_data = {
	.recfmt = 0,
	.hrqsize = 128,
	.hsqsize = 128,
	.qid = 1
};



typedef enum {
	PTL_CP_SERVER_NOT_RUNNING = 10,
	PTL_CP_SERVER_BOOTING,
	PTL_CP_SERVER_RUNNING
} rdma_ptl_cp_server_status_e;


struct rdma_ptl_send_buffer {
	struct ptl_conn_msg conn_msg;
	ptl_handle_md_t md_handle;
};


static struct rdma_ptl_cp_server {
	pthread_mutex_t init_lock;
	/*Dynamically allocated array of receive buffers for new connection messages (new, close, etc)*/
	struct ptl_conn_msg *recv_buffers;
	const char *role;
	ptl_handle_le_t *le_handle;
	uint32_t num_conn_info;
	ptl_handle_eq_t eq_handle;
	ptl_pt_index_t pt_index;
	pthread_t cp_server_cnxt;
	volatile rdma_ptl_cp_server_status_e status;
	uint64_t protocol_version;
} ptl_control_plane_server = {.init_lock = PTHREAD_MUTEX_INITIALIZER, .status = PTL_CP_SERVER_NOT_RUNNING, .role = "Unknown"};



static struct rdma_ptl_connections {
	struct ptl_cm_id *ptl_id[RDMA_PTL_MAX_CONNECTIONS];
} conn_map;

static bool rdma_ptl_conn_map_add(struct ptl_cm_id *ptl_id)
{
	for (uint32_t i = 0; i < RDMA_PTL_MAX_CONNECTIONS; i++) {
		if (conn_map.ptl_id[i]) {
			continue;
		}
		conn_map.ptl_id[i] = ptl_id;
		SPDK_PTL_DEBUG("[%s] CP server: Added new connection in map with uuid: %lu",
			       ptl_control_plane_server.role, ptl_id->uuid);
		return true;
	}
	SPDK_PTL_FATAL("[%s] CP server: No room for new connections", ptl_control_plane_server.role);
	return false;/*No more space for new connections*/
}

static struct ptl_cm_id *rdma_ptl_conn_map_find_from_qp_num(int qp_num)
{

	for (uint32_t i = 0; i < RDMA_PTL_MAX_CONNECTIONS; i++) {
		if (NULL == conn_map.ptl_id[i]) {
			continue;
		}

		if (conn_map.ptl_id[i]->ptl_qp_num == qp_num) {
			return conn_map.ptl_id[i];
		}
	}
	SPDK_PTL_FATAL("[%s] CP server: Connection with qp num: %d not found",
		       ptl_control_plane_server.role, qp_num);
	return NULL;

}


struct ptl_cm_id *rdma_ptl_conn_map_find_from_uuid(uint64_t uuid)
{

	for (uint32_t i = 0; i < RDMA_PTL_MAX_CONNECTIONS; i++) {
		if (NULL == conn_map.ptl_id[i]) {
			continue;
		}

		if (conn_map.ptl_id[i]->uuid == uuid) {
			return conn_map.ptl_id[i];
		}
	}
	SPDK_PTL_FATAL("[%s] CP server: Connection with uuid: %lu not found",
		       ptl_control_plane_server.role, uuid);
	return NULL;
}


static bool rdma_ptl_print_sockaddr(const struct sockaddr *addr)
{
	char ip_str[INET6_ADDRSTRLEN];  // Big enough for both IPv4 and IPv6
	uint16_t port;

	if (!addr) {
		SPDK_PTL_FATAL("NULL address");
	}

	// Handle based on address family
	switch (addr->sa_family) {
	case AF_INET: {
		struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
		inet_ntop(AF_INET, &(addr_in->sin_addr), ip_str, INET6_ADDRSTRLEN);
		port = ntohs(addr_in->sin_port);
		SPDK_PTL_DEBUG("IPv4 Address: %s, Port: %u", ip_str, port);
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
		inet_ntop(AF_INET6, &(addr_in6->sin6_addr), ip_str, INET6_ADDRSTRLEN);
		port = ntohs(addr_in6->sin6_port);
		printf("IPv6 Address: %s, Port: %u, Flow Info: %u, Scope ID: %u\n",
		       ip_str, port, addr_in6->sin6_flowinfo, addr_in6->sin6_scope_id);
		break;
	}
	case AF_UNIX: {
		struct sockaddr_un *addr_un = (struct sockaddr_un *)addr;
		printf("Unix Domain Socket Path: %s\n", addr_un->sun_path);
		break;
	}
	default:
		SPDK_PTL_FATAL("Unknown address family: %d", addr->sa_family);
	}
	return true;
}


/**
 * @brief Processes an open connection request
 */
static void rdma_ptl_handle_open_conn(struct ptl_cm_id *listen_id,
				      struct ptl_conn_msg *conn_msg)
{
	assert(listen_id);
	assert(conn_msg);
	struct rdma_cm_event *fake_event = {0};
	struct ptl_context *ptl_cnxt = ptl_cnxt_get();
	struct ptl_cq *ptl_cq = ptl_cq_get_instance(NULL);
	struct ptl_conn_open *conn_open = &conn_msg->conn_open;
	struct ptl_conn_open new_conn = {.dst_nid = conn_open->src_nid, .dst_pid = conn_open->src_pid, .src_pid = conn_open->dst_pid, .src_nid = conn_open->dst_nid};

	SPDK_PTL_DEBUG("[%s] Got an open connection request from: %d", ptl_control_plane_server.role,
		       rdma_ptl_print_sockaddr(&conn_open->src_addr));
	struct ptl_cm_id * ptl_id = ptl_cm_id_create(listen_id->ptl_channel, listen_id->ptl_context);
  ptl_id->uuid = ptl_uuid_set_target_qp_num(ptl_id->uuid, ptl_id->ptl_qp_num); 
  ptl_id->uuid = ptl_uuid_set_initiator_qp_num(ptl_id->uuid, conn_msg->conn_open.initiator_qp_num);
	SPDK_PTL_DEBUG("[%s] CP server: Created uuid: %lu", ptl_control_plane_server.role, ptl_id->uuid);

	rdma_ptl_conn_map_add(ptl_id);

	struct ptl_qp *ptl_qp = ptl_qp_create(ptl_cnxt->ptl_pd, ptl_cq, ptl_cq, &new_conn);
	ptl_id->ptl_qp = ptl_qp;

	ptl_id->fake_cm_id.qp = &ptl_qp->fake_qp;
	memcpy(&ptl_id->fake_cm_id.route.addr.dst_addr, &conn_open->src_addr, sizeof(conn_open->src_addr));

	rdma_cm_find_matching_local_ip(&ptl_id->fake_cm_id.route.addr.dst_addr,
				       &ptl_id->fake_cm_id.route.addr.src_addr);
	ptl_id->fake_cm_id.qp->qp_num = ptl_id->ptl_qp_num;


	SPDK_PTL_DEBUG("RDMA_LISTEN(): Create a fake connection event to establish queue pair");
	fake_event = ptl_cm_id_create_event(ptl_id, listen_id,
					    RDMA_CM_EVENT_CONNECT_REQUEST,
					    &private_data,
					    sizeof(private_data));

	ptl_cm_id_add_event(listen_id, fake_event);
	assert(ptl_id->ptl_channel);

	SPDK_PTL_DEBUG("Ok created the fake RDMA_CM_EVENT_CONNECT_REQUEST triggering "
		       "it through channel's async fd. QP num is: %d",
		       fake_event->id->qp->qp_num);
	uint64_t value = 1;
	if (write(listen_id->ptl_channel->fake_channel.fd, &value, sizeof(value)) !=
	    sizeof(value)) {
		perror("write to eventfd, reason:");
		SPDK_PTL_FATAL("Failed to write eventfd");
	}
}


static void rdma_ptl_handle_open_conn_reply(struct ptl_cm_id *listen_id,
		struct ptl_conn_msg *conn_msg)
{
	assert(listen_id != NULL);
	struct rdma_cm_event *fake_event;
	struct ptl_conn_open_reply *open_conn_reply = &conn_msg->conn_open_reply;
	struct ptl_cm_id *connection_id;

	if (PTL_OK != open_conn_reply->status) {
		SPDK_PTL_FATAL("[%s], CP server: connection failed with code: %d", ptl_control_plane_server.role,
			       open_conn_reply->status);
	}

  int qp_num = ptl_uuid_get_initiator_qp_num(conn_msg->conn_open_reply.uuid);
	SPDK_PTL_DEBUG("[%s] CP server: Got open connection reply! connection id is: %lu initiator qp num: %d target qp num: %d",
		       ptl_control_plane_server.role, open_conn_reply->uuid, qp_num, ptl_uuid_get_target_qp_num(conn_msg->conn_open_reply.uuid));
	connection_id = rdma_ptl_conn_map_find_from_qp_num(qp_num);
	if (connection_id == NULL) {
		SPDK_PTL_FATAL("[%s] CP server: Could not find connection with qp num: %d",
			       ptl_control_plane_server.role, qp_num);
	}
	connection_id->uuid = open_conn_reply->uuid;
	memcpy(&connection_id->conn_msg, conn_msg, sizeof(*conn_msg));


	/* Create fake RDMA event for compatibility */
	SPDK_PTL_DEBUG("[%s] Got open connection reply creating a fake event to proceed",
		       ptl_control_plane_server.role);
	fake_event = ptl_cm_id_create_event(connection_id, NULL, RDMA_CM_EVENT_CONNECT_RESPONSE,
					    connection_id->conn_param.private_data,
					    connection_id->conn_param.private_data_len);
	ptl_cm_id_add_event(connection_id, fake_event);
	SPDK_PTL_DEBUG("[%s] CP server: OPEN/Close request DONE",
		       ptl_control_plane_server.role);
	/* the conn_msg will be recycled by the main control plain server loop*/
}



static void rdma_ptl_handle_close_conn(struct ptl_cm_id *listen_id,
				       struct ptl_conn_msg *conn_msg)
{
	struct ptl_conn_close *conn_close = &conn_msg->conn_close;

	if (ptl_control_plane_server.protocol_version != conn_msg->msg_header.version) {
		SPDK_PTL_FATAL("[%s], PROTOCOL versions mismatch client uses: %lu %s: %lu",
			       ptl_control_plane_server.role, conn_msg->msg_header.version, ptl_control_plane_server.role,
			       ptl_control_plane_server.protocol_version);
	}

	SPDK_PTL_FATAL("[%s] Got close connection request for connection id: %lu",
		       ptl_control_plane_server.role, conn_close->uuid);

}


static void rdma_ptl_handle_close_conn_reply(struct ptl_cm_id *listen_id,
		struct ptl_conn_msg *conn_msg)
{

	struct ptl_conn_close_reply *close_reply = &conn_msg->conn_close_reply;

	if (PTL_OK != close_reply->status) {
		SPDK_PTL_FATAL("[%s], connection close failed with code: %d", ptl_control_plane_server.role,
			       close_reply->status);
	}

	SPDK_PTL_FATAL("[%s] Got a close connection reply! connection id is: %lu",
		       ptl_control_plane_server.role, close_reply->uuid);
}

/**
 * @brief Control plane server function that handles incoming Portals events
 *
 * This function implements the main event loop for the RDMA/Portals control
 * plane server. Its purpose it listen for new connection requests in order to
 * create a map from queue pair number to a (pid, nid) pair. This is needed to
 * be comIt continuously polls the event queue for incoming messages, processes
 * PUT operations, and manages the recycling of receive buffers through list
 * entries.
 *
 * The function performs the following operations:
 * - Waits for events using PtlEQWait()
 * - Validates that received events are PUT operations
 * - Logs information about the initiator (NID and PID)
 * - Re-registers receive buffers by creating new list entries
 *
 * @param args Pointer to function arguments (unused in current implementation)
 * @return void* Returns NULL (thread function requirement)
 *
 * @note This function runs in an infinite loop and should be launched as a
 * separate thread
 * @warning Function will terminate with SPDK_PTL_FATAL if non-PUT events are
 * received or if any Portals operations fail
 */
static void *rdma_run_ptl_cp_server(void *args)
{
	assert(args);
	struct ptl_cm_id *listen_id = args;
	struct ptl_conn_msg * conn_msg;
	struct ptl_context *ptl_cnxt = ptl_cnxt_get();
	struct rdma_ptl_send_buffer *send_buffer;
	ptl_event_t event;
	int rc;


	SPDK_PTL_DEBUG("[%s] CP server started, waiting for new connections",
		       ptl_control_plane_server.role);
	ptl_control_plane_server.status = PTL_CP_SERVER_RUNNING;
	while (1) {
		/* Wait for events on the control plane event queue */
		rc = PtlEQWait(ptl_control_plane_server.eq_handle, &event);
		send_buffer = event.user_ptr;
		if (rc != PTL_OK) {
			SPDK_PTL_FATAL(
				"PtlEQWait failed in control plane server with code: %d\n", rc);
		}

		if (event.type == PTL_EVENT_AUTO_UNLINK) {
			SPDK_PTL_DEBUG("[%s] CP server: Got an autounlink event continue...",
				       ptl_control_plane_server.role);
			continue;
		}
		if (event.type == PTL_EVENT_SEND) {
			continue;
		}

		if (event.type == PTL_EVENT_ACK &&
		    (send_buffer->conn_msg.msg_header.msg_type ==  PTL_OPEN_CONNECTION_REPLY ||
		     send_buffer->conn_msg.msg_header.msg_type == PTL_CLOSE_CONNECTION_REPLY)) {
			SPDK_PTL_DEBUG("[%s] CP server: Send operation of msg with type: %d arrived, do the cleanup...",
				       ptl_control_plane_server.role, send_buffer->conn_msg.msg_header.msg_type);
			PtlMDRelease(send_buffer->md_handle);
			free(send_buffer);
			continue;
		}

		if (event.type == PTL_EVENT_ACK) {
			SPDK_PTL_DEBUG("[%s] CP server: OPEN_CONN/CLOSE_CONN send operation completed: %d",
				       ptl_control_plane_server.role, send_buffer->conn_msg.msg_header.msg_type);
			continue;
		}

		/* Verify that the event is a PUT operation */
		if (event.type != PTL_EVENT_PUT) {
			SPDK_PTL_FATAL(
				"[%s] Unexpected event type received in control plane server: %d", ptl_control_plane_server.role,
				event.type);
		}

		/*Who is it?*/
		if (event.mlength != sizeof(*conn_msg)) {
			SPDK_PTL_FATAL("[%s] CP server: Wrong size received got: %lu should have been: %lu",
				       ptl_control_plane_server.role, event.mlength, sizeof(*conn_msg));
		}


		assert(event.start);
		conn_msg = event.start;

		SPDK_PTL_DEBUG("[%s] CP server: Received control plane message regarding connection staff",
			       ptl_control_plane_server.role);

		if (PTL_OPEN_CONNECTION == conn_msg->msg_header.msg_type) {
			rdma_ptl_handle_open_conn(listen_id, conn_msg);
		} else if (PTL_OPEN_CONNECTION_REPLY == conn_msg->msg_header.msg_type) {
			rdma_ptl_handle_open_conn_reply(listen_id, conn_msg);
		} else if (PTL_CLOSE_CONNECTION == conn_msg->msg_header.msg_type) {
			rdma_ptl_handle_close_conn(listen_id, conn_msg);
		} else if (PTL_CLOSE_CONNECTION_REPLY == conn_msg->msg_header.msg_type) {
			rdma_ptl_handle_close_conn_reply(listen_id, conn_msg);
		} else {
			SPDK_PTL_FATAL("[%s] Unknown message type: %d", ptl_control_plane_server.role,
				       conn_msg->msg_header.msg_type);
		}

		/* Re-register the receive buffer by appending a new list entry */
		ptl_le_t le;
		memset(&le, 0, sizeof(ptl_le_t));
		le.ignore_bits = RDMA_PTL_IGNORE;
		le.match_bits = RDMA_PTL_MATCH;
		le.match_id.phys.nid = PTL_NID_ANY;
		le.match_id.phys.pid = PTL_PID_ANY;
		le.min_free = 0;
		le.start = event.start;
		le.length = sizeof(struct ptl_conn_msg);
		le.ct_handle = PTL_CT_NONE;
		le.uid = PTL_UID_ANY;
		le.options = PTL_SRV_ME_OPTS;

		rc = PtlLEAppend(ptl_cnxt_get_ni_handle(ptl_cnxt),
				 PTL_CP_SERVER_PTE, &le, PTL_PRIORITY_LIST,
				 event.user_ptr, event.user_ptr);
		if (rc != PTL_OK) {
			SPDK_PTL_FATAL(
				"PtlLEAppend failed in control plane server with code: %d\n",
				rc);
		}
		SPDK_PTL_DEBUG("Re-registered control plane buffer");
	}
	return NULL;
}

/**
 * @brief Called from rdma_listen to setup a new Event Queue for Portal Index
 * PTL_CONTROL_PLANE_PT_INDEX. When client calls rdma_connect they send a
 * message to this EQ to introduce themselves and enable server to build a map
 * containing the queue pair num to nid,pid as required by the Portals API.
 * static void rdma_ptl_control_eq(void) {}
 **/
static void rdma_ptl_boot_cp_server(struct  ptl_cm_id *cm_id, const char *role)
{

	struct ptl_context *ptl_cnxt = ptl_cnxt_get();
	ptl_le_t le;
	int rc;

	PTL_CP_SERVER_LOCK(&ptl_control_plane_server.init_lock);
	if (ptl_control_plane_server.status == PTL_CP_SERVER_RUNNING) {
		SPDK_PTL_WARN("CP server for connections already running, nothing to boot");
		goto exit;
	}

	ptl_control_plane_server.role = role;
	ptl_control_plane_server.protocol_version = PTL_SPDK_PROTOCOL_VERSION;
	ptl_control_plane_server.num_conn_info = PTL_CONTROL_PLANE_NUM_RECV_BUFFERS;
	rc = posix_memalign((void **)&ptl_control_plane_server.recv_buffers, 4096,
			    PTL_CONTROL_PLANE_NUM_RECV_BUFFERS * sizeof(struct ptl_conn_msg));
	if (rc != 0) {
		perror("Reason of posix_memalign failure:");
		SPDK_PTL_FATAL("posix_memalign failed: %d", rc);
	}
	memset(ptl_control_plane_server.recv_buffers, 0x00,
	       ptl_control_plane_server.num_conn_info * sizeof(struct ptl_conn_msg));

	ptl_control_plane_server.le_handle = calloc(ptl_control_plane_server.num_conn_info,
					     sizeof(ptl_handle_le_t));
	/*Create event queue for control plane messages*/
	rc = PtlEQAlloc(ptl_cnxt_get_ni_handle(ptl_cnxt), PTL_CONTROL_PLANE_NUM_RECV_BUFFERS,
			&ptl_control_plane_server.eq_handle);
	if (rc != PTL_OK) {
		SPDK_PTL_FATAL("PtlEQAlloc for the control plane failed with code: %d\n", rc);
	}
	/*Bind it to the portal index*/
	rc = PtlPTAlloc(ptl_cnxt_get_ni_handle(ptl_cnxt), 0, ptl_control_plane_server.eq_handle,
			PTL_CP_SERVER_PTE, &ptl_control_plane_server.pt_index);
	if (rc != PTL_OK) {
		SPDK_PTL_FATAL("Error allocating portal for connection server %d reason: %d",
			       PTL_CP_SERVER_PTE, rc);
	}

	for (uint32_t i = 0; i < ptl_control_plane_server.num_conn_info; i++) {

		memset(&le, 0, sizeof(ptl_le_t));
		le.ignore_bits = RDMA_PTL_IGNORE;
		le.match_bits = RDMA_PTL_MATCH;
		le.match_id.phys.nid = PTL_NID_ANY;
		le.match_id.phys.pid = PTL_PID_ANY;
		le.min_free = 0;
		le.start = &ptl_control_plane_server.recv_buffers[i];
		le.length = sizeof(struct ptl_conn_msg);
		le.ct_handle = PTL_CT_NONE;
		le.uid = PTL_UID_ANY;
		le.options = PTL_SRV_ME_OPTS;

		// Append LE for receiving control messages
		rc = PtlLEAppend(ptl_cnxt_get_ni_handle(ptl_cnxt), PTL_CP_SERVER_PTE, &le,
				 PTL_PRIORITY_LIST, &ptl_control_plane_server.le_handle[i], &ptl_control_plane_server.le_handle[i]);
		if (rc != PTL_OK) {
			SPDK_PTL_FATAL("PtlLEAppend failed in control plane server with code: %d\n", rc);
		}
	}

	if (pthread_create(&ptl_control_plane_server.cp_server_cnxt, NULL,
			   rdma_run_ptl_cp_server, cm_id)) {
		perror("Reason of failure of booting control plane server:");
		SPDK_PTL_FATAL("Failed to boot control plane server");
	}
	while (ptl_control_plane_server.status != PTL_CP_SERVER_RUNNING);


	SPDK_PTL_DEBUG("[%s] CP server: BOOTED control plane server",
		       ptl_control_plane_server.role);
exit:
	PTL_CP_SERVER_UNLOCK(&ptl_control_plane_server.init_lock);
}

struct rdma_event_channel *
rdma_create_event_channel(void)
{
	SPDK_PTL_DEBUG("RDMACM: Intercepted rdma_create_event_channel()");
	struct rdma_cm_ptl_event_channel *ptl_channel = calloc(1UL,
		sizeof(*ptl_channel));
	if (!ptl_channel) {
		SPDK_PTL_DEBUG("RDMACM: Allocation of memory failed");
		return NULL;
	}
	ptl_channel->magic_number = RDMA_CM_PTL_EVENT_CHANNEL_MAGIC_NUMBER;
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

/**
 * ********************************************************************
 * <Subset> of libverbs that Nida implements so nvmf target can operate
 * ********************************************************************
 **/
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
	struct ptl_pd *ptl_pd;
	struct ptl_context *ptl_context = ptl_cnxt_get_from_ibcnxt(context);
	SPDK_PTL_DEBUG("IBVPTL: OK trapped ibv_alloc_pd allocating ptl_pd");
	ptl_pd = ptl_pd_create(ptl_context);
	if (ptl_context->ptl_pd) {
		SPDK_PTL_FATAL("PTL_PD Already set!");
	}
	ptl_context->ptl_pd = ptl_pd;
	return ptl_pd_get_ibv_pd(ptl_pd);
}

struct ibv_context *ibv_open_device(struct ibv_device *device)
{
	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return NULL;
}

struct ibv_cq *ibv_create_cq(struct ibv_context *context, int cqe,
			     void *cq_context, struct ibv_comp_channel *channel,
			     int comp_vector)
{

	SPDK_PTL_DEBUG("IBVPTL: Ok trapped ibv_create_cq time to create the event queue in portals");
	struct ptl_cq *ptl_cq = ptl_cq_get_instance(cq_context);
	SPDK_PTL_DEBUG("Ok set up event queue for PORTALS :-)");
	return ptl_cq_get_ibv_cq(ptl_cq);
}

// Caution! Due to inlining of ibv_poll_cq SPDK_PTL overrides it also in
// ptl_context
// int ibv_poll_cq(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc) {
//     SPDK_PTL_DEBUG("Intercepted ibv_poll_cq");
//     SPDK_PTL_FATAL("UNIMPLEMENTED");
//     return -1;
// }

struct ibv_srq *ibv_create_srq(struct ibv_pd *pd,
			       struct ibv_srq_init_attr *srq_init_attr)
{
	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return NULL;
}

int ibv_modify_srq(struct ibv_srq *srq,
		   struct ibv_srq_attr *srq_attr,
		   int srq_attr_mask)
{
	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return 0;
}


int ibv_destroy_srq(struct ibv_srq *srq)
{
	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return 0;
}
/**
 * ********************************************************************
 * </Subset> of libverbs that Nida implements so nvmf target can operate
 * ********************************************************************
 **/

int rdma_create_id(struct rdma_event_channel *channel, struct rdma_cm_id **id,
		   void *context, enum rdma_port_space ps)
{
	struct ptl_cm_id *ptl_id;
	struct rdma_cm_ptl_event_channel *ptl_channel;
	ptl_channel = rdma_cm_ptl_event_channel_get(channel);
	ptl_id = ptl_cm_id_create(ptl_channel, context);
	rdma_ptl_conn_map_add(ptl_id);
	/*Caution wiring need it, it is accessed later*/
	// ptl_id->fake_cm_id.qp = &ptl_id->fake_qp;
	/*
	 * set PD, CQ, and QP to NULL. These fields are updated through rdma_create_qp!
	 * Don't worry already done in the constructor of the ptl_cm_id object
	 * */

	*id = &ptl_id->fake_cm_id;
	dlist_append(ptl_channel->open_fake_connections, ptl_id);
	SPDK_PTL_DEBUG("Trapped create cm id FAKED it, waking up possible guys for the event");
	return 0;
}

int rdma_bind_addr(struct rdma_cm_id *id, struct sockaddr *addr)
{
	SPDK_PTL_DEBUG("Trapped rdma_bind_addr setting src addresss");
	struct ptl_cm_id * ptl_id = ptl_cm_id_get(id);
	if (ptl_id->object_type != PTL_CM_ID) {
		SPDK_PTL_FATAL("Corrupted ptl_cm_id");
	}
	memcpy(&id->route.addr.src_addr, addr, sizeof(*addr));

	return 0;
}

int rdma_listen(struct rdma_cm_id *id, int backlog)
{

	struct ptl_cm_id * ptl_id = ptl_cm_id_get(id);
	// struct rdma_cm_event *fake_event;
	rdma_ptl_boot_cp_server(ptl_id, "TARGET");

	// SPDK_PTL_DEBUG("RDMA_LISTEN(): Create a fake connection event to establish queue pair no 1");
	// fake_event = ptl_cm_id_create_event(ptl_id, id, RDMA_CM_EVENT_CONNECT_REQUEST, &private_data,
	// 				    sizeof(private_data));

	// /*Fill up fake data about the origin of the guy that wants a new connection*/
	// fake_event->listen_id = id;

	// ptl_cm_id_add_event(ptl_id, fake_event);
	// SPDK_PTL_DEBUG("RDMA_LISTEN(): Ok created the fake RDMA_CM_EVENT_CONNECT_REQUEST triggering it through channel's async fd");
	// uint64_t value = 1;
	// if (write(ptl_id->ptl_channel->fake_channel.fd, &value, sizeof(value)) != sizeof(value)) {
	// 	perror("write to eventfd, reason:");
	// 	SPDK_PTL_FATAL("Failed to write eventfd");
	// }
	return 0;
}

int rdma_get_cm_event(struct rdma_event_channel *channel,
		      struct rdma_cm_event **event)
{

	SPDK_PTL_DEBUG(" --------> Going to take an event...");
	struct rdma_cm_ptl_event_channel *ptl_channel;
	struct rdma_cm_event *fake_event;
	ptl_channel = rdma_cm_ptl_event_channel_get(channel);
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
		/* Clean the event*/
		uint64_t result;
		if (read(channel->fd, &result, sizeof(result)) != sizeof(result)) {
			perror("read");
			SPDK_PTL_DEBUG("Failed to clean the event, go on");
		}
		return 0;
	}
	SPDK_PTL_DEBUG("Got nothing shit EAGAIN!");
	errno = EAGAIN;
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


int rdma_resolve_addr(struct rdma_cm_id *id, struct sockaddr *src_addr,
		      struct sockaddr *dst_addr, int timeout_ms)
{
	struct sockaddr resolve_src_addr;
	struct sockaddr *resolve_src_addr_p;
	struct rdma_cm_event *fake_event;
	struct ptl_cm_id *ptl_id = ptl_cm_id_get(id);

	SPDK_PTL_DEBUG("Keep accounting of the src and dst addr in rdma_cm_id structure");
	if (src_addr) {
		memcpy(&id->route.addr.src_addr, src_addr,
		       (src_addr->sa_family == AF_INET6) ?
		       sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));
	}

	if (!dst_addr) {
		SPDK_PTL_FATAL("No dst addr");
	}
	memcpy(&id->route.addr.dst_addr, dst_addr,
	       (dst_addr->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6)
	       : sizeof(struct sockaddr_in));

	// Set the address family in the route
	id->route.addr.src_addr.sa_family = src_addr ?
					    src_addr->sa_family : dst_addr->sa_family;
	id->route.addr.dst_addr.sa_family = dst_addr->sa_family;

	resolve_src_addr_p = src_addr;
	if (NULL == src_addr) {
		rdma_cm_find_matching_local_ip(dst_addr, &resolve_src_addr);
		resolve_src_addr_p = &resolve_src_addr;
	}

	if (NULL == resolve_src_addr_p) {
		rdma_cm_print_addr_info("DESTINATION IS:", dst_addr);
		SPDK_PTL_FATAL("Could not find a local ip to match destination address");
	}

	SPDK_PTL_DEBUG("----------->   Resolved src addr   <--------------");
	rdma_cm_print_addr_info("SOURCE_ADDR = ", &id->route.addr.src_addr);
	fake_event = ptl_cm_id_create_event(ptl_id, NULL, RDMA_CM_EVENT_ADDR_RESOLVED,
					    ptl_id->fake_data, 0);
	ptl_cm_id_add_event(ptl_id, fake_event);
	SPDK_PTL_DEBUG("Ok stored dst addr and generated fake "
		       "RDMA_CM_EVENT_ADDR_RESOLVED event");
	return 0;
}


int rdma_resolve_route(struct rdma_cm_id *id, int timeout_ms)
{
	struct rdma_cm_event *fake_event;
	struct ifaddrs *ifaddr, *ifa;
	struct sockaddr *dst_addr = &id->route.addr.dst_addr;
	struct sockaddr *src_addr = &id->route.addr.src_addr;
	int family = dst_addr->sa_family;
	int found = 0;
	/*First given the dest address fill the src address*/


	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs failed");
		SPDK_PTL_FATAL("Failed to get IP addresses");
	}

	SPDK_PTL_DEBUG("Looking for interfaces with family %d (AF_INET=%d, AF_INET6=%d)",
		       family, AF_INET, AF_INET6);

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL) {
			continue;
		}

		// Skip if not the address family we're looking for
		if (ifa->ifa_addr->sa_family != family) {
			continue;
		}

		SPDK_PTL_DEBUG("Checking interface: %s", ifa->ifa_name);
		SPDK_PTL_DEBUG("  Flags: 0x%x ", ifa->ifa_flags);


		// Skip loopback
		if (ifa->ifa_flags & IFF_LOOPBACK) {
			printf("  Skipping loopback interface\n");
			continue;
		}

		// Skip if interface is not up and running
		if (!(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_RUNNING)) {
			SPDK_PTL_DEBUG("  Skipping interface - not up/running");
			continue;
		}

		// Copy the address
		memcpy(src_addr, ifa->ifa_addr,
		       (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
		found = 1;
		break;
	}

	freeifaddrs(ifaddr);
	if (false == found) {
		SPDK_PTL_FATAL("Could not find a corresponding SRC IP address");
	}
	struct ptl_cm_id *ptl_id = ptl_cm_id_get(id);
	fake_event = ptl_cm_id_create_event(ptl_id, NULL, RDMA_CM_EVENT_ROUTE_RESOLVED, &private_data,
					    sizeof(private_data));
	ptl_cm_id_add_event(ptl_id, fake_event);
	return 0;
}


/**
 * @brief Extracts the target process ID from the last digit of an RDMA connection's IP address
 *
 * This function examines the destination IP address of an RDMA connection and returns
 * its last digit, which is used as the target process ID. The function supports both
 * IPv4 and IPv6 addresses.
 *
 * @param id Pointer to an RDMA connection identifier structure (rdma_cm_id).
 *           Must not be NULL and must have a valid destination address set.
 *
 * @return The last digit found in the IP address, to be used as the target process ID
 *
 * @throws SPDK_PTL_FATAL in the following cases:
 *         - If the input rdma_cm_id pointer is NULL
 *         - If IPv4/IPv6 address conversion fails
 *         - If the address family is unsupported (neither IPv4 nor IPv6)
 *         - If no digits are found in the IP address
 *
 * @note The function assumes that the last digit of the IP address corresponds
 *       to a valid process ID in the target system.
 *
 * Example usage:
 * @code
 *     struct rdma_cm_id *id = get_grdma_connection();
 *     int target_pid = rdma_ptl_find_target_pid(id);
 *     // target_pid now contains the last digit of the IP address
 * @endcode
 */
static int rdma_ptl_find_target_nid(struct rdma_cm_id *id)
{
	struct sockaddr_in *sin = NULL;
	struct sockaddr_in6 *sin6 = NULL;
	uint8_t last_byte;

	if (NULL == id) {
		SPDK_PTL_FATAL("Invalid RDMA connection ID");
	}

	switch (id->route.addr.dst_addr.sa_family) {
	case AF_INET:
		sin = (struct sockaddr_in *)&id->route.addr.dst_addr;
		// For IPv4, get the last byte directly from the address
		last_byte = ((uint8_t *)&sin->sin_addr.s_addr)[3];
		SPDK_PTL_DEBUG("IPv4 last byte: %d", last_byte);
		return last_byte;

	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)&id->route.addr.dst_addr;
		// For IPv6, get the last byte from the 16-byte address
		last_byte = sin6->sin6_addr.s6_addr[15];
		SPDK_PTL_DEBUG("IPv6 last byte: %d", last_byte);
		return last_byte;

	default:
		SPDK_PTL_FATAL("Unsupported address family: %d", id->route.addr.dst_addr.sa_family);
	}
	return -1;
}


int rdma_create_qp(struct rdma_cm_id *id, struct ibv_pd *pd,
		   struct ibv_qp_init_attr *qp_init_attr)
{
	/**
	 * All the money here. Now everyone (ptl_qp, ptl_cm_id, ptl_pd) should know each other
	 * */
	struct ptl_pd *ptl_pd = ptl_pd_get_from_ibv_pd(pd);
	struct ptl_cm_id *ptl_id = ptl_cm_id_get(id);
	struct ptl_cq *send_queue = ptl_cq_get_from_ibv_cq(qp_init_attr->send_cq);
	struct ptl_cq *recv_queue = ptl_cq_get_from_ibv_cq(qp_init_attr->recv_cq);

	struct ptl_qp *ptl_qp = ptl_id->ptl_qp;
	struct ptl_conn_open conn_open;
	if (ptl_qp) {
		SPDK_PTL_DEBUG("Queue pair already there nothing to do, connected to remote nid: %d pid: %d portals index: %d",
			       ptl_qp->remote_nid, ptl_qp->remote_pid, ptl_qp->remote_pt_index);
		return 0;
	}
	conn_open.dst_nid = rdma_ptl_find_target_nid(id);
	conn_open.dst_pid = PTL_TARGET_PID;
	conn_open.dst_pt_index = PTL_CP_SERVER_PTE;
	conn_open.src_nid = -1;
	conn_open.src_pid = -1;
	SPDK_PTL_DEBUG("Creating queue pair... connected to remote nid: %d pid: %d portals index: %d",
		       conn_open.dst_nid, conn_open.dst_pid, conn_open.dst_pt_index);
	ptl_qp = ptl_qp_create(ptl_pd, send_queue, recv_queue, &conn_open);
  ptl_qp->fake_qp.qp_num = ptl_id->ptl_qp_num;
	/*Update cm_id*/
	ptl_cm_id_set_ptl_qp(ptl_id, ptl_qp);
	ptl_cm_id_set_ptl_pd(ptl_id, ptl_pd);
	ptl_cm_id_set_send_queue(ptl_id, send_queue);
	ptl_cm_id_set_recv_queue(ptl_id, recv_queue);
  ptl_qp->ptl_cm_id = ptl_id;



	SPDK_PTL_DEBUG("Successfully created Portals Queue Pair Object and updated Portal CM ID and Queue Pair pointers");
	return 0;
}



#define RDMA_PTL_CLIENT_MAILBOX_BUFFERS 16UL
/**
 * @brief Establish RDMA connection using Portals PUT operation
 *
 * This function implements the client-side connection establishment by:
 * - Preparing connection information (PID, NID, etc.)
 * - Sending this information to the server using PtlPut
 * - Creating a fake RDMA connection event for compatibility
 *
 * @param id RDMA communication identifier
 * @param conn_param Connection parameters including private data
 * @return 0 on success, negative value on failure
 */
int rdma_connect(struct rdma_cm_id *id, struct rdma_conn_param *conn_param)
{
	struct rdma_ptl_send_buffer *send_buffer;
	struct ptl_cm_id *ptl_id = ptl_cm_id_get(id);
	struct ptl_context *ptl_cnxt = ptl_cnxt_get();
	ptl_process_t target;
	ptl_md_t md;
	int rc;

	/* Boot Control plane server if not booted*/
	rdma_ptl_boot_cp_server(ptl_id, "INITIATOR");

	/*Allocate the buffer for the open connection request*/
	if (posix_memalign((void **)&send_buffer, 4096, sizeof(*send_buffer))) {
		SPDK_PTL_FATAL("Failed to allocate ptl_conn_request");
	}
	send_buffer->conn_msg.msg_header.version = PTL_SPDK_PROTOCOL_VERSION;
	send_buffer->conn_msg.msg_header.msg_type = PTL_OPEN_CONNECTION;

	send_buffer->conn_msg.conn_open.src_nid = ptl_cnxt_get_nid(ptl_cnxt);
	send_buffer->conn_msg.conn_open.src_pid = ptl_cnxt_get_pid(ptl_cnxt);
	send_buffer->conn_msg.conn_open.dst_pt_index =  PTL_CP_SERVER_PTE;
  send_buffer->conn_msg.conn_open.initiator_qp_num = ptl_id->ptl_qp_num;
	memcpy(&send_buffer->conn_msg.conn_open.src_addr, &id->route.addr.src_addr,
	       sizeof(send_buffer->conn_msg.conn_open.src_addr));
	// SPDK_PTL_DEBUG("Source address is");
	// rdma_ptl_print_sockaddr(&id->route.addr.src_addr);
	// SPDK_PTL_DEBUG("Destination address is");
	// rdma_ptl_print_sockaddr(&id->route.addr.dst_addr);

	/* Setup target process identifier */
	send_buffer->conn_msg.conn_open.dst_nid = rdma_ptl_find_target_nid(id);
	send_buffer->conn_msg.conn_open.dst_pid = PTL_TARGET_PID;
	target.phys.nid = send_buffer->conn_msg.conn_open.dst_nid;
	target.phys.pid = send_buffer->conn_msg.conn_open.dst_pid;

	/* Create memory descriptor for the connection info */
	memset(&md, 0, sizeof(ptl_md_t));
	md.start = &send_buffer->conn_msg;
	md.length = sizeof(send_buffer->conn_msg);
	md.options = 0;
	md.eq_handle = ptl_control_plane_server.eq_handle;
	md.ct_handle = PTL_CT_NONE;

	rc = PtlMDBind(ptl_cnxt_get_ni_handle(ptl_cnxt), &md, &send_buffer->md_handle);
	if (rc != PTL_OK) {
		SPDK_PTL_FATAL("PtlMDBind failed with code: %d\n", rc);
	}

	memcpy(&ptl_id->conn_param, conn_param, sizeof(*conn_param));

	SPDK_PTL_DEBUG("[%s] MD of the request done. Performing the actual PtlPut to target nid:%d pid:%d portals_index: %d",
		       ptl_control_plane_server.role,
		       target.phys.nid, target.phys.pid, PTL_CP_SERVER_PTE);
	/* Send connection info to server using PtlPut */
	rc = PtlPut(send_buffer->md_handle,                    /* MD handle */
		    0,                            /* local offset */
		    sizeof(send_buffer->conn_msg),            /* length */
		    PTL_ACK_REQ,                 /* acknowledgment requested */
		    target,                      /* target process */
		    PTL_CP_SERVER_PTE,  /* portal table index */
		    0,                   /* match bits */
		    0,                           /* remote offset */
		    send_buffer,                /* user ptr */
		    0);

	if (rc != PTL_OK) {
		SPDK_PTL_FATAL("PtlPut failed with code: %d\n", rc);
	}

	return 0;
}

// int rdma_connect(struct rdma_cm_id *id, struct rdma_conn_param *conn_param)
// {
// 	struct rdma_cm_event *fake_event;
// 	struct ptl_cm_id *ptl_id = ptl_cm_id_get(id);
//
// 	//original
// 	// ptl_cm_id_set_fake_data(ptl_id, conn_param->private_data);
// 	// ptl_cm_id_create_event(ptl_id, id, RDMA_CM_EVENT_CONNECT_RESPONSE);
// 	fake_event = ptl_cm_id_create_event(ptl_id, id, RDMA_CM_EVENT_CONNECT_RESPONSE,
// 					    conn_param->private_data, conn_param->private_data_len);
// 	ptl_cm_id_add_event(ptl_id, fake_event);
// 	SPDK_PTL_DEBUG("FAKED successfull connection");
// 	return 0;
// }

int rdma_set_option(struct rdma_cm_id *id, int level, int optname, void *optval,
		    size_t optlen)
{
	SPDK_PTL_INFO("Ignoring (for) now options XXX TODO XXX");
	return 0;
}

int rdma_accept(struct rdma_cm_id *id, struct rdma_conn_param *conn_param)
{

	struct rdma_ptl_send_buffer *send_buffer;
	struct ptl_cm_id * ptl_id = ptl_cm_id_get(id);
	int rc;
	ptl_md_t memory_desc;
	struct ptl_context *ptl_cnxt = ptl_cnxt_get();
	ptl_process_t initiator = {.phys.nid = ptl_id->ptl_qp->remote_nid, .phys.pid = ptl_id->ptl_qp->remote_pid};
	SPDK_PTL_DEBUG("[%s] CP server: At accept sending uuid of the connectio: %lu",
		       ptl_control_plane_server.role, ptl_id->uuid);

	if (posix_memalign((void **)&send_buffer, 4096, sizeof(*send_buffer))) {
		SPDK_PTL_FATAL("Failed to allocate memory");
	}
	/*first fill the reply parts*/
	send_buffer->conn_msg.msg_header.version = PTL_SPDK_PROTOCOL_VERSION;
	send_buffer->conn_msg.msg_header.msg_type = PTL_OPEN_CONNECTION_REPLY;
	send_buffer->conn_msg.conn_open_reply.status = PTL_OK;
	send_buffer->conn_msg.conn_open_reply.uuid = ptl_id->uuid;


	/* Create memory descriptor for the connection info */
	memset(&memory_desc, 0, sizeof(ptl_md_t));
	memory_desc.start = &send_buffer->conn_msg;
	memory_desc.length = sizeof(send_buffer->conn_msg);
	memory_desc.options = 0;
	memory_desc.eq_handle = ptl_control_plane_server.eq_handle;
	memory_desc.ct_handle = PTL_CT_NONE;

	rc = PtlMDBind(ptl_cnxt_get_ni_handle(ptl_cnxt), &memory_desc, &send_buffer->md_handle);
	if (rc != PTL_OK) {
		SPDK_PTL_FATAL("[%s] PtlMDBind failed with code: %d\n", ptl_control_plane_server.role, rc);
	}

	rc = PtlPut(send_buffer->md_handle,           // MD handle
		    0,                    // local offset in MD
		    sizeof(send_buffer->conn_msg),   // length to transfer
		    PTL_ACK_REQ,         // request acknowledgment
		    initiator,          // target process
		    PTL_CP_SERVER_PTE,            // portal table index at target
		    0,          // match bits
		    0,                   // remote offset
		    send_buffer,                // user ptr (for events)
		    0);                  // hdr_data

	if (rc != PTL_OK) {
		SPDK_PTL_FATAL("[TARGET] Failed to respond to conn info");
	}
	SPDK_PTL_DEBUG("[%s] DONE: SENT the accept message to initiator nid: %d pid: %d pt_index: %d",
		       ptl_control_plane_server.role,
		       initiator.phys.nid, initiator.phys.pid, PTL_CP_SERVER_PTE);
	return 0;
}

int rdma_disconnect(struct rdma_cm_id *id)
{
	SPDK_PTL_FATAL("UNIMPLEMENTED");
	return 0;
}

