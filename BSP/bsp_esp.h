#ifndef __BSP_ESP_H
#define __BSP_ESP_H

#include "main.h"
// 配置
#define ESP_SERVER_IP   "YOUR IP"
#define ESP_SERVER_PORT 8888
#define ESP_WIFI_SSID "YOUR WIFI"
#define ESP_WIFI_PASS "YOUR PASSWORD"
#define ESP_PAYLOAD_MAX 96u
#define ESP_ST_TCP_LAST ESP_ST_SEND_DATA
/* 调试日志开关：1 = 把所有 AT 收发打到 USART1（串口助手可见） */
#define ESP_DEBUG 1

// 状态机
typedef enum
{
    ESP_ST_BOOT = 0, /* 上电，等模块启动 */
    ESP_ST_AT,       /* "AT" 探测 */
    ESP_ST_ATE0,     /* 关回显 */
    ESP_ST_CWMODE,   /* 设为 Station 模式 */
    ESP_ST_CWJAP,    /* 连 WiFi（慢，3~7 秒） */
    /* TCP阶段状态 */
    ESP_ST_CIPMUX,   /* AT+CIPMUX=0 */
    ESP_ST_CIPMODE,  /* AT+CIPMODE=0 */
    ESP_ST_CIPSTART,  /* AT+CIPSTART 连 TCP */
    ESP_ST_TCP_WAIT,  /* TCP 连不上：退避等待后重试（不重连 WiFi） */
    ESP_ST_TCP_CLOSE, /* 主动断开 TCP（发送失败后重连前先断干净） */
    ESP_ST_TCP_OK,    /* TCP 已连上，空闲（可发数据） */
    ESP_ST_SEND_LEN,  /* 已发 AT+CIPSEND=<len>，等 '>' */
    ESP_ST_SEND_DATA, /* 数据已发出，等 SEND OK */

    ESP_ST_FAIL,     /* 异常出口，不是"阶段"，放最后 */
} esp_state_t;

void esp_init(void);
void esp_task(void); /* 主循环反复调用 */
esp_state_t esp_get_state(void);
const char *esp_state_str(void); /* 给 OLED 显示用 */
uint8_t esp_is_tcp_up(void);
uint8_t  esp_send(const uint8_t *data, uint16_t len);  /* 0=已受理  1=没连上/忙/太长 */
uint32_t esp_get_ok_count(void);                       /* 成功发送数 */
uint32_t esp_get_err_count(void);                      /* 发送失败数 */
#endif