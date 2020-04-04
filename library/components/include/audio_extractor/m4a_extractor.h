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

#ifndef _M4A_EXTRACTOR_H_
#define _M4A_EXTRACTOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "msgutils/ringbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

// Return the data size obtained
typedef int (*m4a_fetch_cb)(char *buf, int wanted_size, long offset, void *fetch_priv);

typedef struct m4a_info {
    uint32_t    samplerate;
    uint32_t    channels;
    uint32_t    bits;
    uint32_t    buffersize;
    uint32_t    bitratemax;
    uint32_t    bitrateavg;
    uint32_t    framesamples;
    uint32_t    framesize;
    uint32_t    timescale;
    uint32_t    duration;

    uint32_t    mdatsize;
    uint32_t    mdatofs;

    uint32_t    stszsize;   /* How many stsz headers */
    uint32_t    stszofs;    /* Offset of stsz headers */
    uint32_t    stszmax;    /* Max frame size */
    uint16_t    *stszdata;  /* Buffer to store stsz frame header, NOTE: TODO: need free by caller */
    uint32_t    stszcurrent;

    // Audio Specific Config data:
    struct {
        uint8_t buf[15];
        uint8_t size;
    } asc;

    bool        firstparse;
    bool        moovtail;
    uint32_t    moovofs;
} m4a_info_t;

int m4a_parse_header(ringbuf_handle_t rb, m4a_info_t *info);

int m4a_extractor(m4a_fetch_cb fetch_cb, void *fetch_priv, m4a_info_t *info);

int m4a_build_adts_header(uint8_t *adts_buf, uint32_t adts_size, uint8_t *asc_buf, uint32_t asc_size, uint32_t frame_size);

#ifdef __cplusplus
}
#endif

#endif
