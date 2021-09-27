#
# Component Makefile
#
# This Makefile should, at the very least, just include $(SDK_PATH)/Makefile. By default,
# this will take the sources in the src/ directory, compile them and link them into
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the SDK documents if you need to do this.
#

COMPONENT_ADD_INCLUDEDIRS := \
    ./i2c_bus \
    ./audio_codec

COMPONENT_PRIV_INCLUDEDIRS := \
    ./audio_codec/driver/include

COMPONENT_SRCDIRS := \
    ./i2c_bus \
    ./audio_codec \
    ./audio_codec/driver/es7243 \
    ./audio_codec/driver/es8311
