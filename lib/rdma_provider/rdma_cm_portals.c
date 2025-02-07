#include <dlfcn.h>
#include <rdma/rdma_cma.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <portals4.h>

#define SERVER_PID 0

#define RDMACMPTL_DEBUG(fmt, ...)                                                 \
  do {                                                                         \
    fprintf(stderr, "[RDMACMPTL_DEBUG] " fmt "\n", ##__VA_ARGS__);                 \
  } while (0)

#define RDMACMPTL_INFO(fmt, ...)                                                 \
  do {                                                                         \
    fprintf(stderr, "[RDMACMPTL_INFO] " fmt "\n", ##__VA_ARGS__);                 \
  } while (0)

#define RDMACMPTL_FATAL(fmt, ...)                                                 \
  do {                                                                         \
    fprintf(stderr, "[RDMACMPTL_FATAL] " fmt "\n", ##__VA_ARGS__);                 \
    _exit(EXIT_FAILURE);   \
  } while (0)

#define RDMACMPTL_WARN(fmt, ...)                                                 \
  do {                                                                         \
    fprintf(stderr, "[RDMACMPTL_WARN] " fmt "\n", ##__VA_ARGS__);                 \
  } while (0)

struct rdma_event_channel *rdma_create_event_channel(void) {
  RDMACMPTL_DEBUG("Intercepted rdma_create_event_channel()");
  struct rdma_event_channel *channel = calloc(1UL, sizeof(*channel));
  if (!channel) {
    RDMACMPTL_DEBUG("Allocation of memory failed");
    return NULL;
  }

  channel->fd = eventfd(0, EFD_NONBLOCK);
  if (channel->fd < 0) {
    free(channel);
    RDMACMPTL_DEBUG("eventfd failed. Reason follows:");
    perror("Reason:");
    return NULL;
  }
  return channel;
}

struct ibv_context **rdma_get_devices(int *num_devices) {
  ptl_handle_ni_t ni_handle;
  int ret;
  RDMACMPTL_DEBUG("Intercepted rdma_get_devices");
  RDMACMPTL_DEBUG("Calling PtlInit()");

  ret = PtlInit();
	if (ret != PTL_OK) {
		RDMACMPTL_FATAL("PtlInit failed");
		_exit(EXIT_FAILURE);
	}

	const char *srv_nid = getenv("SERVER_NID");
	if (srv_nid) {
		ret = PtlNIInit((int)atoi(srv_nid), PTL_NI_MATCHING | PTL_NI_PHYSICAL, SERVER_PID, NULL, NULL,
				&ni_handle);
	} else {
		RDMACMPTL_WARN("SERVER_NID not set. Using default nid PTL_IFACE_DEFAULT=0!");
		ret = PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_MATCHING | PTL_NI_PHYSICAL, SERVER_PID, NULL, NULL,
				&ni_handle);
	}

	if (ret != PTL_OK) {
		RDMACMPTL_FATAL("PtlNIInit failed");
	}
  RDMACMPTL_DEBUG("Initialization DONE with portals Initialization");
  _exit(EXIT_FAILURE);
  return NULL;
}
