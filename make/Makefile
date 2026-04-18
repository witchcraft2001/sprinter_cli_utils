APP      = make
VPATH    = src
SRCS     = main.c util.c vars.c parser.c fs_dss.c exec_dss.c build.c
SDK_DIR  ?= ../../sdcc-sprinter-sdk/
VERSION  ?= 0.1.$(shell date +%Y%m%d)
LOG      ?= 0
STAGELOG ?= $(LOG)
DIAG     ?= 0

include $(SDK_DIR)examples/common.mk

SDCC_FLAGS += -DMAKE_VERSION='"$(VERSION)"' -DMAKE_LOG_ENABLE=$(LOG) -DMAKE_STAGE_LOG_ENABLE=$(STAGELOG) -DMAKE_DIAG_ENABLE=$(DIAG)
