Liteplayer 是一个为嵌入式平台设计的低开销低延时的音频播放器，已运行在千万级别的设备上，包括 Android、iOS、Linux、RTOS 多种终端平台，其中最低配置为 主频192MHz + 内存448KB。

Liteplayer 具有如下特点：
1. 支持 MP3、AAC、M4A、WAV、FLAC、OPUS 格式，支持本地文件、HTTP/HTTPS/HLS、本地播放列表，接口和状态机与 Android MediaPlayer 一致
2. 极低的系统开销，2-3 个线程，低至 80KB 堆内存占用，可保证在 主频192MHz + 内存448KB 的系统上运行顺畅；高配置平台上可配置更大的缓冲区以取得更好的播放体验
3. 高度的移植性，纯 C 语言 C99 标准，已运行在 Linux、Android、iOS、MacOS、FreeRTOS、AliOS-Things 上；如果其平台不支持 POSIX 接口规范，则实现 Thread、Memory、Time 相关的少量 OSAL 接口也可接入
4. 抽象文件读写、网络访问、音频设备输出的接口，以便适配多个三方库，比如网络访问可选择用 httpclient+mbedtls 或者 curl+openssl
5. 适配多个解码器，包括 libmad、pv-mp3、helix-aac、fdk-aac 等等，可根据系统配置和 License 来选择合适解码器
6. 提供丰富的调试手段，可以收集及分析播放链路各节点的音频流数据；提供内存检测手段，能直观查看内存分配细节、分析内存泄漏和内存越界

**播放器基本接口**：
- 提供播放器基本服务，包括 set_data_source、prepare、start、pause、resume、seek、stop、reset 等操作
- [https://github.com/sepnic/liteplayer_priv/blob/master/library/include/liteplayer_main.h](https://github.com/sepnic/liteplayer_priv/blob/master/library/include/liteplayer_main.h)

**播放器高级接口**：
- 提供播放管理器功能，支持本地播放列表，切换上下首、单曲循环等操作
- [https://github.com/sepnic/liteplayer_priv/blob/master/library/include/liteplayer_listplayer.h](https://github.com/sepnic/liteplayer_priv/blob/master/library/include/liteplayer_listplayer.h)

**播放器适配层**：
- 文件读写、网络访问、音频设备输出的抽象接口，默认适配了 "文件读写-标准文件系统"、 "网络访问-httpclient"、"音频设备输出-tinyalsa/OpenSLES/AudioTrack"
- [https://github.com/sepnic/liteplayer_priv/blob/master/library/include/liteplayer_adapter.h](https://github.com/sepnic/liteplayer_priv/blob/master/library/include/liteplayer_adapter.h)

**OSAL 适配层**：
- Thread、Memory、Time 等操作系统相关的抽象接口，如果系统已支持 POSIX 接口规范，则不用修改直接使用即可
- [https://github.com/sepnic/sysutils](https://github.com/sepnic/sysutils)

![LiteplayerArchitecture](https://github.com/sepnic/liteplayer_priv/blob/master/Liteplayer.png)
