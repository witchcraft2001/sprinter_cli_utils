APP      = xcopy
VPATH    = src
SRCS     = main.c util.c input.c buffer.c fs_dss.c
SDK_DIR  ?= ../../sdcc-sprinter-sdk/
VERSION  ?= 0.1.$(shell date +%Y%m%d)
LOG      ?= 0

include $(SDK_DIR)examples/common.mk

APP_CPPFLAGS += -DXCOPY_VERSION='"$(VERSION)"' -DXCOPY_LOG_ENABLE=$(LOG)
