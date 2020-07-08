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
#include <stdint.h>
#include <string.h>

#include "msgutils/os_memory.h"
#include "msgutils/os_time.h"
#include "msgutils/os_logger.h"
#include "opensles_wrapper.h"

#define TAG "[liteplayer]opensles"

struct opensles_priv {
    int samplerate;
    int channels;
};

sink_handle_t opensles_wrapper_open(int samplerate, int channels, void *sink_priv)
{
    struct opensles_priv *priv = OS_CALLOC(1, sizeof(struct opensles_priv));
    if (priv == NULL)
        return NULL;

    OS_LOGD(TAG, "Opening OpenSLES: samplerate=%d, channels=%d", samplerate, channels);

    // todo
    priv->samplerate = samplerate;
    priv->channels = channels;

    return priv;
}

int opensles_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    OS_LOGD(TAG, "Writing OpenSLES: buffer=%d, size=%d", buffer, size);
    // todo
    return size;
}

void opensles_wrapper_close(sink_handle_t handle)
{
    struct wave_priv *priv = (struct wave_priv *)handle;
    OS_LOGD(TAG, "closing OpenSLES");
    // todo
    OS_FREE(priv);
}
