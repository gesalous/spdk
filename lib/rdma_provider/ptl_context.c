#include "ptl_context.h"
#include "portals_log.h"
#include <assert.h>
#include <infiniband/verbs.h>
#include <portals4.h>
#include <stdbool.h>
#include <stdint.h>
#define PTL_SERVER_PID 0
#define PTL_MAGIC_NUMBER 27060211UL/*Bdays of my boys :)*/
#define PTL_EVENT_QUEUE_SIZE 2048
#define PTL_MD_SIZE 1024

#define SPDK_PTL_CONTAINER_OF(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);   \
    (type *)( (char *)__mptr - offsetof(type,member) );})

struct ptl_cnxt_mem_handle {
  void *vaddr;
  size_t size;
  ptl_md_t mem_handle;
  bool inuse;
};

struct ptl_context{
  uint64_t magic_number;
  struct ptl_cnxt_mem_handle mem_handles[PTL_MD_SIZE];/*For local accounting only*/
  ptl_handle_ni_t ni_handle;
  ptl_handle_eq_t eq_handle;
  ptl_pt_index_t portals_idx;
  struct ibv_context fake_ibv_cnxt;
  struct ibv_pd pd;
  struct ibv_cq fake_cq;
  struct spdk_rdma_provider_srq *srq;
  bool initialized;
};

static struct ptl_context ptl_context;

struct ibv_cq * ptl_cnxt_get_fake_ibv_cq(struct ptl_context *cnxt) {
  return &cnxt->fake_cq;
}

void ptl_cnxt_set_eq(struct ptl_context *cnxt, ptl_handle_eq_t eq_handle) {
  assert(cnxt);
  cnxt->eq_handle = eq_handle;
}

static int ptx_cnxt_poll_cq(struct ibv_cq *cq, int num_entries,
                            struct ibv_wc *wc) {
  ptl_event_t event;
  // SPDK_PTL_DEBUG("Trapped ibv_poll_cq");
  struct ptl_context *ptl_cnxt =
      SPDK_PTL_CONTAINER_OF(cq, struct ptl_context, fake_cq);
  if (PTL_MAGIC_NUMBER != ptl_cnxt->magic_number) {
    SPDK_PTL_FATAL("Corrupted portals context");
  }

  int ret = PtlEQGet(ptl_cnxt->eq_handle, &event);
  if (ret == PTL_OK) {
    SPDK_PTL_FATAL("Got event! UNIMPLEMENTED");
  } else if (ret == PTL_EQ_EMPTY) {
    // SPDK_PTL_DEBUG("No events ok COOL");
  } else {
    SPDK_PTL_FATAL("PtlEQGet failed with error code %d", ret);
  }
  return 0;
}

struct ptl_context *ptl_cnxt_get(void) {
  int ret;

  if(ptl_context.initialized)
    return &ptl_context;

  ptl_context.magic_number = PTL_MAGIC_NUMBER;
  SPDK_PTL_DEBUG("Calling PtlInit()");
  ret = PtlInit();
  if (ret != PTL_OK) {
    SPDK_PTL_FATAL("PtlInit failed");
  }

  const char *srv_nid = getenv("SERVER_NID");
  if (srv_nid) {
    ret = PtlNIInit((int)atoi(srv_nid), PTL_NI_MATCHING | PTL_NI_PHYSICAL,
                    PTL_SERVER_PID, NULL, NULL, &ptl_context.ni_handle);
  } else {
    SPDK_PTL_WARN(
        "RDMACM: SERVER_NID not set. Using default nid PTL_IFACE_DEFAULT=0!");
    ret = PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_MATCHING | PTL_NI_PHYSICAL,
                    PTL_SERVER_PID, NULL, NULL, &ptl_context.ni_handle);
  }

  if (ret != PTL_OK) {
    SPDK_PTL_FATAL("RDMACM: PtlNIInit failed");
  }

  ret = PtlEQAlloc(ptl_context.ni_handle, PTL_EVENT_QUEUE_SIZE, &ptl_context.eq_handle);
  if (ret != PTL_OK) {
    SPDK_PTL_FATAL("PtlEQAlloc failed");
  }
  SPDK_PTL_DEBUG("SUCCESSFULLY allocated event queue for PORTALS");

  ret = PtlPTAlloc(ptl_context.ni_handle, 0, ptl_context.eq_handle, PTL_PT_ANY, &ptl_context.portals_idx);
  if (ret != PTL_OK) {
    SPDK_PTL_FATAL("PtlPTAlloc failed");
  }
  SPDK_PTL_DEBUG("Setting callbacl for ptl_cnxt_poll_cq");
  ptl_context.fake_ibv_cnxt.ops.poll_cq = ptx_cnxt_poll_cq;
  ptl_context.fake_cq.context = &ptl_context.fake_ibv_cnxt;

  SPDK_PTL_DEBUG("SUCCESSFULLY create and initialized PORTALS context");
  ptl_context.initialized = true;
  return &ptl_context;
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

bool ptl_cnxt_add_md(struct ptl_context *cnxt, void *vaddr, size_t size,
                     ptl_handle_md_t memory_handle) {
  bool inserted = false;
  for (uint32_t i = 0; i < PTL_MD_SIZE; i++) {
   if (cnxt->mem_handles[i].inuse)
     continue;
   inserted = true;
   cnxt->mem_handles[i].vaddr = vaddr;
   cnxt->mem_handles[i].size = size;
   cnxt->mem_handles[i].inuse = true;
   break;
  }
  return inserted;
}
