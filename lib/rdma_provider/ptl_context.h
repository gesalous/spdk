#ifndef PTL_CONTEXT_H
#define PTL_CONTEXT_H
#include "portals4.h"
#include "ptl_object_types.h"
#include <infiniband/verbs.h>
#include <stdbool.h>

#define PTL_CONTEXT_SERVER_PID 0
struct ptl_context;
struct ibv_context;
struct ibv_pd;

struct ptl_context {
	ptl_obj_type_e object_type;
	ptl_handle_ni_t ni_handle;
	ptl_pt_index_t portals_idx;
  /*gesalous, portals staff*/
  struct ptl_pd *ptl_pd;
	struct ibv_context fake_ibv_cnxt;
	struct ibv_cq fake_cq;
	struct spdk_rdma_provider_srq *srq;
	bool initialized;
};

struct ptl_context *ptl_cnxt_get(void);
struct ibv_context *ptl_cnxt_get_ibv_context(struct ptl_context *cnxt);
struct ptl_context *ptl_cnxt_get_from_ibcnxt(struct ibv_context *ib_cnxt);
struct ptl_context *ptl_cnxt_get_from_ibvpd(struct ibv_pd *ib_pd);


ptl_pt_index_t ptl_cnxt_get_portal_index(struct ptl_context *cnxt);
ptl_handle_ni_t ptl_cnxt_get_ni_handle(struct ptl_context *cnxt);
#endif
