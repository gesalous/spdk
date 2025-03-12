#ifndef PTL_CONFIG_H
#define PTL_CONFIG_H
/**
  * Portal index number where the actual nvme commands and data are transfered
**/
#define PTL_DATA_PLANE_PT_INDEX 0

/**
 * Portal index number where clients use during rdma_connect to notify the
 * targer a)about their presence and b) enable the target to build a lookup
 * table from queue pair id to nid,pid that Portals uses for point to point
 * communication.
 */
#define PTL_CONTROL_PLANE_PT_INDEX 1

/**
 * Size of the Portals event queue
**/
#define PTL_CQ_SIZE 128

#endif

