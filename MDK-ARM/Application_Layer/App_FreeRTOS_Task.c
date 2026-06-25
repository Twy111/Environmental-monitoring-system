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

void Network_Task(void *args);
#define NETWORK_TASK_STACK_SIZE 256
#define NETWORK_TASK_PRIORITY   3
TaskHandle_t Network_Task_Handle;
//  ESP8266 的是 huart1
extern UART_HandleTypeDef huart1;

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
    xSemaphoreGive(Data_Mutex); // 初始化互斥锁为可用状态 
    
    if (Data_Mutex != NULL) {
        xTaskCreate(HC_SR04_Task, "HC_SR04", HC_SR04_TASK_STACK_SIZE, NULL, HC_SR04_TASK_PRIORITY, &HC_SR04_Task_Handle);
        xTaskCreate(DHT11_Task,   "DHT11",   DHT11_TASK_STACK_SIZE,   NULL, DHT11_TASK_PRIORITY,   &DHT11_Task_Handle);
        xTaskCreate(Network_Task, "Network", NETWORK_TASK_STACK_SIZE, NULL, NETWORK_TASK_PRIORITY, &Network_Task_Handle);
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


// 封装一个简单的串口发送函数 (阻塞式)
static void ESP8266_Send_Cmd(char *cmd) {
    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);
    vTaskDelay(pdMS_TO_TICKS(500)); // 给模块一点反应时间
}

void Network_Task(void *args) {
    // 验证成功的 173 字节的连接报文
static uint8_t mqtt_connect_pkt[] = {
    0x10, 0xAD, 0x01, 0x00, 0x04, 0x4D, 0x51, 0x54, 0x54, 0x04, 0xC2, 0x00, 0x78, // 固定报头, 协议名, 标志位, KeepAlive(120s)
    0x00, 0x0D, 0x53, 0x54, 0x4D, 0x33, 0x32, 0x5F, 0x4E, 0x6F, 0x64, 0x65, 0x5F, 0x30, 0x31, // Client ID: STM32_Node_01
    0x00, 0x0A, 0x34, 0x67, 0x52, 0x61, 0x67, 0x31, 0x64, 0x42, 0x63, 0x77, // Username: 4gRag1dBcw
    0x00, 0x86, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6F, 0x6E, 0x3D, 0x32, 0x30, 0x31, 0x38, 0x2D, // Password (Token) Start
    0x31, 0x30, 0x2D, 0x33, 0x31, 0x26, 0x72, 0x65, 0x73, 0x3D, 0x70, 0x72, 0x6F, 0x64, 0x75,
    0x63, 0x74, 0x73, 0x25, 0x32, 0x46, 0x34, 0x67, 0x52, 0x61, 0x67, 0x31, 0x64, 0x42, 0x63,
    0x77, 0x25, 0x32, 0x46, 0x64, 0x65, 0x76, 0x69, 0x63, 0x65, 0x73, 0x25, 0x32, 0x46, 0x53,
    0x54, 0x4D, 0x33, 0x32, 0x5F, 0x4E, 0x6F, 0x64, 0x65, 0x5F, 0x30, 0x31, 0x26, 0x65, 0x74,
    0x3D, 0x31, 0x38, 0x39, 0x33, 0x34, 0x35, 0x36, 0x30, 0x30, 0x30, 0x26, 0x6D, 0x65, 0x74,
    0x68, 0x6F, 0x64, 0x3D, 0x73, 0x68, 0x61, 0x31, 0x26, 0x73, 0x69, 0x67, 0x6E, 0x3D, 0x25,
    0x32, 0x46, 0x62, 0x73, 0x56, 0x4A, 0x41, 0x64, 0x76, 0x31, 0x47, 0x33, 0x4F, 0x35, 0x6E,
    0x64, 0x41, 0x32, 0x4D, 0x76, 0x77, 0x7A, 0x4D, 0x53, 0x54, 0x6B, 0x6B, 0x59, 0x25, 0x33,
    0x44
};

   // 1. 硬件复位或等待 ESP8266 启动完毕
    vTaskDelay(pdMS_TO_TICKS(2000));
    debug_printf("[Network] Start Init ESP8266...\r\n");

    // 连接别人的路由器
    ESP8266_Send_Cmd("AT+CWMODE=1\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 2. 自动联网指令序列
    ESP8266_Send_Cmd("AT+CWJAP=\"iQOO Z10 Turbo\",\"11112222\"\r\n");
    
    
    //连 WiFi 极其耗时，强制给足 8 秒钟时间
    debug_printf("[Network] Waiting for WiFi connection...\r\n");
    vTaskDelay(pdMS_TO_TICKS(8000)); 

    ESP8266_Send_Cmd("AT+CIPSTART=\"TCP\",\"mqtts.heclouds.com\",1883\r\n");
    // TCP 握手也需要时间，延长到 3 秒
    vTaskDelay(pdMS_TO_TICKS(3000)); 
    
    ESP8266_Send_Cmd("AT+CIPMODE=1\r\n"); // 开启透传
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP8266_Send_Cmd("AT+CIPSEND\r\n");   // 准备发报文，此时 ESP 会返回 '>'
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 3. 发送 MQTT CONNECT 握手报文
    debug_printf("[Network] Sending MQTT CONNECT Packet...\r\n");
    
    // 接 ESP8266 的串口
    HAL_UART_Transmit(&huart1, mqtt_connect_pkt, sizeof(mqtt_connect_pkt), 1000);
    
    // 给服务器一点反应时间处理鉴权
    vTaskDelay(pdMS_TO_TICKS(1000));
    debug_printf("[Network] Connect packet sent. Check OneNET online status.\r\n");

    // 连接阶段完成，进入循环
    static TickType_t Net_LastWakeTime;
    Net_LastWakeTime = xTaskGetTickCount();
    
    float current_temp = 0.0f;
    float current_hum = 0.0f;
    float current_dist = 0.0f;

    while(1) {
       // 第一步：取数据
        if (xSemaphoreTake(Data_Mutex, portMAX_DELAY) == pdTRUE) {
            current_temp = Global_SensorData.temperature;
            current_hum  = Global_SensorData.humidity;
            current_dist = Global_SensorData.distance;
            xSemaphoreGive(Data_Mutex); 
        }

        // 第二步：串口调试
        debug_printf("[Network] Publishing Data -> T:%.1f, H:%.1f, D:%.2f\r\n", 
                     current_temp, current_hum, current_dist);

        // 第三步：发往 OneNET
        MQTT_Send_Publish(current_temp, current_hum, current_dist);
        
        // 第四步：每隔 5 秒上报一次
        vTaskDelayUntil(&Net_LastWakeTime, pdMS_TO_TICKS(5000));
    }
}
