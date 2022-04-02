LOCAL_PATH := $(call my-dir)

## mbedtls
include $(CLEAR_VARS)
TOP_DIR := ${LOCAL_PATH}/../..
MBEDTLS_FILES := $(wildcard  ${TOP_DIR}/thirdparty/mbedtls/library/*.c)
LOCAL_SRC_FILES := $(MBEDTLS_FILES:$(LOCAL_PATH)/%=%)
LOCAL_C_INCLUDES += ${TOP_DIR}/thirdparty/mbedtls/include
LOCAL_CFLAGS += -Wall -Werror
LOCAL_CFLAGS += -D_SOCKLEN_T
LOCAL_MODULE := libmbedtls
include $(BUILD_SHARED_LIBRARY)

## sysutils
include $(CLEAR_VARS)
TOP_DIR := ${LOCAL_PATH}/../..
LOCAL_SRC_FILES := \
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_log.c \
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_memory.c \
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_thread.c \
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_time.c \
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/memdbg.c \
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/mlooper.c \
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/mqueue.c \
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/ringbuf.c \
    ${TOP_DIR}/thirdparty/sysutils/source/httpclient/httpclient.c
LOCAL_C_INCLUDES += \
    ${TOP_DIR}/thirdparty/mbedtls/include \
    ${TOP_DIR}/thirdparty/sysutils/include
LOCAL_CFLAGS += -Wall -Werror -DOS_ANDROID
LOCAL_CFLAGS += -DSYSUTILS_HAVE_MBEDTLS_ENABLED
LOCAL_LDLIBS := -llog
LOCAL_SHARED_LIBRARIES += mbedtls
LOCAL_MODULE := libsysutils
include $(BUILD_SHARED_LIBRARY)

## liteplayercore
include $(CLEAR_VARS)
TOP_DIR := ${LOCAL_PATH}/../..
CODEC_FILES := $(wildcard  \
    ${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/src/*.cpp \
    ${TOP_DIR}/thirdparty/codecs/aac-pvaac/*.cpp)
CODEC_FILES := $(CODEC_FILES:$(LOCAL_PATH)/%=%)
LOCAL_SRC_FILES := \
    ${CODEC_FILES} \
    ${TOP_DIR}/library/source/esp_adf/audio_element.c \
    ${TOP_DIR}/library/source/esp_adf/audio_event_iface.c \
    ${TOP_DIR}/library/source/audio_decoder/mp3_pvmp3_wrapper.c \
    ${TOP_DIR}/library/source/audio_decoder/mp3_decoder.c \
    ${TOP_DIR}/library/source/audio_decoder/aac_pvaac_wrapper.c \
    ${TOP_DIR}/library/source/audio_decoder/aac_decoder.c \
    ${TOP_DIR}/library/source/audio_decoder/m4a_decoder.c \
    ${TOP_DIR}/library/source/audio_decoder/wav_decoder.c \
    ${TOP_DIR}/library/source/audio_extractor/mp3_extractor.c \
    ${TOP_DIR}/library/source/audio_extractor/aac_extractor.c \
    ${TOP_DIR}/library/source/audio_extractor/m4a_extractor.c \
    ${TOP_DIR}/library/source/audio_extractor/wav_extractor.c \
    ${TOP_DIR}/library/source/liteplayer_adapter.c \
    ${TOP_DIR}/library/source/liteplayer_source.c \
    ${TOP_DIR}/library/source/liteplayer_parser.c \
    ${TOP_DIR}/library/source/liteplayer_main.c \
    ${TOP_DIR}/library/source/liteplayer_listplayer.c \
    ${TOP_DIR}/library/source/liteplayer_ttsplayer.c
LOCAL_C_INCLUDES += \
    ${TOP_DIR}/library/include \
    ${TOP_DIR}/library/source \
    ${TOP_DIR}/thirdparty/sysutils/include \
    ${TOP_DIR}/thirdparty/codecs \
    ${TOP_DIR}/thirdparty/speexdsp \
    ${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/include \
    ${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/src \
    ${TOP_DIR}/thirdparty/codecs/aac-pvaac
LOCAL_CFLAGS += -Wall -Werror
LOCAL_CFLAGS += -DLITEPLAYER_CONFIG_SINK_FIXED_S16LE -DLITEPLAYER_CONFIG_AAC_SBR
LOCAL_CFLAGS += -Wno-error=narrowing
LOCAL_CFLAGS += -Wno-error=implicit-const-int-float-conversion
LOCAL_CFLAGS += -Wno-error=void-pointer-to-enum-cast
LOCAL_CFLAGS += -DOSCL_IMPORT_REF= -DOSCL_EXPORT_REF= -DOSCL_UNUSED_ARG=\(void\)
LOCAL_CPPFLAGS += -Wall -Werror -std=c++11
LOCAL_LDLIBS := -llog
LOCAL_SHARED_LIBRARIES += sysutils
LOCAL_MODULE := libliteplayer_core
include $(BUILD_SHARED_LIBRARY)

## liteplayeradapter
include $(CLEAR_VARS)
TOP_DIR := ${LOCAL_PATH}/../..
LOCAL_SRC_FILES := \
    ${TOP_DIR}/adapter/source_httpclient_wrapper.c \
    ${TOP_DIR}/adapter/source_file_wrapper.c \
    ${TOP_DIR}/adapter/sink_opensles_wrapper.cpp
LOCAL_C_INCLUDES += \
    ${TOP_DIR}/library/include \
    ${TOP_DIR}/adapter \
    ${TOP_DIR}/thirdparty/sysutils/include
LOCAL_CFLAGS += -Wall -Werror
LOCAL_LDLIBS := -llog -lOpenSLES
LOCAL_SHARED_LIBRARIES += sysutils
LOCAL_MODULE := liteplayer_adapter
include $(BUILD_SHARED_LIBRARY)
