#include "dht11.h"
#include "com_debug.h"

// 复用我们之前写的微秒延时函数 (需确保 main.c 里已经有这个函数，或者把它移到公共的 delay.c 中)
extern void Delay_us(uint32_t us); 

// 动态切换为输出模式
static void DHT11_Mode_Output(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_DATA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
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
static uint8_t DHT11_Read_Bit(void) {
    uint8_t retry = 0;
    
    // 1. 等待 54us 的低电平同步信号结束 (增加超时保护)
    while(HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_RESET && retry < 100) {
        retry++;
        Delay_us(1);
    }
    
    retry = 0; // 重置 retry，准备测高电平
    
    // 2. 测量高电平的时间
    while(HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_SET && retry < 100) {
        retry++;
        Delay_us(1);
    }
    
    // 3. 判断：高电平持续 26-28us 是 '0'，持续 70us 是 '1'
    if(retry > 20) return 1; // 需考虑硬件指令消耗时间周期!!!! 阈值设为20比较稳妥      
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
// 读取 1 个 bit (修复了死循环隐患)


// 核心：读取一次完整数据
uint8_t DHT11_Read_Data(DHT11_Data_TypeDef *DHT11_Data) {
    uint8_t buf[5];
    uint8_t retry = 0;
    
    // --- 第一步：主机发送起始信号 ---
    DHT11_Mode_Output();
    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin, GPIO_PIN_RESET); 
    Delay_us(20000); // 延时 20ms (注意：你原代码注释写了1000ms，那是错的，代码 20 是对的)
    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin, GPIO_PIN_SET);   

    DHT11_Mode_Input(); // 释放总线，变为输入
    // __disable_irq(); // 关中断防干扰是对的
    // --- 第二步：完整的 3 步握手响应检测 ---
    
    // 2.1 等待 DHT11 拉低 (取代你原先卡死的那个 while)
    retry = 0;
    while(HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_SET && retry < 100) {
        retry++; Delay_us(1);
    }
    if(retry >= 100) {
        debug_printf("[dht11.c:] Error: Wait for response LOW timeout\r\n");
        // __enable_irq();
        return 1; // 传感器彻底无响应
    }

    // 2.2 等待 DHT11 80us 低电平响应结束 (等待拉高)
    retry = 0;
    while(HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_RESET && retry < 100) {
        retry++; Delay_us(1);
    }
    if(retry >= 100) {
        debug_printf("[dht11.c:] Error: DHT11 response LOW duration timeout\r\n");
        // __enable_irq();
        return 1; 
    }

    // 2.3 等待 DHT11 80us 高电平准备信号结束 (等待拉低，准备收数据)
    retry = 0;
    while(HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_SET && retry < 100) {
        retry++; Delay_us(1);
    }
    if(retry >= 100) {
        debug_printf("[dht11.c:] Error: DHT11 response HIGH duration timeout\r\n");
        // __enable_irq();
        return 1; 
    }

    // --- 第三步：接收 40 bit 数据 ---

    for(uint8_t i = 0; i < 5; i++) {
        buf[i] = DHT11_Read_Byte();
    }
    // __enable_irq();

    // --- 第四步：校验和提取数据 ---
    if((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) == buf[4]) {
        DHT11_Data->hum_int  = buf[0];
        DHT11_Data->hum_dec  = buf[1];
        DHT11_Data->temp_int = buf[2];
        DHT11_Data->temp_dec = buf[3];
        return 0; // 读取成功
    } else {
        debug_printf("[dht11.c:] DHT11 checksum error\r\n");
        return 1; // 校验失败
    }
}
void DHT11_Warmup(DHT11_Data_TypeDef *DHT11_Data) {
    // 确保 DHT11 引脚被拉高，释放总线
    DHT11_Mode_Output();
    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin, GPIO_PIN_SET);

    // 延时 2 秒，等待 DHT11 跨越上电不稳定期
    HAL_Delay(2000); 

    /* ================= 2. 传感器预热阶段 ================= */
    debug_printf("DHT11 Waking up...\r\n");
    // 执行一次“空读（Dummy Read）”，专门用来吃掉第一次报错，不用管它的返回值
    DHT11_Read_Data(DHT11_Data); 

    // 空读结束后，再次延时 2 秒，满足 DHT11 两次读取的最短间隔
    HAL_Delay(2000); 
    debug_printf("DHT11 Ready!\r\n");
}
