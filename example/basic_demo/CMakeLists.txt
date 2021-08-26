cmake_minimum_required(VERSION 2.8)
project(basic_demo)

MESSAGE(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    # Support alsa?
    set (ENABLE_LINUX_ALSA "YES")
endif()

set(TOP_DIR "${CMAKE_SOURCE_DIR}/../..")

# cflags: OS_LINUX, OS_ANDROID, OS_APPLE, OS_RTOS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOS_APPLE")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Werror -Wno-error=unused-function")
# cflag: if enable aac sbr
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAAC_ENABLE_SBR")
# cflag: libspeexdsp: FIXED_POINT/FLOATING_POINT, USE_NEON/USE_SSE
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFLOATING_POINT -DUSE_SSE")
# cflags: if socketlen_t defined
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_SOCKLEN_T")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_HTTPCLIENT_MBEDTLS")
# cflags: if support 24bits or 32bits
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_PCM_S24LE_S32LE_SUPPORT")

# include files
include_directories(${TOP_DIR}/thirdparty/sysutils/include)
include_directories(${TOP_DIR}/thirdparty/codecs)
include_directories(${TOP_DIR}/library/include)
include_directories(${TOP_DIR}/library/source)
include_directories(${TOP_DIR}/adapter/external/mbedtls/include)
include_directories(${TOP_DIR}/adapter)

# include files
include_directories(${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/include)
include_directories(${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/src)
include_directories(${TOP_DIR}/thirdparty/codecs/aac-fdk/libAACdec/include)
include_directories(${TOP_DIR}/thirdparty/codecs/aac-fdk/libPCMutils/include)
include_directories(${TOP_DIR}/thirdparty/codecs/aac-fdk/libFDK/include)
include_directories(${TOP_DIR}/thirdparty/codecs/aac-fdk/libSYS/include)
include_directories(${TOP_DIR}/thirdparty/codecs/aac-fdk/libMpegTPDec/include)
include_directories(${TOP_DIR}/thirdparty/codecs/aac-fdk/libSBRdec/include)
include_directories(${TOP_DIR}/thirdparty/codecs/aac-fdk/libArithCoding/include)
include_directories(${TOP_DIR}/thirdparty/codecs/aac-fdk/libDRCdec/include)
include_directories(${TOP_DIR}/thirdparty/codecs/aac-fdk/libSACdec/include)

# source files
file(GLOB THIRDPARTY_SRC src
    ${TOP_DIR}/thirdparty/sysutils/osal/unix/*.c
    ${TOP_DIR}/thirdparty/sysutils/source/cutils/*.c
    ${TOP_DIR}/thirdparty/sysutils/source/httpclient/*.c
    ${TOP_DIR}/thirdparty/codecs/mp3-pvmp3/src/*.cpp
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libAACdec/src/*.cpp
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libPCMutils/src/*.cpp
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libFDK/src/*.cpp
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libSYS/src/*.cpp
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libMpegTPDec/src/*.cpp
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libSBRdec/src/*.cpp
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libArithCoding/src/*.cpp
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libDRCdec/src/*.cpp
    ${TOP_DIR}/thirdparty/codecs/aac-fdk/libSACdec/src/*.cpp
)
set (ADAPTER_SRC
    ${TOP_DIR}/adapter/external/mbedtls/library/aes.c
    ${TOP_DIR}/adapter/external/mbedtls/library/aesni.c
    ${TOP_DIR}/adapter/external/mbedtls/library/arc4.c
    ${TOP_DIR}/adapter/external/mbedtls/library/aria.c
    ${TOP_DIR}/adapter/external/mbedtls/library/asn1parse.c
    ${TOP_DIR}/adapter/external/mbedtls/library/asn1write.c
    ${TOP_DIR}/adapter/external/mbedtls/library/base64.c
    ${TOP_DIR}/adapter/external/mbedtls/library/bignum.c
    ${TOP_DIR}/adapter/external/mbedtls/library/blowfish.c
    ${TOP_DIR}/adapter/external/mbedtls/library/camellia.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ccm.c
    ${TOP_DIR}/adapter/external/mbedtls/library/certs.c
    ${TOP_DIR}/adapter/external/mbedtls/library/chacha20.c
    ${TOP_DIR}/adapter/external/mbedtls/library/chachapoly.c
    ${TOP_DIR}/adapter/external/mbedtls/library/cipher.c
    ${TOP_DIR}/adapter/external/mbedtls/library/cipher_wrap.c
    ${TOP_DIR}/adapter/external/mbedtls/library/cmac.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ctr_drbg.c
    ${TOP_DIR}/adapter/external/mbedtls/library/debug.c
    ${TOP_DIR}/adapter/external/mbedtls/library/des.c
    ${TOP_DIR}/adapter/external/mbedtls/library/dhm.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ecdh.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ecdsa.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ecjpake.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ecp.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ecp_curves.c
    ${TOP_DIR}/adapter/external/mbedtls/library/entropy.c
    ${TOP_DIR}/adapter/external/mbedtls/library/entropy_poll.c
    ${TOP_DIR}/adapter/external/mbedtls/library/error.c
    ${TOP_DIR}/adapter/external/mbedtls/library/gcm.c
    ${TOP_DIR}/adapter/external/mbedtls/library/havege.c
    ${TOP_DIR}/adapter/external/mbedtls/library/hkdf.c
    ${TOP_DIR}/adapter/external/mbedtls/library/hmac_drbg.c
    ${TOP_DIR}/adapter/external/mbedtls/library/md.c
    ${TOP_DIR}/adapter/external/mbedtls/library/md2.c
    ${TOP_DIR}/adapter/external/mbedtls/library/md4.c
    ${TOP_DIR}/adapter/external/mbedtls/library/md5.c
    ${TOP_DIR}/adapter/external/mbedtls/library/md_wrap.c
    ${TOP_DIR}/adapter/external/mbedtls/library/memory_buffer_alloc.c
    ${TOP_DIR}/adapter/external/mbedtls/library/net_sockets.c
    ${TOP_DIR}/adapter/external/mbedtls/library/nist_kw.c
    ${TOP_DIR}/adapter/external/mbedtls/library/oid.c
    ${TOP_DIR}/adapter/external/mbedtls/library/padlock.c
    ${TOP_DIR}/adapter/external/mbedtls/library/pem.c
    ${TOP_DIR}/adapter/external/mbedtls/library/pk.c
    ${TOP_DIR}/adapter/external/mbedtls/library/pk_wrap.c
    ${TOP_DIR}/adapter/external/mbedtls/library/pkcs11.c
    ${TOP_DIR}/adapter/external/mbedtls/library/pkcs12.c
    ${TOP_DIR}/adapter/external/mbedtls/library/pkcs5.c
    ${TOP_DIR}/adapter/external/mbedtls/library/pkparse.c
    ${TOP_DIR}/adapter/external/mbedtls/library/pkwrite.c
    ${TOP_DIR}/adapter/external/mbedtls/library/platform.c
    ${TOP_DIR}/adapter/external/mbedtls/library/platform_util.c
    ${TOP_DIR}/adapter/external/mbedtls/library/poly1305.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ripemd160.c
    ${TOP_DIR}/adapter/external/mbedtls/library/rsa.c
    ${TOP_DIR}/adapter/external/mbedtls/library/rsa_internal.c
    ${TOP_DIR}/adapter/external/mbedtls/library/sha1.c
    ${TOP_DIR}/adapter/external/mbedtls/library/sha256.c
    ${TOP_DIR}/adapter/external/mbedtls/library/sha512.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ssl_cache.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ssl_ciphersuites.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ssl_cli.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ssl_cookie.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ssl_srv.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ssl_ticket.c
    ${TOP_DIR}/adapter/external/mbedtls/library/ssl_tls.c
    ${TOP_DIR}/adapter/external/mbedtls/library/threading.c
    ${TOP_DIR}/adapter/external/mbedtls/library/timing.c
    ${TOP_DIR}/adapter/external/mbedtls/library/version.c
    ${TOP_DIR}/adapter/external/mbedtls/library/version_features.c
    ${TOP_DIR}/adapter/external/mbedtls/library/x509.c
    ${TOP_DIR}/adapter/external/mbedtls/library/x509_create.c
    ${TOP_DIR}/adapter/external/mbedtls/library/x509_crl.c
    ${TOP_DIR}/adapter/external/mbedtls/library/x509_crt.c
    ${TOP_DIR}/adapter/external/mbedtls/library/x509_csr.c
    ${TOP_DIR}/adapter/external/mbedtls/library/x509write_crt.c
    ${TOP_DIR}/adapter/external/mbedtls/library/x509write_csr.c
    ${TOP_DIR}/adapter/external/mbedtls/library/xtea.c
    ${TOP_DIR}/adapter/source_httpclient_wrapper.c
    ${TOP_DIR}/adapter/source_fatfs_wrapper.c
    ${TOP_DIR}/adapter/sink_wave_wrapper.c
)

if (ENABLE_LINUX_ALSA)
    # Force SRC to 48KHz/Stereo for tinyalsa device
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_LINUX_ALSA")
    list(APPEND ADAPTER_SRC
        ${TOP_DIR}/adapter/sink_alsa_wrapper.c
    )
endif()

# source files
set (LITEPLAYER_SRC
    ${TOP_DIR}/library/source/esp_adf/audio_element.c
    ${TOP_DIR}/library/source/esp_adf/audio_event_iface.c
    ${TOP_DIR}/library/source/esp_adf/audio_pipeline.c
    ${TOP_DIR}/library/source/audio_decoder/mp3_pvmp3_wrapper.c
    ${TOP_DIR}/library/source/audio_decoder/mp3_decoder.c
    ${TOP_DIR}/library/source/audio_decoder/aac_fdk_wrapper.c
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

# basic_demo
add_executable(basic_demo basic_demo.c ${THIRDPARTY_SRC} ${LITEPLAYER_SRC} ${ADAPTER_SRC})
target_link_libraries(basic_demo pthread)
if (ENABLE_LINUX_ALSA)
target_link_libraries(basic_demo asound)
endif()
