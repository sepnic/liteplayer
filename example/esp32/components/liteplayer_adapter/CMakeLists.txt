set(TOP_DIR ../../../..)
set(LITEPLAYER_DIR ${TOP_DIR}/library)
set(THIRDPARTY_DIR ${TOP_DIR}/thirdparty)
set(ADAPTER_DIR ${TOP_DIR}/adapter)

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES sysutils driver audio_board)

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
    sink_esp32_i2s_wrapper.c
)

register_component()
