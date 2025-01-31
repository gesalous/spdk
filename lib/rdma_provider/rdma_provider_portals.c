/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright (C) 2020 Intel Corporation. All rights reserved.
 *   Copyright (c) Mellanox Technologies LTD. All rights reserved.
 *   Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#include <rdma/rdma_cma.h>

#include "spdk/stdinc.h"
#include "spdk/string.h"
#include "spdk/likely.h"

#include "spdk_internal/rdma_provider.h"
#include "spdk_internal/rdma_utils.h"
#include "spdk/log.h"

struct spdk_rdma_provider_qp *
spdk_rdma_provider_qp_create(struct rdma_cm_id *cm_id,
			     struct spdk_rdma_provider_qp_init_attr *qp_attr)
{
  fprintf(stderr,"%s:%s:%d\n",__FILE__,__func__,__LINE__);
  return NULL;
}

int
spdk_rdma_provider_qp_accept(struct spdk_rdma_provider_qp *spdk_rdma_qp,
			     struct rdma_conn_param *conn_param)
{
	assert(spdk_rdma_qp != NULL);
	assert(spdk_rdma_qp->cm_id != NULL);

  fprintf(stderr,"%s:%s:%d\n",__FILE__,__func__,__LINE__);
	return 0;
}

int
spdk_rdma_provider_qp_complete_connect(struct spdk_rdma_provider_qp *spdk_rdma_qp)
{
	/* Nothing to be done for Verbs */
  fprintf(stderr,"%s:%s:%d\n",__FILE__,__func__,__LINE__);
	return 0;
}

void
spdk_rdma_provider_qp_destroy(struct spdk_rdma_provider_qp *spdk_rdma_qp)
{
	assert(spdk_rdma_qp != NULL);

  fprintf(stderr,"%s:%s:%d\n",__FILE__,__func__,__LINE__);

	free(spdk_rdma_qp);
}

int
spdk_rdma_provider_qp_disconnect(struct spdk_rdma_provider_qp *spdk_rdma_qp)
{

	assert(spdk_rdma_qp != NULL);
  fprintf(stderr,"%s:%s:%d\n",__FILE__,__func__,__LINE__);
  return 0;
}

bool
spdk_rdma_provider_qp_queue_send_wrs(struct spdk_rdma_provider_qp *spdk_rdma_qp,
				     struct ibv_send_wr *first)
{

	assert(spdk_rdma_qp);
	assert(first);
  return false;
}

int
spdk_rdma_provider_qp_flush_send_wrs(struct spdk_rdma_provider_qp *spdk_rdma_qp,
				     struct ibv_send_wr **bad_wr)
{
  fprintf(stderr,"%s:%s:%d\n",__FILE__,__func__,__LINE__);
  return 0;
}

bool
spdk_rdma_provider_accel_sequence_supported(void)
{
  fprintf(stderr,"%s:%s:%d\n",__FILE__,__func__,__LINE__);
	return false;
}
