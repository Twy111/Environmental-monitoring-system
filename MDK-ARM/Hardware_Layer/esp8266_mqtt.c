
#include "esp8266_mqtt.h"
// 你的 OneNET 物模型上报 Topic (注意替换你的 ProductID 和 DeviceName)
#define ONENET_TOPIC "$sys/4gRag1dBcw/STM32_Node_01/thing/property/post"

// 动态打包并发送 MQTT PUBLISH 报文
void MQTT_Send_Publish(float temp, float hum, float dist) {
    static uint8_t mqtt_tx_buf[512]; // 足够大的发送缓冲区
    static char json_payload[256];
    
    // 1. 组装 OneNET 要求的 JSON 格式数据 (物模型标准格式)
    sprintf(json_payload, 
            "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{"
            "\"temperature\":{\"value\":%.1f},"
            "\"humidity\":{\"value\":%.1f},"
            "\"distance\":{\"value\":%.2f}}}", 
            temp, hum, dist);

    int topic_len = strlen(ONENET_TOPIC);
    int payload_len = strlen(json_payload);
    
    // MQTT 报文的“剩余长度” = 主题长度占用(2字节) + 主题字符串长度 + 载荷长度
    int remain_len = 2 + topic_len + payload_len;
    
    int index = 0;
    // 2. 固定报头: 0x30 表示 PUBLISH 报文，QoS=0
    mqtt_tx_buf[index++] = 0x30; 
    
    // 3. 动态计算“剩余长度” (MQTT 协议规定的变长编码算法)
    do {
        uint8_t encodedByte = remain_len % 128;
        remain_len = remain_len / 128;
        if (remain_len > 0) {
            encodedByte |= 128;
        }
        mqtt_tx_buf[index++] = encodedByte;
    } while (remain_len > 0);
    
    // 4. 写入 Topic 长度 (高位在前，低位在后)
    mqtt_tx_buf[index++] = (topic_len >> 8) & 0xFF;
    mqtt_tx_buf[index++] = topic_len & 0xFF;
    
    // 5. 写入 Topic 字符串
    memcpy(&mqtt_tx_buf[index], ONENET_TOPIC, topic_len);
    index += topic_len;
    
    // 6. 写入 JSON 数据载荷
    memcpy(&mqtt_tx_buf[index], json_payload, payload_len);
    index += payload_len;
    
    // 7. 发送给 ESP8266 
    HAL_UART_Transmit(&huart1, mqtt_tx_buf, index, 1000);
}
