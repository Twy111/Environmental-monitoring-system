#ifndef __APP_FREERTOS_TASK_H__
#define __APP_FREERTOS_TASK_H__

#include "FreeRTOS.h"
#include "task.h"
#include "hcsr04.h"
#include "dht11.h"
#include "semphr.h"
#include "com_debug.h"
#include "Com_delay.h"

void App_FreeRTOS_Start(void);

#endif /* __APP_FREERTOS_TASK_H__ */
