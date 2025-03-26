#ifndef PTL_CONNECTION_H
#define PTL_CONNECTION_H
#include <rdma/rdma_cma.h>
#include <stdint.h>

struct ptl_conn_info {
	uint64_t version;
	int src_nid;
	int src_pid;
	/*where client expects the reply*/
	int dst_pt_index;
  int dst_nid;
  int dst_pid;
  size_t page_size;
  size_t alignment_size;
};

struct ptl_conn_info_reply {
	uint64_t version;
	int status;
	int pad;
  size_t page_size;
  size_t alignment_size;
};
#endif
