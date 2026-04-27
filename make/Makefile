APP      = make
VPATH    = src
SRCS     = main.c util.c vars.c parser.c fs_dss.c exec_dss.c build.c
SDK_DIR  ?= ../../sdcc-sprinter-sdk/
CRT0_PAGE2 ?= 1
VERSION  ?= 0.1.$(shell date +%Y%m%d)
LOG      ?= 0
STAGELOG ?= $(LOG)
DIAG     ?= 0
DATA_LOC ?= 0x8400
RUNTIME_STACK ?= 0xBFFF

include $(SDK_DIR)examples/common.mk

APP_CPPFLAGS += -DMAKE_VERSION='"$(VERSION)"' -DMAKE_LOG_ENABLE=$(LOG) -DMAKE_STAGE_LOG_ENABLE=$(STAGELOG) -DMAKE_DIAG_ENABLE=$(DIAG)

.PHONY: check-layout
all: check-layout

check-layout: $(APP).exe
	$(PYTHON) ../run/check_sdcc_layout.py $(BUILD)/$(APP).map $(RUNTIME_STACK)
