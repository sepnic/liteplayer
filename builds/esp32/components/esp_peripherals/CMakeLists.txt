set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES sysutils driver)

set(COMPONENT_ADD_INCLUDEDIRS
    ./i2c_bus
    ./audio_codec
)

set (COMPONENT_PRIV_INCLUDEDIRS
    ./audio_codec/driver/include
)

set(COMPONENT_SRCS
    ./i2c_bus/i2c_bus.c
    ./audio_codec/audio_hal.c
    ./audio_codec/driver/es8311/es8311.c
 #   ./audio_codec/driver/es7243/es7243.c
)

register_component()
