APP      = xcopy
VPATH    = src
SRCS     = main.c util.c input.c buffer.c fs_dss.c
SDK_DIR  ?= ../../sdcc-sprinter-sdk/
VERSION  ?= 0.1.$(shell date +%Y%m%d)
LOG      ?= 0
RUNTIME_STACK ?= 0xBFFF
MIN_STACK_GAP ?= 0x0800

include $(SDK_DIR)examples/common.mk

APP_CPPFLAGS += -DXCOPY_VERSION='"$(VERSION)"' -DXCOPY_LOG_ENABLE=$(LOG)

.PHONY: check-layout
all: check-layout

check-layout: $(APP).exe
	$(PYTHON) ../run/check_sdcc_layout.py $(BUILD)/$(APP).map $(RUNTIME_STACK) $(MIN_STACK_GAP)
