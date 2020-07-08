LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

TOP_DIR := ${LOCAL_PATH}/../../..

THIRDPARTY_FILES := $(wildcard  ${TOP_DIR}/thirdparty/codec/mp3-pvmp3/src/*.cpp \
                                ${TOP_DIR}/thirdparty/codec/aac-fdk/libAACdec/src/*.cpp \
                                ${TOP_DIR}/thirdparty/codec/aac-fdk/libPCMutils/src/*.cpp \
                                ${TOP_DIR}/thirdparty/codec/aac-fdk/libFDK/src/*.cpp \
                                ${TOP_DIR}/thirdparty/codec/aac-fdk/libSYS/src/*.cpp \
                                ${TOP_DIR}/thirdparty/codec/aac-fdk/libMpegTPDec/src/*.cpp \
                                ${TOP_DIR}/thirdparty/codec/aac-fdk/libSBRdec/src/*.cpp \
                                ${TOP_DIR}/thirdparty/codec/aac-fdk/libArithCoding/src/*.cpp \
                                ${TOP_DIR}/thirdparty/codec/aac-fdk/libDRCdec/src/*.cpp \
                                ${TOP_DIR}/thirdparty/codec/aac-fdk/libSACdec/src/*.cpp \
                                ${TOP_DIR}/thirdparty/mbedtls/library/*.c \
                                ${TOP_DIR}/thirdparty/httpclient/httpclient.c \
                                ${TOP_DIR}/thirdparty/speexdsp/resample.c)
THIRDPARTY_FILES := $(THIRDPARTY_FILES:$(LOCAL_PATH)/%=%)

LOCAL_SRC_FILES := \
    ${THIRDPARTY_FILES} \
    ${TOP_DIR}/library/source/esp_adf/audio_element.c \
    ${TOP_DIR}/library/source/esp_adf/audio_event_iface.c \
    ${TOP_DIR}/library/source/esp_adf/audio_pipeline.c \
    ${TOP_DIR}/library/source/audio_decoder/mp3_pvmp3_wrapper.c \
    ${TOP_DIR}/library/source/audio_decoder/mp3_decoder.c \
    ${TOP_DIR}/library/source/audio_decoder/aac_fdk_wrapper.c \
    ${TOP_DIR}/library/source/audio_decoder/aac_decoder.c \
    ${TOP_DIR}/library/source/audio_decoder/m4a_decoder.c \
    ${TOP_DIR}/library/source/audio_decoder/wav_decoder.c \
    ${TOP_DIR}/library/source/audio_extractor/mp3_extractor.c \
    ${TOP_DIR}/library/source/audio_extractor/aac_extractor.c \
    ${TOP_DIR}/library/source/audio_extractor/m4a_extractor.c \
    ${TOP_DIR}/library/source/audio_extractor/wav_extractor.c \
    ${TOP_DIR}/library/source/audio_resampler/audio_resampler.c \
    ${TOP_DIR}/library/source/audio_stream/sink_stream.c \
    ${TOP_DIR}/library/source/liteplayer_source.c \
    ${TOP_DIR}/library/source/liteplayer_parser.c \
    ${TOP_DIR}/library/source/liteplayer_debug.c \
    ${TOP_DIR}/library/source/liteplayer_main.c \
    ${TOP_DIR}/library/source/liteplayer_manager.c \
    ${TOP_DIR}/adapter/httpclient_wrapper.c \
    ${TOP_DIR}/adapter/fatfs_wrapper.c \
    ${TOP_DIR}/adapter/opensles_wrapper.c

LOCAL_C_INCLUDES += \
    ${TOP_DIR}/library/include \
    ${TOP_DIR}/library/source \
    ${TOP_DIR}/adapter \
    ${TOP_DIR}/thirdparty/msgutils/include \
    ${TOP_DIR}/thirdparty/httpclient \
    ${TOP_DIR}/thirdparty/mbedtls/include \
    ${TOP_DIR}/thirdparty/codec \
    ${TOP_DIR}/thirdparty/speexdsp \
    ${TOP_DIR}/thirdparty/codec/mp3-pvmp3/include \
    ${TOP_DIR}/thirdparty/codec/mp3-pvmp3/src \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libAACdec/include \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libPCMutils/include \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libFDK/include \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libSYS/include \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libMpegTPDec/include \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libSBRdec/include \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libArithCoding/include \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libDRCdec/include \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libSACdec/include

LOCAL_CFLAGS += -Wall -Werror -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=inline-asm
LOCAL_CFLAGS += -DAAC_ENABLE_SBR -DFIXED_POINT -D_SOCKLEN_T

LOCAL_SHARED_LIBRARIES += msgutils

LOCAL_MODULE := libliteplayer
include $(BUILD_SHARED_LIBRARY)

include $(LOCAL_PATH)/prebuild/Android.mk
