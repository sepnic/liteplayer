// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __ESP_LOG_H__
#define __ESP_LOG_H__

#include "msgutils/os_logger.h"

#ifdef __cplusplus
extern "C" {
#endif

    #define ESP_LOGE( tag, format, ... ) OS_LOGE(tag, format, ##__VA_ARGS__)
    #define ESP_LOGW( tag, format, ... ) OS_LOGW(tag, format, ##__VA_ARGS__)
    #define ESP_LOGI( tag, format, ... ) OS_LOGI(tag, format, ##__VA_ARGS__)
    #define ESP_LOGD( tag, format, ... ) OS_LOGD(tag, format, ##__VA_ARGS__)
    #define ESP_LOGV( tag, format, ... ) OS_LOGV(tag, format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* __ESP_LOG_H__ */
