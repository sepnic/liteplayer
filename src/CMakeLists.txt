cmake_minimum_required(VERSION 2.8)
project(liteplayer_core)

set(TOP_DIR "${CMAKE_SOURCE_DIR}/..")

# cflags: OS_LINUX, OS_ANDROID, OS_APPLE, OS_RTOS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -fPIC -std=gnu99 -Wall -Werror -DOS_APPLE")

# include files
include_directories(${TOP_DIR}/thirdparty/sysutils/include)
include_directories(${TOP_DIR}/include)

# liteplayer files
set(CODECS_SRC
    ${TOP_DIR}/thirdparty/codecs/mp3-mad/bit.c
    ${TOP_DIR}/thirdparty/codecs/mp3-mad/decoder.c
    ${TOP_DIR}/thirdparty/codecs/mp3-mad/fixed.c
    ${TOP_DIR}/thirdparty/codecs/mp3-mad/frame.c
    ${TOP_DIR}/thirdparty/codecs/mp3-mad/huffman.c
    ${TOP_DIR}/thirdparty/codecs/mp3-mad/layer12.c
    ${TOP_DIR}/thirdparty/codecs/mp3-mad/layer3.c
    ${TOP_DIR}/thirdparty/codecs/mp3-mad/stream.c
    ${TOP_DIR}/thirdparty/codecs/mp3-mad/synth.c
    ${TOP_DIR}/thirdparty/codecs/mp3-mad/timer.c
    ${TOP_DIR}/thirdparty/codecs/mp3-mad/version.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/aacdec.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/aactabs.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/bitstream.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/buffers.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/dct4.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/decelmnt.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/dequant.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/fft.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/filefmt.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/huffman.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/hufftabs.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/imdct.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/noiseless.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/pns.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/sbr.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/sbrfft.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/sbrfreq.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/sbrhfadj.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/sbrhfgen.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/sbrhuff.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/sbrimdct.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/sbrmath.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/sbrqmf.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/sbrside.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/sbrtabs.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/stproc.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/tns.c
    ${TOP_DIR}/thirdparty/codecs/aac-helix/trigtabs.c)
set(LITEPLAYER_SRC
    ${CODECS_SRC}
    ${TOP_DIR}/src/esp_adf/audio_element.c
    ${TOP_DIR}/src/esp_adf/audio_event_iface.c
    ${TOP_DIR}/src/audio_decoder/mp3_mad_wrapper.c
    ${TOP_DIR}/src/audio_decoder/mp3_decoder.c
    ${TOP_DIR}/src/audio_decoder/aac_helix_wrapper.c
    ${TOP_DIR}/src/audio_decoder/aac_decoder.c
    ${TOP_DIR}/src/audio_decoder/m4a_decoder.c
    ${TOP_DIR}/src/audio_decoder/wav_decoder.c
    ${TOP_DIR}/src/audio_extractor/mp3_extractor.c
    ${TOP_DIR}/src/audio_extractor/aac_extractor.c
    ${TOP_DIR}/src/audio_extractor/m4a_extractor.c
    ${TOP_DIR}/src/audio_extractor/wav_extractor.c
    ${TOP_DIR}/src/liteplayer_adapter.c
    ${TOP_DIR}/src/liteplayer_source.c
    ${TOP_DIR}/src/liteplayer_parser.c
    ${TOP_DIR}/src/liteplayer_debug.c
    ${TOP_DIR}/src/liteplayer_main.c
    ${TOP_DIR}/src/liteplayer_listplayer.c
    ${TOP_DIR}/src/liteplayer_ttsplayer.c)
add_library(liteplayer_core STATIC ${LITEPLAYER_SRC})
target_compile_options(liteplayer_core PRIVATE
    -Wno-error=int-to-pointer-cast
    #platform: __arm__/__amd64__, __GNUC__
    -D__amd64__
    #libmad: LITEPLAYER_CONFIG_MAD_FPM_64BIT/LITEPLAYER_CONFIG_MAD_FPM_ARM/LITEPLAYER_CONFIG_MAD_FPM_DEFAULT
    -DLITEPLAYER_CONFIG_MAD_FPM_DEFAULT
    #helix-aac: don't config it if sbr isn't needed for rtos devices
    -DLITEPLAYER_CONFIG_AAC_SBR
    #sink: don't config it if sink support S24LE/S32LE
    -DLITEPLAYER_CONFIG_SINK_FIXED_S16LE)
target_include_directories(liteplayer_core PRIVATE ${TOP_DIR}/thirdparty/codecs ${TOP_DIR}/src)

# sysutils files
set(SYSUTILS_SRC
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_log.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_memory.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_thread.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_time.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/memdbg.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/mlooper.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/mqueue.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/ringbuf.c)
add_library(sysutils STATIC ${SYSUTILS_SRC})
