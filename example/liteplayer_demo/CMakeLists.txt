cmake_minimum_required(VERSION 2.8)

# Support tinalsa?
#set (ENABLE_TINYALSA "YES")

set(TOP_DIR "${CMAKE_SOURCE_DIR}/../..")

# include files
include_directories(${TOP_DIR}/thirdparty/msgutils/include)
include_directories(${TOP_DIR}/thirdparty/httpclient)
include_directories(${TOP_DIR}/thirdparty/mbedtls/include)
include_directories(${TOP_DIR}/thirdparty/codec)
include_directories(${TOP_DIR}/thirdparty/speexdsp)

# include files
include_directories(${TOP_DIR}/library/core/include)
include_directories(${TOP_DIR}/library/components/include)
include_directories(${TOP_DIR}/player)
include_directories(${TOP_DIR}/adapter)

# include files
include_directories(${TOP_DIR}/thirdparty/codec/mp3-pvmp3/include)
include_directories(${TOP_DIR}/thirdparty/codec/mp3-pvmp3/src)
include_directories(${TOP_DIR}/thirdparty/codec/aac-fdk/libAACdec/include)
include_directories(${TOP_DIR}/thirdparty/codec/aac-fdk/libPCMutils/include)
include_directories(${TOP_DIR}/thirdparty/codec/aac-fdk/libFDK/include)
include_directories(${TOP_DIR}/thirdparty/codec/aac-fdk/libSYS/include)
include_directories(${TOP_DIR}/thirdparty/codec/aac-fdk/libMpegTPDec/include)
include_directories(${TOP_DIR}/thirdparty/codec/aac-fdk/libSBRdec/include)
include_directories(${TOP_DIR}/thirdparty/codec/aac-fdk/libArithCoding/include)
include_directories(${TOP_DIR}/thirdparty/codec/aac-fdk/libDRCdec/include)
include_directories(${TOP_DIR}/thirdparty/codec/aac-fdk/libSACdec/include)

# source files
file(GLOB THIRDPARTY_SRC src
    ${TOP_DIR}/thirdparty/msgutils/source/*.c
    ${TOP_DIR}/thirdparty/msgutils/port/*.c
    ${TOP_DIR}/thirdparty/mbedtls/library/*.c
    ${TOP_DIR}/thirdparty/httpclient/*.c
    ${TOP_DIR}/thirdparty/codec/mp3-pvmp3/src/*.cpp
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libAACdec/src/*.cpp
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libPCMutils/src/*.cpp
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libFDK/src/*.cpp
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libSYS/src/*.cpp
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libMpegTPDec/src/*.cpp
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libSBRdec/src/*.cpp
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libArithCoding/src/*.cpp
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libDRCdec/src/*.cpp
    ${TOP_DIR}/thirdparty/codec/aac-fdk/libSACdec/src/*.cpp
)
list(APPEND THIRDPARTY_SRC
    ${TOP_DIR}/thirdparty/speexdsp/resample.c
)
set (ADAPTER_SRC
    ${TOP_DIR}/adapter/httpclient_wrapper.c
    ${TOP_DIR}/adapter/fatfs_wrapper.c
    ${TOP_DIR}/adapter/wave_wrapper.c
)

if (ENABLE_TINYALSA)
    # Force SRC to 48KHz/Stereo for tinyalsa device
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_TINYALSA")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_SRC")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCONFIG_SRC_QUALITY=8")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCONFIG_SRC_OUT_RATE=48000")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCONFIG_SRC_OUT_CHANNELS=2")

    include_directories(${TOP_DIR}/thirdparty/tinyalsa/include)
    list(APPEND THIRDPARTY_SRC
        ${TOP_DIR}/thirdparty/tinyalsa/pcm.c
        ${TOP_DIR}/thirdparty/tinyalsa/mixer.c
    )
    list(APPEND ADAPTER_SRC
        ${TOP_DIR}/adapter/tinyalsa_wrapper.c
    )
endif()

# source files
set (LITEPLAYER_SRC
    ${TOP_DIR}/library/core/audio_element.c
    ${TOP_DIR}/library/core/audio_event_iface.c
    ${TOP_DIR}/library/core/audio_pipeline.c
    ${TOP_DIR}/library/components/audio_decoder/mp3_pvmp3_wrapper.c
    ${TOP_DIR}/library/components/audio_decoder/mp3_decoder.c
    ${TOP_DIR}/library/components/audio_decoder/aac_fdk_wrapper.c
    ${TOP_DIR}/library/components/audio_decoder/aac_decoder.c
    ${TOP_DIR}/library/components/audio_decoder/m4a_decoder.c
    ${TOP_DIR}/library/components/audio_decoder/wav_decoder.c
    ${TOP_DIR}/library/components/audio_extractor/mp3_extractor.c
    ${TOP_DIR}/library/components/audio_extractor/aac_extractor.c
    ${TOP_DIR}/library/components/audio_extractor/m4a_extractor.c
    ${TOP_DIR}/library/components/audio_extractor/wav_extractor.c
    ${TOP_DIR}/library/components/audio_resampler/audio_resampler.c
    ${TOP_DIR}/library/components/audio_stream/file_stream.c
    ${TOP_DIR}/library/components/audio_stream/sink_stream.c
    ${TOP_DIR}/library/components/audio_stream/http_stream.c
    ${TOP_DIR}/player/liteplayer_source.c
    ${TOP_DIR}/player/liteplayer_parser.c
    ${TOP_DIR}/player/liteplayer_debug.c
    ${TOP_DIR}/player/liteplayer_main.c
    ${TOP_DIR}/player/liteplayer_manager.c
)

# cflags: OS_LINUX, OS_ANDROID, OS_MACOSX, OS_IOS, OS_FREERTOS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOS_MACOSX")

# ENABLE_MEMORY_LEAK_DETECT
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_MEMORY_LEAK_DETECT")

# ENABLE_MEMORY_OVERFLOW_DETECT
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_MEMORY_OVERFLOW_DETECT")

# cflag: if enable aac sbr
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAAC_ENABLE_SBR")

# cflag: libspeexdsp: FIXED_POINT/FLOATING_POINT, USE_NEON/USE_SSE
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFLOATING_POINT -DUSE_SSE")

# cflag: resampler: ENABLE_SRC, CONFIG_SRC_QUALITY

# cflags: compile paramters
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -g -Wall -Werror -Wno-error=unused-function")

# ldflags
set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS} -Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed")

#message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")

# liteplayer_demo
add_executable(liteplayer_demo liteplayer_demo.c ${THIRDPARTY_SRC} ${LITEPLAYER_SRC} ${ADAPTER_SRC})
target_link_libraries(liteplayer_demo pthread m)
