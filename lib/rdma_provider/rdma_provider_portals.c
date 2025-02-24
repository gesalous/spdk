/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright (C) 2020 Intel Corporation. All rights reserved.
 *   Copyright (c) Mellanox Technologies LTD. All rights reserved.
 *   Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */
#include "portals4.h"
#include "spdk/likely.h"
#include "spdk/stdinc.h"
#include "spdk/string.h"
#include <rdma/rdma_cma.h>

#include "portals_log.h"
#include "ptl_context.h"
#include "rdma_cm_ptl_id.h"
#include "spdk/log.h"
#include "spdk/util.h"
#include "spdk_internal/rdma_provider.h"
#include "spdk_internal/rdma_utils.h"

//from common.c staff
#define SPDK_PTL_PROVIDER_SRQ_MAGIC_NUMBER 27081983UL
#define SPDK_PTL_PROVIDER_QP_MAGIC_NUMBER 19082018UL
#define SPDK_PTL_CHECK_SRQ(X) \
    if ((X)->magic_number != SPDK_PTL_PROVIDER_SRQ_MAGIC_NUMBER) { \
        SPDK_PTL_FATAL("Corrupted PORTALS SRQ"); \
    }

struct spdk_portals_provider_srq {
	uint64_t magic_number;
	struct spdk_rdma_provider_srq fake_srq;
	struct ptl_context *ptl_context;
};

struct spdk_portals_provider_qp {
	uint64_t magic_number;
	struct ptl_context *ptl_context;
	struct rdma_cm_ptl_id *ptl_id;
	struct spdk_rdma_provider_qp fake_spdk_rdma_qp;
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
	portals_srq->magic_number = SPDK_PTL_PROVIDER_SRQ_MAGIC_NUMBER;
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

bool spdk_rdma_provider_srq_queue_recv_wrs(
	struct spdk_rdma_provider_srq *rdma_srq, struct ibv_recv_wr *first)
{
	struct ibv_recv_wr *last;
	struct spdk_rdma_provider_wr_stats *recv_stats;
	struct spdk_rdma_provider_recv_wr_list *recv_wrs;
	bool ret;

	assert(rdma_srq->stats);
	recv_stats = rdma_srq->stats;
	uint64_t diff = recv_stats->num_submitted_wrs;
	recv_stats->num_submitted_wrs++;
	last = first;
	while (last->next != NULL) {
		last = last->next;
		recv_stats->num_submitted_wrs++;
	}

	recv_wrs = &rdma_srq->recv_wrs;

	if (recv_wrs->first == NULL) {
		recv_wrs->first = first;
		recv_wrs->last = last;
		ret = true;
	} else {
		recv_wrs->last->next = first;
		recv_wrs->last = last;
		ret = false;
	}
	SPDK_PTL_DEBUG("Done exact the same steps as in IBV case total "
		       "submitted wrs: %lu current: %lu",
		       recv_stats->num_submitted_wrs,
		       recv_stats->num_submitted_wrs - diff);
	return ret;
}


/**
 * @brief Cleans up the ibv_recv_wr descriptors. In the vanilla implementation hardware handles the cleanup process. However,
 * since SPDK_PTL translates the ibv_recv_wr into PTLMEAppend entries, SPDK_PTL needs to do the cleanup.
 */
// static bool spdk_ptl_clean_up_recv_wr_desc(struct ibv_recv_wr *recv_wrs) {
//   return true;
// }


int
spdk_rdma_provider_srq_flush_recv_wrs(struct spdk_rdma_provider_srq *rdma_srq,
				      struct ibv_recv_wr **bad_wr)
{
	ptl_handle_ni_t nic;
	ptl_pt_index_t pt_index;
	ptl_le_t le;
	ptl_md_t md;
	ptl_handle_le_t le_handle;
	int ret;

	if (spdk_unlikely(rdma_srq->recv_wrs.first == NULL)) {
		return 0;
	}
	//Now it's time to append the entries in portals
	int i = 0;
	for (struct ibv_recv_wr *wr = rdma_srq->recv_wrs.first; wr != NULL;
	     wr = wr->next) {
		SPDK_PTL_DEBUG("Ok iterated entry: %d", i++);
		struct spdk_portals_provider_srq *portals_srq =
			SPDK_CONTAINEROF(rdma_srq, struct spdk_portals_provider_srq, fake_srq);
		SPDK_PTL_CHECK_SRQ(portals_srq);
		nic = ptl_cnxt_get_ni_handle(portals_srq->ptl_context);
		pt_index = ptl_cnxt_get_portal_index(portals_srq->ptl_context);
		SPDK_PTL_DEBUG("Num sge is %d", wr->num_sge);
		for (int i = 0; i < wr->num_sge; i++) {
			uint64_t memory_address = wr->sg_list[i].addr;  // Memory address of the buffer
			uint32_t length = wr->sg_list[i].length;        // Length of the buffer
			uint32_t lkey = wr->sg_list[i].lkey;            // Local key for the memory region

			SPDK_PTL_DEBUG("SGE %d: Address = 0x%lx, Length = %u, LKey = 0x%x\n",
				       i, memory_address, length, lkey);
			// Initialize the matching entry
			memset(&le, 0, sizeof(ptl_le_t));

			// Setup the memory descriptor
			md.start = (void *)wr->sg_list[i].addr;
			md.length = wr->sg_list[i].length;
			md.options = PTL_MD_EVENT_SUCCESS_DISABLE;  // Adjust options as needed
			md.eq_handle = PTL_EQ_NONE;                // Adjust if you need events

			// Setup the list entry
			le.start = md.start;
			le.length = md.length;
			le.uid = PTL_UID_ANY;                      // Accept from any user
			le.options = PTL_LE_OP_PUT;                // Allow PUT operations
			le.match_id.rank = PTL_RANK_ANY;           // Accept from any rank
			le.match_bits = 0;                         // Adjust match bits as needed
			le.ignore_bits = 0;                        // Adjust ignore bits as needed

			// Place custom context
			void *user_ptr = (void *)(uintptr_t)wr->wr_id;

			// Append the memory entry
			ret = PtlMEAppend(
				      nic,                    // Network interface handle
				      pt_index,               // Portal table index
				      &le,                    // List entry
				      PTL_PRIORITY_LIST,      // List type (PRIORITY or OVERFLOW)
				      user_ptr,               // User pointer (can be used to store wr_id)
				      &le_handle              // Returned handle
			      );
			if (PTL_OK != ret) {
				SPDK_PTL_FATAL("Failed to append memory entry");
			}

		}
	}
	SPDK_PTL_DEBUG("Ok append the memory entries in Portals");
	SPDK_PTL_DEBUG("UNIMPLEMENTED do the cleanup? XXX TODO XXX!");
	// rc = ibv_post_srq_recv(rdma_srq->srq, rdma_srq->recv_wrs.first, bad_wr);
	rdma_srq->recv_wrs.first = NULL;
	rdma_srq->stats->doorbell_updates++;
	return 0;
}

bool spdk_rdma_provider_qp_queue_recv_wrs(
	struct spdk_rdma_provider_qp *spdk_rdma_qp, struct ibv_recv_wr *first)
{

	assert(spdk_rdma_qp);
	assert(first);
	SPDK_PTL_DEBUG("DOING SAME STAFF AS VERBS");
	struct spdk_rdma_provider_wr_stats *recv_stats =
			&spdk_rdma_qp->stats->recv;
	struct spdk_rdma_provider_recv_wr_list *recv_wrs =
			&spdk_rdma_qp->recv_wrs;
	struct ibv_recv_wr *last;

	recv_stats->num_submitted_wrs++;
	last = first;
	while (last->next != NULL) {
		last = last->next;
		recv_stats->num_submitted_wrs++;
	}

	if (recv_wrs->first == NULL) {
		recv_wrs->first = first;
		recv_wrs->last = last;
		return true;
	} else {
		recv_wrs->last->next = first;
		recv_wrs->last = last;
		return false;
	}
}

int
spdk_rdma_provider_qp_flush_recv_wrs(struct spdk_rdma_provider_qp *spdk_rdma_qp,
				     struct ibv_recv_wr **bad_wr)
{
	SPDK_PTL_DEBUG("PORTALS REGISTERING THE BUFFERS FOR THE *SINGLE* QUEUE PAIR CASE (NOT SRQ)");
	struct spdk_portals_provider_qp *portals_qp;
	ptl_handle_ni_t nic;
	ptl_pt_index_t pt_index;
	ptl_le_t le;
	ptl_md_t md;
	ptl_handle_md_t md_handle;
	ptl_handle_le_t le_handle;
	int ret;

	if (spdk_unlikely(spdk_rdma_qp->recv_wrs.first == NULL)) {
		return 0;
	}
	/* gesalous start */
	portals_qp = SPDK_CONTAINEROF(spdk_rdma_qp, struct spdk_portals_provider_qp, fake_spdk_rdma_qp);
	if (SPDK_PTL_PROVIDER_QP_MAGIC_NUMBER != portals_qp->magic_number) {
		SPDK_PTL_FATAL("Corrupted Portals QP!");
	}

	if (spdk_unlikely(spdk_rdma_qp->recv_wrs.first == NULL)) {
		return 0;
	}
	//Now it's time to append the entries in portals
	for (struct ibv_recv_wr *wr = spdk_rdma_qp->recv_wrs.first; wr != NULL;
	     wr = wr->next) {
		nic = ptl_cnxt_get_ni_handle(portals_qp->ptl_context);
		pt_index = ptl_cnxt_get_portal_index(portals_qp->ptl_context);
		SPDK_PTL_DEBUG("Num sge is %d", wr->num_sge);
		for (int i = 0; i < wr->num_sge; i++) {
			uint64_t memory_address = wr->sg_list[i].addr;  // Memory address of the buffer
			uint32_t length = wr->sg_list[i].length;        // Length of the buffer
			uint32_t lkey = wr->sg_list[i].lkey;            // Local key for the memory region

			SPDK_PTL_DEBUG("SGE %d: Address = 0x%lx, Length = %u, LKey = 0x%x\n",
				       i, memory_address, length, lkey);
			// Initialize the matching entry
			memset(&le, 0, sizeof(ptl_le_t));

			// Setup the memory descriptor
			md.start = (void *)wr->sg_list[i].addr;
			md.length = wr->sg_list[i].length;
			md.options = PTL_MD_EVENT_SUCCESS_DISABLE;  // Adjust options as needed
			md.eq_handle = PTL_EQ_NONE;                // Adjust if you need events
			// First bind the memory
			ret = PtlMDBind(
				      nic,                    // Network interface handle
				      &md,                    // Memory descriptor
				      &md_handle             // Returned MD handle
			      );
			if (PTL_OK != ret) {
				SPDK_PTL_FATAL("Failed to bind memory descriptor");
			}
    	// Setup the list entry
			le.start = md.start;
			le.length = md.length;
			le.uid = PTL_UID_ANY;                      // Accept from any user
			le.options = PTL_LE_OP_PUT;                // Allow PUT operations
			le.match_id.rank = PTL_RANK_ANY;           // Accept from any rank
			le.match_bits = 0;                         // Adjust match bits as needed
			le.ignore_bits = 0;                        // Adjust ignore bits as needed

			// Place custom context
			void *user_ptr = (void *)(uintptr_t)wr->wr_id;

			// Append the memory entry
			ret = PtlMEAppend(
				      nic,                    // Network interface handle
				      pt_index,               // Portal table index
				      &le,                    // List entry
				      PTL_PRIORITY_LIST,      // List type (PRIORITY or OVERFLOW)
				      user_ptr,               // User pointer (can be used to store wr_id)
				      &le_handle              // Returned handle
			      );
			if (PTL_OK != ret) {
				SPDK_PTL_FATAL("Failed to append memory entry");
			}

		}
	}
	SPDK_PTL_DEBUG("Ok append the memory entries in Portals");

	/* gesalous end */
	spdk_rdma_qp->recv_wrs.first = NULL;
	spdk_rdma_qp->stats->recv.doorbell_updates++;

	return 0;
}
// common end

struct spdk_rdma_provider_qp *
spdk_rdma_provider_qp_create(struct rdma_cm_id *cm_id,
			     struct spdk_rdma_provider_qp_init_attr *qp_attr)
{
	SPDK_PTL_DEBUG("DOING BASICALLY THE SAME STAFF as the original");
	struct spdk_portals_provider_qp *spdk_portals_qp;
	struct spdk_rdma_provider_qp *spdk_rdma_qp;
	int rc;
	struct ibv_qp_init_attr attr = {.qp_context = qp_attr->qp_context,
						.send_cq = qp_attr->send_cq,
						.recv_cq = qp_attr->recv_cq,
						.srq = qp_attr->srq,
						.cap = qp_attr->cap,
						.qp_type = IBV_QPT_RC
	};

	if (qp_attr->domain_transfer) {
		SPDK_PTL_FATAL(
			"PORTALS provider doesn't support memory domain transfer functionality");
		return NULL;
	}

	spdk_portals_qp = calloc(1UL, sizeof(*spdk_portals_qp));
	if (!spdk_portals_qp) {
		SPDK_ERRLOG("qp memory allocation failed");
	}

	spdk_portals_qp->ptl_context = ptl_cnxt_get();
	spdk_portals_qp->ptl_id = rdma_cm_ptl_id_get(cm_id);
	spdk_portals_qp->magic_number =  SPDK_PTL_PROVIDER_QP_MAGIC_NUMBER;

	spdk_rdma_qp = &spdk_portals_qp->fake_spdk_rdma_qp;


	if (qp_attr->stats) {
		spdk_rdma_qp->stats = qp_attr->stats;
		spdk_rdma_qp->shared_stats = true;
	} else {
		spdk_rdma_qp->stats = calloc(1UL, sizeof(*spdk_rdma_qp->stats));
		if (!spdk_rdma_qp->stats) {
			SPDK_ERRLOG("qp statistics memory allocation failed\n");
			free(spdk_rdma_qp);
			return NULL;
		}
	}

	rc = rdma_create_qp(cm_id, qp_attr->pd, &attr);
	if (rc) {
		SPDK_ERRLOG("Failed to create qp, rc %d, errno %s (%d)\n", rc,
			    spdk_strerror(errno), errno);
		free(spdk_portals_qp);
		return NULL;
	}
	spdk_rdma_qp->qp = cm_id->qp;
	spdk_rdma_qp->cm_id = cm_id;
	spdk_rdma_qp->domain = spdk_rdma_utils_get_memory_domain(qp_attr->pd);
	if (!spdk_rdma_qp->domain) {
		spdk_rdma_provider_qp_destroy(spdk_rdma_qp);
		return NULL;
	}

	qp_attr->cap = attr.cap;
	return spdk_rdma_qp;
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

int spdk_rdma_provider_qp_complete_connect(
	struct spdk_rdma_provider_qp *spdk_rdma_qp)
{
	/* Nothing to be done for Portals */
	SPDK_PTL_DEBUG("CREATE FAKE RDMA_CM_EVENT_ESTABLISHED event");
	rdma_cm_ptl_id_create_event(rdma_cm_ptl_id_get(spdk_rdma_qp->cm_id),
				    spdk_rdma_qp->cm_id,
				    RDMA_CM_EVENT_ESTABLISHED);
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
	SPDK_PTL_DEBUG("No accel sequence supported in PORTALS");
	return false;
}

