cmake_minimum_required(VERSION 2.8)
project(liteplayer_demo)

# Support tinalsa?
#set (ENABLE_TINYALSA "YES")

set(TOP_DIR "${CMAKE_SOURCE_DIR}/../..")
set(LIB_DIR "${TOP_DIR}/build/out/lib")

# include files
include_directories(${TOP_DIR}/thirdparty/msgutils/include)
include_directories(${TOP_DIR}/thirdparty/httpclient)
include_directories(${TOP_DIR}/thirdparty/mbedtls/include)
include_directories(${TOP_DIR}/thirdparty/speexdsp)
include_directories(${TOP_DIR}/library/include)
include_directories(${TOP_DIR}/adapter)

# source files
set (THIRDPARTY_SRC
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
    include_directories(${TOP_DIR}/thirdparty/tinyalsa/include)
    list(APPEND THIRDPARTY_SRC
        ${TOP_DIR}/thirdparty/tinyalsa/pcm.c
        ${TOP_DIR}/thirdparty/tinyalsa/mixer.c
    )
    list(APPEND ADAPTER_SRC
        ${TOP_DIR}/adapter/tinyalsa_wrapper.c
    )
endif()

# cflags: compile paramters
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -g -Wall -Werror")
# cflags: if socketlen_t defined
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_SOCKLEN_T")

# ldflags
set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS} -Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed")


LINK_DIRECTORIES(${LIB_DIR}/libliteplayer)

# liteplayer_mngr
add_executable(liteplayer_mngr liteplayer_mngr.c ${ADAPTER_SRC} ${THIRDPARTY_SRC})
target_link_libraries(liteplayer_mngr libliteplayer_s.a pthread m)
