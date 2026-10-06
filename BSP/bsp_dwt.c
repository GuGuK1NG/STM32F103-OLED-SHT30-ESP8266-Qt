/**
 ******************************************************************************
 * @file    bsp_dwt.c
 * @brief   DWT 微秒延时实现
 *          Cortex-M3 的 DWT(Data Watchpoint and Trace) 单元带一个 32 位
 *          周期计数器 CYCCNT，每来一个 CPU 时钟周期加 1。
 *          72MHz 下 1us = 72 个计数。
 ******************************************************************************
 */

#include "bsp_dwt.h"

/* DWT->CYCCNT 是 32 位，在 72MHz 下约 59.6 秒回绕一次。
 * 差值运算用无符号数，回绕时会自然"借位"，所以比较用减法是真确的。 */

void dwt_init(void)
{
    /* 1. 使能 DWT 单元的跟踪功能（DEMCR 的 TRCENA 位） */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* 2. 清零周期计数器 */
    DWT->CYCCNT = 0;

    /* 3. 使能周期计数器（CTRL 的 CYCCNTENA 位） */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void dwt_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    /* SystemCoreClock 在 72MHz 时 = 72000000，所以这里等于 72*us */
    uint32_t ticks = us * (SystemCoreClock / 1000000U);

    /* 用减法比较，自动处理计数器回绕 */
    while ((DWT->CYCCNT - start) < ticks)
    {
        /* 空等待。这里不需要 __NOP()，因为 DWT->CYCCNT 是 volatile 的，
         * 编译器无法把这个循环优化掉。 */
    }
}

void dwt_delay_ms(uint32_t ms)
{
    while (ms--)
    {
        dwt_delay_us(1000);
    }
}

uint32_t dwt_get_tick(void)
{
    return DWT->CYCCNT;
}

uint32_t dwt_elapsed_us(uint32_t start)
{
    uint32_t diff = DWT->CYCCNT - start;   /* 自动处理回绕 */
    return diff / (SystemCoreClock / 1000000U);
}