/*
 * Copyright 2019-2020 LUOYUN <sysu.zqlong@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "msgutils/os_thread.h"
#include "esp_adf/esp_log.h"
#include "esp_adf/audio_common.h"
#include "audio_extractor/aac_extractor.h"
#include "audio_extractor/m4a_extractor.h"

#define TAG "M4A_EXTRACTOR"

// FIXME: If low memory, please reduce STSZ_MAX_BUFFER, that will failed to parse for some m4a resource
#define STSZ_MAX_BUFFER       (128*1024)
#define STREAM_BUFFER_SIZE    (1024)

#define M4A_PARSER_TASK_PRIO  (OS_THREAD_PRIO_NORMAL)
#define M4A_PARSER_TASK_STACK (2048)

typedef enum aac_error {
    AAC_ERR_NONE          = -0x00,    /* no error */
    AAC_ERR_FAIL          = -0x01,    /* input buffer too small */
    AAC_ERR_UNSUPPORTED   = -0x02,    /* invalid (null) buffer pointer */
    AAC_ERR_NOMEM         = -0x03,    /* not enough memory */
    AAC_ERR_OPCODE        = -0x04,    /* opcode error */
    AAC_ERR_STARVE_0      = -0x05,    /* no data remaining, need more data */
    AAC_ERR_STARVE_1      = -0x06,    /* still have data left but not enough for continue handling */
    AAC_ERR_STARVE_2      = -0x07,    /* ATOM_DATA finish, no data remaining, need more data to process ATOM_NAME. */
    AAC_ERR_LOSTSYNC      = -0x08,    /* lost synchronization */
    AAC_ERR_AGAIN         = -0x09,    /* try again */
    AAC_ERR_EOF           = -0x0A,    /* EOF */
} AAC_ERR_T;

enum ATOM_TYPE {
    ATOM_STOP = 0,      /* end of atoms */
    ATOM_NAME,          /* plain atom */
    ATOM_DESCENT,       /* starts group of children */
    ATOM_ASCENT,        /* ends group */
    ATOM_DATA,
};

struct atom_box {
    int opcode;
    void *data;
};

struct atom_parser {
    ringbuf_handle_t    rb;
    uint8_t             data[STREAM_BUFFER_SIZE];
    uint32_t            offset;
    struct atom_box    *atom;
    m4a_info_t         *m4a_info;
};
typedef struct atom_parser *atom_parser_handle_t;

/* Swap bytes in 16 bit value.  */
static inline unsigned short bswap16(unsigned short __bsx){
    return ((((__bsx) >> 8) & 0xff) | (((__bsx) & 0xff) << 8));
}

/* Swap bytes in 32 bit value.  */
static inline unsigned int bswap32(unsigned int __bsx) {
    return ((((__bsx) & 0xff000000) >> 24) | (((__bsx) & 0x00ff0000) >>  8) |
            (((__bsx) & 0x0000ff00) <<  8) | (((__bsx) & 0x000000ff) << 24));
}

static inline uint32_t u32in(char *buf)
{
    uint32_t u32 = *((uint32_t *)buf);
    return bswap32(u32);
}

static inline uint16_t u16in(char *buf)
{
    uint16_t u16 = *((uint16_t*)buf);
    return bswap16(u16);
}

static inline uint8_t u8in(char *buf)
{
    uint8_t u8 = *((uint8_t*)buf);
    return (u8);
}

static inline void datain(uint8_t *buf_out, char *buf_in, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        buf_out[i] = u8in(buf_in); buf_in += 1;
    }
}

static int32_t atom_rb_read(atom_parser_handle_t handle, int32_t wanted_size)
{
    int32_t byte_read = 0;

    while (wanted_size > STREAM_BUFFER_SIZE) {
        byte_read = rb_read(handle->rb, (char *)handle->data, STREAM_BUFFER_SIZE, AUDIO_MAX_DELAY);
        if (byte_read < 0) {
            ESP_LOGE(TAG, "Failed to read rb, ret=%d", byte_read);
            return byte_read;
        } else {
            handle->offset += byte_read;
            wanted_size -= byte_read;
        }
    }

    if (wanted_size != 0) {
        byte_read = rb_read(handle->rb, (char *)handle->data, wanted_size, AUDIO_MAX_DELAY);
        if (byte_read < 0) {
            ESP_LOGE(TAG, "Failed to read rb, ret=%d", byte_read);
            return byte_read;
        } else {
            handle->offset += byte_read;
        }
    }

    return 0;
}

static AAC_ERR_T dummyin(atom_parser_handle_t handle, uint32_t atom_size)
{
    return atom_rb_read(handle, atom_size);
}

static AAC_ERR_T mdhdin(atom_parser_handle_t handle, uint32_t atom_size)
{
    char *buf = (char *)handle->data;
    m4a_info_t *m4a_info = handle->m4a_info;

    uint16_t wanted_byte = 6*sizeof(uint32_t);
    int32_t ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // version/flags
    u32in(buf); buf += 4;
    // Creation time
    u32in(buf); buf += 4;
    // Modification time
    u32in(buf); buf += 4;
    // Time scale
    m4a_info->timescale = u32in(buf); buf += 4;
    m4a_info->samplerate = m4a_info->timescale; // fixup after AudioSpecificConfig
    // Duration
    m4a_info->duration = u32in(buf); buf += 4;
    // Language
    u16in(buf); buf += 2;
    // pre_defined
    u16in(buf); buf += 2;

    if (atom_size > wanted_byte)
        return atom_rb_read(handle, atom_size-wanted_byte);
    else
        return AAC_ERR_NONE;
}

static AAC_ERR_T hdlr1in(atom_parser_handle_t handle, uint32_t atom_size)
{
    char *buf = (char *)handle->data;
    uint16_t wanted_byte = 6*sizeof(uint32_t);

    int32_t ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // version/flags
    u32in(buf); buf += 4;
    // Component type
    u32in(buf); buf += 4;

    // Component subtype
    uint8_t subtype[4] = {0};
    datain(subtype, buf, 4); buf += 4;
    if (memcmp("soun", subtype, 4) != 0) {
        ESP_LOGE(TAG, "hdlr error, expect subtype is soun, subtype=%s", subtype);
        return AAC_ERR_UNSUPPORTED;
    }

    // reserved
    u32in(buf); buf += 4;
    u32in(buf); buf += 4;
    u32in(buf); buf += 4;

    if (atom_size > wanted_byte)
        return atom_rb_read(handle, atom_size-wanted_byte);
    else
        return AAC_ERR_NONE;
}

static AAC_ERR_T stsdin(atom_parser_handle_t handle, uint32_t atom_size)
{
    char *buf = (char *)handle->data;
    uint16_t wanted_byte = 2*sizeof(uint32_t);

    int32_t ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // version/flags
    u32in(buf); buf += 4;

    uint32_t entries = u32in(buf); buf += 4;
    if (entries != 1) {
        ESP_LOGE(TAG, "stsd error, number of entries should be 1, entries=%d", entries);
        return AAC_ERR_UNSUPPORTED;
    }
    return AAC_ERR_NONE;
};

static AAC_ERR_T mp4ain(atom_parser_handle_t handle, uint32_t atom_size)
{
    char *buf = (char *)handle->data;
    m4a_info_t *m4a_info = handle->m4a_info;

    uint16_t wanted_byte = 7*sizeof(uint32_t);
    int32_t ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // Reserved (6 bytes)
    u32in(buf); buf += 4;
    u16in(buf); buf += 2;
    // Data reference index
    u16in(buf); buf += 2;
    // Version
    u16in(buf); buf += 2;
    // Revision level
    u16in(buf); buf += 2;
    // Vendor
    u32in(buf); buf += 4;
    // Number of channels
    m4a_info->channels = u16in(buf); buf += 2; // fixup after AudioSpecificConfig
    // Sample size (bits)
    m4a_info->bits = u16in(buf); buf += 2;
    // Compression ID
    u16in(buf); buf += 2;
    // Packet size
    u16in(buf); buf += 2;
    // Sample rate (16.16)
    // fractional framerate, probably not for audio
    // rate integer part
    u16in(buf); buf += 2;
    // rate reminder part
    u16in(buf); buf += 2;

    return AAC_ERR_NONE;
}

static uint32_t getsize(char *buf, uint8_t *read)
{
    uint8_t cnt = 0;
    uint32_t size = 0;
    for (cnt = 0; cnt < 4; cnt++) {
        uint8_t tmp = u8in(buf); buf += 1;

        size <<= 7;
        size |= (tmp & 0x7F);
        if (!(tmp & 0x80)) {
            break;
        }
    }
    *read = cnt + 1;
    return size;
}

static AAC_ERR_T esdsin(atom_parser_handle_t handle, uint32_t atom_size)
{
    // descriptor tree:
    // MP4ES_Descriptor
    //   MP4DecoderConfigDescriptor
    //      MP4DecSpecificInfoDescriptor
    //   MP4SLConfigDescriptor
    enum {
        MP4ESDescrTag = 3,
        MP4DecConfigDescrTag = 4,
        MP4DecSpecificDescrTag = 5,
        MP4SLConfigDescrTag = 6,
    };

    uint8_t read = 0;
    char *buf = (char *)handle->data;
    m4a_info_t *m4a_info = handle->m4a_info;

    int32_t ret = atom_rb_read(handle, atom_size);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // version/flags
    u32in(buf); buf += 4;
    
    if (u8in(buf) != MP4ESDescrTag) {  // 1
        return AAC_ERR_FAIL;
    }
    buf += 1;

    getsize(buf, &read); buf += read;

    // ESID
    u16in(buf); buf += 2;
    // flags(url(bit 6); ocr(5); streamPriority (0-4)):
    u8in(buf); buf += 1;

    if (u8in(buf) != MP4DecConfigDescrTag) { // 1
        return AAC_ERR_FAIL;
    }
    buf += 1;

    getsize(buf, &read); buf += read;
    if (u8in(buf) != 0x40) { /* not MPEG-4 audio */ // 1
        return AAC_ERR_FAIL;
    }
    buf += 1;

    // stream type
    u8in(buf); buf += 1;
    // buffer size db (24 bits)
    m4a_info->buffersize = u16in(buf) << 8; buf += 2;
    m4a_info->buffersize |= u8in(buf); buf += 1;
    // bitrate
    m4a_info->bitratemax = u32in(buf); buf += 4;
    m4a_info->bitrateavg = u32in(buf); buf += 4;

    if (u8in(buf) != MP4DecSpecificDescrTag) {// 1
        return AAC_ERR_FAIL;
    }
    buf += 1;

    m4a_info->asc.size = getsize(buf, &read); buf += read;
    if (m4a_info->asc.size > sizeof(m4a_info->asc.buf)) {
        return AAC_ERR_FAIL;
    }

    // get AudioSpecificConfig
    datain(m4a_info->asc.buf, buf, m4a_info->asc.size); // max 16
    buf += m4a_info->asc.size;

    if (u8in(buf) != MP4SLConfigDescrTag) { // 1
        return AAC_ERR_FAIL;
    }
    buf += 1;

    getsize(buf, &read); buf += read;

    // "predefined" (no idea)
    u8in(buf); buf += 1;

    return AAC_ERR_NONE;
}

static AAC_ERR_T sttsin(atom_parser_handle_t handle, uint32_t atom_size)
{
    if (atom_size < 16) {   //min stts size
        return AAC_ERR_FAIL;
    }
    return atom_rb_read(handle, atom_size);
}

static AAC_ERR_T stszin(atom_parser_handle_t handle, uint32_t atom_size)
{
    m4a_info_t *m4a_info = handle->m4a_info;
    uint16_t wanted_byte = 3*sizeof(uint32_t);
    char *buf            = (char *)handle->data;
    int32_t ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // version/flags
    u32in(buf); buf += 4;
    // Sample size
    u32in(buf); buf += 4;
    // Number of entries
    m4a_info->stszsize = u32in(buf);  buf += 4;
    m4a_info->stszofs  = handle->offset;

    /**
    * To save memeory, we assuem all frame size is 16bit width(not bigger than 0xFFFF)
    * So the moment we got frame count(stszsize), we'll check how many memory is needed
    * to store stsz header. And return fail if bigger than default buffer.
    */
    if (m4a_info->stszsize*sizeof(int16_t) > STSZ_MAX_BUFFER) {
        ESP_LOGE(TAG, "Large STSZ(%u), out of memory", m4a_info->stszsize*sizeof(int16_t));
        return AAC_ERR_NOMEM;
    }

    uint32_t frame_size = 0;
    m4a_info->stszdata = (uint16_t*)audio_calloc(m4a_info->stszsize, sizeof(uint16_t));
    for (int32_t cnt = 0; cnt < m4a_info->stszsize; cnt++) {
        ret = atom_rb_read(handle, sizeof(uint32_t));
        AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

        uint32_t u32 = *(uint32_t*)(handle->data);
        frame_size = bswap32(u32);
        if (m4a_info->stszmax < frame_size) {
            m4a_info->stszmax = frame_size;
        }
        m4a_info->stszdata[cnt] = frame_size & 0xFFFF;
    }

    ESP_LOGV(TAG, "STSZ max frame size: %u", m4a_info->stszmax);
    if (m4a_info->stszmax > 0xFFFF) {
        return AAC_ERR_UNSUPPORTED;
    }

    return AAC_ERR_NONE;
}

static AAC_ERR_T stcoin(atom_parser_handle_t handle, uint32_t atom_size)
{
    char *buf = (char *)handle->data;
    m4a_info_t *m4a_info = handle->m4a_info;
    uint16_t wanted_byte = 3*sizeof(uint32_t);

    int32_t ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    // version/flags
    u32in(buf); buf += 4;
    // Number of entries
    uint32_t entries = u32in(buf); buf += 4;
    if (entries < 1) {
        ESP_LOGE(TAG, "stco error, number of entries should > 1, entries=%d", entries);
        return AAC_ERR_UNSUPPORTED;
    }

    // first chunk offset
    m4a_info->mdatofs = u32in(buf); buf += 4;
    return AAC_ERR_NONE;
    //return atom_rb_read(handle, atom_size-wanted_byte);
}

static AAC_ERR_T atom_parse(atom_parser_handle_t handle)
{
    char *buf = NULL;
    int32_t ret = AAC_ERR_NONE;

    if (handle->atom->opcode == ATOM_DESCENT) {
        ESP_LOGV(TAG, "Atom is descent");
        return AAC_ERR_NONE;
    } else if (handle->atom->opcode == ATOM_ASCENT) {
        ESP_LOGV(TAG, "Atom is ascent");
        return AAC_ERR_NONE;
    }

    if (handle->atom->opcode != ATOM_NAME) {
        ESP_LOGE(TAG, "Invalid opcode, expect ATOM_NAME");
        return AAC_ERR_OPCODE;
    } else {
        ESP_LOGV(TAG, "Looking for '%s' at offset[%u]", (char *)handle->atom->data, handle->offset);
    }

_next_atom:
    buf = (char *)handle->data;
    ret = atom_rb_read(handle, 8);
    AUDIO_ERR_CHECK(TAG, ret == 0, return ret);

    uint8_t atom_name[4] = {0};
    uint32_t atom_size = 0;

    atom_size = u32in(buf); buf += 4;
    datain(atom_name, buf, 4); buf += 4;

    ESP_LOGV(TAG, "atom[%s], size[%u], offset[%u]", atom_name, atom_size, handle->offset);
    if (memcmp(atom_name, handle->atom->data, sizeof(atom_name)) == 0) {
        ESP_LOGV(TAG, "----OK----");
        goto atom_found;
    }
    else {
        if (atom_size > 8) {
            ret = atom_rb_read(handle, atom_size-8);
            AUDIO_ERR_CHECK(TAG, ret == 0, return ret);
        }
        goto _next_atom;
    }

atom_found:
    handle->atom++;
    if (handle->atom->opcode == ATOM_DESCENT) {
        ESP_LOGV(TAG, "Atom is descent");
        return AAC_ERR_NONE;
    }

    if (handle->atom->opcode != ATOM_DATA) {
        ESP_LOGE(TAG, "Invalid opcode, expect ATOM_DATA");
        return AAC_ERR_OPCODE;
    }

    int32_t err = ((AAC_ERR_T(*)(atom_parser_handle_t, uint32_t))handle->atom->data)(handle, atom_size-8);
    return err;
}

static AAC_ERR_T moovin(atom_parser_handle_t handle, uint32_t atom_size)
{
    AAC_ERR_T err;

    static struct atom_box mvhd[] = {
        {ATOM_NAME, "mvhd"},
        {ATOM_DATA, dummyin},
    };
    static struct atom_box trak[] = {
        {ATOM_NAME, "trak"},
        {ATOM_DESCENT, NULL},
        {ATOM_NAME, "tkhd"},
        {ATOM_DATA, dummyin},
        {ATOM_NAME, "mdia"},
        {ATOM_DESCENT, NULL},
        {ATOM_NAME, "mdhd"},
        {ATOM_DATA, mdhdin},
        {ATOM_NAME, "hdlr"},
        {ATOM_DATA, hdlr1in},
        {ATOM_NAME, "minf"},
        {ATOM_DESCENT, NULL},
        {ATOM_NAME, "smhd"},
        {ATOM_DATA, dummyin},
        {ATOM_NAME, "dinf"},
        {ATOM_DATA, dummyin},
        {ATOM_NAME, "stbl"},
        {ATOM_DESCENT, NULL},
        {ATOM_NAME, "stsd"},
        {ATOM_DATA, stsdin},
        {ATOM_DESCENT, NULL},
        {ATOM_NAME, "mp4a"},
        {ATOM_DATA, mp4ain},
        {ATOM_DESCENT, NULL},
        {ATOM_NAME, "esds"},
        {ATOM_DATA, esdsin},
        {ATOM_ASCENT, NULL},
        {ATOM_ASCENT, NULL},
        {ATOM_NAME, "stts"},
        {ATOM_DATA, sttsin},
        {ATOM_NAME, "stsc"},
        {ATOM_DATA, dummyin},
        {ATOM_NAME, "stsz"},
        {ATOM_DATA, stszin},
        {ATOM_NAME, "stco"},
        {ATOM_DATA, stcoin},
        {0}
    };

    handle->atom = mvhd;
    err = atom_parse(handle);
    if (err != AAC_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to parse mvhd atom");
        return err;
    }

    handle->atom = trak;
    while (1) {
        if (handle->atom->opcode == 0){
            ESP_LOGV(TAG, "Finisehd to parse trak atom");
            break;
        }

        err = atom_parse(handle);
        if (err != AAC_ERR_NONE) {
            ESP_LOGE(TAG, "Failed to parse trak atom");
            return err;
        }

        handle->atom++;
    }

    return AAC_ERR_NONE;
}

static int m4a_parse_asc(m4a_info_t *m4a_info)
{
    if (m4a_info->asc.size >= 2) {
        static const uint32_t sample_rates[] = {
            96000, 88200, 64000, 48000, 44100, 32000,
            24000, 22050, 16000, 12000, 11025, 8000
        };
        uint16_t config = (m4a_info->asc.buf[0] << 8 | m4a_info->asc.buf[1]);
        uint8_t sample_rate_index = (config >> 7) & 0x0f;
        uint8_t channels_num = (config >> 3) & 0x07;
        if (sample_rate_index < 12) {
            m4a_info->samplerate = sample_rates[sample_rate_index];
            m4a_info->channels = channels_num;
            return AAC_ERR_NONE;
        }
    }
    return AAC_ERR_FAIL;
}

static void m4a_dump_info(m4a_info_t *m4a_info)
{
    ESP_LOGD(TAG, "M4A INFO:");
    ESP_LOGD(TAG, "  >Channels          : %u", m4a_info->channels);
    ESP_LOGD(TAG, "  >Sampling rate     : %u", m4a_info->samplerate);
    ESP_LOGD(TAG, "  >Bits per sample   : %u", m4a_info->bits);
    ESP_LOGD(TAG, "  >Buffer size       : %u", m4a_info->buffersize);
    ESP_LOGD(TAG, "  >Max bitrate       : %u", m4a_info->bitratemax);
    ESP_LOGD(TAG, "  >Average bitrate   : %u", m4a_info->bitrateavg);
    ESP_LOGD(TAG, "  >Frames            : %u", m4a_info->stszsize);
    ESP_LOGD(TAG, "  >ASC buff          : %x:%x:%x:%x:%x:%x:%x",
             m4a_info->asc.buf[0], m4a_info->asc.buf[1], m4a_info->asc.buf[2],
             m4a_info->asc.buf[3], m4a_info->asc.buf[4], m4a_info->asc.buf[5],
             m4a_info->asc.buf[6]);
    ESP_LOGD(TAG, "  >ASC size          : %u", m4a_info->asc.size);
    ESP_LOGD(TAG, "  >Duration          : %.1f sec", (float)m4a_info->duration/m4a_info->timescale);
    ESP_LOGD(TAG, "  >Data offset/size  : %u/%u", m4a_info->mdatofs, m4a_info->mdatsize);
}

static AAC_ERR_T m4a_check_header(atom_parser_handle_t handle)
{
    char *buf = (char *)handle->data;
    uint32_t pos = 0;
    uint32_t atom_size = 0;
    uint8_t atom_name[4] = {0};
    //m4a_info_t *m4a_info = handle->m4a_info;

    uint16_t wanted_byte = 2*sizeof(uint32_t);
    int32_t ret = 0;

    handle->m4a_info->firstparse = true;

    ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return AAC_ERR_FAIL);

    atom_size = u32in(buf); buf += 4;
    datain(atom_name, buf, 4); buf += 4;

    ESP_LOGV(TAG, "atom[%s], size[%u], offset[%u]", atom_name, atom_size, handle->offset);
    if (memcmp(atom_name, "ftyp", 4) != 0) {
        ESP_LOGE(TAG, "Not M4A audio");
        return AAC_ERR_UNSUPPORTED;
    }

next_atom:
    pos += atom_size;

    if (atom_size > 8) {
        ret = atom_rb_read(handle, atom_size-8);
        AUDIO_ERR_CHECK(TAG, ret == 0, return AAC_ERR_FAIL);
    }

    buf = (char *)handle->data;
    wanted_byte = 2*sizeof(uint32_t);
    ret = atom_rb_read(handle, wanted_byte);
    AUDIO_ERR_CHECK(TAG, ret == 0, return AAC_ERR_FAIL);

    atom_size = u32in(buf); buf += 4;
    datain(atom_name, buf, 4); buf += 4;

    ESP_LOGV(TAG, "atom[%s], size[%u], offset[%u]", atom_name, atom_size, handle->offset);
    if (memcmp(atom_name, "mdat", 4) == 0) {
        ESP_LOGV(TAG, "moov behide of mdat: mdatofs=%u, mdatsize=%u", pos, atom_size);
        handle->m4a_info->mdatsize = atom_size;
        handle->m4a_info->mdatofs = pos;
        handle->m4a_info->moovofs = handle->m4a_info->mdatofs + handle->m4a_info->mdatsize;
        handle->m4a_info->moovtail = true;
        rb_reset(handle->rb);
        return AAC_ERR_AGAIN;
    } else if (memcmp(atom_name, "moov", 4) == 0) {
        ESP_LOGV(TAG, "moov ahead of mdat");
        handle->m4a_info->moovtail = false;
        return moovin(handle, 8);
    } else {
        goto next_atom;
    }

    return AAC_ERR_FAIL;
}

int m4a_parse_header(ringbuf_handle_t rb, m4a_info_t *info)
{
    static struct atom_box moov[] = {
        {ATOM_NAME, "moov"},
        {ATOM_DATA, moovin},
        {0}
    };

    struct atom_parser parser = {
        .rb = rb,
        .offset = 0,
        .atom = NULL,
        .m4a_info = info,
    };
    AAC_ERR_T err = AAC_ERR_FAIL;

    if (info->firstparse) {
        if (info->moovtail) {
            parser.offset = info->moovofs;
        }
        else {
            ESP_LOGE(TAG, "Failed to check moov");
            goto finish;
        }
    }
    else {
        err = m4a_check_header(&parser);
        if (err == AAC_ERR_AGAIN) {
            ESP_LOGV(TAG, "moov behide of mdat, please check again with new offset(%u)", info->moovofs);
        }
        goto finish;
    }

    /* Start to parse moov header */
    parser.atom = moov;
    err = atom_parse(&parser);
    AUDIO_ERR_CHECK(TAG, err == AAC_ERR_NONE, goto finish);

finish:
    if (err == AAC_ERR_NONE) {
        err = m4a_parse_asc(info);
        m4a_dump_info(info);
    }

    rb_done_read(rb);
    return err;
}

struct m4a_reader_priv {
    m4a_info_t *info;
    ringbuf_handle_t rb;
    int ret;
};

static void *m4a_reader_parse_thread(void *arg)
{
    struct m4a_reader_priv *priv = (struct m4a_reader_priv *)arg;
    priv->ret = m4a_parse_header(priv->rb, priv->info);
    return NULL;
}

int m4a_extractor(m4a_fetch_cb fetch_cb, void *fetch_priv, m4a_info_t *info)
{
    ringbuf_handle_t rb_atom = NULL;
    os_thread_t tid = NULL;
    char buffer[STREAM_BUFFER_SIZE];
    int bytes_writen, bytes_read, offset = 0;
    bool double_check = false;

    rb_atom = rb_create(STREAM_BUFFER_SIZE);
    if (rb_atom == NULL)
        return AAC_ERR_FAIL;

    struct m4a_reader_priv priv = {
        .info = info,
        .rb = rb_atom,
        .ret = AAC_ERR_FAIL,
    };

    struct os_threadattr tattr = {
        .name = "ael_m4aparser",
        .priority = M4A_PARSER_TASK_PRIO,
        .stacksize = M4A_PARSER_TASK_STACK,
        .joinable = true,
    };

m4a_parse:
    tid = OS_THREAD_CREATE(&tattr, m4a_reader_parse_thread, &priv);
    if (tid == NULL) {
        ESP_LOGE(TAG, "Failed to create task to parse m4a");
        goto m4a_finish;
    }

    do {
        bytes_read = fetch_cb(buffer, sizeof(buffer), offset, fetch_priv);
        if (bytes_read <= 0)
            break;

        offset += bytes_read;
        bytes_writen = 0;

        do {
            bytes_writen = rb_write(rb_atom, &buffer[bytes_writen], bytes_read, AUDIO_MAX_DELAY);
            if (bytes_writen > 0) {
                bytes_read -= bytes_writen;
            }
            else {
                if (bytes_writen == RB_DONE) {
                    ESP_LOGV(TAG, "RB done write");
                }
                else if(bytes_writen == RB_ABORT) {
                    ESP_LOGV(TAG, "RB abort write");
                }
                else {
                    ESP_LOGW(TAG, "RB write fail, ret=%d", bytes_writen);
                }
                goto m4a_writen;
            }
        } while(bytes_read > 0);
    } while (1);

m4a_writen:
    rb_done_write(rb_atom);
    OS_THREAD_JOIN(tid, NULL);

    if (!double_check) {
        double_check = true;
        if (priv.ret == AAC_ERR_AGAIN && info->firstparse && info->moovtail) {
            rb_reset(rb_atom);
            offset = info->moovofs;
            goto m4a_parse;
        }
    }

m4a_finish:
    rb_destroy(rb_atom);
    return priv.ret;
}
