#ifndef PTL_CONNECTION_H
#define PTL_CONNECTION_H
#include <rdma/rdma_cma.h>
#include <stdint.h>

struct ptl_conn_info {
	uint64_t version;
	int target_nid;
	int target_pid;
	int initiator_nid;
	int initiator_pid;
	/*where client expects the reply*/
	int pt_index;
};

struct ptl_conn_info_reply {
	uint64_t version;
	int status;
};
#endif
