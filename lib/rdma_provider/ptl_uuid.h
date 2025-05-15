#ifndef PTL_UUID_H
#define PTL_UUID_H
#include "ptl_log.h"
#include <stdint.h>
#define PTL_UUID_IGNORE_MASK 0x3FFFFFFFFFFFFFFFUL
#define PTL_UUID_SEND_RECV_MASK 0x4000000000000000UL
#define PTL_UUID_RMA_MASK 0x8000000000000000UL

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
#endif
