cmake_minimum_required(VERSION 2.8)

set(TOP_DIR "${CMAKE_SOURCE_DIR}/../..")
set(MSGUTILS_DIR "${TOP_DIR}/thirdparty/msgutils")
set(LITECORE_DIR "${TOP_DIR}/library")
set(LITEPLAYER_DIR "${TOP_DIR}/player")
set(LITEPLAYER_ADAPTER_DIR "${TOP_DIR}/adapter")

# include files
include_directories(${MSGUTILS_DIR}/include)
include_directories(${LITECORE_DIR}/core/include)
include_directories(${LITECORE_DIR}/components/include)
include_directories(${LITEPLAYER_DIR})
include_directories(${LITEPLAYER_ADAPTER_DIR})

# source files
set (MSGUTILS_SRC
    ${MSGUTILS_DIR}/source/memory_detect.c
    ${MSGUTILS_DIR}/source/msglooper.c
    ${MSGUTILS_DIR}/source/msgqueue.c
    ${MSGUTILS_DIR}/source/ringbuf.c
    ${MSGUTILS_DIR}/source/smartptr.c
    ${MSGUTILS_DIR}/source/stllist.c
    ${MSGUTILS_DIR}/source/sw_timer.c
    ${MSGUTILS_DIR}/source/sw_watchdog.c
    ${MSGUTILS_DIR}/source/portable/os_logger.c
    ${MSGUTILS_DIR}/source/portable/os_thread.c
    ${MSGUTILS_DIR}/source/portable/os_time.c
    ${MSGUTILS_DIR}/source/portable/os_timer.c
)

# source files
set (LITECORE_SRC
    "${LITECORE_DIR}/core/audio_element.c"
    "${LITECORE_DIR}/core/audio_event_iface.c"
    "${LITECORE_DIR}/core/audio_pipeline.c"
    "${LITECORE_DIR}/components/audio_decoder/libmad/bit.c"
    "${LITECORE_DIR}/components/audio_decoder/libmad/decoder.c"
    "${LITECORE_DIR}/components/audio_decoder/libmad/fixed.c"
    "${LITECORE_DIR}/components/audio_decoder/libmad/frame.c"
    "${LITECORE_DIR}/components/audio_decoder/libmad/huffman.c"
    "${LITECORE_DIR}/components/audio_decoder/libmad/layer12.c"
    "${LITECORE_DIR}/components/audio_decoder/libmad/layer3.c"
    "${LITECORE_DIR}/components/audio_decoder/libmad/stream.c"
    "${LITECORE_DIR}/components/audio_decoder/libmad/synth.c"
    "${LITECORE_DIR}/components/audio_decoder/libmad/timer.c"
    "${LITECORE_DIR}/components/audio_decoder/libmad/version.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/bitstream.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/buffers.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/dct32.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/dequant.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/dqchan.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/huffman.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/hufftabs.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/imdct.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/mp3dec.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/mp3tabs.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/polyphase.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/scalfact.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/stproc.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/subband.c"
#    "${LITECORE_DIR}/components/audio_decoder/helixmp3/trigtabs.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/aacdec.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/aactabs.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/bitstream.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/buffers.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/dct4.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/decelmnt.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/dequant.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/fft.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/filefmt.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/huffman.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/hufftabs.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/imdct.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/noiseless.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/pns.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/sbr.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/sbrfft.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/sbrfreq.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/sbrhfadj.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/sbrhfgen.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/sbrhuff.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/sbrimdct.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/sbrmath.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/sbrqmf.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/sbrside.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/sbrtabs.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/stproc.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/tns.c"
    "${LITECORE_DIR}/components/audio_decoder/helixaac/trigtabs.c"
    "${LITECORE_DIR}/components/audio_decoder/mp3_mad_wrapper.c"
#    "${LITECORE_DIR}/components/audio_decoder/mp3_helix_wrapper.c"
    "${LITECORE_DIR}/components/audio_decoder/mp3_decoder.c"
    "${LITECORE_DIR}/components/audio_decoder/aac_helix_wrapper.c"
    "${LITECORE_DIR}/components/audio_decoder/aac_decoder.c"
    "${LITECORE_DIR}/components/audio_decoder/m4a_decoder.c"
    "${LITECORE_DIR}/components/audio_decoder/wav_decoder.c"
    "${LITECORE_DIR}/components/audio_extractor/mp3_extractor.c"
    "${LITECORE_DIR}/components/audio_extractor/aac_extractor.c"
    "${LITECORE_DIR}/components/audio_extractor/m4a_extractor.c"
    "${LITECORE_DIR}/components/audio_extractor/wav_extractor.c"
    "${LITECORE_DIR}/components/audio_resampler/libspeexdsp/resample.c"
    "${LITECORE_DIR}/components/audio_resampler/audio_resampler.c"
    "${LITECORE_DIR}/components/audio_stream/fatfs_stream.c"
    "${LITECORE_DIR}/components/audio_stream/alsa_stream.c"
    "${LITECORE_DIR}/components/audio_stream/http_stream.c"
)

# source files
set (LITEPLAYER_SRC
    ${LITEPLAYER_DIR}/liteplayer_source.c
    ${LITEPLAYER_DIR}/liteplayer_parser.c
    ${LITEPLAYER_DIR}/liteplayer_debug.c
    ${LITEPLAYER_DIR}/liteplayer_main.c
)

# source files
set (MAIN_SRC
    ${LITEPLAYER_ADAPTER_DIR}/alsastub_wrapper.c
    ${LITEPLAYER_ADAPTER_DIR}/httpstub_wrapper.c
    ${LITEPLAYER_ADAPTER_DIR}/fatfs_wrapper.c
    ${CMAKE_SOURCE_DIR}/liteplayer_demo.c
)

# cflags: OS_LINUX, OS_ANDROID, OS_MACOSX, OS_IOS, OS_FREERTOS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOS_MACOSX")

# ENABLE_MEMORY_LEAK_DETECT
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_MEMORY_LEAK_DETECT")

# ENABLE_MEMORY_OVERFLOW_DETECT
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_MEMORY_OVERFLOW_DETECT")

# cflag: libmad: FPM_64BIT/FPM_INTEL/FPM_ARM/FPM_MIPS/FPM_DEFAULT
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFPM_64BIT")

# cflag: helix: __arm__/__amd64__, __GNUC__
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__amd64__ -DUSE_DEFAULT_STDLIB")

# cflag: libspeexdsp: FIXED_POINT/FLOATING_POINT, USE_NEON/USE_SSE
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFIXED_POINT")

# cflag: resampler: ENABLE_SRC, DEFAULT_SRC_QUALITY
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_SRC -DDEFAULT_SRC_QUALITY=8")

# cflags: compile paramters
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -g -Wall -Werror -Wno-error=pointer-to-int-cast -Wno-error=int-to-pointer-cast")

# ldflags
set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS} -Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed")

#message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")

# liteplayer_demo
add_executable(liteplayer_demo ${MAIN_SRC}  ${MSGUTILS_SRC} ${LITECORE_SRC} ${LITEPLAYER_SRC})
target_link_libraries(liteplayer_demo pthread m)
