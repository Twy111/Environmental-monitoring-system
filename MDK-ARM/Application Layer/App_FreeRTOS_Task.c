#include "App_FreeRTOS_Task.h"



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
#define DHT11_TASK_PERIOD 1000
DHT11_Data_TypeDef env_data;


SemaphoreHandle_t uart_mutex;

void App_FreeRTOS_Start(void)
{
    uart_mutex = xSemaphoreCreateMutex();  // ´´½¨»¥³âËø
    /* Create tasks here */
    xTaskCreate(HC_SR04_Task, "HC_SR04_Task", HC_SR04_TASK_STACK_SIZE, NULL, HC_SR04_TASK_PRIORITY, &HC_SR04_Task_Handle);
    xTaskCreate(DHT11_Task, "DHT11_Task", DHT11_TASK_STACK_SIZE, NULL, DHT11_TASK_PRIORITY, &DHT11_Task_Handle);

    /* Start the scheduler */
    vTaskStartScheduler();
}

void HC_SR04_Task(void *args){
    static TickType_t HC_SR04_LastWakeTime = 0;
    
    while(1){
        HC_SR04_LastWakeTime = xTaskGetTickCount();
        /* Task code here */
        if (hcsr04.state == HCSR04_IDLE) {
            hcsr04.state = HCSR04_WAIT_RISING; 
            HCSR04_Trigger();                 
        }
        
        if (hcsr04.state == HCSR04_FINISH) {
              if(hcsr04.high_level_time_us < 25000) {
                hcsr04.distance_cm = hcsr04.high_level_time_us * 0.017f;
            } else {
                hcsr04.distance_cm = -1.0f;
            }

            // vTaskDelay(pdMS_TO_TICKS(10));
            hcsr04.state = HCSR04_IDLE;
            xSemaphoreTake(uart_mutex, portMAX_DELAY);
            debug_printf("Distance: %.2f cm\r\n", hcsr04.distance_cm);
            xSemaphoreGive(uart_mutex);
        } 
        vTaskDelayUntil(&HC_SR04_LastWakeTime, pdMS_TO_TICKS(HC_SR04_TASK_PERIOD));
    }
}
void DHT11_Task(void *args){
    static TickType_t DHT11_LastWakeTime = 0;
    
    while(1){
            DHT11_LastWakeTime = xTaskGetTickCount();
            if (DHT11_Read_Data(&env_data) == 0) {
                debug_printf("Temp: %d.%d C, Hum: %d.%d %%\r\n", 
                        env_data.temp_int, env_data.temp_dec, 
                        env_data.hum_int, env_data.hum_dec);
            } else {
                xSemaphoreTake(uart_mutex, portMAX_DELAY);
                debug_printf("DHT11 Error!\r\n");
                xSemaphoreGive(uart_mutex);
            }
            vTaskDelayUntil(&DHT11_LastWakeTime, pdMS_TO_TICKS(DHT11_TASK_PERIOD));
        }

}
