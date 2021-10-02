// Copyright (c) 2019-2021 Qinglong<sysu.zqlong@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _MP3_EXTRACTOR_H_
#define _MP3_EXTRACTOR_H_

#ifdef __cplusplus
extern "C" {
#endif

// Return the data size obtained
typedef int (*mp3_fetch_cb)(char *buf, int wanted_size, long offset, void *fetch_priv);

struct mp3_info {
    int channels;
    int sample_rate;
    int bit_rate;
    int frame_size;
    int frame_start_offset;
};

int mp3_find_syncword(char *buf, int size);

int mp3_parse_header(char *buf, int buf_size, struct mp3_info *info);

int mp3_extractor(mp3_fetch_cb fetch_cb, void *fetch_priv, struct mp3_info *info);

#ifdef __cplusplus
}
#endif

#endif
