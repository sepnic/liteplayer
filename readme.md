## 1. Architecture Diagram

```
+-----------------------------------------------------------------------------------+
|                                   APPLICATION                                     |
|                                                                                   |
|    +---------------+  +---------------+  +---------------+  +---------------+     |
|    |   http url    |  |  local file   |  |  tts stream   |  |   playlist    |     |
|    +---------------+  +---------------+  +---------------+  +---------------+     |
+-----------------------------------------------------------------------------------+

+-----------------------------------------------------------------------------------+
|                                    LITEPLAYER                                     |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  |                                   PLAYER                                    |  |
|  |                                                                             |  |
|  | +---------------+  +---------------+  +---------------+  +---------------+  |  |
|  | | media parser  |  | media source  |  | player manager|  |    debugger   |  |  |
|  | +---------------+  +---------------+  +---------------+  +---------------+  |  |
|  +-----------------------------------------------------------------------------+  |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  |                                   LIBRARY                                   |  |
|  |                                                                             |  |
|  |  +---------------------+  +---------------------+  +---------------------+  |  |
|  |  |       ESP ADF       |  |      COMPONENTS     |  |        COMMON       |  |  |
|  |  |                     |  |                     |  |                     |  |  |
|  |  |  +---------------+  |  |  +---------------+  |  |  +---------------+  |  |  |
|  |  |  | audio pipeline|  |  |  |   extractor   |  |  |  |    OS API     |  |  |  |
|  |  |  +---------------+  |  |  +---------------+  |  |  +---------------+  |  |  |
|  |  |  +---------------+  |  |  +---------------+  |  |  +---------------+  |  |  |
|  |  |  | audio element |  |  |  |    decoder    |  |  |  |   msglooper   |  |  |  |
|  |  |  +---------------+  |  |  +---------------+  |  |  +---------------+  |  |  |
|  |  |  +---------------+  |  |  +---------------+  |  |  +---------------+  |  |  |
|  |  |  |  audio event  |  |  |  |   resampler   |  |  |  |   msgqueue    |  |  |  |
|  |  |  +---------------+  |  |  +---------------+  |  |  +---------------+  |  |  |
|  |  |                     |  |  +---------------+  |  |  +---------------+  |  |  |
|  |  |                     |  |  |     stream    |  |  |  |    ringbuf    |  |  |  |
|  |  |                     |  |  +---------------+  |  |  +---------------+  |  |  |
|  |  |                     |  |                     |  |  +---------------+  |  |  |
|  |  |                     |  |                     |  |  | memory_detect |  |  |  |
|  |  |                     |  |                     |  |  +---------------+  |  |  |
|  |  +---------------------+  +---------------------+  +---------------------+  |  |
|  +-----------------------------------------------------------------------------+  |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  |                                   ADAPTER                                   |  |
|  |                                                                             |  |
|  |  +---------------------+  +---------------------+  +---------------------+  |  |
|  |  |    file wrapper     |  |     http wrapper    |  |     sink wrapper    |  |  |
|  |  +---------------------+  +---------------------+  +---------------------+  |  |
|  +-----------------------------------------------------------------------------+  |
|                                                                                   |
+-----------------------------------------------------------------------------------+

+-----------------------------------------------------------------------------------+
|                               ADAPTER IMPLEMENTATION                              |
|                                                                                   |
|     +---------------------+  +---------------------+  +---------------------+     |
|     |       fatfs         |  |     http client     |  |      alsa lib       |     |
|     +---------------------+  +---------------------+  +---------------------+     |
|     +---------------------+  +---------------------+  +---------------------+     |
|     |       spifs         |  |        curl         |  |      tinyalsa       |     |
|     +---------------------+  +---------------------+  +---------------------+     |
|                                                                                   |
+-----------------------------------------------------------------------------------+
```

## 2. File Structure

```
.
├── adapter                      [适配层目录]
│   ├── pcmout_wrapper.cpp          1. sink适配接口实现[默认适配tinyalsa接口]
│   ├── pcmout_wrapper.h
│   ├── fatfs_wrapper.c             2. file适配接口实现[默认适配标准文件系统接口]
│   ├── fatfs_wrapper.h
│   ├── httpclient_wrapper.c        3. http适配接口实现[默认适配httpclient]
│   └── httpclient_wrapper.h
│
├── player                       [播放器目录]
│   ├── liteplayer_adapter.h        1. 播放器适配层(sink/file/http)接口定义
│   ├── liteplayer_config.h         2. 播放器配置层，包括播放器各线程优先级、栈空间、缓冲区大小等等
│   ├── liteplayer_main.c           3. 播放器主程序入口，标准播放器接口实现
│   ├── liteplayer_main.h
│   ├── liteplayer_debug.c          4. debug组件(上传解码后的PCM数据到本地)，使用说明见tools/readme.md
│   ├── liteplayer_debug.h
│   ├── liteplayer_parser.c         5. parser组件(解析音频头部信息)，依赖于file/http接口
│   ├── liteplayer_parser.h
│   ├── liteplayer_source.c         6. source组件(从http/file获取数据并填充到ringbuffer)，依赖于
│   └── liteplayer_source.h            file/http接口
│
├── build                        [编译目录]
│   └── build.sh                    1. MacosX/Ubuntu上编译脚本，编译library/example使用，本地调试使用
│
├── example                      [例程目录]
│   └── liteplayer_demo             1. 例程：从http/file读取数据并解码，保存PCM数据到file中
│
├── library                      [组件及音频框架目录]
│   ├── CMakeLists.txt              1. 动态库编译脚本，与下面脚本的区别是使用helix-aac
│   ├── CMakeLists.txt-faad2        2. 动态库编译脚本，与上面脚本的区别是使用faad2-aac
│   ├── components
│   │   ├── audio_decoder           [解码器组件]
│   │   │   ├── aac_decoder.c          1. aac解码组件，封装为一个audio element
│   │   │   ├── aac_faad_wrapper.c
│   │   │   ├── aac_helix_wrapper.c
│   │   │   ├── helixaac                  [helix-aac解码开源库]
│   │   │   ├── libfaad                   [faad2-aac解码开源库]
│   │   │   ├── libmad                    [mad-mp3解码开源库]
│   │   │   ├── m4a_decoder.c          2. m4a解码组件，封装为一个audio element
│   │   │   ├── mp3_decoder.c          3. mp3解码组件，封装为一个audio element
│   │   │   ├── mp3_mad_wrapper.c
│   │   │   └── wav_decoder.c          4. wav解码组件，封装为一个audio element
│   │   ├── audio_extractor         [解析器组件]
│   │   │   ├── aac_extractor.c        1. aac头部解析器
│   │   │   ├── m4a_extractor.c        2. m4a头部解析器
│   │   │   ├── mp3_extractor.c        3. mp3头部解析器
│   │   │   └── wav_extractor.c        4. wav头部解析器
│   │   ├── audio_resampler         [重采样组件]
│   │   │   ├── audio_resampler.c
│   │   │   └── libspeexdsp            speexdsp开源库，仅使用resample模块
│   │   ├── audio_stream            [source/sink组件]
│   │   │   ├── alsa_stream.c          1. sink输出组件，封装为一个audio element，里面可根据需要是否使用SRC
│   │   │   ├── fatfs_stream.c         2. file读写组件[播放器没有使用]
│   │   │   └── http_stream.c          3. http读写组件[播放器没有使用]
│   │   └── include
│   │       ├── audio_decoder
│   │       │   ├── aac_decoder.h
│   │       │   ├── m4a_decoder.h
│   │       │   ├── mp3_decoder.h
│   │       │   └── wav_decoder.h
│   │       ├── audio_extractor
│   │       │   ├── aac_extractor.h
│   │       │   ├── m4a_extractor.h
│   │       │   ├── mp3_extractor.h
│   │       │   └── wav_extractor.h
│   │       ├── audio_resampler
│   │       │   └── audio_resampler.h
│   │       └── audio_stream
│   │           ├── alsa_stream.h
│   │           ├── fatfs_stream.h
│   │           └── http_stream.h
│   └── core                       [ESP开源音频框架]
│       ├── audio_element.c          1. 定义open/read/process/write/close等一些回调函数，每个组件实现
│       │                               这些回调即可嵌入到这套框架中，事实上element是起一个task来循环
│       │                               "msg_handle->read->process->write"
│       ├── audio_event_iface.c      2. 对消息队列/队列集的封装，主要是element消息/状态的接收/发送
│       ├── audio_pipeline.c         3. 把各个element拼成一条链路，通过ringbuffer来链接element，上一个
│       │                               element写入到ringbuffer，下一个element从该ringbuffer中读取数
│       │                               据；比如"http_source->rb->mp3_decoder->rb->sink"就是一条url
│       │                               播放链路，不同场景选择的element不一样，拼接的顺序不一样，实现
│       │                               链路高度配置化
│       └── include
│           └── esp_adf            [ESP头文件]
│
├── module.mk
│
└── thirdparty
    └── msgutils                   [通用基础库目录]
        ├── CMakeLists.txt
        ├── include
        │   └── msgutils
        │       ├── common_list.h      linux/android list
        │       ├── msglooper.h        thread looper to handle message
        │       ├── msgqueue.h         message queue and queue-set
        │       ├── os_logger.h        log utils to format and save log [portable]
        │       ├── os_memory.h        light weight utils to detect memory leak and overflow
        │       ├── os_thread.h        thread interfaces that platform dependent [portable]
        │       ├── os_time.h          time interfaces that platform dependent [portable]
        │       ├── os_timer.h         timer interfaces that platform dependent [portable]
        │       ├── ringbuf.h          thread-safe ring buffer
        │       ├── smartptr.h         smart pointer for c
        │       └── stllist.h          list container is similar to stl list

```

## 3. Demo Usage

### 3.1. Playing local media

./liteplayer_demo ./test.mp3
./liteplayer_demo ./test.m4a

### 3.2. Playing network media

./liteplayer_demo http://ailabsaicloudservice.alicdn.com/player/resources/23a2d715f019c0e345235f379fa26a30.mp3
