#  SPDX-License-Identifier: BSD-3-Clause
#  Copyright (C) 2015 Intel Corporation.
#  All rights reserved.
#

SPDK_ROOT_DIR := $(abspath $(CURDIR)/../../../..)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk

DIRS-y = base64.c bit_array.c cpuset.c crc16.c crc32_ieee.c crc32c.c crc64.c dif.c \
	 file.c iov.c math.c net.c pipe.c string.c xor.c

ifeq ($(OS), Linux)
DIRS-y += fd_group.c
endif

.PHONY: all clean $(DIRS-y)

all: $(DIRS-y)
clean: $(DIRS-y)

include $(SPDK_ROOT_DIR)/mk/spdk.subdirs.mk
