APP      = deltree
VPATH    = src
SRCS     = main.c util.c input.c fs_dss.c
SDK_DIR  ?= ../../sdcc-sprinter-sdk/
CRT0_PAGE2 ?= 1
VERSION  ?= 0.1.$(shell date +%Y%m%d)
LOG      ?= 0

include $(SDK_DIR)examples/common.mk

SDCC_FLAGS += -DDELTREE_VERSION='"$(VERSION)"' -DDELTREE_LOG_ENABLE=$(LOG)
