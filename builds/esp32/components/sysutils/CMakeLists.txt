set(TOP_DIR ../../../../thirdparty/sysutils)

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES pthread mbedtls)

set(COMPONENT_ADD_INCLUDEDIRS ${TOP_DIR}/include)

set(COMPONENT_SRCS
    ${TOP_DIR}/osal/esp8266/os_log.c
    ${TOP_DIR}/osal/esp8266/os_memory.c
    ${TOP_DIR}/osal/esp8266/os_thread.c
    ${TOP_DIR}/osal/esp8266/os_time.c
    ${TOP_DIR}/osal/esp8266/os_timer.c
    ${TOP_DIR}/osal/esp8266/os_misc.c
    ${TOP_DIR}/source/cutils/memdbg.c
    ${TOP_DIR}/source/cutils/mlooper.c
    ${TOP_DIR}/source/cutils/mqueue.c
    ${TOP_DIR}/source/cutils/ringbuf.c
    ${TOP_DIR}/source/cutils/swtimer.c
    ${TOP_DIR}/source/cipher/sha2.c
    ${TOP_DIR}/source/cipher/hmac_sha2.c
    ${TOP_DIR}/source/cipher/md5.c
    ${TOP_DIR}/source/cipher/base64.c
    ${TOP_DIR}/source/httpclient/httpclient.c
)

register_component()

target_compile_options(${COMPONENT_TARGET} PRIVATE -DOS_RTOS -DOS_FREERTOS_ESP32 -DENABLE_HTTPCLIENT_MBEDTLS)
