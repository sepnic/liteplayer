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
#include <string.h>

#include "esp_adf/esp_log.h"
#include "httpstub_wrapper.h"

#define TAG "httpwrapper"

http_handle_t httpstub_wrapper_open(const char *url, long long content_pos, void *priv)
{
    FILE *file = fopen(url, "rb");
    if (file != NULL)
        fseek(file, (long)content_pos, SEEK_SET);
    return NULL;
}

int httpstub_wrapper_read(http_handle_t handle, char *buffer, int size)
{
    FILE *file = (FILE *)handle;
    return fread(buffer, 1, size, file);
}

long long httpstub_wrapper_filesize(http_handle_t handle)
{
    FILE *file = (FILE *)handle;
    long current = ftell(file);
    long end = 0;
    fseek(file, 0, SEEK_END);
    end = ftell(file);
    fseek(file, current, SEEK_SET);
    return end;
}

int httpstub_wrapper_seek(http_handle_t handle, long offset)
{
    FILE *file = (FILE *)handle;
    return fseek(file, offset, SEEK_SET);
}

void httpstub_wrapper_close(http_handle_t handle)
{
    FILE *file = (FILE *)handle;
    fclose(file);
}
