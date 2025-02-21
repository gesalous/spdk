#ifndef PTL_CONTEXT_H
#define PTL_CONTEXT_H
#include "portals4.h"
#include <stdbool.h>
struct ptl_context;
struct ibv_context;
struct ibv_pd;

struct ptl_context *ptl_cnxt_get(void);
struct ibv_context *ptl_cnxt_get_ibv_context(struct ptl_context *cnxt);
struct ibv_pd *ptl_cnxt_get_ibv_pd(struct ptl_context *cnxt);
struct ptl_context *ptl_cnxt_get_from_ibcnxt(struct ibv_context *ib_cnxt);
struct ptl_context *ptl_cnxt_get_from_ibvpd(struct ibv_pd *ib_pd);

ptl_handle_eq_t ptl_cnxt_get_event_queue(struct ptl_context *cnxt);
ptl_pt_index_t ptl_cnxt_get_portal_index(struct ptl_context *cnxt);
ptl_handle_ni_t ptl_cnxt_get_ni_handle(struct ptl_context *cnxt);
bool ptl_cnxt_add_md(struct ptl_context *cnxt, void *vaddr, size_t size,
		     ptl_handle_md_t memory_handle);
struct ibv_cq *ptl_cnxt_get_fake_ibv_cq(struct ptl_context *cnxt);
void ptl_cnxt_set_eq(struct ptl_context *cnxt, ptl_handle_eq_t eq_handle);
#endif
