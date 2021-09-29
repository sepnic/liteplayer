#
# Component Makefile
#
# This Makefile should, at the very least, just include $(SDK_PATH)/Makefile. By default,
# this will take the sources in the src/ directory, compile them and link them into
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the SDK documents if you need to do this.
#

TOP_DIR := ../../../..
LITEPLAYER_DIR := ${TOP_DIR}/library
THIRDPARTY_DIR := ${TOP_DIR}/thirdparty
ADAPTER_DIR := ${TOP_DIR}/adapter

COMPONENT_ADD_INCLUDEDIRS := \
    ${ADAPTER_DIR} \
    .
COMPONENT_PRIV_INCLUDEDIRS := \
    ${THIRDPARTY_DIR}/sysutils/include \
    ${LITEPLAYER_DIR}/include

COMPONENT_SRCDIRS := \
    ${ADAPTER_DIR} \
    .

COMPONENT_OBJS := \
    ${ADAPTER_DIR}/source_httpclient_wrapper.o \
    sink_esp32_i2s_wrapper.o

#CFLAGS += -DOS_RTOS -DOS_FREERTOS_ESP8266
