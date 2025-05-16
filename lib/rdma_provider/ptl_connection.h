#ifndef PTL_CONNECTION_H
#define PTL_CONNECTION_H
#include <rdma/rdma_cma.h>
#include <stdint.h>
typedef enum {PTL_OPEN_CONNECTION = 12, PTL_OPEN_CONNECTION_REPLY, PTL_CLOSE_CONNECTION, PTL_CLOSE_CONNECTION_REPLY} ptl_conn_msg_type_e;


struct ptl_conn_msg_header {
	ptl_conn_msg_type_e msg_type;
	uint64_t version;
};

struct ptl_conn_open {
	struct sockaddr src_addr;
	int src_nid;
	int src_pid;
	/*where client expects the reply*/
	int dst_pt_index;
	int dst_nid;
	int dst_pid;
	int initiator_qp_num;
};

struct ptl_conn_open_reply {
	uint64_t uuid;
	int status;
};

struct ptl_conn_close {
	uint64_t uuid;
};

struct ptl_conn_close_reply {
	uint64_t uuid;
	int status;
};

struct ptl_conn_msg {
	struct ptl_conn_msg_header msg_header;
	union {
		struct ptl_conn_open conn_open;
		struct ptl_conn_open_reply conn_open_reply;
		struct ptl_conn_close conn_close;
		struct ptl_conn_close_reply conn_close_reply;
	};
};
#endif
