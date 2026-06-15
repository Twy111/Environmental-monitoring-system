#include "hcsr04.h"
// 定义状态机和控制块

volatile HCSR04_InfoTypeDef hcsr04 = {HCSR04_IDLE, 0, 0, 0, 0.0f};

// 重写 HAL 库的定时器输入捕获回调函数
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        if (hcsr04.state == HCSR04_WAIT_RISING) {
            // 记录上升沿时间戳
            hcsr04.capture_rising = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            // 切换极性为下降沿
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_FALLING);
            hcsr04.state = HCSR04_WAIT_FALLING;
        }
        else if (hcsr04.state == HCSR04_WAIT_FALLING) {
            // 记录下降沿时间戳
            hcsr04.capture_falling = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            
            // 计算时间差 (处理 16 位定时器 ARR 溢出的情况)
            if (hcsr04.capture_falling >= hcsr04.capture_rising) {
                hcsr04.high_level_time_us = hcsr04.capture_falling - hcsr04.capture_rising;
            } else {
                hcsr04.high_level_time_us = (0xFFFF - hcsr04.capture_rising) + hcsr04.capture_falling + 1; 
            }
            
            // 恢复极性为上升沿，为下次做准备
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_RISING);
            hcsr04.state = HCSR04_FINISH;
        }
    }
}

void HCSR04_Trigger(void) {

    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_Port, HCSR04_TRIG_Pin, GPIO_PIN_SET);
    Delay_us(15); 
    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_Port, HCSR04_TRIG_Pin, GPIO_PIN_RESET);
}
