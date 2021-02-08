LOCAL_PATH := $(call my-dir)

## sysutils
include $(CLEAR_VARS)
TOP_DIR := ${LOCAL_PATH}/../../..
LOCAL_SRC_FILES := \
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/memory_debug.c \
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/msglooper.c \
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/msgqueue.c \
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/ringbuf.c \
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/sw_timer.c \
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/sw_watchdog.c \
    ${TOP_DIR}/thirdparty/sysutils/osal/os_logger.c \
    ${TOP_DIR}/thirdparty/sysutils/osal/os_thread.c \
    ${TOP_DIR}/thirdparty/sysutils/osal/os_time.c \
    ${TOP_DIR}/thirdparty/sysutils/osal/os_timer.c \
    ${TOP_DIR}/thirdparty/sysutils/source/utils/Looper.cpp \
    ${TOP_DIR}/thirdparty/sysutils/source/utils/Thread.cpp \
    ${TOP_DIR}/thirdparty/sysutils/source/utils/cJSON.cpp \
    ${TOP_DIR}/thirdparty/sysutils/source/utils/JsonWrapper.cpp \
    ${TOP_DIR}/thirdparty/sysutils/source/utils/RefBase.cpp \
    ${TOP_DIR}/thirdparty/sysutils/source/utils/SharedBuffer.cpp \
    ${TOP_DIR}/thirdparty/sysutils/source/utils/safe_iop.c \
    ${TOP_DIR}/thirdparty/sysutils/source/utils/VectorImpl.cpp \
    ${TOP_DIR}/thirdparty/sysutils/source/utils/Unicode.cpp \
    ${TOP_DIR}/thirdparty/sysutils/source/utils/Static.cpp \
    ${TOP_DIR}/thirdparty/sysutils/source/utils/String8.cpp \
    ${TOP_DIR}/thirdparty/sysutils/source/utils/String16.cpp \
    ${TOP_DIR}/thirdparty/sysutils/source/utils/StringUtils.cpp
LOCAL_C_INCLUDES += ${TOP_DIR}/thirdparty/sysutils/include
LOCAL_CFLAGS += -Wall -Werror -DOS_ANDROID
LOCAL_CPPFLAGS += -Wall -Werror -DOS_ANDROID -std=gnu++11
LOCAL_LDLIBS := -llog
LOCAL_MODULE := libsysutils
include $(BUILD_SHARED_LIBRARY)

## liteplayercore
include $(CLEAR_VARS)
TOP_DIR := ${LOCAL_PATH}/../../..
THIRDPARTY_FILES := $(wildcard  \
    ${TOP_DIR}/thirdparty/codec/mp3-pvmp3/src/*.cpp \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libAACdec/src/*.cpp \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libPCMutils/src/*.cpp \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libFDK/src/*.cpp \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libSYS/src/*.cpp \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libMpegTPDec/src/*.cpp \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libSBRdec/src/*.cpp \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libArithCoding/src/*.cpp \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libDRCdec/src/*.cpp \
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libSACdec/src/*.cpp \
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
    ${TOP_DIR}/library/source/liteplayer_manager.c
LOCAL_C_INCLUDES += \
    ${TOP_DIR}/library/include \
    ${TOP_DIR}/library/source \
    ${TOP_DIR}/thirdparty/sysutils/include \
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
LOCAL_CFLAGS += -DOS_ANDROID -DAAC_ENABLE_SBR -DFIXED_POINT
LOCAL_CFLAGS += -Wall -Werror -Wno-error=unused-function -Wno-error=unused-variable
LOCAL_LDLIBS := -llog
LOCAL_SHARED_LIBRARIES += sysutils
LOCAL_MODULE := libliteplayer_core
include $(BUILD_SHARED_LIBRARY)

## liteplayeradapter
include $(CLEAR_VARS)
TOP_DIR := ${LOCAL_PATH}/../../..
THIRDPARTY_FILES := $(wildcard  ${TOP_DIR}/thirdparty/mbedtls/library/*.c \
                                ${TOP_DIR}/thirdparty/httpclient/*.c)
THIRDPARTY_FILES := $(THIRDPARTY_FILES:$(LOCAL_PATH)/%=%)
LOCAL_SRC_FILES := \
    ${THIRDPARTY_FILES} \
    ${TOP_DIR}/adapter/httpclient_wrapper.c \
    ${TOP_DIR}/adapter/fatfs_wrapper.c \
    ${TOP_DIR}/adapter/opensles_wrapper.cpp
LOCAL_C_INCLUDES += \
    ${TOP_DIR}/library/include \
    ${TOP_DIR}/adapter \
    ${TOP_DIR}/thirdparty/sysutils/include \
    ${TOP_DIR}/thirdparty/httpclient \
    ${TOP_DIR}/thirdparty/mbedtls/include
LOCAL_CFLAGS += -DOS_ANDROID -D_SOCKLEN_T -Wno-error=inline-asm
LOCAL_CFLAGS += -Wall -Werror
LOCAL_LDLIBS := -llog -lOpenSLES
LOCAL_SHARED_LIBRARIES += sysutils
LOCAL_MODULE := liteplayer_adapter
include $(BUILD_SHARED_LIBRARY)
