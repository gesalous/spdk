SPDK_ROOT_DIR := $(abspath $(CURDIR)/../../..)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
include $(SPDK_ROOT_DIR)/mk/spdk.modules.mk

APP = simple_read

C_SRCS := simple_read_test.c

SPDK_LIB_LIST += nvme vmd

ifeq ($(OS),Linux)
SYS_LIBS += -laio
endif

include $(SPDK_ROOT_DIR)/mk/spdk.app.mk

install: $(APP)
	$(INSTALL_APP)

uninstall:
	$(UNINSTALL_APP)
