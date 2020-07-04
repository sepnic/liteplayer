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

#ifndef _LITEPLAYER_CONFIG_H_
#define _LITEPLAYER_CONFIG_H_

#include "msgutils/os_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

// player manager definations
#define DEFAULT_MANAGER_TASK_PRIO                ( OS_THREAD_PRIO_HIGH )
#define DEFAULT_MANAGER_TASK_STACKSIZE           ( 1024*4 )
#define DEFAULT_PLAYLIST_FILE_SUFFIX             ".playlist"
#define DEFAULT_PLAYLIST_URL_MAX                 ( 200 )
#define DEFAULT_PLAYLIST_BUFFER_SIZE             ( 1024*32 )

// stream mode definations
#define DEFAULT_STREAM_FIXED_URL                 "/websocket/tts.mp3"
#define DEFAULT_STREAM_FIXED_CODEC               ( AUDIO_CODEC_MP3 )
#define DEFAULT_STREAM_FIXED_SAMPLERATE          ( 16000 )
#define DEFAULT_STREAM_FIXED_CHANNELS            ( 1 )

// decoder element definations
#define DEFAULT_DECODER_TASK_PRIO                ( OS_THREAD_PRIO_HIGH )
#define DEFAULT_DECODER_TASK_STACKSIZE           ( 1024*16 )
#define DEFAULT_DECODER_RINGBUF_SIZE             ( 1024*32 )
#define DEFAULT_DECODER_BUFFER_SIZE              ( 512 )

// sink element definations
#define DEFAULT_SINK_TASK_PRIO                   ( OS_THREAD_PRIO_HARD_REALTIME )
#define DEFAULT_SINK_TASK_STACKSIZE              ( 1024*16 )
#define DEFAULT_SINK_RINGBUF_SIZE                ( 1024*8 )
#define DEFAULT_SINK_BUFFER_SIZE                 ( 1024*2 )
#define DEFAULT_SINK_OUT_RATE                    ( 48000 )
#define DEFAULT_SINK_OUT_CHANNELS                ( 2 )

// media source definations
#define DEFAULT_MEDIA_SOURCE_TASK_PRIO           ( OS_THREAD_PRIO_HIGH )
#define DEFAULT_MEDIA_SOURCE_TASK_STACKSIZE      ( 1024*6 )
#define DEFAULT_MEDIA_SOURCE_BUFFER_SIZE         ( 1024*8+1 )
#define DEFAULT_MEDIA_SOURCE_HTTP_RINGBUF_SIZE   ( 1024*128 )
#define DEFAULT_MEDIA_SOURCE_FILE_RINGBUF_SIZE   ( 1024*32 )
#define DEFAULT_MEDIA_SOURCE_STREAM_RINGBUF_SIZE ( 1024*32 )

// media parser definations
#define DEFAULT_MEDIA_PARSER_TASK_PRIO           ( OS_THREAD_PRIO_NORMAL )
#define DEFAULT_MEDIA_PARSER_TASK_STACKSIZE      ( 1024*8 )
#define DEFAULT_MEDIA_PARSER_BUFFER_SIZE         ( 1024*2 )

// socket upload definations
#define DEFAULT_SOCKET_UPLOAD_START              "GENIE_SOCKET_UPLOAD_START"
#define DEFAULT_SOCKET_UPLOAD_END                "GENIE_SOCKET_UPLOAD_END"
#define DEFAULT_SOCKET_UPLOAD_TASK_PRIO          ( OS_THREAD_PRIO_NORMAL )
#define DEFAULT_SOCKET_UPLOAD_TASK_STACKSIZE     ( 1024*2 )
#define DEFAULT_SOCKET_UPLOAD_RINGBUF_SIZE       ( 1024*256 )
#define DEFAULT_SOCKET_UPLOAD_WRITE_TIMEOUT      ( 2000 )

#ifdef __cplusplus
}
#endif

#endif