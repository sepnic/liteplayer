## socket_upload.py

### 1. 目的作用

通过 socket 上传语音数据到本地，数据上传前需上传 "GENIE_SOCKET_UPLOAD_START" 字符串，表示上传开始；数据上传后需上传 "GENIE_SOCKET_UPLOAD_END" 字符串，表示上传结束。设备端代码实现见 liteplayer_debug.c。
一般用在声学录音测试，需要长时间收集录音数据；或者播放器解码后的 PCM 数据上传，以便定位播放器解码问题还是底层 ALSA 设备问题。

### 2. 使用方法

#### 2.1. 设备端接口说明

1. socket_upload_start(addr, port)：创建 socket upload 线程和资源

2. socket_upload_fill_data(handle, data, size)：填充数据到 socket upload 缓冲区

3. socket_upload_stop(handle)：停止 socket upload，阻塞等待 socket upload 线程退出

#### 2.2. socket server 启动说明

1. python socket_upload.py

2. 生成的文件在同一级目录下，文件名如 record-2019-12-06-19-49-57.pcm

