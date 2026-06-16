#ifndef HCSR04_H
#define HCSR04_H

#include "main.h"
#include "Com_delay.h"

typedef enum {
    HCSR04_IDLE = 0,
    HCSR04_WAIT_RISING,
    HCSR04_WAIT_FALLING,
    HCSR04_FINISH
} HCSR04_StateTypeDef;

typedef struct {
    HCSR04_StateTypeDef state;
    uint32_t capture_rising;
    uint32_t capture_falling;
    uint32_t high_level_time_us; 
    float distance_cm;           
} HCSR04_InfoTypeDef;

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);
void HCSR04_Trigger(void);
#endif /* HCSR04_H */
