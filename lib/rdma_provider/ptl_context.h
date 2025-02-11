#ifndef PTL_CONTEXT_H
#define PTL_CONTEXT_H

struct ptl_context;
struct ibv_context;
struct ibv_pd;

struct ptl_context * ptl_cnxt_create(void);
struct ibv_context * ptl_get_ibv_context(struct ptl_context *cnxt);
struct ibv_pd * ptl_get_ibv_pd(struct ptl_context *cnxt);
struct ptl_context * ptl_get_cnxt_ibcnxt(struct ibv_context *ib_cnxt);
struct ptl_context *ptl_get_cnxt_from_ibpd(struct ibv_pd *ib_pd);
#endif
