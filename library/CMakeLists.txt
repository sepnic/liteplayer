cmake_minimum_required(VERSION 3.4.1)
project(liteplayer_core)

set(TOP_DIR "${CMAKE_SOURCE_DIR}/..")
set(THIRDPARTY_DIR "${TOP_DIR}/thirdparty")

# include files
include_directories(${THIRDPARTY_DIR}/msgutils/include)
include_directories(${THIRDPARTY_DIR}/codec)
include_directories(${THIRDPARTY_DIR}/speexdsp)
include_directories(${TOP_DIR}/library/include)
include_directories(${TOP_DIR}/library/source)

# source files
set (THIRDPARTY_SRC
    ${THIRDPARTY_DIR}/msgutils/source/cutils/memory_debug.c
    ${THIRDPARTY_DIR}/msgutils/source/cutils/msglooper.c
    ${THIRDPARTY_DIR}/msgutils/source/cutils/msgqueue.c
    ${THIRDPARTY_DIR}/msgutils/source/cutils/ringbuf.c
    ${THIRDPARTY_DIR}/msgutils/source/cutils/sw_timer.c
    ${THIRDPARTY_DIR}/msgutils/source/cutils/sw_watchdog.c
    ${THIRDPARTY_DIR}/msgutils/osal/os_logger.c
    ${THIRDPARTY_DIR}/msgutils/osal/os_thread.c
    ${THIRDPARTY_DIR}/msgutils/osal/os_time.c
    ${THIRDPARTY_DIR}/msgutils/osal/os_timer.c
    ${THIRDPARTY_DIR}/codec/mp3-mad/bit.c
    ${THIRDPARTY_DIR}/codec/mp3-mad/decoder.c
    ${THIRDPARTY_DIR}/codec/mp3-mad/fixed.c
    ${THIRDPARTY_DIR}/codec/mp3-mad/frame.c
    ${THIRDPARTY_DIR}/codec/mp3-mad/huffman.c
    ${THIRDPARTY_DIR}/codec/mp3-mad/layer12.c
    ${THIRDPARTY_DIR}/codec/mp3-mad/layer3.c
    ${THIRDPARTY_DIR}/codec/mp3-mad/stream.c
    ${THIRDPARTY_DIR}/codec/mp3-mad/synth.c
    ${THIRDPARTY_DIR}/codec/mp3-mad/timer.c
    ${THIRDPARTY_DIR}/codec/mp3-mad/version.c
    ${THIRDPARTY_DIR}/codec/aac-helix/aacdec.c
    ${THIRDPARTY_DIR}/codec/aac-helix/aactabs.c
    ${THIRDPARTY_DIR}/codec/aac-helix/bitstream.c
    ${THIRDPARTY_DIR}/codec/aac-helix/buffers.c
    ${THIRDPARTY_DIR}/codec/aac-helix/dct4.c
    ${THIRDPARTY_DIR}/codec/aac-helix/decelmnt.c
    ${THIRDPARTY_DIR}/codec/aac-helix/dequant.c
    ${THIRDPARTY_DIR}/codec/aac-helix/fft.c
    ${THIRDPARTY_DIR}/codec/aac-helix/filefmt.c
    ${THIRDPARTY_DIR}/codec/aac-helix/huffman.c
    ${THIRDPARTY_DIR}/codec/aac-helix/hufftabs.c
    ${THIRDPARTY_DIR}/codec/aac-helix/imdct.c
    ${THIRDPARTY_DIR}/codec/aac-helix/noiseless.c
    ${THIRDPARTY_DIR}/codec/aac-helix/pns.c
    ${THIRDPARTY_DIR}/codec/aac-helix/sbr.c
    ${THIRDPARTY_DIR}/codec/aac-helix/sbrfft.c
    ${THIRDPARTY_DIR}/codec/aac-helix/sbrfreq.c
    ${THIRDPARTY_DIR}/codec/aac-helix/sbrhfadj.c
    ${THIRDPARTY_DIR}/codec/aac-helix/sbrhfgen.c
    ${THIRDPARTY_DIR}/codec/aac-helix/sbrhuff.c
    ${THIRDPARTY_DIR}/codec/aac-helix/sbrimdct.c
    ${THIRDPARTY_DIR}/codec/aac-helix/sbrmath.c
    ${THIRDPARTY_DIR}/codec/aac-helix/sbrqmf.c
    ${THIRDPARTY_DIR}/codec/aac-helix/sbrside.c
    ${THIRDPARTY_DIR}/codec/aac-helix/sbrtabs.c
    ${THIRDPARTY_DIR}/codec/aac-helix/stproc.c
    ${THIRDPARTY_DIR}/codec/aac-helix/tns.c
    ${THIRDPARTY_DIR}/codec/aac-helix/trigtabs.c
    ${THIRDPARTY_DIR}/speexdsp/resample.c
)

# source files
set (LITEPLAYER_SRC
    ${TOP_DIR}/library/source/esp_adf/audio_element.c
    ${TOP_DIR}/library/source/esp_adf/audio_event_iface.c
    ${TOP_DIR}/library/source/esp_adf/audio_pipeline.c
    ${TOP_DIR}/library/source/audio_decoder/mp3_mad_wrapper.c
    ${TOP_DIR}/library/source/audio_decoder/mp3_decoder.c
    ${TOP_DIR}/library/source/audio_decoder/aac_helix_wrapper.c
    ${TOP_DIR}/library/source/audio_decoder/aac_decoder.c
    ${TOP_DIR}/library/source/audio_decoder/m4a_decoder.c
    ${TOP_DIR}/library/source/audio_decoder/wav_decoder.c
    ${TOP_DIR}/library/source/audio_extractor/mp3_extractor.c
    ${TOP_DIR}/library/source/audio_extractor/aac_extractor.c
    ${TOP_DIR}/library/source/audio_extractor/m4a_extractor.c
    ${TOP_DIR}/library/source/audio_extractor/wav_extractor.c
    ${TOP_DIR}/library/source/audio_resampler/audio_resampler.c
    ${TOP_DIR}/library/source/audio_stream/sink_stream.c
    ${TOP_DIR}/library/source/liteplayer_source.c
    ${TOP_DIR}/library/source/liteplayer_parser.c
    ${TOP_DIR}/library/source/liteplayer_debug.c
    ${TOP_DIR}/library/source/liteplayer_main.c
    ${TOP_DIR}/library/source/liteplayer_manager.c
)

# cflags: OS_LINUX, OS_ANDROID, OS_MACOSX, OS_IOS, OS_FREERTOS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOS_MACOSX")

# cflag: libmad: FPM_64BIT/FPM_INTEL/FPM_ARM/FPM_MIPS/FPM_DEFAULT
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFPM_64BIT")

# cflag: helix: __arm__/__amd64__, __GNUC__
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__amd64__ -DUSE_DEFAULT_STDLIB")

# cflag: if enable aac sbr
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAAC_ENABLE_SBR")

# cflag: libspeexdsp: FIXED_POINT/FLOATING_POINT, USE_NEON/USE_SSE
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFIXED_POINT")

# cflag: resampler: ENABLE_SRC, CONFIG_SRC_QUALITY
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_SRC -DCONFIG_SRC_QUALITY=8")

# cflags: compile paramters
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Werror")
# cflags: if socketlen_t defined
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_SOCKLEN_T")

#message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")

# liteplayer_core lib
add_library(liteplayer_core   SHARED ${THIRDPARTY_SRC} ${LITEPLAYER_SRC})
add_library(liteplayer_core_s STATIC ${THIRDPARTY_SRC} ${LITEPLAYER_SRC})
