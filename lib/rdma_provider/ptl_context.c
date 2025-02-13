#include "ptl_context.h"
#include "portals_log.h"
#include <infiniband/verbs.h>
#include <portals4.h>
#include <stdint.h>

#define PTL_SERVER_PID 0
#define PTL_MAGIC_NUMBER 27060211UL/*Bdays of my boys :)*/
#define PTL_EVENT_QUEUE_SIZE 2048

#define SPDK_PTL_CONTAINER_OF(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);   \
    (type *)( (char *)__mptr - offsetof(type,member) );})

struct ptl_context{
  uint64_t magic_number;
  ptl_handle_ni_t ni_handle;
  ptl_handle_eq_t eq_handle;
  ptl_pt_index_t portals_idx;
  struct ibv_context fake_ibv_cnxt;
  struct ibv_pd pd;
};

struct ptl_context *ptl_cnxt_create(void) {
  struct ptl_context *cnxt;
  int ret;

  cnxt = calloc(1UL, sizeof(*cnxt));
  if (NULL == cnxt) {
    SPDK_PTL_FATAL("RDMACM: Memory allocation failed for portals context");
  }
  cnxt->magic_number = PTL_MAGIC_NUMBER;
  SPDK_PTL_DEBUG("Calling PtlInit()");
  ret = PtlInit();
  if (ret != PTL_OK) {
    SPDK_PTL_FATAL("PtlInit failed");
  }

  const char *srv_nid = getenv("SERVER_NID");
  if (srv_nid) {
    ret = PtlNIInit((int)atoi(srv_nid), PTL_NI_MATCHING | PTL_NI_PHYSICAL,
                    PTL_SERVER_PID, NULL, NULL, &cnxt->ni_handle);
  } else {
    SPDK_PTL_WARN(
        "RDMACM: SERVER_NID not set. Using default nid PTL_IFACE_DEFAULT=0!");
    ret = PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_MATCHING | PTL_NI_PHYSICAL,
                    PTL_SERVER_PID, NULL, NULL, &cnxt->ni_handle);
  }

  if (ret != PTL_OK) {
    SPDK_PTL_FATAL("RDMACM: PtlNIInit failed");
  }

  ret = PtlEQAlloc(cnxt->ni_handle, PTL_EVENT_QUEUE_SIZE, &cnxt->eq_handle);
  if (ret != PTL_OK) {
    SPDK_PTL_FATAL("PtlEQAlloc failed");
  }
  SPDK_PTL_DEBUG("SUCCESSFULLY allocated event queue for PORTALS");

  ret = PtlPTAlloc(cnxt->ni_handle, 0, cnxt->eq_handle, PTL_PT_ANY, &cnxt->portals_idx);
  if (ret != PTL_OK) {
    SPDK_PTL_FATAL("PtlPTAlloc failed");
  }
  SPDK_PTL_DEBUG("SUCCESSFULLY allocated PORTALS INDEX");
  return cnxt;
}

struct ibv_context * ptl_cnxt_get_ibv_context(struct ptl_context *cnxt){
  return &cnxt->fake_ibv_cnxt;
}

struct ptl_context *ptl_cnxt_get_from_ibcnxt(struct ibv_context *ib_cnxt) {
  struct ptl_context *cnxt =
      SPDK_PTL_CONTAINER_OF(ib_cnxt, struct ptl_context, fake_ibv_cnxt);
  if (PTL_MAGIC_NUMBER != cnxt->magic_number) {
   SPDK_PTL_FATAL("Corrupted portals context, magic number does not match");
  }
  return cnxt;
}

struct ptl_context *ptl_cnxt_get_from_ibvpd(struct ibv_pd *ib_pd) {
  struct ptl_context *cnxt =
      SPDK_PTL_CONTAINER_OF(ib_pd, struct ptl_context, pd);
  if (PTL_MAGIC_NUMBER != cnxt->magic_number) {
   SPDK_PTL_FATAL("Corrupted portals context, magic number does not match");
  }
  return cnxt;
}

struct ibv_pd *ptl_cnxt_get_ibv_pd(struct ptl_context *cnxt) { return &cnxt->pd; }

ptl_handle_eq_t ptl_cnxt_get_event_queue(struct ptl_context *cnxt) {
  return cnxt->eq_handle;
}

ptl_pt_index_t ptl_cnxt_get_portal_index(struct ptl_context *cnxt) {
  return cnxt->portals_idx;
}

ptl_handle_ni_t ptl_cnxt_get_ni_handle(struct ptl_context *cnxt) {
  return cnxt->ni_handle;
}
