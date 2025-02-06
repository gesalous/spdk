#include <dlfcn.h>
#include <rdma/rdma_cma.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <portals4.h>

#define DEBUG_RDMACM(fmt, ...)                                                 \
  do {                                                                         \
    fprintf(stderr, "PORTALS_WRAP: " fmt "\n", ##__VA_ARGS__);                 \
  } while (0)

struct rdma_event_channel *rdma_create_event_channel(void) {
  DEBUG_RDMACM("Intercepted rdma_create_event_channel()");
  struct rdma_event_channel *channel = calloc(1UL, sizeof(*channel));
  if (!channel) {
    DEBUG_RDMACM("Allocation of memory failed");
    return NULL;
  }

  channel->fd = eventfd(0, EFD_NONBLOCK);
  if (channel->fd < 0) {
    free(channel);
    DEBUG_RDMACM("eventfd failed. Reason follows:");
    perror("Reason:");
    return NULL;
  }
  return channel;
}
