/*
 * Copyright (C) 2019-2023 Qinglong<sysu.zqlong@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* This is a JNI example where we use native methods to play music
 * using the native liteplayer* APIs.
 * See the corresponding Java source file located at:
 *
 *   src/com/example/liteplayerdemo/Liteplayer.java
 *
 * In this example we use assert() for "impossible" error conditions,
 * and explicit handling and recovery for more likely error conditions.
 */

#include <jni.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <android/log.h>

#include "liteplayer_main.h"
#include "liteplayer_adapter.h"
#include "source_file_wrapper.h"
#include "source_httpclient_wrapper.h"
#include "sink_opensles_wrapper.h"

#define TAG "LiteplayerJNI"

//#define ENABLE_OPENSLES

#define JAVA_CLASS_NAME "com/sepnic/liteplayer/Liteplayer"
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

#define OS_LOGF(tag, ...) __android_log_print(ANDROID_LOG_FATAL,tag, __VA_ARGS__)
#define OS_LOGE(tag, ...) __android_log_print(ANDROID_LOG_ERROR, tag, __VA_ARGS__)
#define OS_LOGW(tag, ...) __android_log_print(ANDROID_LOG_WARN, tag, __VA_ARGS__)
#define OS_LOGI(tag, ...) __android_log_print(ANDROID_LOG_INFO, tag, __VA_ARGS__)
#define OS_LOGD(tag, ...) __android_log_print(ANDROID_LOG_DEBUG, tag, __VA_ARGS__)
#define OS_LOGV(tag, ...) //__android_log_print(ANDROID_LOG_VERBOSE, tag, __VA_ARGS__)

class liteplayer_jni {
public:
    liteplayer_jni()
      : mPlayerhandle(nullptr), mOnStateChanged(nullptr), mClass(nullptr), mObject(nullptr) {}
    ~liteplayer_jni() = default;
    liteplayer_handle_t mPlayerhandle;
    jmethodID   mOnStateChanged;
#if !defined(ENABLE_OPENSLES)
    jmethodID   mOnPcmOpen;
    jmethodID   mOnPcmWrite;
    jmethodID   mOnPcmClose;
#endif
    jclass      mClass;
    jobject     mObject;
};

static JavaVM *sJavaVM = nullptr;

static void jniThrowException(JNIEnv *env, const char *className, const char *msg) {
    jclass clazz = env->FindClass(className);
    if (!clazz) {
        OS_LOGE(TAG, "Unable to find exception class %s", className);
        /* ClassNotFoundException now pending */
        return;
    }
    if (env->ThrowNew(clazz, msg) != JNI_OK) {
        OS_LOGE(TAG, "Failed throwing '%s' '%s'", className, msg);
        /* an exception, most likely OOM, will now be pending */
    }
    env->DeleteLocalRef(clazz);
}

static bool jniGetEnv(JNIEnv **env, jboolean *attached)
{
    jint res = sJavaVM->GetEnv((void**) env, JNI_VERSION_1_6);
    if (res != JNI_OK) {
        res = sJavaVM->AttachCurrentThread(env, nullptr);
        if (res != JNI_OK) {
            OS_LOGE(TAG, "Failed to AttachCurrentThread, errcode=%d", res);
            return false;
        }
        *attached = JNI_TRUE;
    }
    return true;
}

#if !defined(ENABLE_OPENSLES)
static const char *audiotrack_wrapper_name()
{
    return "audiotrack";
}

static sink_handle_t audiotrack_wrapper_open(int samplerate, int channels, int bits, void *sink_priv)
{
    OS_LOGD(TAG, "Opening AudioTrack: samplerate=%d, channels=%d, bits=%d", samplerate, channels, bits);

    JNIEnv *env;
    jboolean attached = JNI_FALSE;
    if (!jniGetEnv(&env, &attached))
        return nullptr;

    auto player = reinterpret_cast<liteplayer_jni *>(sink_priv);
    jint res = env->CallStaticIntMethod(player->mClass, player->mOnPcmOpen, player->mObject, samplerate, channels, bits);

    if (attached)
        sJavaVM->DetachCurrentThread();
    return res == 0 ? (sink_handle_t)player : nullptr;
}

static int audiotrack_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    OS_LOGV(TAG, "Writing AudioTrack: buffer=%p, size=%d", buffer, size);

    JNIEnv *env;
    jboolean attached = JNI_FALSE;
    if (!jniGetEnv(&env, &attached))
        return -1;

    jbyteArray byteArray = env->NewByteArray(size);
    env->SetByteArrayRegion(byteArray, 0, size, reinterpret_cast<const jbyte *>(buffer));

    auto player = reinterpret_cast<liteplayer_jni *>(handle);
    env->CallStaticIntMethod(player->mClass, player->mOnPcmWrite, player->mObject, byteArray, size);

    env->DeleteLocalRef(byteArray);
    if (attached)
        sJavaVM->DetachCurrentThread();
    return size;
}

static void audiotrack_wrapper_close(sink_handle_t handle)
{
    OS_LOGD(TAG, "Closing AudioTrack");

    JNIEnv *env;
    jboolean attached = JNI_FALSE;
    if (!jniGetEnv(&env, &attached))
        return;

    auto player = reinterpret_cast<liteplayer_jni *>(handle);
    env->CallStaticVoidMethod(player->mClass, player->mOnPcmClose, player->mObject);

    if (attached)
        sJavaVM->DetachCurrentThread();
}
#endif

static int Liteplayer_native_stateCallback(enum liteplayer_state state, int errcode, void *callback_priv)
{
    OS_LOGD(TAG, "Liteplayer_native_stateCallback: state=%d, errcode=%d", state, errcode);

    JNIEnv *env;
    jboolean attached = JNI_FALSE;
    if (!jniGetEnv(&env, &attached))
        return -1;

    auto player = reinterpret_cast<liteplayer_jni *>(callback_priv);
    env->CallStaticVoidMethod(player->mClass, player->mOnStateChanged, player->mObject, state, errcode);

    if (attached)
        sJavaVM->DetachCurrentThread();
    return 0;
}

static jlong Liteplayer_native_create(JNIEnv* env, jobject thiz, jobject weak_this)
{
    OS_LOGD(TAG, "Liteplayer_native_create");

    auto *player = new liteplayer_jni();

    jclass clazz;
    clazz = env->FindClass(JAVA_CLASS_NAME);
    if (clazz == nullptr) {
        OS_LOGE(TAG, "Failed to find class: %s", JAVA_CLASS_NAME);
        delete player;
        return (jlong)nullptr;
    }
    player->mOnStateChanged = env->GetStaticMethodID(clazz, "onStateChangedFromNative", "(Ljava/lang/Object;II)V");
    if (player->mOnStateChanged == nullptr) {
        OS_LOGE(TAG, "Failed to get onStateChangedFromNative mothod");
        delete player;
        return (jlong)nullptr;
    }
#if !defined(ENABLE_OPENSLES)
    player->mOnPcmOpen = env->GetStaticMethodID(clazz, "onPcmOpenFromNative", "(Ljava/lang/Object;III)I");
    if (player->mOnPcmOpen == nullptr) {
        OS_LOGE(TAG, "Failed to get onPcmOpenFromNative mothod");
        delete player;
        return (jlong)nullptr;
    }
    player->mOnPcmWrite = env->GetStaticMethodID(clazz, "onPcmWriteFromNative", "(Ljava/lang/Object;[BI)I");
    if (player->mOnPcmWrite == nullptr) {
        OS_LOGE(TAG, "Failed to get onPcmWriteFromNative mothod");
        delete player;
        return (jlong)nullptr;
    }
    player->mOnPcmClose = env->GetStaticMethodID(clazz, "onPcmCloseFromNative", "(Ljava/lang/Object;)V");
    if (player->mOnPcmClose == nullptr) {
        OS_LOGE(TAG, "Failed to get onPcmCloseFromNative mothod");
        delete player;
        return (jlong)nullptr;
    }
#endif
    env->DeleteLocalRef(clazz);

    // Hold onto Liteplayer class for use in calling the static method that posts events to the application thread.
    clazz = env->GetObjectClass(thiz);
    if (clazz == nullptr) {
        OS_LOGE(TAG, "Failed to find class: %s", JAVA_CLASS_NAME);
        delete player;
        jniThrowException(env, "java/lang/Exception", nullptr);
        return (jlong)nullptr;
    }
    player->mClass = (jclass)env->NewGlobalRef(clazz);
    // We use a weak reference so the Liteplayer object can be garbage collected.
    // The reference is only used as a proxy for callbacks.
    player->mObject  = env->NewGlobalRef(weak_this);

    player->mPlayerhandle = liteplayer_create();
    if (player->mPlayerhandle == nullptr) {
        delete player;
        return (jlong)nullptr;
    }
    liteplayer_register_state_listener(player->mPlayerhandle, Liteplayer_native_stateCallback, player);
    // Register sink adapter
    struct sink_wrapper sink_ops = {
#if !defined(ENABLE_OPENSLES)
            .priv_data = player,
            .name = audiotrack_wrapper_name,
            .open = audiotrack_wrapper_open,
            .write = audiotrack_wrapper_write,
            .close = audiotrack_wrapper_close,
#else
            .priv_data = NULL,
            .name = opensles_wrapper_name,
            .open = opensles_wrapper_open,
            .write = opensles_wrapper_write,
            .close = opensles_wrapper_close,
#endif
    };
    liteplayer_register_sink_wrapper(player->mPlayerhandle, &sink_ops);
    // Register file adapter
    struct source_wrapper file_ops = {
            .async_mode = false,
            .buffer_size = 2*1024,
            .priv_data = nullptr,
            .url_protocol = file_wrapper_url_protocol,
            .open = file_wrapper_open,
            .read = file_wrapper_read,
            .content_pos = file_wrapper_content_pos,
            .content_len = file_wrapper_content_len,
            .seek = file_wrapper_seek,
            .close = file_wrapper_close,
    };
    liteplayer_register_source_wrapper(player->mPlayerhandle, &file_ops);
    // Register http adapter
    struct source_wrapper http_ops = {
            .async_mode = true,
            .buffer_size = 256*1024,
            .priv_data = nullptr,
            .url_protocol = httpclient_wrapper_url_protocol,
            .open = httpclient_wrapper_open,
            .read = httpclient_wrapper_read,
            .content_pos = httpclient_wrapper_content_pos,
            .content_len = httpclient_wrapper_content_len,
            .seek = httpclient_wrapper_seek,
            .close = httpclient_wrapper_close,
    };
    liteplayer_register_source_wrapper(player->mPlayerhandle, &http_ops);

    return (jlong)player;
}

static jint Liteplayer_native_setDataSource(JNIEnv *env, jobject thiz, jlong handle, jstring path)
{
    OS_LOGD(TAG, "Liteplayer_native_setDataSource");
    auto player = reinterpret_cast<liteplayer_jni *>(handle);
    if (player == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    if (path == nullptr) {
        jniThrowException(env, "java/lang/IllegalArgumentException", nullptr);
        return -1;
    }
    const char *url = env->GetStringUTFChars(path, nullptr);
    if (url == nullptr) {
        jniThrowException(env, "java/lang/RuntimeException", "Out of memory");
        return -1;
    }
    jint res = liteplayer_set_data_source(player->mPlayerhandle, url);
    env->ReleaseStringUTFChars(path, url);
    return res;
}

static jint Liteplayer_native_prepareAsync(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_prepareAsync");
    auto player = reinterpret_cast<liteplayer_jni *>(handle);
    if (player == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    return (jint) liteplayer_prepare_async(player->mPlayerhandle);
}

static jint Liteplayer_native_start(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_start");
    auto player = reinterpret_cast<liteplayer_jni *>(handle);
    if (player == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    return (jint) liteplayer_start(player->mPlayerhandle);
}

static jint Liteplayer_native_pause(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_pause");
    auto player = reinterpret_cast<liteplayer_jni *>(handle);
    if (player == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    return (jint) liteplayer_pause(player->mPlayerhandle);
}

static jint Liteplayer_native_resume(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_resume");
    auto player = reinterpret_cast<liteplayer_jni *>(handle);
    if (player == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    return (jint) liteplayer_resume(player->mPlayerhandle);
}

static jint Liteplayer_native_seekTo(JNIEnv *env, jobject thiz, jlong handle, jint msec)
{
    OS_LOGD(TAG, "Liteplayer_native_seekTo");
    auto player = reinterpret_cast<liteplayer_jni *>(handle);
    if (player == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    return (jint) liteplayer_seek(player->mPlayerhandle, msec);
}

static jint Liteplayer_native_stop(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_stop");
    auto player = reinterpret_cast<liteplayer_jni *>(handle);
    if (player == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    return (jint) liteplayer_stop(player->mPlayerhandle);
}

static jint Liteplayer_native_reset(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_reset");
    auto player = reinterpret_cast<liteplayer_jni *>(handle);
    if (player == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return -1;
    }
    return (jint) liteplayer_reset(player->mPlayerhandle);
}

static jint Liteplayer_native_getCurrentPosition(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_getCurrentPosition");
    auto player = reinterpret_cast<liteplayer_jni *>(handle);
    if (player == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return 0;
    }
    int msec = 0;
    liteplayer_get_position(player->mPlayerhandle, &msec);
    return (jint)msec;
}

static jint Liteplayer_native_getDuration(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_getCurrentPosition");
    auto player = reinterpret_cast<liteplayer_jni *>(handle);
    if (player == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return 0;
    }
    int msec = 0;
    liteplayer_get_duration(player->mPlayerhandle, &msec);
    return (jint)msec;
}

static void Liteplayer_native_destroy(JNIEnv *env, jobject thiz, jlong handle)
{
    OS_LOGD(TAG, "Liteplayer_native_destroy");
    auto player = reinterpret_cast<liteplayer_jni *>(handle);
    if (player == nullptr) {
        jniThrowException(env, "java/lang/IllegalStateException", nullptr);
        return;
    }
    liteplayer_destroy(player->mPlayerhandle);
    // remove global references
    env->DeleteGlobalRef(player->mObject);
    env->DeleteGlobalRef(player->mClass);
    delete player;
}

static JNINativeMethod gMethods[] = {
        {"native_create", "(Ljava/lang/Object;)J", (void *)Liteplayer_native_create},
        {"native_destroy", "(J)V", (void *)Liteplayer_native_destroy},
        {"native_setDataSource", "(JLjava/lang/String;)I", (void *)Liteplayer_native_setDataSource},
        {"native_prepareAsync", "(J)I", (void *)Liteplayer_native_prepareAsync},
        {"native_start", "(J)I", (void *)Liteplayer_native_start},
        {"native_pause", "(J)I", (void *)Liteplayer_native_pause},
        {"native_resume", "(J)I", (void *)Liteplayer_native_resume},
        {"native_seekTo", "(JI)I", (void *)Liteplayer_native_seekTo},
        {"native_stop", "(J)I", (void *)Liteplayer_native_stop},
        {"native_reset", "(J)I", (void *)Liteplayer_native_reset},
        {"native_getCurrentPosition", "(J)I", (void *)Liteplayer_native_getCurrentPosition},
        {"native_getDuration", "(J)I", (void *)Liteplayer_native_getDuration},
};

static int registerNativeMethods(JNIEnv *env, const char *className,JNINativeMethod *getMethods, int methodsNum)
{
    jclass clazz;
    clazz = env->FindClass(className);
    if (clazz == nullptr) {
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz,getMethods,methodsNum) < 0) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv* env = nullptr;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK) {
        OS_LOGE(TAG, "Failed to get env");
        goto bail;
    }
    assert(env != nullptr);

    if (registerNativeMethods(env, JAVA_CLASS_NAME, gMethods, NELEM(gMethods)) != JNI_TRUE) {
        OS_LOGE(TAG, "Failed to register native methods");
        goto bail;
    }

    sJavaVM = vm;
    /* success -- return valid version number */
    result = JNI_VERSION_1_6;

bail:
    return result;
}
