#include "portals_log.h"
#include "ptl_context.h"
#include "spdk/util.h"
#include <dlfcn.h>
#include <infiniband/verbs.h>
#include <portals4.h>
#include <rdma/rdma_cma.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <unistd.h>
#define SPDK_PTL_EQ_SIZE 16
#define RDMA_CM_PTL_MAGIG_NUMBER 14101983UL

struct rdma_cm_ptl_id{
  uint64_t magic_number;
  struct rdma_cm_id fake_cm_id;
};


struct rdma_event_channel *
rdma_create_event_channel(void) {
  SPDK_PTL_DEBUG("RDMACM: Intercepted rdma_create_event_channel()");
  struct rdma_event_channel *channel = calloc(1UL, sizeof(*channel));
  if (!channel) {
    SPDK_PTL_DEBUG("RDMACM: Allocation of memory failed");
    return NULL;
  }

  channel->fd = eventfd(0, EFD_NONBLOCK);
  if (channel->fd < 0) {
    free(channel);
    SPDK_PTL_DEBUG("RDMACM: eventfd failed. Reason follows:");
    perror("Reason:");
    return NULL;
  }
  return channel;
}

struct ibv_context **rdma_get_devices(int *num_devices) {
  struct ibv_context **devices;
  struct ptl_context *cnxt;
  int ret;

  SPDK_PTL_DEBUG("RDMACM: Intercepted rdma_get_devices");
  SPDK_PTL_DEBUG("RDMACM: Calling PtlInit()");
  cnxt = ptl_cnxt_get();

  devices = calloc(1UL, 2 * sizeof(struct ibv_context *));
  if(NULL == devices){
    SPDK_PTL_FATAL("RDMACM: Failed to allocate memory for device list");
  }
 
  devices[0] = ptl_cnxt_get_ibv_context(cnxt);
  devices[0]->async_fd = eventfd(0, EFD_NONBLOCK);
  if (devices[0]->async_fd < 0) {
    SPDK_PTL_FATAL("RDMACM: Failed to create async fd");
  }
  SPDK_PTL_DEBUG("RDMACM: Created this async_fd thing");
  devices[0]->device = calloc(1UL,sizeof(struct ibv_device));
  if(NULL == devices[0]->device){
    SPDK_PTL_FATAL("RDMACM: No memory");
  }
  // Set device name and other attributes
  strcpy(devices[0]->device->name, "portals_device");
  strcpy(devices[0]->device->dev_name, "bxi0");
  strcpy(devices[0]->device->dev_path, "/dev/portals0");
  if(num_devices)
    *num_devices = 1;
  devices[1] = NULL;
  SPDK_PTL_DEBUG("RDMACM: Initialization DONE with portals Initialization, encapsulated portals_context inside ibv_context");
  return devices;
}

/* Subset of libverbs that Nida implements so nvmf target can boot*/
int ibv_query_device(struct ibv_context *context,
                     struct ibv_device_attr *device_attr) {
  SPDK_PTL_DEBUG("IBVPTL: Trapped ibv_query_device filling it with reasonable values...");

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
  SPDK_PTL_DEBUG("IBVPTL: Trapped ibv_query_device filling it with reasonable values...DONE");
  return 0;
}

struct ibv_pd *ibv_alloc_pd(struct ibv_context *context) {
  struct ptl_context *cnxt = ptl_cnxt_get_from_ibcnxt(context);
  SPDK_PTL_DEBUG("IBVPTL: OK trapped ibv_alloc_pd sending dummy pd portals "
                 "does not need it");
  return ptl_cnxt_get_ibv_pd(cnxt);
}

struct ibv_context *ibv_open_device(struct ibv_device *device) {
  SPDK_PTL_FATAL("UNIMPLEMENTED");
}

struct ibv_cq *ibv_create_cq(struct ibv_context *context, int cqe,
                             void *cq_context, struct ibv_comp_channel *channel,
                             int comp_vector) {
  
  ptl_handle_eq_t eq_handle;
  struct ptl_context *ptl_context = ptl_cnxt_get_from_ibcnxt(context);
  SPDK_PTL_DEBUG("IBVPTL: Ok trapped ibv_create_cq time to create the event queue in portals");

  int ret = PtlEQAlloc(ptl_cnxt_get_ni_handle(ptl_context), SPDK_PTL_EQ_SIZE, &eq_handle);
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
                   void *context, enum rdma_port_space ps) {
  struct rdma_cm_ptl_id *ptl_id =
      calloc(1UL, sizeof(struct rdma_cm_ptl_id));
  struct ptl_context *cnxt = ptl_cnxt_get();
  ptl_id->fake_cm_id.verbs = ptl_cnxt_get_ibv_context(cnxt);
  ptl_id->magic_number = RDMA_CM_PTL_MAGIG_NUMBER;
  *id = &ptl_id->fake_cm_id;
  SPDK_PTL_DEBUG("Trapped create cm id FAKED it");
  return 0;
}

int rdma_bind_addr(struct rdma_cm_id *id, struct sockaddr *addr) {
  SPDK_PTL_DEBUG("Trapped rdma_bind_addr FAKED it");
  struct rdma_cm_ptl_id * ptl_id = SPDK_CONTAINEROF(id, struct rdma_cm_ptl_id, fake_cm_id);
  if(ptl_id->magic_number != RDMA_CM_PTL_MAGIG_NUMBER){
    SPDK_PTL_FATAL("Corrupted ptl cm id");
  }
  return 0;
}

int rdma_listen(struct rdma_cm_id *id, int backlog) {
  SPDK_PTL_DEBUG("Trapped rdma listen");
  struct rdma_cm_ptl_id * ptl_id = SPDK_CONTAINEROF(id, struct rdma_cm_ptl_id, fake_cm_id);
  if(ptl_id->magic_number != RDMA_CM_PTL_MAGIG_NUMBER){
    SPDK_PTL_FATAL("Corrupted ptl cm id");
  }
  SPDK_PTL_DEBUG("FAKE IT UNTIL YOU MAKE IT");
  return 0;
}

int rdma_get_cm_event(struct rdma_event_channel *channel,
                      struct rdma_cm_event **event) {
  SPDK_PTL_FATAL("UNIMPLEMENTED");
}
