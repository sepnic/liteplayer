LOCAL_PATH := $(call my-dir)

## sysutils
include $(CLEAR_VARS)
TOP_DIR := ${LOCAL_PATH}/../../..
LOCAL_SRC_FILES := \
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_log.c \
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_memory.c \
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_thread.c \
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_time.c \
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/memdbg.c \
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/mlooper.c \
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/mqueue.c \
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/ringbuf.c
LOCAL_C_INCLUDES += ${TOP_DIR}/thirdparty/sysutils/include
LOCAL_CFLAGS += -Wall -Werror -DOS_ANDROID
LOCAL_CPPFLAGS += -Wall -Werror -DOS_ANDROID -std=c++11
LOCAL_LDLIBS := -llog
LOCAL_MODULE := libsysutils
include $(BUILD_SHARED_LIBRARY)

## liteplayercore
include $(CLEAR_VARS)
TOP_DIR := ${LOCAL_PATH}/../../..
THIRDPARTY_FILES := $(wildcard  \
    ${TOP_DIR}/thirdparty/speexdsp/resample.c \
    ${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/src/*.cpp \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libAACdec/src/*.cpp \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libPCMutils/src/*.cpp \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libFDK/src/*.cpp \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libSYS/src/*.cpp \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libMpegTPDec/src/*.cpp \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libSBRdec/src/*.cpp \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libArithCoding/src/*.cpp \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libDRCdec/src/*.cpp \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libSACdec/src/*.cpp)
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
    ${TOP_DIR}/library/source/audio_stream/sink_stream.c \
    ${TOP_DIR}/library/source/liteplayer_source.c \
    ${TOP_DIR}/library/source/liteplayer_parser.c \
    ${TOP_DIR}/library/source/liteplayer_debug.c \
    ${TOP_DIR}/library/source/liteplayer_main.c \
    ${TOP_DIR}/library/source/liteplayer_manager.c \
    ${TOP_DIR}/library/source/liteplayer_resampler.c
LOCAL_C_INCLUDES += \
    ${TOP_DIR}/library/include \
    ${TOP_DIR}/library/source \
    ${TOP_DIR}/thirdparty/sysutils/include \
    ${TOP_DIR}/thirdparty/codecs \
    ${TOP_DIR}/thirdparty/speexdsp \
    ${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/include \
    ${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/src \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libAACdec/include \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libPCMutils/include \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libFDK/include \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libSYS/include \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libMpegTPDec/include \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libSBRdec/include \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libArithCoding/include \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libDRCdec/include \
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libSACdec/include
LOCAL_CFLAGS += -DOS_ANDROID
LOCAL_CFLAGS += -DCONFIG_SPEEXDSP_FIXED_POINT -DENABLE_DECODER_AAC_SBR -DCONFIG_SINK_FIXED_S16LE
LOCAL_CFLAGS += -Wall -Werror -Wno-error=unused-function -Wno-error=unused-variable
LOCAL_LDLIBS := -llog
LOCAL_SHARED_LIBRARIES += sysutils
LOCAL_MODULE := libliteplayer_core
include $(BUILD_SHARED_LIBRARY)

## liteplayeradapter
include $(CLEAR_VARS)
TOP_DIR := ${LOCAL_PATH}/../../..
THIRDPARTY_FILES := $(wildcard  ${TOP_DIR}/adapter/external/mbedtls/library/*.c \
                                ${TOP_DIR}/thirdparty/sysutils/source/httpclient/*.c)
THIRDPARTY_FILES := $(THIRDPARTY_FILES:$(LOCAL_PATH)/%=%)
LOCAL_SRC_FILES := \
    ${THIRDPARTY_FILES} \
    ${TOP_DIR}/adapter/source_httpclient_wrapper.c \
    ${TOP_DIR}/adapter/source_fatfs_wrapper.c \
    ${TOP_DIR}/adapter/sink_opensles_wrapper.cpp
LOCAL_C_INCLUDES += \
    ${TOP_DIR}/library/include \
    ${TOP_DIR}/adapter \
    ${TOP_DIR}/thirdparty/sysutils/include \
    ${TOP_DIR}/adapter/external/mbedtls/include
LOCAL_CFLAGS += -DOS_ANDROID -D_SOCKLEN_T -DENABLE_HTTPCLIENT_MBEDTLS
LOCAL_CFLAGS += -Wall -Werror -Wno-error=inline-asm
LOCAL_LDLIBS := -llog -lOpenSLES
LOCAL_SHARED_LIBRARIES += sysutils
LOCAL_MODULE := liteplayer_adapter
include $(BUILD_SHARED_LIBRARY)
