#ifndef PTL_UUID_H
#define PTL_UUID_H
#include "ptl_log.h"
#include <stdint.h>
/* We use most significant byte for matching*/
#define PTL_UUID_IGNORE_MASK    0x00FFFFFFFFFFFFFFUL
#define PTL_UUID_SEND_RECV_MASK 0x4000000000000000UL
#define PTL_UUID_RMA_MASK       0x8000000000000000UL

/*We keep target's qp num in the last four least significant (0,1,2,3) bytes of uuid*/
#define PTL_UUID_TARGET_QP_NUM_MASK    0xFFFFFFFF00000000UL
/*We keep initiator's qp num in bytes 4,5,6*/
#define PTL_UUID_INITIATOR_QP_NUM_MASK 0xFF000000FFFFFFFFUL
#define PTL_UUID_INITIATOR_MAX_QP_NUM 8388607

typedef enum {
	PTL_SEND_RECV = 19,
	PTL_RMA
} ptl_uuid_op_type_e;


static inline uint64_t ptl_uuid_set_op_type(uint64_t uuid, ptl_uuid_op_type_e op)
{
	if (op == PTL_SEND_RECV) {
		return (uuid & PTL_UUID_IGNORE_MASK) | PTL_UUID_SEND_RECV_MASK;
	}

	if (op == PTL_RMA) {
		return (uuid & PTL_UUID_IGNORE_MASK) | PTL_UUID_RMA_MASK;
	}

	SPDK_PTL_FATAL("Unknown type of operation");
	return 0;
}

static inline int ptl_uuid_set_target_qp_num(uint64_t uuid, int qp_num)
{
	// SPDK_PTL_DEBUG("CP server: Setting target's qp num to %d", qp_num);
	/*clear first*/
	uuid &= PTL_UUID_TARGET_QP_NUM_MASK;
	uuid |= (uint64_t)(uint32_t)qp_num;
	return uuid;
}

static inline int ptl_uuid_get_target_qp_num(uint64_t uuid)
{
	int32_t qp_num = (int32_t)(uuid & 0x00000000FFFFFFFFULL);
	// SPDK_PTL_DEBUG("CP server: Getting target's qp num: %d", qp_num);
	return qp_num;
}




static inline uint64_t ptl_uuid_set_initiator_qp_num(uint64_t uuid, int qp_num)
{
	// SPDK_PTL_DEBUG("CP server: Setting initiator's qp num to %d", qp_num);
	if (qp_num < 0 || qp_num > PTL_UUID_INITIATOR_MAX_QP_NUM) {
		SPDK_PTL_FATAL("Too large");
	}
	uint32_t masked = (uint32_t)(qp_num & 0x00FFFFFF);
	uuid &= uuid & PTL_UUID_INITIATOR_QP_NUM_MASK;
	uuid |= ((uint64_t)masked) << 32;
	return uuid;
}

static inline int ptl_uuid_get_initiator_qp_num(uint64_t uuid)
{
	int qp_num = (uuid >> 32) & 0x00FFFFFF;
	// SPDK_PTL_DEBUG("CP server: Getting initiator's qp num: %d", qp_num);
	return (uuid >> 32) & 0x00FFFFFF;
}


#endif

