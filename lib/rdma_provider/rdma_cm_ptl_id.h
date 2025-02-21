#ifndef RDMA_CM_PTL_ID_H
#define RDMA_CM_PTL_ID_H
#include "lib/rdma_provider/portals_log.h"
#include "spdk/util.h"
#include <rdma/rdma_cma.h>
#include <stdint.h>
#include <sys/socket.h>
#define RDMA_CM_PTL_ID_MAGIG_NUMBER 14101983UL
struct rdma_cm_ptl_id {
	uint64_t magic_number;
	struct rdma_cm_ptl_event_channel *ptl_channel;
	struct sockaddr src_addr;
	struct sockaddr dest_addr;
	struct rdma_cm_id fake_cm_id;
	struct ibv_qp fake_qp;
	struct ibv_qp_init_attr qp_init_attr;
  //needed for connection setup and shit
  const void *fake_data;
};

static inline struct rdma_cm_ptl_id *rdma_cm_ptl_id_get(struct rdma_cm_id *id)
{
	struct rdma_cm_ptl_id *ptl_id =
		SPDK_CONTAINEROF(id, struct rdma_cm_ptl_id, fake_cm_id);
	if (RDMA_CM_PTL_ID_MAGIG_NUMBER != ptl_id->magic_number) {
		SPDK_PTL_FATAL("Corrupted PTL ID");
	}
	return ptl_id;
}

void rdma_cm_ptl_id_create_event(struct rdma_cm_ptl_id *ptl_id,
				 struct rdma_cm_id *id,
				 enum rdma_cm_event_type event_type);

static inline struct sockaddr *
rdma_cm_ptl_id_get_src_addr(struct rdma_cm_ptl_id *ptl_id)
{
	return &ptl_id->src_addr;
}

void rdma_cm_ptl_id_set_fake_data(struct rdma_cm_ptl_id *ptl_id, const void *fake_data);
#endif
