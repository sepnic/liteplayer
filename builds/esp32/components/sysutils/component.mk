#
# Component Makefile
#
# This Makefile should, at the very least, just include $(SDK_PATH)/Makefile. By default,
# this will take the sources in the src/ directory, compile them and link them into
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the SDK documents if you need to do this.
#

TOP_DIR := ../../../../thirdparty/sysutils

COMPONENT_ADD_INCLUDEDIRS := \
    ${TOP_DIR}/include

COMPONENT_SRCDIRS := \
    ${TOP_DIR}/osal/esp8266 \
    ${TOP_DIR}/source/cutils \
    ${TOP_DIR}/source/cipher \
    ${TOP_DIR}/source/httpclient

CFLAGS += -DOS_RTOS -DOS_FREERTOS_ESP8266 -DENABLE_HTTPCLIENT_MBEDTLS
