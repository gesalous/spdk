#ifndef PTL_UUID_H
#define PTL_UUID_H
#include "ptl_log.h"
#include <stdint.h>

/* We use the 2 most significant bytes for match bits */
#define PTL_UUID_IGNORE_MASK          0x0000FFFFFFFFFFFFUL
typedef enum {
	PTL_SEND_RECV = 19,
	PTL_RMA
} ptl_uuid_op_type_e;


uint64_t ptl_uuid_set_op_type(uint64_t uuid, ptl_uuid_op_type_e op);

uint64_t ptl_uuid_set_target_qp_num(uint64_t uuid, int qp_num);

int ptl_uuid_get_target_qp_num(uint64_t uuid);

uint64_t ptl_uuid_set_initiator_qp_num(uint64_t uuid, int qp_num);

int ptl_uuid_get_initiator_qp_num(uint64_t uuid);

int ptl_uuid_get_cq_num(uint64_t uuid);

uint64_t ptl_uuid_set_cq_num(uint64_t uuid, int cq_num);
#endif

