#include "esp8266_mqtt.h"
// OneNET 物模型上报 Topic
#define ONENET_TOPIC "$sys/4gRag1dBcw/STM32_Node_01/thing/property/post"

// 动态打包并发送 MQTT PUBLISH 报文
void MQTT_Send_Publish(float temp, float hum, float dist) {
    // 定义一个静态数组作为发送缓冲区。使用 static 可以避免每次调用都在栈上分配 512 字节，防止 FreeRTOS 任务栈溢出。
    static uint8_t mqtt_tx_buf[512]; 
    // 定义一个用于存放 JSON 字符串的数组。
    static char json_payload[256];
    
    // 1. 组装 OneNET 要求的 JSON 格式数据 (物模型标准格式)
    // 根据 OneNET 物模型要求，使用 sprintf 将传入的浮点数格式化填入 JSON 模板中。
    // %.1f 保留一位小数，%.2f 保留两位小数。
    sprintf(json_payload, 
            "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{"
            "\"temperature\":{\"value\":%.1f},"
            "\"humidity\":{\"value\":%.1f},"
            "\"distance\":{\"value\":%.2f}}}", 
            temp, hum, dist);

    // 获取 Topic 的字符串长度
    int topic_len = strlen(ONENET_TOPIC);
    // 获取刚刚生成的 JSON 字符串的实际长度
    int payload_len = strlen(json_payload);
    
    // MQTT 报文的“剩余长度” (Remaining Length) 计算。
    // 规定：剩余长度 = 可变报头的长度 + 有效载荷的长度。
    // PUBLISH 报文的可变报头包含：2 字节的 Topic 长度字段 + Topic 字符串本身。
    // 因此这里是 2 + topic_len + payload_len。
    int remain_len = 2 + topic_len + payload_len;
    
    // 用于记录当前写入 mqtt_tx_buf 数组的位置偏移量。
    int index = 0;
    
    // 2. 固定报头: 0x30 表示 PUBLISH 报文，QoS=0
    // MQTT 报文第一个字节：高 4 位 0011 (即 3) 表示 PUBLISH 报文。
    // 低 4 位 0000 表示 DUP=0, QoS=0, RETAIN=0。组合起来就是 0x30。
    mqtt_tx_buf[index++] = 0x30; 
    
    // 3. 动态计算“剩余长度” (MQTT 协议规定的变长编码算法)
    // 这是 MQTT 协议的精髓之一。因为数据长度不固定，MQTT 使用 1-4 个字节来编码剩余长度。
    // 每个字节的最高位 (bit 7) 是一个标志位，表示“是否还有下一个字节”。低 7 位用于实际计数值。
    do {
        // 取出 remain_len 的低 7 位 ( remain_len % 128 )
        uint8_t encodedByte = remain_len % 128;
        // 长度右移 7 位，准备处理下一批数据
        remain_len = remain_len / 128;
        // 如果长度还没除完（大于0），说明还需要下一个字节来存，就把当前字节的最高位置 1 (即 | 128)
        if (remain_len > 0) {
            encodedByte |= 128;
        }
        // 将计算好的字节存入发送缓冲区
        mqtt_tx_buf[index++] = encodedByte;
    } while (remain_len > 0);
    
    // 4. 写入 Topic 长度 (高位在前，低位在后)
    // MQTT 协议规定多字节整数必须是大端模式 (Big-Endian)，即高字节在前。
    // 将 topic_len 右移 8 位并与 0xFF 按位与，提取高 8 位。
    mqtt_tx_buf[index++] = (topic_len >> 8) & 0xFF;
    // 提取 topic_len 的低 8 位。
    mqtt_tx_buf[index++] = topic_len & 0xFF;
    
    // 5. 写入 Topic 字符串
    // 使用 memcpy 直接将 Topic 字符串的 ASCII 码拷贝到缓冲区中。
    memcpy(&mqtt_tx_buf[index], ONENET_TOPIC, topic_len);
    // 更新偏移量。
    index += topic_len;
    
    // 6. 写入 JSON 数据载荷
    // 将之前格式化好的 JSON 字符串拷贝到缓冲区的最后部分。
    memcpy(&mqtt_tx_buf[index], json_payload, payload_len);
    // 更新偏移量，此时 index 就是整个 MQTT 报文的总字节数。
    index += payload_len;
    
    // 7. 发送给 ESP8266 
    // 调用 STM32 HAL 库的串口阻塞发送函数。
    // 把构造好的纯粹 MQTT 二进制流发给处于透传模式的 ESP8266，从而经由 4G/WiFi 发给 OneNET 平台。
    HAL_UART_Transmit(&huart1, mqtt_tx_buf, index, 1000);
}