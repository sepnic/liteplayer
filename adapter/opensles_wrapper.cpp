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
#include <stdbool.h>
#include <string.h>
#include <list>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "cutils/os_memory.h"
#include "cutils/os_thread.h"
#include "cutils/os_time.h"
#include "cutils/os_logger.h"
#include "utils/Mutex.h"
#include "opensles_wrapper.h"

#define TAG "[liteplayer]opensles"

#define MIN_BUFFER_QUEUE_LEN 2

class OutBuffer {
public:
    OutBuffer(char *buf, int len) {
        data = new char[len];
        size = len;
        memcpy(data, buf, len);
    }

    ~OutBuffer() {
        delete [] data;
    }

    char *data;
    int size;
};

struct opensles_priv {
    SLObjectItf engineObj;
    SLEngineItf engineItf;
    SLObjectItf outmixObj;
    SLObjectItf playerObj;
    SLPlayItf   playerItf;
    SLAndroidSimpleBufferQueueItf playerBufferQueue;

    SLuint32 queueSize;
    std::list<OutBuffer *> *bufferList;
    msgutils::Mutex *bufferLock;
    bool hasStarted;
};

// this callback handler is called every time a buffer finishes playing
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    struct opensles_priv *priv = (struct opensles_priv *)context;
    msgutils::Mutex::Autolock _l(priv->bufferLock);

    // free the buffer that finishes playing
    if (!priv->bufferList->empty()) {
        delete priv->bufferList->front();
        priv->bufferList->pop_front();
        // notify writing-thread that the list has space to store new buffer
        priv->bufferLock->condSignal();
    }
}

sink_handle_t opensles_wrapper_open(int samplerate, int channels, void *sink_priv)
{
    OS_LOGD(TAG, "Opening OpenSLES: samplerate=%d, channels=%d", samplerate, channels);
    struct opensles_priv *priv = (struct opensles_priv *)OS_CALLOC(1, sizeof(struct opensles_priv));
    if (priv == NULL)
        return NULL;

    // todo: optimize queue-size to reduce latency and fix underrun
    priv->queueSize = MIN_BUFFER_QUEUE_LEN * channels;
    if (samplerate > 16000)
        priv->queueSize *= (samplerate/16000);

    SLresult result = SL_RESULT_SUCCESS;
    do {
        // create engine
        result = slCreateEngine(&priv->engineObj, 0, NULL, 0, NULL, NULL);
        if (SL_RESULT_SUCCESS != result) break;
        // realize the engine
        result = (*priv->engineObj)->Realize(priv->engineObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;
        // get the engine interface, which is needed in order to create other objects
        result = (*priv->engineObj)->GetInterface(priv->engineObj, SL_IID_ENGINE, &priv->engineItf);
        if (SL_RESULT_SUCCESS != result) break;
        // create output mix
        result = (*priv->engineItf)->CreateOutputMix(priv->engineItf, &priv->outmixObj,  0, NULL, NULL);
        if (SL_RESULT_SUCCESS != result) break;
        // realize the output mix
        result = (*priv->outmixObj)->Realize(priv->outmixObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;

        // configure audio sink
        SLDataLocator_OutputMix outmix = {SL_DATALOCATOR_OUTPUTMIX, priv->outmixObj};
        SLDataSink audioSnk = {&outmix, NULL};
        // configure audio source
        SLDataLocator_AndroidSimpleBufferQueue bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, priv->queueSize};
        SLDataFormat_PCM format_pcm = {
                SL_DATAFORMAT_PCM,
                static_cast<SLuint32>(channels),                 // channel count
                static_cast<SLuint32>(samplerate*1000),          // sample rate in mili second
                SL_PCMSAMPLEFORMAT_FIXED_16,                     // bitsPerSample
                SL_PCMSAMPLEFORMAT_FIXED_16,                     // containerSize
                channels == 1 ? SL_SPEAKER_FRONT_LEFT : (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT),
                SL_BYTEORDER_LITTLEENDIAN                        // endianness
        };
        SLDataSource audioSrc = {&bufq, &format_pcm};

        // create audio player
        const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
        const SLboolean req[] = {SL_BOOLEAN_TRUE};
        result = (*priv->engineItf)->CreateAudioPlayer(priv->engineItf, &priv->playerObj, &audioSrc, &audioSnk,
                sizeof(ids)/sizeof(SLInterfaceID), ids, req);
        if (SL_RESULT_SUCCESS != result) break;
        // realize the player
        result = (*priv->playerObj)->Realize(priv->playerObj, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) break;
        // get the play interface
        result = (*priv->playerObj)->GetInterface(priv->playerObj, SL_IID_PLAY, &priv->playerItf);
        if (SL_RESULT_SUCCESS != result) break;
        // get the buffer queue interface
        result = (*priv->playerObj)->GetInterface(priv->playerObj, SL_IID_BUFFERQUEUE, &priv->playerBufferQueue);
        if (SL_RESULT_SUCCESS != result) break;
        // register callback on the buffer queue
        result = (*priv->playerBufferQueue)->RegisterCallback(priv->playerBufferQueue, bqPlayerCallback, priv);
        if (SL_RESULT_SUCCESS != result) break;

        priv->bufferList = new std::list<OutBuffer *>();
        priv->bufferLock = new msgutils::Mutex();
    } while (0);

    if (SL_RESULT_SUCCESS == result) {
        return priv;
    }
    else {
        opensles_wrapper_close(priv);
        return NULL;
    }
}

int opensles_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    OS_LOGV(TAG, "Writing OpenSLES: buffer=%p, size=%d", buffer, size);
    struct opensles_priv *priv = (struct opensles_priv *)handle;
    OutBuffer *outbuf = new OutBuffer(buffer, size);

    msgutils::Mutex::Autolock _l(priv->bufferLock);

    // waiting the list is available
    while (priv->bufferList->size() >= priv->queueSize)
        priv->bufferLock->condWait();
    priv->bufferList->push_back(outbuf);

    SLresult result = (*priv->playerBufferQueue)->Enqueue(priv->playerBufferQueue, outbuf->data, outbuf->size);
    if (SL_RESULT_SUCCESS != result)
        return -1;

    if (!priv->hasStarted) {
        if (priv->bufferList->size() >= priv->queueSize) {
            priv->hasStarted = true;
            // set the player's state to playing
            result = (*priv->playerItf)->SetPlayState(priv->playerItf, SL_PLAYSTATE_PLAYING);
            if (SL_RESULT_SUCCESS != result)
                return -1;
        }
    }

    return size;
}

void opensles_wrapper_close(sink_handle_t handle)
{
    OS_LOGD(TAG, "closing OpenSLES");
    struct opensles_priv *priv = (struct opensles_priv *)handle;

    // waiting all buffers in the list finished playing
    {
        msgutils::Mutex::Autolock _l(priv->bufferLock);
        while (!priv->bufferList->empty())
            priv->bufferLock->condWait();
    }

    if (priv->bufferList != NULL) {
        for (auto iter =priv->bufferList->begin(); iter != priv->bufferList->end(); iter++) {
            delete *iter;
        }
        delete priv->bufferList;
    }

    if (priv->bufferLock != NULL)
        delete priv->bufferLock;

    if (priv->playerObj != NULL)
        (*priv->playerObj)->Destroy(priv->playerObj);
    if (priv->outmixObj != NULL)
        (*priv->outmixObj)->Destroy(priv->outmixObj);
    if (priv->engineObj != NULL)
        (*priv->engineObj)->Destroy(priv->engineObj);

    OS_FREE(priv);
}
