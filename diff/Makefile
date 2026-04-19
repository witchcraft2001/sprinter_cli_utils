APP      = diff
VPATH    = src
SRCS     = main.c util.c reader.c diff_core.c
SDK_DIR  ?= ../../sdcc-sprinter-sdk/
VERSION  ?= 0.1.$(shell date +%Y%m%d)
LOG      ?= 0

include $(SDK_DIR)examples/common.mk

SDCC_FLAGS += -DDIFF_VERSION='"$(VERSION)"' -DDIFF_LOG_ENABLE=$(LOG)
