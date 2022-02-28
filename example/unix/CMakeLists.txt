cmake_minimum_required(VERSION 2.8)
project(liteplayer_demo)

MESSAGE(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    option(HAVE_LINUX_ALSA_ENABLED "HAVE LINUX ALSA ENABLED" "ON")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    option(HAVE_PORT_AUDIO_ENABLED "HAVE PORT AUDIO ENABLED" "ON")
endif()

set(TOP_DIR "${CMAKE_SOURCE_DIR}/../..")

# cflags: OS_LINUX, OS_ANDROID, OS_APPLE, OS_RTOS
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -DOS_APPLE")

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=gnu99 -Wall -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror")

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -DSYSUTILS_HAVE_MEMORY_LEAK_DETECT_ENABLED")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSYSUTILS_HAVE_MEMORY_LEAK_DETECT_ENABLED")

# include files
include_directories(${TOP_DIR}/thirdparty/sysutils/include)
include_directories(${TOP_DIR}/library/include)
include_directories(${TOP_DIR}/adapter)

# sysutils files
set(SYSUTILS_SRC
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_log.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_memory.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_thread.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_time.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_timer.c
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/os_misc.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/memdbg.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/mlooper.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/mqueue.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/ringbuf.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/swtimer.c
    ${TOP_DIR}/thirdparty/sysutils/source/httpclient/httpclient.c
)
add_library(sysutils STATIC ${SYSUTILS_SRC})
target_compile_options(sysutils PRIVATE -DSYSUTILS_HAVE_MBEDTLS_ENABLED)
target_include_directories(sysutils PRIVATE ${TOP_DIR}/thirdparty/mbedtls/include)

# source files
file(GLOB LITEPLAYER_CODEC_SRC src
    ${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/src/*.cpp
    ${TOP_DIR}/thirdparty/codecs/aac-pvaac/*.cpp
    ${TOP_DIR}/thirdparty/speexdsp/resample.c
)
set(LITEPLAYER_CORE_SRC
    ${LITEPLAYER_CODEC_SRC}
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
    ${TOP_DIR}/library/source/liteplayer_debug.c
    ${TOP_DIR}/library/source/liteplayer_main.c
    ${TOP_DIR}/library/source/liteplayer_listplayer.c
    ${TOP_DIR}/library/source/liteplayer_ttsplayer.c
    ${TOP_DIR}/library/source/liteplayer_resampler.c
)
add_library(liteplayer_core STATIC ${LITEPLAYER_CORE_SRC})
target_compile_options(liteplayer_core PRIVATE
    -D__amd64__
    -DLITEPLAYER_CONFIG_SPEEXDSP_FIXED_POINT
    -DLITEPLAYER_CONFIG_SINK_FIXED_S16LE
    -DOSCL_IMPORT_REF= -DOSCL_EXPORT_REF= -DOSCL_UNUSED_ARG=\(void\)
    -Wno-error=unused-value -Wno-error=unused-function -Wno-error=narrowing
)
target_include_directories(liteplayer_core PRIVATE
    ${TOP_DIR}/thirdparty/codecs
    ${TOP_DIR}/thirdparty/speexdsp
    ${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/include
    ${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/src
    ${TOP_DIR}/thirdparty/codecs/aac-pvaac
    ${TOP_DIR}/library/source
)

set(MBEDTLS_SRC
    ${TOP_DIR}/thirdparty/mbedtls/library/aes.c
    ${TOP_DIR}/thirdparty/mbedtls/library/aesni.c
    ${TOP_DIR}/thirdparty/mbedtls/library/arc4.c
    ${TOP_DIR}/thirdparty/mbedtls/library/aria.c
    ${TOP_DIR}/thirdparty/mbedtls/library/asn1parse.c
    ${TOP_DIR}/thirdparty/mbedtls/library/asn1write.c
    ${TOP_DIR}/thirdparty/mbedtls/library/base64.c
    ${TOP_DIR}/thirdparty/mbedtls/library/bignum.c
    ${TOP_DIR}/thirdparty/mbedtls/library/blowfish.c
    ${TOP_DIR}/thirdparty/mbedtls/library/camellia.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ccm.c
    ${TOP_DIR}/thirdparty/mbedtls/library/certs.c
    ${TOP_DIR}/thirdparty/mbedtls/library/chacha20.c
    ${TOP_DIR}/thirdparty/mbedtls/library/chachapoly.c
    ${TOP_DIR}/thirdparty/mbedtls/library/cipher.c
    ${TOP_DIR}/thirdparty/mbedtls/library/cipher_wrap.c
    ${TOP_DIR}/thirdparty/mbedtls/library/cmac.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ctr_drbg.c
    ${TOP_DIR}/thirdparty/mbedtls/library/debug.c
    ${TOP_DIR}/thirdparty/mbedtls/library/des.c
    ${TOP_DIR}/thirdparty/mbedtls/library/dhm.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ecdh.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ecdsa.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ecjpake.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ecp.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ecp_curves.c
    ${TOP_DIR}/thirdparty/mbedtls/library/entropy.c
    ${TOP_DIR}/thirdparty/mbedtls/library/entropy_poll.c
    ${TOP_DIR}/thirdparty/mbedtls/library/error.c
    ${TOP_DIR}/thirdparty/mbedtls/library/gcm.c
    ${TOP_DIR}/thirdparty/mbedtls/library/havege.c
    ${TOP_DIR}/thirdparty/mbedtls/library/hkdf.c
    ${TOP_DIR}/thirdparty/mbedtls/library/hmac_drbg.c
    ${TOP_DIR}/thirdparty/mbedtls/library/md.c
    ${TOP_DIR}/thirdparty/mbedtls/library/md2.c
    ${TOP_DIR}/thirdparty/mbedtls/library/md4.c
    ${TOP_DIR}/thirdparty/mbedtls/library/md5.c
    ${TOP_DIR}/thirdparty/mbedtls/library/md_wrap.c
    ${TOP_DIR}/thirdparty/mbedtls/library/memory_buffer_alloc.c
    ${TOP_DIR}/thirdparty/mbedtls/library/net_sockets.c
    ${TOP_DIR}/thirdparty/mbedtls/library/nist_kw.c
    ${TOP_DIR}/thirdparty/mbedtls/library/oid.c
    ${TOP_DIR}/thirdparty/mbedtls/library/padlock.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pem.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pk.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pk_wrap.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pkcs11.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pkcs12.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pkcs5.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pkparse.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pkwrite.c
    ${TOP_DIR}/thirdparty/mbedtls/library/platform.c
    ${TOP_DIR}/thirdparty/mbedtls/library/platform_util.c
    ${TOP_DIR}/thirdparty/mbedtls/library/poly1305.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ripemd160.c
    ${TOP_DIR}/thirdparty/mbedtls/library/rsa.c
    ${TOP_DIR}/thirdparty/mbedtls/library/rsa_internal.c
    ${TOP_DIR}/thirdparty/mbedtls/library/sha1.c
    ${TOP_DIR}/thirdparty/mbedtls/library/sha256.c
    ${TOP_DIR}/thirdparty/mbedtls/library/sha512.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ssl_cache.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ssl_ciphersuites.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ssl_cli.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ssl_cookie.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ssl_srv.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ssl_ticket.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ssl_tls.c
    ${TOP_DIR}/thirdparty/mbedtls/library/threading.c
    ${TOP_DIR}/thirdparty/mbedtls/library/timing.c
    ${TOP_DIR}/thirdparty/mbedtls/library/version.c
    ${TOP_DIR}/thirdparty/mbedtls/library/version_features.c
    ${TOP_DIR}/thirdparty/mbedtls/library/x509.c
    ${TOP_DIR}/thirdparty/mbedtls/library/x509_create.c
    ${TOP_DIR}/thirdparty/mbedtls/library/x509_crl.c
    ${TOP_DIR}/thirdparty/mbedtls/library/x509_crt.c
    ${TOP_DIR}/thirdparty/mbedtls/library/x509_csr.c
    ${TOP_DIR}/thirdparty/mbedtls/library/x509write_crt.c
    ${TOP_DIR}/thirdparty/mbedtls/library/x509write_csr.c
    ${TOP_DIR}/thirdparty/mbedtls/library/xtea.c
)
add_library(mbedtls STATIC ${MBEDTLS_SRC})
target_include_directories(mbedtls PRIVATE ${TOP_DIR}/thirdparty/mbedtls/include)

set(LITEPLAYER_ADAPTER_SRC
    ${TOP_DIR}/adapter/source_httpclient_wrapper.c
    ${TOP_DIR}/adapter/source_file_wrapper.c
    ${TOP_DIR}/adapter/sink_wave_wrapper.c
)
if(HAVE_LINUX_ALSA_ENABLED)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_LINUX_ALSA_ENABLED")
    list(APPEND LITEPLAYER_ADAPTER_SRC ${TOP_DIR}/adapter/sink_alsa_wrapper.c)
elseif(HAVE_PORT_AUDIO_ENABLED)
    set(PORTAUDIO_DIR "${TOP_DIR}/thirdparty/portaudio")
    include_directories(${PORTAUDIO_DIR}/include)
    include_directories(${PORTAUDIO_DIR}/src/common)
    include_directories(${PORTAUDIO_DIR}/src/os/unix)
    include(TestBigEndian)
    TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
    if(IS_BIG_ENDIAN)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPA_BIG_ENDIAN")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPA_LITTLE_ENDIAN")
    endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=deprecated-declarations")
    set(PORTAUDIO_SRC
        ${PORTAUDIO_DIR}/src/common/pa_allocation.c
        ${PORTAUDIO_DIR}/src/common/pa_converters.c
        ${PORTAUDIO_DIR}/src/common/pa_cpuload.c
        ${PORTAUDIO_DIR}/src/common/pa_debugprint.c
        ${PORTAUDIO_DIR}/src/common/pa_dither.c
        ${PORTAUDIO_DIR}/src/common/pa_front.c
        ${PORTAUDIO_DIR}/src/common/pa_process.c
        ${PORTAUDIO_DIR}/src/common/pa_ringbuffer.c
        ${PORTAUDIO_DIR}/src/common/pa_stream.c
        ${PORTAUDIO_DIR}/src/common/pa_trace.c
        ${PORTAUDIO_DIR}/src/hostapi/skeleton/pa_hostapi_skeleton.c
        ${PORTAUDIO_DIR}/src/os/unix/pa_unix_hostapis.c
        ${PORTAUDIO_DIR}/src/os/unix/pa_unix_util.c)
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPA_USE_ALSA=1")
        set(PORTAUDIO_SRC ${PORTAUDIO_SRC}
            ${PORTAUDIO_DIR}/src/hostapi/alsa/pa_linux_alsa.c)
    elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPA_USE_COREAUDIO=1")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Accelerate")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreAudio")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework AudioToolbox")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework AudioUnit")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreServices")
        set(PORTAUDIO_SRC ${PORTAUDIO_SRC}
            ${PORTAUDIO_DIR}/src/hostapi/coreaudio/pa_mac_core.c
            ${PORTAUDIO_DIR}/src/hostapi/coreaudio/pa_mac_core_blocking.c
            ${PORTAUDIO_DIR}/src/hostapi/coreaudio/pa_mac_core_utilities.c)
    endif()

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_PORT_AUDIO_ENABLED")
    list(APPEND LITEPLAYER_ADAPTER_SRC
        ${PORTAUDIO_SRC}
        ${TOP_DIR}/adapter/sink_portaudio_wrapper.c)
endif()
add_library(liteplayer_adapter STATIC ${LITEPLAYER_ADAPTER_SRC})

# basic_demo
add_executable(basic_demo basic_demo.c)
target_link_libraries(basic_demo liteplayer_core liteplayer_adapter sysutils mbedtls pthread m)

# playlist_demo
add_executable(playlist_demo playlist_demo.c)
target_link_libraries(playlist_demo liteplayer_core liteplayer_adapter sysutils mbedtls pthread m)

# tts_demo
add_executable(tts_demo tts_demo.c)
target_link_libraries(tts_demo liteplayer_core liteplayer_adapter sysutils mbedtls pthread m)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(basic_demo asound)
    target_link_libraries(playlist_demo asound)
    target_link_libraries(tts_demo asound)
endif()

file(COPY
    ${CMAKE_SOURCE_DIR}/test.mp3
    ${CMAKE_SOURCE_DIR}/test.m4a
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
)
