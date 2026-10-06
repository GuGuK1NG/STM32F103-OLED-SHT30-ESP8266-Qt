/**
 ******************************************************************************
 * @file    bsp_dwt.h
 * @brief   DWT 微秒延时
 *          Cortex-M3 内核自带的 DWT->CYCCNT 计数器，精度到 CPU 周期，
 *          不受编译器优化等级影响（对比：空循环延时在 -O3 下会被整个优化掉）
 ******************************************************************************
 */

#ifndef __BSP_DWT_H
#define __BSP_DWT_H

#include "main.h"

/* 初始化 DWT 计数器，必须在 main() 开头调用一次 */
void dwt_init(void);

/* 微秒级延时 */
void dwt_delay_us(uint32_t us);

/* 毫秒级延时（复用 HAL_Delay） */
void dwt_delay_ms(uint32_t ms);

/* 读取当前计数值，用于非阻塞计时 */
uint32_t dwt_get_tick(void);

/* 计算从 start 到现在经过的微秒数 */
uint32_t dwt_elapsed_us(uint32_t start);

#endif /* __BSP_DWT_H */