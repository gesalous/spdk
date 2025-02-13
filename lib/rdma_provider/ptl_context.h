#ifndef PTL_CONTEXT_H
#define PTL_CONTEXT_H
#include "portals4.h"
struct ptl_context;
struct ibv_context;
struct ibv_pd;

struct ptl_context * ptl_cnxt_create(void);
struct ibv_context * ptl_cnxt_get_ibv_context(struct ptl_context *cnxt);
struct ibv_pd * ptl_cnxt_get_ibv_pd(struct ptl_context *cnxt);
struct ptl_context * ptl_cnxt_get_from_ibcnxt(struct ibv_context *ib_cnxt);
struct ptl_context *ptl_cnxt_get_from_ibvpd(struct ibv_pd *ib_pd);

ptl_handle_eq_t ptl_cnxt_get_event_queue(struct ptl_context *cnxt);
ptl_pt_index_t ptl_cnxt_get_portal_index(struct ptl_context *cnxt);
ptl_handle_ni_t ptl_cnxt_get_ni_handle(struct ptl_context *cnxt);
#endif
