#include "bsp_esp_uart.h"
#include "bsp_esp_uart.h"
#include "usart.h"      /* huart2 */
#include <string.h>

//环形缓冲
#define ESP_UART_BUF_SIZE   256u
#define ESP_UART_BUF_MASK   (ESP_UART_BUF_SIZE - 1u)

static uint8_t           s_buf[ESP_UART_BUF_SIZE];
static volatile uint16_t s_head = 0;    /* 只由中断写 */
static volatile uint16_t s_tail = 0;    /* 只由主循环写 */
static uint8_t           s_rx_byte;     /* HAL_UART_Receive_IT 的落点 */

void esp_uart_init(void){
    s_head = 0;
    s_tail = 0;
    HAL_UART_Receive_IT(&huart2,&s_rx_byte,1);
}

uint16_t esp_uart_available(void){
    return (uint16_t)((s_head - s_tail) & ESP_UART_BUF_MASK);
}
uint16_t esp_uart_read(uint8_t *dst,uint16_t max){
    uint16_t head = s_head;     /* volatile 16 位读是原子的，先快照 */
    uint16_t tail = s_tail;
    uint16_t n = 0;
    while((tail != head)&& (n<max)){
        dst[n++] = s_buf[tail];     /* 取出一个字节*/
        tail = (uint16_t)((tail +1u)&ESP_UART_BUF_MASK);    /* 更新尾指针 */
    }
    s_tail = tail;  /* 更新尾指针 */
    return n;
}
void esp_uart_flush(void){
    s_tail = s_head;  /* 丢弃缓冲 */
	
}

void esp_uart_write(uint8_t *buf,uint16_t len){
    HAL_UART_Transmit(&huart2,(uint8_t *)buf,len,1000u);
}

void esp_uart_puts(const char *str){
    HAL_UART_Transmit(&huart2, (uint8_t *)str, (uint16_t)strlen(str), 1000u);
    HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2u, 1000u);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        uint16_t next = (uint16_t)((s_head + 1u) & ESP_UART_BUF_MASK);
        if (next != s_tail)          /* 未满才存；满了丢这个字节 */
        {
            s_buf[s_head] = s_rx_byte;   /* 先写数据 */
            s_head = next;               /* 后发布 head，顺序不能反 */
        }
        HAL_UART_Receive_IT(&huart2, &s_rx_byte, 1);
    }
}

/* 出错回调：不写这个，USART2 一出错（溢出/帧错）RX 就永久停摆 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
        huart->RxState = HAL_UART_STATE_READY;
        HAL_UART_Receive_IT(&huart2, &s_rx_byte, 1);
    }
}