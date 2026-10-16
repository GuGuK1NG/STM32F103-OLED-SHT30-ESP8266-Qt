#ifndef __BSP_ESP_UART_H
#define __BSP_ESP_UART_H

#include "main.h"

void esp_uart_init(void);   /* 启动 USART2 单字节中断接收 */
uint16_t esp_uart_available(void);  /* 当前可读字节数 */
uint16_t esp_uart_read(uint8_t *dst,uint16_t max);  /* 非阻塞取字节，返回实际个数 */
void esp_uart_flush(void); /* 丢弃缓冲（重连后用） */
void esp_uart_write(uint8_t *buf,uint16_t len);  /* 阻塞写字节 */
void esp_uart_puts(const char *str);  /* 发送字符串 + 自动补 "\r\n" */
#endif