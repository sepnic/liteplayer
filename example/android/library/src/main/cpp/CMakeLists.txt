cmake_minimum_required(VERSION 3.4.1)
project(liteplayer_jni)

set(TOP_DIR "${CMAKE_SOURCE_DIR}/../../../../../..")

# include files
include_directories(${TOP_DIR}/library/include)
include_directories(${TOP_DIR}/adapter)

# cflags: compile paramters
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=gnu99 -Wall -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror")

# mbedtls
file(GLOB MBEDTLS_SRC src ${TOP_DIR}/thirdparty/mbedtls/library/*.c)
add_library(mbedtls STATIC ${MBEDTLS_SRC})
target_compile_options(mbedtls PRIVATE -D_SOCKLEN_T)
target_include_directories(mbedtls PRIVATE ${TOP_DIR}/thirdparty/mbedtls/include)

# sysutils
set(SYSUTILS_SRC
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_log.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_memory.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_thread.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_time.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/memdbg.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/mlooper.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/mqueue.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/ringbuf.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/lockfree_ringbuf.c
    ${TOP_DIR}/thirdparty/sysutils/source/httpclient/httpclient.c)
add_library(sysutils STATIC ${SYSUTILS_SRC})
target_compile_options(sysutils PRIVATE -DOS_ANDROID -DSYSUTILS_HAVE_MBEDTLS_ENABLED)
target_include_directories(sysutils PRIVATE
    ${TOP_DIR}/thirdparty/sysutils/include
    ${TOP_DIR}/thirdparty/mbedtls/include)

# liteplayer_core
file(GLOB CODEC_SRC src
    ${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/src/*.cpp
    ${TOP_DIR}/thirdparty/codecs/aac-pvaac/*.cpp)
set(LITEPLAYER_CORE_SRC
    ${CODEC_SRC}
    ${TOP_DIR}/library/source/esp_adf/audio_element.c
    ${TOP_DIR}/library/source/esp_adf/audio_event_iface.c
    ${TOP_DIR}/library/source/audio_decoder/mp3_pvmp3_wrapper.c
    ${TOP_DIR}/library/source/audio_decoder/mp3_decoder.c
    ${TOP_DIR}/library/source/audio_decoder/aac_pvaac_wrapper.c
    ${TOP_DIR}/library/source/audio_decoder/aac_decoder.c
    ${TOP_DIR}/library/source/audio_decoder/m4a_decoder.c
    ${TOP_DIR}/library/source/audio_decoder/wav_decoder.c
    ${TOP_DIR}/library/source/audio_extractor/mp3_extractor.c
    ${TOP_DIR}/library/source/audio_extractor/aac_extractor.c
    ${TOP_DIR}/library/source/audio_extractor/m4a_extractor.c
    ${TOP_DIR}/library/source/audio_extractor/wav_extractor.c
    ${TOP_DIR}/library/source/liteplayer_adapter.c
    ${TOP_DIR}/library/source/liteplayer_source.c
    ${TOP_DIR}/library/source/liteplayer_parser.c
    ${TOP_DIR}/library/source/liteplayer_main.c
    ${TOP_DIR}/library/source/liteplayer_listplayer.c
    ${TOP_DIR}/library/source/liteplayer_ttsplayer.c)
add_library(liteplayer_core STATIC ${LITEPLAYER_CORE_SRC})
target_compile_options(liteplayer_core PRIVATE
    -DLITEPLAYER_CONFIG_SINK_FIXED_S16LE -DLITEPLAYER_CONFIG_AAC_SBR
    -DOSCL_IMPORT_REF= -DOSCL_EXPORT_REF= -DOSCL_UNUSED_ARG=\(void\)
    -Wno-error=unused-value -Wno-error=unused-function -Wno-error=narrowing
    -Wno-error=implicit-const-int-float-conversion -Wno-error=void-pointer-to-enum-cast)
target_include_directories(liteplayer_core PRIVATE
    ${TOP_DIR}/thirdparty/sysutils/include
    ${TOP_DIR}/thirdparty/codecs
    ${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/include
    ${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/src
    ${TOP_DIR}/thirdparty/codecs/aac-pvaac
    ${TOP_DIR}/library/include
    ${TOP_DIR}/library/source
    ${TOP_DIR}/adapter)

# liteplayer_adapter
set(LITEPLAYER_ADAPTER_SRC
    ${TOP_DIR}/adapter/source_httpclient_wrapper.c
    ${TOP_DIR}/adapter/source_file_wrapper.c
    ${TOP_DIR}/adapter/sink_opensles_wrapper.cpp)
add_library(liteplayer_adapter STATIC ${LITEPLAYER_ADAPTER_SRC})
target_include_directories(liteplayer_adapter PRIVATE
    ${TOP_DIR}/thirdparty/sysutils/include ${TOP_DIR}/library/include)

# liteplayer-jni
add_library(liteplayer-jni SHARED liteplayer-jni.cpp)
target_link_libraries(liteplayer-jni
    liteplayer_core liteplayer_adapter sysutils mbedtls OpenSLES android log)
