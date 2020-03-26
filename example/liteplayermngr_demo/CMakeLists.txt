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
include_directories(${TOP_DIR}/library/core/include)
include_directories(${TOP_DIR}/library/components/include)
include_directories(${TOP_DIR}/player)
include_directories(${TOP_DIR}/adapter)

# source files
set (THIRDPARTY_SRC
    # msgutils src
    ${TOP_DIR}/thirdparty/msgutils/source/memory_detect.c
    ${TOP_DIR}/thirdparty/msgutils/source/msglooper.c
    ${TOP_DIR}/thirdparty/msgutils/source/msgqueue.c
    ${TOP_DIR}/thirdparty/msgutils/source/ringbuf.c
    ${TOP_DIR}/thirdparty/msgutils/source/smartptr.c
    ${TOP_DIR}/thirdparty/msgutils/source/stllist.c
    ${TOP_DIR}/thirdparty/msgutils/source/sw_timer.c
    ${TOP_DIR}/thirdparty/msgutils/source/sw_watchdog.c
    ${TOP_DIR}/thirdparty/msgutils/source/portable/os_logger.c
    ${TOP_DIR}/thirdparty/msgutils/source/portable/os_thread.c
    ${TOP_DIR}/thirdparty/msgutils/source/portable/os_time.c
    ${TOP_DIR}/thirdparty/msgutils/source/portable/os_timer.c

    # httpclient src
    ${TOP_DIR}/thirdparty/httpclient/httpclient.c

    # mbedtls src
    ${TOP_DIR}/thirdparty/mbedtls/library/aes.c
    ${TOP_DIR}/thirdparty/mbedtls/library/aesni.c
    ${TOP_DIR}/thirdparty/mbedtls/library/arc4.c
    ${TOP_DIR}/thirdparty/mbedtls/library/asn1parse.c
    ${TOP_DIR}/thirdparty/mbedtls/library/asn1write.c
    ${TOP_DIR}/thirdparty/mbedtls/library/base64.c
    ${TOP_DIR}/thirdparty/mbedtls/library/bignum.c
    ${TOP_DIR}/thirdparty/mbedtls/library/blowfish.c
    ${TOP_DIR}/thirdparty/mbedtls/library/camellia.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ccm.c
    ${TOP_DIR}/thirdparty/mbedtls/library/cipher.c
    ${TOP_DIR}/thirdparty/mbedtls/library/cipher_wrap.c
    ${TOP_DIR}/thirdparty/mbedtls/library/cmac.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ctr_drbg.c
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
    ${TOP_DIR}/thirdparty/mbedtls/library/hmac_drbg.c
    ${TOP_DIR}/thirdparty/mbedtls/library/md.c
    ${TOP_DIR}/thirdparty/mbedtls/library/md2.c
    ${TOP_DIR}/thirdparty/mbedtls/library/md4.c
    ${TOP_DIR}/thirdparty/mbedtls/library/md5.c
    ${TOP_DIR}/thirdparty/mbedtls/library/md_wrap.c
    ${TOP_DIR}/thirdparty/mbedtls/library/memory_buffer_alloc.c
    ${TOP_DIR}/thirdparty/mbedtls/library/oid.c
    ${TOP_DIR}/thirdparty/mbedtls/library/padlock.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pem.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pk.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pk_wrap.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pkcs12.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pkcs5.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pkparse.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pkwrite.c
    ${TOP_DIR}/thirdparty/mbedtls/library/platform.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ripemd160.c
    ${TOP_DIR}/thirdparty/mbedtls/library/rsa.c
    ${TOP_DIR}/thirdparty/mbedtls/library/rsa_internal.c
    ${TOP_DIR}/thirdparty/mbedtls/library/sha1.c
    ${TOP_DIR}/thirdparty/mbedtls/library/sha256.c
    ${TOP_DIR}/thirdparty/mbedtls/library/sha512.c
    ${TOP_DIR}/thirdparty/mbedtls/library/threading.c
    ${TOP_DIR}/thirdparty/mbedtls/library/timing.c
    ${TOP_DIR}/thirdparty/mbedtls/library/version.c
    ${TOP_DIR}/thirdparty/mbedtls/library/version_features.c
    ${TOP_DIR}/thirdparty/mbedtls/library/xtea.c
    ${TOP_DIR}/thirdparty/mbedtls/library/certs.c
    ${TOP_DIR}/thirdparty/mbedtls/library/pkcs11.c
    ${TOP_DIR}/thirdparty/mbedtls/library/x509.c
    ${TOP_DIR}/thirdparty/mbedtls/library/x509_create.c
    ${TOP_DIR}/thirdparty/mbedtls/library/x509_crl.c
    ${TOP_DIR}/thirdparty/mbedtls/library/x509_crt.c
    ${TOP_DIR}/thirdparty/mbedtls/library/x509_csr.c
    ${TOP_DIR}/thirdparty/mbedtls/library/x509write_crt.c
    ${TOP_DIR}/thirdparty/mbedtls/library/x509write_csr.c
    ${TOP_DIR}/thirdparty/mbedtls/library/debug.c
    ${TOP_DIR}/thirdparty/mbedtls/library/net_sockets.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ssl_cache.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ssl_ciphersuites.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ssl_cli.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ssl_cookie.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ssl_srv.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ssl_ticket.c
    ${TOP_DIR}/thirdparty/mbedtls/library/ssl_tls.c

    # codec src
    ${TOP_DIR}/thirdparty/codec/mp3-mad/bit.c
    ${TOP_DIR}/thirdparty/codec/mp3-mad/decoder.c
    ${TOP_DIR}/thirdparty/codec/mp3-mad/fixed.c
    ${TOP_DIR}/thirdparty/codec/mp3-mad/frame.c
    ${TOP_DIR}/thirdparty/codec/mp3-mad/huffman.c
    ${TOP_DIR}/thirdparty/codec/mp3-mad/layer12.c
    ${TOP_DIR}/thirdparty/codec/mp3-mad/layer3.c
    ${TOP_DIR}/thirdparty/codec/mp3-mad/stream.c
    ${TOP_DIR}/thirdparty/codec/mp3-mad/synth.c
    ${TOP_DIR}/thirdparty/codec/mp3-mad/timer.c
    ${TOP_DIR}/thirdparty/codec/mp3-mad/version.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/aacdec.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/aactabs.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/bitstream.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/buffers.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/dct4.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/decelmnt.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/dequant.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/fft.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/filefmt.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/huffman.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/hufftabs.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/imdct.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/noiseless.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/pns.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/sbr.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/sbrfft.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/sbrfreq.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/sbrhfadj.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/sbrhfgen.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/sbrhuff.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/sbrimdct.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/sbrmath.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/sbrqmf.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/sbrside.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/sbrtabs.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/stproc.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/tns.c
    ${TOP_DIR}/thirdparty/codec/aac-helix/trigtabs.c
    ${TOP_DIR}/thirdparty/speexdsp/resample.c
)

# source files
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
    ${TOP_DIR}/library/components/audio_decoder/mp3_mad_wrapper.c
    ${TOP_DIR}/library/components/audio_decoder/mp3_decoder.c
    ${TOP_DIR}/library/components/audio_decoder/aac_helix_wrapper.c
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

# cflag: libmad: FPM_64BIT/FPM_INTEL/FPM_ARM/FPM_MIPS/FPM_DEFAULT
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFPM_64BIT")

# cflag: helix: __arm__/__amd64__, __GNUC__
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__amd64__ -DUSE_DEFAULT_STDLIB")

# cflag: libspeexdsp: FIXED_POINT/FLOATING_POINT, USE_NEON/USE_SSE
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFLOATING_POINT -DUSE_SSE")

# cflags: compile paramters
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -g -Wall -Werror")

# ldflags
set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS} -Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed")

#message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")

# liteplayermngr_demo
add_executable(liteplayermngr_demo liteplayermngr_demo.c ${ADAPTER_SRC} ${THIRDPARTY_SRC} ${LITEPLAYER_SRC})
target_link_libraries(liteplayermngr_demo pthread m)
