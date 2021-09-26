set(TOP_DIR ../../../..)
set(LITEPLAYER_DIR ${TOP_DIR}/library)
set(ADAPTER_DIR ${TOP_DIR}/adapter)

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES sysutils)

set(COMPONENT_ADD_INCLUDEDIRS
    ${ADAPTER_DIR}
    .
)

set (COMPONENT_PRIV_INCLUDEDIRS
    ${THIRDPARTY_DIR}/sysutils/include
    ${LITEPLAYER_DIR}/include
)

set(COMPONENT_SRCS
    ${ADAPTER_DIR}/source_httpclient_wrapper.c
    source_esp8266_builtin_wrapper.c
    sink_esp8266_i2s_wrapper.c
)

register_component()

#target_compile_options(${COMPONENT_LIB} PRIVATE -DOS_RTOS -DOS_FREERTOS_ESP8266)
