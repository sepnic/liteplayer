```
.
├── adapter                      [适配层目录]
│   ├── alsa_wrapper.cpp            1. sink适配接口实现[默认适配tinyalsa接口]
│   ├── alsa_wrapper.h
│   ├── fatfs_wrapper.c             2. file适配接口实现[默认适配标准文件系统fopen/fread/fwrite/fseek/fclose]
│   ├── fatfs_wrapper.h
│   ├── http_wrapper.c              3. http适配接口实现[默认适配httpclient]
│   └── http_wrapper.h
│
├── player                       [播放器目录]
│   ├── liteplayer_adapter.h        1. 播放器适配层接口定义，包括sink/file/http这三套接口，要求厂家或SDK实现
│   ├── liteplayer_config.h         2. 播放器配置层，包括播放器各线程优先级、栈空间、ringbuffer大小等等
│   ├── liteplayer_main.c           3. 播放器主程序入口，包括create/set_data_source/prepare/start/pause
│   ├── liteplayer_main.h              /stop/reset/destroy等标准播放器接口实现
│   ├── liteplayer_debug.c          4. debug组件(上传解码后的PCM数据到本地)，使用说明见tools/readme.md
│   ├── liteplayer_debug.h
│   ├── liteplayer_parser.c         5. parser组件(解析音频头部信息)，依赖于file/http接口
│   ├── liteplayer_parser.h
│   ├── liteplayer_source.c         6. source组件(从http/file获取数据并填充到ringbuffer)，依赖于file/
│   └── liteplayer_source.h            http接口
│
├── build                        [编译目录]
│   └── build.sh                    1. MacosX/Ubuntu上编译脚本，编译library/example使用，本地调试使用
│
├── example                      [例程目录]
│   ├── fatfs_m4aplayer             1. 例程：从file读取m4a文件并解码，保存PCM数据到file中
│   └── fatfs_mp3player             2. 例程：从file读取mp3文件并解码，保存PCM数据到file中
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
│       ├── audio_element.c          1. 定义open/read/process/write/close等一些回调函数，每个组件实
│       │                               现这些回调即可嵌入到这套框架中，事实上element是起一个task来循环
│       │                               "msg_handle -> read -> process -> write"
│       ├── audio_event_iface.c      2. 对消息队列/队列集的封装，主要是element消息/状态的接收/发送
│       ├── audio_pipeline.c         3. 把各个element拼成一条链路，通过ringbuffer来链接element，上一个
│       │                               element写入到ringbuffer，下一个element从该ringbuffer中读取
│       │                               数据；比如"http_source-> rb ->mp3_decoder-> rb ->sink"
│       │                               就是一条url播放链路，
│       │                               不同场景选择的element不一样，拼接的顺序不一样，实现链路高度配置化
│       └── include
│           └── esp_adf            [ESP头文件]
│
├── module.mk
│
└── thirdparty
    └── msgutils                  [通用基础库目录]
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