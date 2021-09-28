## How to build liteplayer for esp32

### Setup ESP-IDF

See https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html 

My esp-idf info:
 - REPO: https://github.com/espressif/esp-idf.git
 - BRANCH: master
 - HEAD: 3e370c4296247b349aa3b9a0076c05b9946d47dc

### Setup the environment variables

I suppose you have a installtion directory named 'espressif'.

``` bash
export IDF_PATH="$espressif/esp-idf"                   # idf source path
export IDF_TOOLS_PATH="$espressif/tools/esp-idf-tools" # idf tools path, see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#customizing-the-tools-installation-path
. $IDF_PATH/export.sh
```

### Build the project

``` bash
cd builds/esp32
idf.py set-target esp32
idf.py menuconfig
idf.py build                     # build
idf.py -p <PORT> flash monitor   # flash image and output serial message to monitor
```

Note: 'componments/liteplayer_adapter/sink_ESP32-LyraT-Mini_wrapper.*' is just for ESP32-LyraT-Mini board
