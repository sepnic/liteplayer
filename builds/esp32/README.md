## How to build liteplayer for esp32

### Setup ESP-ADF

See https://docs.espressif.com/projects/esp-adf/zh_CN/latest/get-started/index.html

My esp-adf info:
 - REPO: https://github.com/espressif/esp-adf.git
 - BRANCH: master
 - HEAD: 6fa029230263cec69f13167b1810c06d37cbc20c

### Setup the environment variables

I suppose you have a installtion directory named 'espressif'.

``` bash
export ADF_PATH="$espressif/esp-adf"
export IDF_PATH="$ADF_PATH/esp-idf"
. $IDF_PATH/export.sh
```

### Build the project

``` bash
cd builds/esp32
idf.py menuconfig
idf.py build                     # build
idf.py -p <PORT> flash monitor   # flash image and output serial message to monitor
```

