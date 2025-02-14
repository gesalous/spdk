/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright (C) 2020 Intel Corporation. All rights reserved.
 *   Copyright (c) Mellanox Technologies LTD. All rights reserved.
 *   Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#include <rdma/rdma_cma.h>
#include "portals4.h"
#include "spdk/likely.h"
#include "spdk/stdinc.h"
#include "spdk/string.h"

#include "portals_log.h"
#include "ptl_context.h"
#include "spdk/log.h"
#include "spdk_internal/rdma_provider.h"
#include "spdk_internal/rdma_utils.h"

//from common.c staff
#define SPDK_PTL_PROVIDER_MAGIC_NUMBER 27081983UL
struct spdk_portals_provider_srq{
  uint64_t magic_number;
  struct spdk_rdma_provider_srq fake_srq;
  struct ptl_context *ptl_context;
};


struct spdk_rdma_provider_srq *
spdk_rdma_provider_srq_create(struct spdk_rdma_provider_srq_init_attr *init_attr)
{
	assert(init_attr);
	assert(init_attr->pd);
  struct ptl_context * ptl_context;
  struct spdk_portals_provider_srq *portals_srq;
  struct spdk_rdma_provider_srq * rdma_srq;

  ptl_context = ptl_cnxt_get_from_ibvpd(init_attr->pd);
  SPDK_PTL_DEBUG("Ok got portals context from ibv_pd!");
  portals_srq = calloc(1UL, sizeof(*portals_srq));
	if (!portals_srq) {
		SPDK_PTL_FATAL("Can't allocate memory for SRQ handle\n");
	}
  portals_srq->magic_number = SPDK_PTL_PROVIDER_MAGIC_NUMBER;
  portals_srq->ptl_context = ptl_context;
  rdma_srq = &portals_srq->fake_srq;


	if (init_attr->stats) {
		rdma_srq->stats = init_attr->stats;
		rdma_srq->shared_stats = true;
	} else {
		rdma_srq->stats = calloc(1UL, sizeof(*rdma_srq->stats));
		if (!rdma_srq->stats) {
			SPDK_PTL_FATAL("SRQ statistics memory allocation failed");
			free(rdma_srq);
			return NULL;
		}
	}

	// rdma_srq->srq = ibv_create_srq(init_attr->pd, &init_attr->srq_init_attr);
  rdma_srq->srq = NULL;/*On purpose*/
	// if (!rdma_srq->srq) {
	// 	if (!init_attr->stats) {
	// 		free(rdma_srq->stats);
	// 	}
	// 	SPDK_ERRLOG("Unable to create SRQ, errno %d (%s)\n", errno, spdk_strerror(errno));
	// 	free(rdma_srq);
	// 	return NULL;
	// }
  SPDK_PTL_DEBUG("Ok emulated the RDMA_SRQ creation with PORTALS!");
	return rdma_srq;
}

int
spdk_rdma_provider_srq_destroy(struct spdk_rdma_provider_srq *rdma_srq)
{
	if (!rdma_srq) {
		return 0;
	}

	assert(rdma_srq->srq);
  SPDK_PTL_FATAL("UNIMPLEMENTED");
	free(rdma_srq);
  return -1;
}

static inline bool
rdma_queue_recv_wrs(struct spdk_rdma_provider_recv_wr_list *recv_wrs, struct ibv_recv_wr *first,
		    struct spdk_rdma_provider_wr_stats *recv_stats)
{
  SPDK_PTL_FATAL("UNIMPLEMENTED");
  return false;
}

bool
spdk_rdma_provider_srq_queue_recv_wrs(struct spdk_rdma_provider_srq *rdma_srq,
				      struct ibv_recv_wr *first)
{
	assert(rdma_srq);
	assert(first);
  SPDK_PTL_FATAL("UNIMPLEMENTED");
  return false;
}

int
spdk_rdma_provider_srq_flush_recv_wrs(struct spdk_rdma_provider_srq *rdma_srq,
				      struct ibv_recv_wr **bad_wr)
{
  SPDK_PTL_FATAL("UNIMPLEMENTED");
  return -1;
}

bool
spdk_rdma_provider_qp_queue_recv_wrs(struct spdk_rdma_provider_qp *spdk_rdma_qp,
				     struct ibv_recv_wr *first)
{
	assert(spdk_rdma_qp);
	assert(first);
  SPDK_PTL_FATAL("UNIMPLEMENTED");
  return false;
}

int
spdk_rdma_provider_qp_flush_recv_wrs(struct spdk_rdma_provider_qp *spdk_rdma_qp,
				     struct ibv_recv_wr **bad_wr)
{
  SPDK_PTL_FATAL("UNIMPLEMENTED");
	return -1;
}
// common end

struct spdk_rdma_provider_qp *
spdk_rdma_provider_qp_create(struct rdma_cm_id *cm_id,
			     struct spdk_rdma_provider_qp_init_attr *qp_attr)
{
  SPDK_PTL_FATAL("UNIMPLEMENTED");
  return NULL;
}

int
spdk_rdma_provider_qp_accept(struct spdk_rdma_provider_qp *spdk_rdma_qp,
			     struct rdma_conn_param *conn_param)
{
	assert(spdk_rdma_qp != NULL);
	assert(spdk_rdma_qp->cm_id != NULL);
  SPDK_PTL_FATAL("UNIMPLEMENTED");
	return 0;
}

int
spdk_rdma_provider_qp_complete_connect(struct spdk_rdma_provider_qp *spdk_rdma_qp)
{
	/* Nothing to be done for Verbs */
  SPDK_PTL_FATAL("UNIMPLEMENTED");
	return 0;
}

void
spdk_rdma_provider_qp_destroy(struct spdk_rdma_provider_qp *spdk_rdma_qp)
{
	assert(spdk_rdma_qp != NULL);
  SPDK_PTL_FATAL("UNIMPLEMENTED");
	free(spdk_rdma_qp);
}

int
spdk_rdma_provider_qp_disconnect(struct spdk_rdma_provider_qp *spdk_rdma_qp)
{
	assert(spdk_rdma_qp != NULL);
  SPDK_PTL_FATAL("UNIMPLEMENTED");
  return 0;
}

bool
spdk_rdma_provider_qp_queue_send_wrs(struct spdk_rdma_provider_qp *spdk_rdma_qp,
				     struct ibv_send_wr *first)
{

	assert(spdk_rdma_qp);
	assert(first);
  SPDK_PTL_FATAL("UNIMPLEMENTED");
  return false;
}

int
spdk_rdma_provider_qp_flush_send_wrs(struct spdk_rdma_provider_qp *spdk_rdma_qp,
				     struct ibv_send_wr **bad_wr)
{
  SPDK_PTL_FATAL("UNIMPLEMENTED");
  return 0;
}

bool
spdk_rdma_provider_accel_sequence_supported(void)
{
  SPDK_PTL_FATAL("UNIMPLEMENTED");
	return false;
}

