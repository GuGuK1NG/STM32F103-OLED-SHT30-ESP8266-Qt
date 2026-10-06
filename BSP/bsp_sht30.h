#ifndef __BSP_SHT30_H
#define __BSP_SHT30_H

#include "main.h"
#include "bsp_i2c.h"
#include "bsp_dwt.h"
#define SHT30_ADDR 0x88

uint8_t sht30_init(i2c_soft_t *bus);
uint8_t sht30_read(float *temp, float *humi);
//状态机函数
void sht30_start(void);                      /* 请求测量，立即返回 */
void sht30_task(void);                       /* 主循环里反复调用，推进状态机 */
uint8_t sht30_get(float *temp, float *humi); /* 取最新值，0 = 有有效数据 */

#endif /* __BSP_SHT30_H */