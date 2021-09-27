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

COMPONENT_ADD_INCLUDEDIRS := \
    ${LITEPLAYER_DIR}/include

COMPONENT_PRIV_INCLUDEDIRS := \
    ${THIRDPARTY_DIR}/sysutils/include \
    ${THIRDPARTY_DIR}/codecs \
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/include \
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src \
    ${LITEPLAYER_DIR}/source

COMPONENT_SRCDIRS := \
    ${LITEPLAYER_DIR}/source/esp_adf \
    ${LITEPLAYER_DIR}/source/audio_decoder \
    ${LITEPLAYER_DIR}/source/audio_extractor \
    ${LITEPLAYER_DIR}/source \
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src \
    ${THIRDPARTY_DIR}/codecs/aac-helix

COMPONENT_OBJEXCLUDE := \
    ${LITEPLAYER_DIR}/source/esp_adf/audio_pipeline.o \
    ${LITEPLAYER_DIR}/source/audio_decoder/aac_fdk_wrapper.o \
    ${LITEPLAYER_DIR}/source/audio_decoder/mp3_mad_wrapper.o \
    ${LITEPLAYER_DIR}/source/liteplayer_resampler.o

CFLAGS += -DOS_RTOS -DOS_FREERTOS_ESP8266 -DARDUINO -DCONFIG_SINK_FIXED_S16LE
