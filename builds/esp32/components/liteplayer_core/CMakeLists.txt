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
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/include
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src
    ${LITEPLAYER_DIR}/source
)

set(COMPONENT_SRCS
    ${LITEPLAYER_DIR}/source/esp_adf/audio_element.c
    ${LITEPLAYER_DIR}/source/esp_adf/audio_event_iface.c
    ${LITEPLAYER_DIR}/source/audio_decoder/mp3_pvmp3_wrapper.c
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
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_alias_reduction.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_huffman_parsing.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_crc.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_imdct_synth.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_dct_16.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_mdct_18.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_dct_6.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_mdct_6.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_dct_9.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_mpeg2_get_scale_data.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_decode_header.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_mpeg2_get_scale_factors.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_decode_huff_cw.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_mpeg2_stereo_proc.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_dequantize_sample.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_normalize.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_equalizer.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_poly_phase_synthesis.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_framedecoder.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_polyphase_filter_window.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_get_main_data_size.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_reorder.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_get_scale_factors.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_seek_synch.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_get_side_info.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_stereo_proc.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_getbits.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_tables.cpp
    ${THIRDPARTY_DIR}/codecs/mp3-pvmp3/src/pvmp3_huffman_decoding.cpp
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

target_compile_options(${COMPONENT_TARGET} PRIVATE -DOS_RTOS -DOS_FREERTOS_ESP8266 -DARDUINO -DCONFIG_SINK_FIXED_S16LE)
