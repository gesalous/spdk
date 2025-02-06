#  SPDX-License-Identifier: BSD-3-Clause
#  Copyright (C) 2020 Intel Corporation. All rights reserved.
#  Copyright (c) Mellanox Technologies LTD. All rights reserved.
#  Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#

SPDK_ROOT_DIR := $(abspath $(CURDIR)/../..)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk

SO_VER := 7
SO_MINOR := 0

SPDK_MAP_FILE = $(abspath $(CURDIR)/spdk_rdma_provider.map)

LIBNAME = rdma_provider

# Wrapper library settings
WRAPPER_LIBNAME = rdmawrap
WRAPPER_SRCS = rdma_cm_portals.c
WRAPPER_SO = lib$(WRAPPER_LIBNAME).so

ifeq ($(CONFIG_RDMA_PROV),verbs)
C_SRCS = common.c
C_SRCS += rdma_provider_verbs.c
else ifeq ($(CONFIG_RDMA_PROV),mlx5_dv)
C_SRCS = common.c
C_SRCS += rdma_provider_mlx5_dv.c
LOCAL_SYS_LIBS += -lmlx5
else ifeq ($(CONFIG_RDMA_PROV),portals)
C_SRCS = rdma_provider_portals.c
# Build wrapper library for development
EXTRA_TARGETS = $(WRAPPER_SO)
# Remove verbs and rdmacm for portals
LOCAL_SYS_LIBS = -lportals4
else
$(error Wrong RDMA provider specified: $(CONFIG_RDMA_PROV))
endif

ifneq ($(CONFIG_RDMA_PROV),portals)
LOCAL_SYS_LIBS += -libverbs -lrdmacm
endif

#Attach only if FreeBSD and RDMA is specified with configure
ifeq ($(OS),FreeBSD)
# Mellanox - MLX4 HBA Userspace Library
ifneq ("$(wildcard /usr/lib/libmlx4.*)","")
LOCAL_SYS_LIBS += -lmlx4
endif
# Mellanox - MLX5 HBA Userspace Library
ifneq ("$(wildcard /usr/lib/libmlx5.*)","")
LOCAL_SYS_LIBS += -lmlx5
endif
# Chelsio HBA Userspace Library
ifneq ("$(wildcard /usr/lib/libcxgb4.*)","")
LOCAL_SYS_LIBS += -lcxgb4
endif
endif

# Wrapper library build rules
$(WRAPPER_SO): $(WRAPPER_SRCS)
	$(CC) -shared -o $@ $^ -fPIC -ldl

.PHONY: wrapper_clean
wrapper_clean:
	$(RM) $(WRAPPER_SO)

clean: wrapper_clean

# Add wrapper library to all target if we're using portals
ifeq ($(CONFIG_RDMA_PROV),portals)
all: $(EXTRA_TARGETS)
endif

# Explicit target for building the wrapper library
.PHONY: wrapper
wrapper: $(WRAPPER_SO)

include $(SPDK_ROOT_DIR)/mk/spdk.lib.mk

