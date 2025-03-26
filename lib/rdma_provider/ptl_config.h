#ifndef PTL_CONFIG_H
#define PTL_CONFIG_H

#define PTL_TARGET_PID 16

#define PTL_SPDK_PROTOCOL_VERSION 1UL
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
#define PTL_CONTROL_PLANE_TARGET_MAILBOX 1

#define PTL_CONTROL_PLANE_INITIATOR_MAILBOX 2
/**
 * Number of recv buffers posted through PtlLEAppend for receiving new
 *connection info from clients.
 **/
#define PTL_CONTROL_PLANE_NUM_RECV_BUFFERS 2048U

/**
 * Size of the Portals event queue
**/
#define PTL_CQ_SIZE 128


#define PTL_IGNORE 0xffffffff

#define PTL_MATCH 1

#define PTL_SRV_ME_OPTS                                                                                                 \
	PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_MAY_ALIGN | PTL_ME_IS_ACCESSIBLE | PTL_ME_MANAGE_LOCAL | \
		PTL_ME_NO_TRUNCATE | PTL_LE_USE_ONCE

#define PTL_IOVEC_SIZE 2


#endif

