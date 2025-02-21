#ifndef SPDK_PTL_MACROS_H
#define SPDK_PTL_MACROS_H

#define RDMA_CM_LOCK_INIT(x) do { \
    if (pthread_mutex_init(x,NULL)) { \
        SPDK_PTL_FATAL("Failed to init mutex"); \
    } \
} while(0)

#define RDMA_CM_LOCK(x) do { \
    if (pthread_mutex_lock(x)) { \
        SPDK_PTL_FATAL("Failed to lock mutex"); \
    } \
} while(0)

#define RDMA_CM_UNLOCK(x) do { \
    if (pthread_mutex_unlock(x)) { \
        SPDK_PTL_FATAL("Failed to unlock mutex"); \
    } \
} while(0)


#endif
