#include "dht11.h"

// 复用我们之前写的微秒延时函数 (需确保 main.c 里已经有这个函数，或者把它移到公共的 delay.c 中)
extern void Delay_us_Rough(uint32_t us); 

// 动态切换为输出模式
static void DHT11_Mode_Output(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_DATA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &GPIO_InitStruct);
}

// 动态切换为输入模式
static void DHT11_Mode_Input(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_DATA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP; // 上拉电阻，保持总线默认高电平
    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &GPIO_InitStruct);
}

// 读取 1 个 bit
static uint8_t DHT11_Read_Bit(void) {
    uint8_t retry = 0;
    // 1. 等待 50us 的低电平同步信号结束
    while(HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_RESET && retry < 100) {
        retry++;
        Delay_us_Rough(1);
    }
    
    retry = 0;
    // 2. 测量高电平的时间
    while(HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_SET && retry < 100) {
        retry++;
        Delay_us_Rough(1);
    }
    
    // 3. 判断：高电平持续约 28us 是 '0'，持续约 70us 是 '1'
    if(retry > 40) return 1;
    else return 0;
}

// 读取 1 个 byte (8 个 bit)
static uint8_t DHT11_Read_Byte(void) {
    uint8_t i, data = 0;
    for(i = 0; i < 8; i++) {
        data <<= 1;          // 高位先出，左移
        data |= DHT11_Read_Bit();
    }
    return data;
}

// 核心：读取一次完整数据 (返回 0 表示成功，1 表示失败)
uint8_t DHT11_Read_Data(DHT11_Data_TypeDef *DHT11_Data) {
    uint8_t buf[5];
    uint8_t retry = 0;

    // --- 第一步：主机发送起始信号 ---
    DHT11_Mode_Output();
    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin, GPIO_PIN_RESET);
    HAL_Delay(20); // 拉低至少 18ms
    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin, GPIO_PIN_SET);
    Delay_us_Rough(30); // 主机拉高 20~40us

    // --- 第二步：切换输入，等待 DHT11 响应 ---
    DHT11_Mode_Input();
    
    // 等待 DHT11 拉低 80us
    while(HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_SET && retry < 100) {
        retry++; Delay_us_Rough(1);
    }
    if(retry >= 100) return 1; // 超时，传感器无响应
    
    retry = 0;
    // 等待 DHT11 拉高 80us
    while(HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_RESET && retry < 100) {
        retry++; Delay_us_Rough(1);
    }
    if(retry >= 100) return 1; // 超时

    // --- 第三步：接收 40 bit 数据 ---
    for(uint8_t i = 0; i < 5; i++) {
        buf[i] = DHT11_Read_Byte();
    }

    // --- 第四步：校验和提取数据 ---
    if((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4]) {
        DHT11_Data->hum_int  = buf[0];
        DHT11_Data->hum_dec  = buf[1];
        DHT11_Data->temp_int = buf[2];
        DHT11_Data->temp_dec = buf[3];
        return 0; // 读取成功
    }
    
    return 1; // 校验失败
}
