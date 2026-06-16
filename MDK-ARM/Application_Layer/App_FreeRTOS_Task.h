#ifndef __APP_FREERTOS_TASK_H__
#define __APP_FREERTOS_TASK_H__

#include "FreeRTOS.h"
#include "task.h"
#include "hcsr04.h"
#include "dht11.h"
#include "semphr.h"
#include "com_debug.h"
#include "Com_delay.h"
#include "esp8266_mqtt.h"

// 定义一个全局结构体
typedef struct {
    float temperature;
    float humidity;
    float distance;
} SensorData_t;

void App_FreeRTOS_Start(void);

#endif /* __APP_FREERTOS_TASK_H__ */
