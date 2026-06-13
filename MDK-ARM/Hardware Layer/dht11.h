#ifndef __DHT11_H
#define __DHT11_H

#include "main.h"

// 定义温湿度结构体
typedef struct {
    uint8_t temp_int;    // 温度整数
    uint8_t temp_dec;    // 温度小数
    uint8_t hum_int;     // 湿度整数
    uint8_t hum_dec;     // 湿度小数
} DHT11_Data_TypeDef;

// 函数声明
uint8_t DHT11_Read_Data(DHT11_Data_TypeDef *DHT11_Data);

#endif
