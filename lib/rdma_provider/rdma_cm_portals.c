#include <dlfcn.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <portals4.h>

#define SERVER_PID 0
#define MAGIC_NUMBER 270883UL

#define RDMACMPTL_DEBUG(fmt, ...)                                             \
    do {                                                                      \
        time_t t = time(NULL);                                                \
        struct tm *tm = localtime(&t);                                        \
        char timestamp[32];                                                   \
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);      \
        fprintf(stderr, "[RDMACMPTL_DEBUG][%s][%s:%s:%d] " fmt "\n",         \
                timestamp, __FILE__, __func__, __LINE__, ##__VA_ARGS__);      \
    } while (0)

#define RDMACMPTL_INFO(fmt, ...)                                             \
    do {                                                                      \
        time_t t = time(NULL);                                                \
        struct tm *tm = localtime(&t);                                        \
        char timestamp[32];                                                   \
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);      \
        fprintf(stderr, "[RDMACMPTL_INFO][%s][%s:%s:%d] " fmt "\n",         \
                timestamp, __FILE__, __func__, __LINE__, ##__VA_ARGS__);      \
    } while (0)


#define RDMACMPTL_WARN(fmt, ...)                                             \
    do {                                                                      \
        time_t t = time(NULL);                                                \
        struct tm *tm = localtime(&t);                                        \
        char timestamp[32];                                                   \
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);      \
        fprintf(stderr, "[RDMACMPTL_WARN][%s][%s:%s:%d] " fmt "\n",         \
                timestamp, __FILE__, __func__, __LINE__, ##__VA_ARGS__);      \
    } while (0)


#define RDMACMPTL_FATAL(fmt, ...)                                             \
    do {                                                                      \
        time_t t = time(NULL);                                                \
        struct tm *tm = localtime(&t);                                        \
        char timestamp[32];                                                   \
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);      \
        fprintf(stderr, "[RDMACMPTL_FATAL][%s][%s:%s:%d] " fmt "\n",         \
                timestamp, __FILE__, __func__, __LINE__, ##__VA_ARGS__);      \
        _exit(EXIT_FAILURE);   \
    } while (0)


#define RDMACMPTL_WARN(fmt, ...)                                             \
    do {                                                                      \
        time_t t = time(NULL);                                                \
        struct tm *tm = localtime(&t);                                        \
        char timestamp[32];                                                   \
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);      \
        fprintf(stderr, "[RDMACMPTL_WARN][%s][%s:%s:%d] " fmt "\n",         \
                timestamp, __FILE__, __func__, __LINE__, ##__VA_ARGS__);      \
    } while (0)

#define IBVPTL_DEBUG(fmt, ...)                                             \
    do {                                                                      \
        time_t t = time(NULL);                                                \
        struct tm *tm = localtime(&t);                                        \
        char timestamp[32];                                                   \
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);      \
        fprintf(stderr, "[IBVPTL_DEBUG][%s][%s:%s:%d] " fmt "\n",         \
                timestamp, __FILE__, __func__, __LINE__, ##__VA_ARGS__);      \
    } while (0)

#define IBVPTL_FATAL(fmt, ...)                                             \
    do {                                                                      \
        time_t t = time(NULL);                                                \
        struct tm *tm = localtime(&t);                                        \
        char timestamp[32];                                                   \
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);      \
        fprintf(stderr, "[IBVPTL_FATAL][%s][%s:%s:%d] " fmt "\n",         \
                timestamp, __FILE__, __func__, __LINE__, ##__VA_ARGS__);      \
        _exit(EXIT_FAILURE); \
    } while (0)

#define PTL_CONTAINER_OF(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);   \
    (type *)( (char *)__mptr - offsetof(type,member) );})


struct ptl_context{
  uint64_t magic_number;
  ptl_handle_ni_t ni_handle;
  struct ibv_context fake_ibv_cnxt;
  struct ibv_pd pd;
};


struct rdma_event_channel *rdma_create_event_channel(void) {
  RDMACMPTL_DEBUG("Intercepted rdma_create_event_channel()");
  struct rdma_event_channel *channel = calloc(1UL, sizeof(*channel));
  if (!channel) {
    RDMACMPTL_DEBUG("Allocation of memory failed");
    return NULL;
  }

  channel->fd = eventfd(0, EFD_NONBLOCK);
  if (channel->fd < 0) {
    free(channel);
    RDMACMPTL_DEBUG("eventfd failed. Reason follows:");
    perror("Reason:");
    return NULL;
  }
  return channel;
}

struct ibv_context **rdma_get_devices(int *num_devices) {
  struct ibv_context **devices;
  struct ptl_context *cnxt;
  int ret;

  RDMACMPTL_DEBUG("Intercepted rdma_get_devices");
  RDMACMPTL_DEBUG("Calling PtlInit()");

  cnxt = calloc(1UL, sizeof(*cnxt));
  if(NULL == cnxt){
    RDMACMPTL_FATAL("Memory allocation failed for portals context?");
  }
  cnxt->magic_number = MAGIC_NUMBER;
  ret = PtlInit();
	if (ret != PTL_OK) {
		RDMACMPTL_FATAL("PtlInit failed");
		_exit(EXIT_FAILURE);
	}

	const char *srv_nid = getenv("SERVER_NID");
	if (srv_nid) {
		ret = PtlNIInit((int)atoi(srv_nid), PTL_NI_MATCHING | PTL_NI_PHYSICAL, SERVER_PID, NULL, NULL,
				&cnxt->ni_handle);
	} else {
		RDMACMPTL_WARN("SERVER_NID not set. Using default nid PTL_IFACE_DEFAULT=0!");
		ret = PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_MATCHING | PTL_NI_PHYSICAL, SERVER_PID, NULL, NULL,
				&cnxt->ni_handle);
	}

	if (ret != PTL_OK) {
		RDMACMPTL_FATAL("PtlNIInit failed");
	}

  devices = calloc(1UL, sizeof(struct ibv_context *));
  if(NULL == devices){
    RDMACMPTL_FATAL("Failed to allocate memory for device list");
  }

  devices[0] = &cnxt->fake_ibv_cnxt;
  cnxt->fake_ibv_cnxt.async_fd = eventfd(0, EFD_NONBLOCK);
  if (cnxt->fake_ibv_cnxt.async_fd < 0) {
    RDMACMPTL_FATAL("Failed to create async fd");
  }
  RDMACMPTL_DEBUG("Created this async_fd thing");
  cnxt->fake_ibv_cnxt.device = calloc(1UL,sizeof(struct ibv_device));
  if(NULL == cnxt->fake_ibv_cnxt.device){
    RDMACMPTL_FATAL("No memory");
  }
  // Set device name and other attributes
  strcpy(cnxt->fake_ibv_cnxt.device->name, "portals_device");
  strcpy(cnxt->fake_ibv_cnxt.device->dev_name, "bxi0");
  strcpy(cnxt->fake_ibv_cnxt.device->dev_path, "/dev/portals0");

  RDMACMPTL_DEBUG("Initialization DONE with portals Initialization, encapsulated portals_context inside ibv_context");
  return devices;
}

/* Subset of libverbs that Nida implements so nvmf target can boot*/
int ibv_query_device(struct ibv_context *context,
                     struct ibv_device_attr *device_attr) {
  IBVPTL_DEBUG("Trapped ibv_query_device filling it with reasonable values...");

  // Zero out the structure first
  memset(device_attr, 0, sizeof(struct ibv_device_attr));

  device_attr->vendor_id = 0x02c9;       //Fake Mellanox id 
  device_attr->vendor_part_id = 0x1017;  // Fake Mellanox part id
  device_attr->max_qp = 256;             // Number of QPs you'll support
  device_attr->max_cq = 256;             // Number of CQs
  device_attr->max_mr = 256;             // Number of Memory Regions
  device_attr->max_pd = 256;             // Number of Protection Domains
  device_attr->max_qp_wr = 4096;         // Max Work Requests per QP
  device_attr->max_cqe = 4096;           // Max CQ entries
  device_attr->max_mr_size = UINT64_MAX; // Max size of Memory Region
  device_attr->max_sge = 32;             // Max Scatter/Gather Elements
  device_attr->max_sge_rd = 32;          // Max SGE for RDMA read
  device_attr->max_qp_rd_atom = 16;      // Max outstanding RDMA reads
  device_attr->max_qp_init_rd_atom = 16; // Initial RDMA read resources
  device_attr->max_srq = 256;            // Max Shared Receive Queues
  device_attr->max_srq_wr = 4096;        // Max SRQ work requests
  device_attr->max_srq_sge = 32;         // Max SGE for SRQ

  // Set capabilities flags
  device_attr->device_cap_flags =
      IBV_DEVICE_RESIZE_MAX_WR |  // Support QP/CQ resize
      IBV_DEVICE_BAD_PKEY_CNTR |  // Support bad pkey counter
      IBV_DEVICE_BAD_QKEY_CNTR |  // Support bad qkey counter
      IBV_DEVICE_RAW_MULTI |      // Support raw packet QP
      IBV_DEVICE_AUTO_PATH_MIG |  // Support auto path migration
      IBV_DEVICE_CHANGE_PHY_PORT; // Support changing physical port

  IBVPTL_DEBUG("Trapped ibv_query_device filling it with reasonable values...DONE");
  return 0;
}

struct ibv_pd *ibv_alloc_pd(struct ibv_context *context) {
  struct ptl_context *cnxt =
      PTL_CONTAINER_OF(context, struct ptl_context, fake_ibv_cnxt);
  if (MAGIC_NUMBER != cnxt->magic_number) {
    IBVPTL_FATAL("Checked failed not a valid portals context");
  }
  IBVPTL_DEBUG("OK trapped ibv_alloc_pd sending dummy pd portals does not need it");
  return &cnxt->pd;
}
