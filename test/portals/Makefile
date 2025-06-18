SPDK_ROOT_DIR := $(abspath $(CURDIR)/../..)

UNIT_TEST_LINK_ENV = 1
TEST_FILE = spdk_nvmeof_test.c

include $(SPDK_ROOT_DIR)/mk/spdk.unittest.mk

CFLAGS += $(ENV_CFLAGS)
