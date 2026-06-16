#include "App_FreeRTOS_Task.h"


SensorData_t Global_SensorData = {0}; // 全局传感器数据结构体
SemaphoreHandle_t Data_Mutex = NULL; // 互斥锁

void HC_SR04_Task(void *args);
#define HC_SR04_TASK_STACK_SIZE 128
#define HC_SR04_TASK_PRIORITY   2
TaskHandle_t HC_SR04_Task_Handle;
#define HC_SR04_TASK_PERIOD 1000
extern volatile HCSR04_InfoTypeDef hcsr04;

void DHT11_Task(void *args);
#define DHT11_TASK_STACK_SIZE 128
#define DHT11_TASK_PRIORITY   2
TaskHandle_t DHT11_Task_Handle;
#define DHT11_TASK_PERIOD 2000
DHT11_Data_TypeDef env_data;


// 查询任务运行时间的函数
void tim_Task(void *args);
#define TIM_TASK_STACK_SIZE 128
#define TIM_TASK_PRIORITY   2
TaskHandle_t tim_Task_Handle;
uint8_t tim_buffer[256]; // 用于存储运行时间统计信息的缓冲区

void Console_Display_Task(void *args);
#define CONSOLE_TASK_STACK_SIZE 256
#define CONSOLE_TASK_PRIORITY   1
TaskHandle_t Console_Display_Task_Handle;

void App_FreeRTOS_Start(void)
{
    // 在创建任务前，必须先创建好互斥锁！
    Data_Mutex = xSemaphoreCreateMutex();
    
    if (Data_Mutex != NULL) {
        xTaskCreate(HC_SR04_Task, "HC_SR04", HC_SR04_TASK_STACK_SIZE, NULL, HC_SR04_TASK_PRIORITY, &HC_SR04_Task_Handle);
        xTaskCreate(DHT11_Task,   "DHT11",   DHT11_TASK_STACK_SIZE,   NULL, DHT11_TASK_PRIORITY,   &DHT11_Task_Handle);
        xTaskCreate(Console_Display_Task, "Console", CONSOLE_TASK_STACK_SIZE, NULL, CONSOLE_TASK_PRIORITY, &Console_Display_Task_Handle);
        xTaskCreate(tim_Task,     "TIM",     TIM_TASK_STACK_SIZE,     NULL, TIM_TASK_PRIORITY,     &tim_Task_Handle);
        
        vTaskStartScheduler();
    } else {
        // 如果内存不足导致锁创建失败，直接卡死
        debug_printf("Fatal Error: Failed to create mutex!\r\n");
        while(1); 
    }
}

void HC_SR04_Task(void *args){
    static TickType_t HC_SR04_LastWakeTime = 0;
    HC_SR04_LastWakeTime = xTaskGetTickCount();
    
    while(1){
        if (hcsr04.state == HCSR04_IDLE) {
            hcsr04.state = HCSR04_WAIT_RISING; 
            HCSR04_Trigger();                 
        }
        
        if (hcsr04.state == HCSR04_FINISH) {
            if(hcsr04.high_level_time_us < 25000) {
                hcsr04.distance_cm = hcsr04.high_level_time_us * 0.017f;
            } else {
                hcsr04.distance_cm = -1.0f; // 测量超时或错误
            }
            hcsr04.state = HCSR04_IDLE;
            
            // --- 取锁写入数据 ---
            if (xSemaphoreTake(Data_Mutex, portMAX_DELAY) == pdTRUE) {
                Global_SensorData.distance = hcsr04.distance_cm;
                xSemaphoreGive(Data_Mutex); // 解锁
            }
        } 
        vTaskDelayUntil(&HC_SR04_LastWakeTime, pdMS_TO_TICKS(HC_SR04_TASK_PERIOD));
    }
}
    
void DHT11_Task(void *args){
    DHT11_Warmup(&env_data); 
    static TickType_t DHT11_LastWakeTime = 0;
    DHT11_LastWakeTime = xTaskGetTickCount();
    
    while(1){
        if (DHT11_Read_Data(&env_data) == 0) {
            // --- 取锁写入数据 ---
            if (xSemaphoreTake(Data_Mutex, portMAX_DELAY) == pdTRUE) {
                Global_SensorData.temperature = env_data.temp_int + env_data.temp_dec * 0.1f;
                Global_SensorData.humidity = env_data.hum_int + env_data.hum_dec * 0.1f;
                xSemaphoreGive(Data_Mutex); // 解锁
            }
        }
        // 注意：出错时不操作互斥锁，全局变量会保持上一次的正确值
        vTaskDelayUntil(&DHT11_LastWakeTime, pdMS_TO_TICKS(DHT11_TASK_PERIOD));
    }
}

void Console_Display_Task(void *args){
    float t = 0.0f, h = 0.0f, d = 0.0f;
    static TickType_t Console_LastWakeTime;
    // 给传感器一点时间去获取第一批数据
    vTaskDelay(pdMS_TO_TICKS(2500));
    Console_LastWakeTime = xTaskGetTickCount();
    while(1){
        // 拿最新的数据
        if (xSemaphoreTake(Data_Mutex, portMAX_DELAY) == pdTRUE) {
            t = Global_SensorData.temperature;
            h = Global_SensorData.humidity;
            d = Global_SensorData.distance;
            xSemaphoreGive(Data_Mutex); // 解锁
        }

        // 打印仪表盘
        debug_printf("\r\n=======================================\r\n");
        debug_printf("||     Environment Monitoring Panel         ||\r\n");
        debug_printf("=======================================\r\n");
        debug_printf("|| Temperature : %5.1f C                    ||\r\n", t);
        debug_printf("|| Humidity    : %5.1f %%                    ||\r\n", h);
        if(d >= 0.0f) {
            debug_printf("|| Distance    : %5.2f cm                  ||\r\n", d);
        } else {
            debug_printf("|| Distance    :  OutOfRange               ||\r\n");
        }
        debug_printf("=======================================\r\n\r\n");
        
        // 每隔 2 秒刷新一次
        vTaskDelayUntil(&Console_LastWakeTime, pdMS_TO_TICKS(2000));
    }
}
void tim_Task(void *args){
    static TickType_t tim_LastWakeTime = 0;
    tim_LastWakeTime = xTaskGetTickCount();
    while(1){
        vTaskGetRunTimeStats((char*)tim_buffer); 
        debug_printf("\r\n[CPU Stats]\r\n%s", tim_buffer);
        vTaskDelayUntil(&tim_LastWakeTime, pdMS_TO_TICKS(10000)); // 10秒看一次就行，不用太频繁
    }
}
