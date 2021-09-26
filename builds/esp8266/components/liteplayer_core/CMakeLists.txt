set(TOP_DIR ../../../..)
set(LITEPLAYER_DIR ${TOP_DIR}/library)
set(THIRDPARTY_DIR ${TOP_DIR}/thirdparty)

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES sysutils)

set(COMPONENT_ADD_INCLUDEDIRS
    ${LITEPLAYER_DIR}/include
)

set (COMPONENT_PRIV_INCLUDEDIRS
    ${THIRDPARTY_DIR}/sysutils/include
    ${THIRDPARTY_DIR}/codecs
    ${LITEPLAYER_DIR}/source
)

set(COMPONENT_SRCS
    ${LITEPLAYER_DIR}/source/esp_adf/audio_element.c
    ${LITEPLAYER_DIR}/source/esp_adf/audio_event_iface.c
    ${LITEPLAYER_DIR}/source/audio_decoder/mp3_mad_wrapper.c
    ${LITEPLAYER_DIR}/source/audio_decoder/mp3_decoder.c
    ${LITEPLAYER_DIR}/source/audio_decoder/aac_helix_wrapper.c
    ${LITEPLAYER_DIR}/source/audio_decoder/aac_decoder.c
    ${LITEPLAYER_DIR}/source/audio_decoder/m4a_decoder.c
    ${LITEPLAYER_DIR}/source/audio_decoder/wav_decoder.c
    ${LITEPLAYER_DIR}/source/audio_extractor/mp3_extractor.c
    ${LITEPLAYER_DIR}/source/audio_extractor/aac_extractor.c
    ${LITEPLAYER_DIR}/source/audio_extractor/m4a_extractor.c
    ${LITEPLAYER_DIR}/source/audio_extractor/wav_extractor.c
    ${LITEPLAYER_DIR}/source/liteplayer_adapter.c
    ${LITEPLAYER_DIR}/source/liteplayer_source.c
    ${LITEPLAYER_DIR}/source/liteplayer_parser.c
    ${LITEPLAYER_DIR}/source/liteplayer_debug.c
    ${LITEPLAYER_DIR}/source/liteplayer_main.c
    ${LITEPLAYER_DIR}/source/liteplayer_listplayer.c
    ${LITEPLAYER_DIR}/source/liteplayer_ttsplayer.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/bit.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/decoder.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/fixed.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/frame.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/huffman.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/layer12.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/layer3.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/stream.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/synth.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/timer.c
    ${THIRDPARTY_DIR}/codecs/mp3-mad/version.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/aacdec.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/aactabs.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/bitstream.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/buffers.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/dct4.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/decelmnt.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/dequant.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/fft.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/filefmt.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/huffman.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/hufftabs.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/imdct.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/noiseless.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/pns.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbr.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrfft.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrfreq.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrhfadj.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrhfgen.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrhuff.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrimdct.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrmath.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrqmf.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrside.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/sbrtabs.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/stproc.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/tns.c
    ${THIRDPARTY_DIR}/codecs/aac-helix/trigtabs.c
)

register_component()

target_compile_options(${COMPONENT_LIB} PRIVATE -DOS_RTOS -DOS_FREERTOS_ESP8266 -DARDUINO -DCONFIG_MAD_FPM_DEFAULT -DCONFIG_SINK_FIXED_S16LE)
