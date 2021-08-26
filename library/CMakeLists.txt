cmake_minimum_required(VERSION 2.8)
project(liteplayer_core)

set(TOP_DIR "${CMAKE_SOURCE_DIR}/..")
set(THIRDPARTY_DIR "${TOP_DIR}/thirdparty")

# cflags: OS_LINUX, OS_ANDROID, OS_APPLE, OS_RTOS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Werror -DOS_APPLE")
# cflag: libmad: FPM_64BIT/FPM_INTEL/FPM_ARM/FPM_MIPS/FPM_DEFAULT
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFPM_64BIT")
# cflag: helix: __arm__/__amd64__, __GNUC__
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__amd64__ -DUSE_DEFAULT_STDLIB")
# cflag: if enable aac sbr
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAAC_ENABLE_SBR")
# cflag: libspeexdsp: FIXED_POINT/FLOATING_POINT, USE_NEON/USE_SSE
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFIXED_POINT")
# cflag: resampler: ENABLE_SRC, CONFIG_SRC_QUALITY, only support S16LE
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_SRC -DCONFIG_SRC_QUALITY=8")
# cflags: support S24LE or S32LE? don't enable it if ENABLE_SRC
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_PCM_S24LE_S32LE_SUPPORT")

# include files
include_directories(${THIRDPARTY_DIR}/sysutils/include)
include_directories(${THIRDPARTY_DIR}/codecs)
include_directories(${TOP_DIR}/library/include)
include_directories(${TOP_DIR}/library/source)
# liteplayer files
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
    ${TOP_DIR}/library/source/audio_resampler/speexdsp/resample.c
    ${TOP_DIR}/library/source/audio_stream/sink_stream.c
    ${TOP_DIR}/library/source/liteplayer_source.c
    ${TOP_DIR}/library/source/liteplayer_parser.c
    ${TOP_DIR}/library/source/liteplayer_debug.c
    ${TOP_DIR}/library/source/liteplayer_main.c
    ${TOP_DIR}/library/source/liteplayer_manager.c
)
# codecs files
set (CODECS_SRC
    ${THIRDPARTY_DIR}/codecs/mp3-mad/bit.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/decoder.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/fixed.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/frame.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/huffman.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/layer12.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/layer3.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/stream.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/synth.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/timer.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/version.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/aacdec.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/aactabs.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/bitstream.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/buffers.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/dct4.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/decelmnt.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/dequant.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/fft.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/filefmt.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/huffman.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/hufftabs.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/imdct.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/noiseless.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/pns.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbr.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrfft.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrfreq.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrhfadj.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrhfgen.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrhuff.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrimdct.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrmath.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrqmf.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrside.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrtabs.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/stproc.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/tns.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/trigtabs.c
)
# sysutils files
set (SYSUTILS_SRC
    ${THIRDPARTY_DIR}/sysutils/osal/unix/os_log.c
    ${THIRDPARTY_DIR}/sysutils/osal/unix/os_memory.c
    ${THIRDPARTY_DIR}/sysutils/osal/unix/os_thread.c
    ${THIRDPARTY_DIR}/sysutils/osal/unix/os_time.c
    ${THIRDPARTY_DIR}/sysutils/source/cutils/memdbg.c
    ${THIRDPARTY_DIR}/sysutils/source/cutils/mlooper.c
    ${THIRDPARTY_DIR}/sysutils/source/cutils/mqueue.c
    ${THIRDPARTY_DIR}/sysutils/source/cutils/ringbuf.c
)

# liteplayer_core lib
add_library(liteplayer_core   SHARED ${CODECS_SRC} ${LITEPLAYER_SRC})
add_library(liteplayer_core_s STATIC ${CODECS_SRC} ${LITEPLAYER_SRC})
target_link_libraries(liteplayer_core sysutils)

# sysutils lib
add_library(sysutils   SHARED ${SYSUTILS_SRC})
add_library(sysutils_s STATIC ${SYSUTILS_SRC})
