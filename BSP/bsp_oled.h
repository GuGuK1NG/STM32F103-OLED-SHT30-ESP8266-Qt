#ifndef __BSP_OLED_H
#define __BSP_OLED_H

#include "main.h"
#include "bsp_i2c.h"

#define OLED_ADDR 0x78

void oled_init(i2c_soft_t *bus);
void oled_clear(void);
void oled_refresh(void);
void oled_draw_point(uint8_t x, uint8_t y, uint8_t color);
void oled_show_char(uint8_t x, uint8_t y, uint8_t chr, uint8_t size, uint8_t mode); //照抄官方算法
void oled_show_string(uint8_t x, uint8_t y, const char *str, uint8_t size);
void oled_show_num(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size);


#endif