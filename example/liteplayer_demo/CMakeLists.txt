cmake_minimum_required(VERSION 2.8)

# Support tinalsa?
#set (ENABLE_TINYALSA "YES")

set(TOP_DIR "${CMAKE_SOURCE_DIR}/../..")
set(FDK_DIR "${TOP_DIR}/library/components/audio_decoder/fdk-aac")

# include files
include_directories(${TOP_DIR}/thirdparty/msgutils/include)
include_directories(${TOP_DIR}/thirdparty/httpclient)
include_directories(${TOP_DIR}/thirdparty/mbedtls/include)
include_directories(${TOP_DIR}/thirdparty/tinyalsa/include)

# include files
include_directories(${TOP_DIR}/library/core/include)
include_directories(${TOP_DIR}/library/components/include)
include_directories(${TOP_DIR}/player)
include_directories(${TOP_DIR}/adapter)

# include files
include_directories(${FDK_DIR}/libAACdec/include)
include_directories(${FDK_DIR}/libPCMutils/include)
include_directories(${FDK_DIR}/libFDK/include)
include_directories(${FDK_DIR}/libSYS/include)
include_directories(${FDK_DIR}/libMpegTPDec/include)
include_directories(${FDK_DIR}/libSBRdec/include)
include_directories(${FDK_DIR}/libArithCoding/include)
include_directories(${FDK_DIR}/libDRCdec/include)
include_directories(${FDK_DIR}/libSACdec/include)

# source files
if (ENABLE_TINYALSA)
file(GLOB THIRDPARTY_SRC src
    ${TOP_DIR}/thirdparty/msgutils/source/*.c
    ${TOP_DIR}/thirdparty/msgutils/source/portable/*.c
    ${TOP_DIR}/thirdparty/mbedtls/library/*.c
    ${TOP_DIR}/thirdparty/httpclient/*.c
    ${TOP_DIR}/thirdparty/tinyalsa/pcm.c
    ${TOP_DIR}/thirdparty/tinyalsa/mixer.c
)
set (ADAPTER_SRC
    ${TOP_DIR}/adapter/httpclient_wrapper.c
    ${TOP_DIR}/adapter/fatfs_wrapper.c
    ${TOP_DIR}/adapter/tinyalsa_wrapper.c
)
else()
file(GLOB THIRDPARTY_SRC src
    ${TOP_DIR}/thirdparty/msgutils/source/*.c
    ${TOP_DIR}/thirdparty/msgutils/source/portable/*.c
    ${TOP_DIR}/thirdparty/mbedtls/library/*.c
    ${TOP_DIR}/thirdparty/httpclient/*.c
)
set (ADAPTER_SRC
    ${TOP_DIR}/adapter/httpclient_wrapper.c
    ${TOP_DIR}/adapter/fatfs_wrapper.c
    ${TOP_DIR}/adapter/wave_wrapper.c
)
endif()

# source files
file(GLOB LITEPLAYER_SRC src
    ${TOP_DIR}/library/core/*.c
    ${TOP_DIR}/library/components/audio_decoder/libmad/*.c
    ${FDK_DIR}/libAACdec/src/*.cpp
    ${FDK_DIR}/libPCMutils/src/*.cpp
    ${FDK_DIR}/libFDK/src/*.cpp
    ${FDK_DIR}/libSYS/src/*.cpp
    ${FDK_DIR}/libMpegTPDec/src/*.cpp
    ${FDK_DIR}/libSBRdec/src/*.cpp
    ${FDK_DIR}/libArithCoding/src/*.cpp
    ${FDK_DIR}/libDRCdec/src/*.cpp
    ${FDK_DIR}/libSACdec/src/*.cpp
    ${TOP_DIR}/library/components/audio_decoder/mp3_mad_wrapper.c
    ${TOP_DIR}/library/components/audio_decoder/mp3_decoder.c
    ${TOP_DIR}/library/components/audio_decoder/aac_fdk_wrapper.c
    ${TOP_DIR}/library/components/audio_decoder/aac_decoder.c
    ${TOP_DIR}/library/components/audio_decoder/m4a_decoder.c
    ${TOP_DIR}/library/components/audio_decoder/wav_decoder.c
    ${TOP_DIR}/library/components/audio_extractor/*.c
    ${TOP_DIR}/library/components/audio_resampler/libspeexdsp/resample.c
    ${TOP_DIR}/library/components/audio_resampler/audio_resampler.c
    ${TOP_DIR}/library/components/audio_stream/*.c
    ${TOP_DIR}/player/*.c
)

# cflags: OS_LINUX, OS_ANDROID, OS_MACOSX, OS_IOS, OS_FREERTOS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOS_MACOSX")
if (ENABLE_TINYALSA)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_TINYALSA")
endif()

# ENABLE_MEMORY_LEAK_DETECT
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_MEMORY_LEAK_DETECT")

# ENABLE_MEMORY_OVERFLOW_DETECT
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_MEMORY_OVERFLOW_DETECT")

# cflag: libmad: FPM_64BIT/FPM_INTEL/FPM_ARM/FPM_MIPS/FPM_DEFAULT
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFPM_64BIT")

# cflag: helix: __arm__/__amd64__, __GNUC__
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__amd64__ -DUSE_DEFAULT_STDLIB")

# cflag: libspeexdsp: FIXED_POINT/FLOATING_POINT, USE_NEON/USE_SSE
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFLOATING_POINT -DUSE_SSE")

# cflag: resampler: ENABLE_SRC, CONFIG_SRC_QUALITY
if (ENABLE_TINYALSA)
    # Force SRC to 48KHz/Stereo for tinyalsa device
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_SRC")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCONFIG_SRC_QUALITY=8")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCONFIG_SRC_OUT_RATE=48000")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCONFIG_SRC_OUT_CHANNELS=2")
endif()

# cflags: compile paramters
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -g -Wall -Werror -Wno-error=unused-function")

# ldflags
set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS} -Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed")

#message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")

# liteplayer_demo
add_executable(liteplayer_demo liteplayer_demo.c ${THIRDPARTY_SRC} ${LITEPLAYER_SRC} ${ADAPTER_SRC})
target_link_libraries(liteplayer_demo pthread m)
