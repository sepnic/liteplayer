cmake_minimum_required(VERSION 2.8)

set(TOP_DIR "${CMAKE_SOURCE_DIR}/../..")
set(LIB_CORE_DIR "${TOP_DIR}/library/core")
set(LIB_COMPONENTS_DIR "${TOP_DIR}/library/components")
set(MSGUTILS_DIR "${TOP_DIR}/thirdparty/msgutils")

# include files
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${MSGUTILS_DIR}/include)
include_directories(${LIB_CORE_DIR}/include)
include_directories(${LIB_COMPONENTS_DIR}/include)

# source files
set (LIB_SRC
      "${LIB_CORE_DIR}/audio_element.c"
      "${LIB_CORE_DIR}/audio_event_iface.c"
      "${LIB_CORE_DIR}/audio_pipeline.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/aacdec.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/aactabs.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/bitstream.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/buffers.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/dct4.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/decelmnt.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/dequant.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/fft.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/filefmt.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/huffman.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/hufftabs.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/imdct.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/noiseless.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/pns.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/sbr.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/sbrfft.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/sbrfreq.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/sbrhfadj.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/sbrhfgen.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/sbrhuff.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/sbrimdct.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/sbrmath.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/sbrqmf.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/sbrside.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/sbrtabs.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/stproc.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/tns.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/helixaac/trigtabs.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/aac_helix_wrapper.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/aac_decoder.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/m4a_decoder.c"
      "${LIB_COMPONENTS_DIR}/audio_decoder/wav_decoder.c"
      "${LIB_COMPONENTS_DIR}/audio_extractor/aac_extractor.c"
      "${LIB_COMPONENTS_DIR}/audio_extractor/m4a_extractor.c"
      "${LIB_COMPONENTS_DIR}/audio_extractor/wav_extractor.c"
      "${LIB_COMPONENTS_DIR}/audio_resampler/libspeexdsp/resample.c"
      "${LIB_COMPONENTS_DIR}/audio_resampler/audio_resampler.c"
      "${LIB_COMPONENTS_DIR}/audio_stream/fatfs_stream.c"
)

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
set (MAIN_SRC
      ${CMAKE_SOURCE_DIR}/fatfs_m4aplayer.c
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

# fatfs_mp3player bin
add_executable(fatfs_m4aplayer ${MAIN_SRC} ${LIB_SRC} ${MSGUTILS_SRC})
target_link_libraries(fatfs_m4aplayer pthread m)
