cmake_minimum_required(VERSION 2.8)

set(OUT_LIB_DIR "${CMAKE_SOURCE_DIR}/../build/out/lib")

# include files
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/../thirdparty/msgutils/include)
include_directories(${CMAKE_SOURCE_DIR}/core/include)
include_directories(${CMAKE_SOURCE_DIR}/components/include)

# source files
set (LITECORE_SRC
    "${CMAKE_SOURCE_DIR}/core/audio_element.c"
    "${CMAKE_SOURCE_DIR}/core/audio_event_iface.c"
    "${CMAKE_SOURCE_DIR}/core/audio_pipeline.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/libmad/bit.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/libmad/decoder.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/libmad/fixed.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/libmad/frame.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/libmad/huffman.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/libmad/layer12.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/libmad/layer3.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/libmad/stream.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/libmad/synth.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/libmad/timer.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/libmad/version.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/aacdec.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/aactabs.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/bitstream.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/buffers.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/dct4.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/decelmnt.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/dequant.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/fft.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/filefmt.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/huffman.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/hufftabs.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/imdct.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/noiseless.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/pns.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/sbr.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/sbrfft.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/sbrfreq.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/sbrhfadj.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/sbrhfgen.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/sbrhuff.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/sbrimdct.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/sbrmath.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/sbrqmf.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/sbrside.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/sbrtabs.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/stproc.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/tns.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/helixaac/trigtabs.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/mp3_mad_wrapper.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/mp3_decoder.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/aac_helix_wrapper.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/aac_decoder.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/m4a_decoder.c"
    "${CMAKE_SOURCE_DIR}/components/audio_decoder/wav_decoder.c"
    "${CMAKE_SOURCE_DIR}/components/audio_extractor/mp3_extractor.c"
    "${CMAKE_SOURCE_DIR}/components/audio_extractor/aac_extractor.c"
    "${CMAKE_SOURCE_DIR}/components/audio_extractor/m4a_extractor.c"
    "${CMAKE_SOURCE_DIR}/components/audio_extractor/wav_extractor.c"
    "${CMAKE_SOURCE_DIR}/components/audio_resampler/libspeexdsp/resample.c"
    "${CMAKE_SOURCE_DIR}/components/audio_resampler/audio_resampler.c"
    "${CMAKE_SOURCE_DIR}/components/audio_stream/file_stream.c"
    "${CMAKE_SOURCE_DIR}/components/audio_stream/sink_stream.c"
    "${CMAKE_SOURCE_DIR}/components/audio_stream/http_stream.c"
)

# cflags: OS_LINUX, OS_ANDROID, OS_MACOSX, OS_IOS, OS_FREERTOS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOS_MACOSX")

# cflag: libmad: FPM_64BIT/FPM_INTEL/FPM_ARM/FPM_MIPS/FPM_DEFAULT
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFPM_64BIT")

# cflag: helix: __arm__/__amd64__, __GNUC__
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__amd64__ -DUSE_DEFAULT_STDLIB")

# cflag: libspeexdsp: FIXED_POINT/FLOATING_POINT, USE_NEON/USE_SSE
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFIXED_POINT")

# cflag: resampler: ENABLE_SRC, CONFIG_SRC_QUALITY
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_SRC -DCONFIG_SRC_QUALITY=10")

# cflags: compile paramters
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -g -Wall -Werror -Wno-error=pointer-to-int-cast -Wno-error=int-to-pointer-cast")

# ldflags
set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS} -Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed")

#message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")

link_directories(${OUT_LIB_DIR}/libmsgutils)

# litecore lib
add_library(litecore   SHARED ${LITECORE_SRC})
add_library(litecore_s STATIC ${LITECORE_SRC})

# thirdparty libraries dependance
target_link_libraries(litecore msgutils)
