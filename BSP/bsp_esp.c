#include "bsp_esp.h"
#include "bsp_esp_uart.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define ESP_LINE_MAX   128u      /* 单行应答最大长度 */

/* 各状态的超时时间（毫秒） */
#define T_BOOT      1500u
#define T_AT         500u
#define T_ATE0      1000u
#define T_CWMODE    1000u
#define T_CWJAP    15000u
#define T_FAIL      3000u
#define T_CIPMUX     1000u
#define T_CIPMODE    1000u
#define T_CIPSTART   8000u
#define T_TCP_BACKOFF 2000u   /* TCP 重连退避基数：2s→4s→8s→16s 封顶 */
#define TCP_RETRY_MAX 10u     /* TCP 连续失败这么多次后，从头把 WiFi 也对齐一遍 */
#define T_PROMPT 2000u /*等待 > 提示符*/
#define T_SEND_OK 3000u /*等待 "SEND OK"*/

/* 状态机上下文 */
static struct {
    esp_state_t st;
    uint32_t    t_enter;      /* 进入本状态的时刻 */
    uint8_t     cmd_sent;     /* 本状态的命令是否已发出 */
    uint8_t     tries;        /* 当前状态的重试次数 */
    uint8_t     tcp_retry;    /* TCP 连续失败次数（跨状态保留，用来算退避） */

    char        line[ESP_LINE_MAX];   /* 行缓冲 */
    uint16_t    line_len;

    /* token 标志：由 esp_poll_rx 置位，每轮 esp_task 开头清掉 */
    uint8_t     got_ok;
    uint8_t     got_error;
    uint8_t     got_fail;

    uint8_t     got_connect;     /* 收到 "CONNECT" */
    uint8_t     got_closed;      /* 收到 "CLOSED" */
    uint8_t     got_wifi_disc;   /* 收到 "WIFI DISCONNECT" */

    uint8_t     got_prompt;      /* 收到 '>' */
    uint8_t     got_send_ok;     /* 收到 "SEND OK" */
    uint8_t     got_send_fail;   /* 收到 "SEND FAIL" */

    /* 发送队列：同一时刻只允许一个包 */
    uint8_t     pending;         /* 1 = 有待发数据 */
    char        payload[ESP_PAYLOAD_MAX];
    uint16_t    payload_len;

    uint32_t    ok_count;        /* 成功发送计数 */
    uint32_t    err_count;       /* 发送失败计数 */
} esp;

/* ---------- 内部工具 ---------- */

static void esp_clear_tokens(void)
{
    esp.got_ok    = 0;
    esp.got_error = 0;
    esp.got_fail  = 0;
    esp.got_connect   = 0;
    esp.got_closed    = 0;
    esp.got_wifi_disc = 0;
    esp.got_prompt = 0;
    esp.got_send_ok = 0;
    esp.got_send_fail = 0;
}

/* 切换状态（并丢掉上一状态可能残留的字节） */
static void esp_go(esp_state_t next)
{
    esp.st       = next;
    esp.cmd_sent = 0;
    esp.tries    = 0;
    esp.t_enter  = HAL_GetTick();
    esp_uart_flush();
}

/* 进入新状态但「不发命令」—— 给"命令已经发过"的转移用（如 CIPSEND 之后）。
   和 esp_go 的区别：① cmd_sent 由参数决定 ② 不 flush！
   发完数据后 SEND OK 可能已在路上，flush 会把它冲掉。 */
static void esp_enter(esp_state_t next, uint8_t cmd_sent)
{
    esp.st       = next;
    esp.cmd_sent = cmd_sent;
    esp.t_enter  = HAL_GetTick();
}

/* 同一状态内重发命令 */
static void esp_resend(void)
{
    esp.cmd_sent = 0;
    esp.t_enter  = HAL_GetTick();
}

/* 非阻塞超时判断（可安全跨回绕） */
static uint8_t esp_timeout(uint32_t ms)
{
    return (HAL_GetTick() - esp.t_enter) >= ms;
}

/* 发送一条 AT 命令（自动补 "\r\n"），并记录下来 */
static void esp_send_cmd(const char *fmt, ...)
{
    char    buf[128];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    esp_uart_write((uint8_t *)buf, (uint16_t)strlen(buf));
    esp_uart_write((uint8_t *)"\r\n", 2u);

    esp.cmd_sent = 1;
    esp.t_enter  = HAL_GetTick();

#if ESP_DEBUG
    printf("[TX] %s\r\n", buf);
#endif
}

/* ---------- 接收与解析 ---------- */

/* 收到一整行，判断它是什么 */
static void esp_match_line(const char *s)
{
#if ESP_DEBUG
    printf("[RX] %s\r\n", s);
#endif
    if  (strcmp(s,"SEND OK")==0)    esp.got_send_ok=1;
    else if (strstr(s,"SEND FAIL")!=0) esp.got_send_fail=1;
    else if (strcmp(s, "OK") == 0)          esp.got_ok    = 1;
    else if (strstr(s, "ERROR") != 0)       esp.got_error = 1;
    else if (strstr(s, "FAIL")  != 0)       esp.got_fail  = 1;
    else if (strcmp(s, "CONNECT") == 0)           esp.got_connect   = 1;
    else if (strstr(s, "ALREADY CONNECTED") != 0) esp.got_connect   = 1;
    else if (strcmp(s, "CLOSED")  == 0)           esp.got_closed    = 1;
    else if (strstr(s, "WIFI DISCONNECT") != 0)   esp.got_wifi_disc = 1;
    /* 回显行、"busy p..."、"WIFI GOT IP" 等一律忽略 */
}

/* 把环形缓冲里的字节全部吃掉，按 '\n' 切成行 */
static void esp_poll_rx(void)
{
    uint8_t ch;

    while (esp_uart_read(&ch, 1u) == 1u)
    {
        if (ch == '>')                 /* 提示符不带换行，必须按裸字符识别 */
        {
            esp.line_len = 0;
            esp.got_prompt = 1;
            continue;
        }
        if (ch == '\n')                /* 一行结束 → 交给匹配器 */
        {
            esp.line[esp.line_len] = '\0';
            if (esp.line_len > 0u) esp_match_line(esp.line);
            esp.line_len = 0;
            continue;
        }
        if (ch == '\r') continue;      /* 丢掉 CR */

        if (esp.line_len < (ESP_LINE_MAX - 1u))
            esp.line[esp.line_len++] = (char)ch;
        else
            esp.line_len = 0;          /* 行太长：丢弃整行，防止后续错位 */
    }
}

/* 判断"TCP 阶段"是否已经开始 */
static uint8_t esp_in_tcp_stage(void)
{
    return (esp.st >= ESP_ST_CIPMUX && esp.st <= ESP_ST_TCP_LAST) ? 1u : 0u;
}

/* 「我以为 TCP 连接还在」的状态 —— CLOSED 只在这几种状态下需要处理。
   ⚠ 不能写成"在 TCP 阶段就处理"：模块在 CIPSTART 失败后也会补发 CLOSED，
   那会把 ESP_ST_TCP_WAIT 的退避踢掉，变成全速空转。
   ⚠ 也不能只写 ESP_ST_TCP_OK：SEND OK 和 CLOSED 可能落在同一轮 poll 里，
   那时状态还是 ESP_ST_SEND_DATA，漏掉就只能走"慢路径"（多丢 4~5 包）。 */
static uint8_t esp_link_believed_up(void)
{
    return (esp.st == ESP_ST_TCP_OK ||
            esp.st == ESP_ST_SEND_LEN ||
            esp.st == ESP_ST_SEND_DATA) ? 1u : 0u;
}

/* 异步事件都被优先处理 */
static void esp_handle_async(void)
{
    /* WiFi 掉线 —— 最严重，不管在哪一步都回去重连 WiFi */
    if (esp.got_wifi_disc && esp_in_tcp_stage())
    {
        esp.tcp_retry = 0;
        esp_go(ESP_ST_CWJAP);       /* 回到连 WiFi（esp_go 内部已清缓冲） */
        return;
    }
    /* TCP 被对端关闭 —— 只有"我以为连接还在"时才需要处理 */
    if (esp.got_closed && esp_link_believed_up())
    {
        esp.pending = 0;            /* 这一包发不出去了，扔掉（别让它变成重传的重复包） */
        esp_go(ESP_ST_CIPSTART);    /* 只重连 TCP，不动 WiFi */
        return;
    }
}

void esp_init(void){
    memset(&esp, 0, sizeof esp);
    esp.st = ESP_ST_BOOT;
    esp.t_enter = HAL_GetTick();
    esp_uart_flush();
}

void esp_task(void)
{
    esp_clear_tokens();     /* 先清零，本轮接收到的才有效 */
    esp_poll_rx();
    esp_handle_async();     /* 先处理异步事件，优先级最高 */
    switch (esp.st)
    {
    case ESP_ST_BOOT:
        if (esp_timeout(T_BOOT)) esp_go(ESP_ST_AT);
        break;

    case ESP_ST_AT:
        if      (!esp.cmd_sent)    esp_send_cmd("AT");
        else if (esp.got_ok)       esp_go(ESP_ST_ATE0);
        else if (esp_timeout(T_AT) || esp.got_error)
        {
            if (++esp.tries > 5u)  esp_go(ESP_ST_FAIL);
            else                   esp_resend();
        }
        break;

    case ESP_ST_ATE0:
        if      (!esp.cmd_sent)    esp_send_cmd("ATE0");
        else if (esp.got_ok)       esp_go(ESP_ST_CWMODE);
        else if (esp_timeout(T_ATE0) || esp.got_error)
        {
            if (++esp.tries > 3u)  esp_go(ESP_ST_FAIL);
            else                   esp_resend();
        }
        break;

    case ESP_ST_CWMODE:
        if      (!esp.cmd_sent)    esp_send_cmd("AT+CWMODE=1");
        else if (esp.got_ok)       esp_go(ESP_ST_CWJAP);
        else if (esp_timeout(T_CWMODE) || esp.got_error)
        {
            if (++esp.tries > 3u)  esp_go(ESP_ST_FAIL);
            else                   esp_resend();
        }
        break;

    case ESP_ST_CWJAP:
        if (!esp.cmd_sent)
            esp_send_cmd("AT+CWJAP=\"%s\",\"%s\"", ESP_WIFI_SSID, ESP_WIFI_PASS);
        else if (esp.got_ok)
        {
            esp.tcp_retry = 0;      /* 新的一轮，TCP 重试计数清零 */
            esp_go(ESP_ST_CIPMUX);
        }
        else if (esp_timeout(T_CWJAP) || esp.got_error || esp.got_fail)
        {
            if (++esp.tries >= 3u) esp_go(ESP_ST_FAIL);
            else                   esp_resend();
        }
        break;

    case ESP_ST_FAIL:
        if (esp_timeout(T_FAIL)) esp_go(ESP_ST_AT);
        break;

    case ESP_ST_CIPMUX:
        if      (!esp.cmd_sent)               esp_send_cmd("AT+CIPMUX=0");
        else if (esp.got_ok)                  esp_go(ESP_ST_CIPMODE);
        else if (esp_timeout(T_CIPMUX) || esp.got_error)
        {
            if (++esp.tries > 3u) esp_go(ESP_ST_FAIL);
            else                  esp_resend();
        }
        break;

    case ESP_ST_CIPMODE:
        if      (!esp.cmd_sent)               esp_send_cmd("AT+CIPMODE=0");
        else if (esp.got_ok)                  esp_go(ESP_ST_CIPSTART);
        else if (esp_timeout(T_CIPMODE) || esp.got_error)
        {
            if (++esp.tries > 3u) esp_go(ESP_ST_FAIL);
            else                  esp_resend();
        }
        break;

    case ESP_ST_CIPSTART:
        if (!esp.cmd_sent)
            esp_send_cmd("AT+CIPSTART=\"TCP\",\"%s\",%u",
                         ESP_SERVER_IP, (unsigned)ESP_SERVER_PORT);
        else if (esp.got_connect || esp.got_ok)
        {
            esp.tcp_retry = 0;
            esp_go(ESP_ST_TCP_OK);
        }
        else if (esp_timeout(T_CIPSTART) || esp.got_error || esp.got_fail)
        {
            /* WiFi 是好的，只是服务器连不上（没开 / 防火墙拦了 / IP 写错）。
               退避后重试 CIPSTART 即可，绝不重连 WiFi —— 重连 WiFi 要 3~7 秒，白等。 */
            if (esp.tcp_retry < 255u) esp.tcp_retry++;
            esp_go(ESP_ST_TCP_WAIT);
        }
        break;

    case ESP_ST_TCP_WAIT:
        /* 指数退避：2s → 4s → 8s → 16s（封顶），到点重试 CIPSTART */
        if (esp.tcp_retry >= TCP_RETRY_MAX)
        {
            esp.tcp_retry = 0;
            esp_go(ESP_ST_CWJAP);      /* 试太多次了，从头把 WiFi 也对齐一遍 */
        }
        else
        {
            uint8_t sh = (esp.tcp_retry > 0u) ? (uint8_t)(esp.tcp_retry - 1u) : 0u;
            if (sh > 3u) sh = 3u;
            if (esp_timeout(T_TCP_BACKOFF << sh)) esp_go(ESP_ST_CIPSTART);
        }
        break;

    case ESP_ST_TCP_OK:
        if (esp.pending) esp_go(ESP_ST_SEND_LEN);
        break;

    case ESP_ST_SEND_LEN:
        if(!esp.cmd_sent){
            esp_send_cmd("AT+CIPSEND=%u",(unsigned)esp.payload_len);
        }
        else if(esp.got_prompt){
            /*收到 '>' 裸发数据，不加 \r\n*/
            esp_uart_write((uint8_t *)esp.payload,esp.payload_len);
#if ESP_DEBUG
            printf("[TX-D] %u bytes\r\n", (unsigned)esp.payload_len);
#endif
            esp_enter(ESP_ST_SEND_DATA, 1u);   /* 不能用 esp_go，会 flush 掉 SEND OK */
        }
        else if(esp_timeout(T_PROMPT)){
            ++esp.err_count;
            esp.pending=0;
            if(esp.tcp_retry<255u){
                ++esp.tcp_retry;
            }
            esp_go(ESP_ST_TCP_CLOSE);//断开后重连
        }

        break;  
    case ESP_ST_SEND_DATA:
        if(esp.got_send_ok){
            ++esp.ok_count;
            esp.pending=0;
            esp_go(ESP_ST_TCP_OK);
        }
        else if(esp.got_error||esp.got_fail||esp_timeout(T_SEND_OK)){
            ++esp.err_count;
            esp.pending=0;
            if(esp.tcp_retry<255u){
                ++esp.tcp_retry;
            }
            esp_go(ESP_ST_TCP_CLOSE); /*断开后再重连*/
        }
        break;
    case ESP_ST_TCP_CLOSE:
        if(!esp.cmd_sent){
            esp_send_cmd("AT+CIPCLOSE");
        }
        else if (esp.got_error||esp.got_ok){
            esp_go(ESP_ST_TCP_WAIT);
        }
        else if(esp_timeout(T_CIPMUX)){
            esp_go(ESP_ST_TCP_WAIT);
        }
        break;
    default:
        esp_go(ESP_ST_AT);
        break;
    }
}

esp_state_t esp_get_state(void) { return esp.st; }

uint8_t esp_is_tcp_up(void) { return (esp.st == ESP_ST_TCP_OK) ? 1u : 0u; }

const char *esp_state_str(void)
{
    switch (esp.st)
    {
    case ESP_ST_BOOT:    return "BOOT";
    case ESP_ST_AT:      return "AT";
    case ESP_ST_ATE0:    return "ATE0";
    case ESP_ST_CWMODE:  return "WMODE";
    case ESP_ST_CWJAP:   return "JOIN";
    case ESP_ST_FAIL:    return "FAIL";
    case ESP_ST_CIPMUX:   return "MUX";
    case ESP_ST_CIPMODE:  return "NORM";
    case ESP_ST_CIPSTART: return "CONN";
    case ESP_ST_TCP_WAIT: return "WAIT";
    case ESP_ST_TCP_OK:   return "TCP";
    case ESP_ST_TCP_CLOSE: return "CLOSE";
    case ESP_ST_SEND_LEN: return "SEND";
    case ESP_ST_SEND_DATA: return "TX";
    default:             return "?";
    }
}

uint8_t esp_send(const uint8_t *data,uint16_t len){
    if (esp.st != ESP_ST_TCP_OK)
    {
        return 1u;
    } /*还没有连接上*/
    if (esp.pending)
    {
        return 1u;
    } /*还有没发送完的数据*/
    if (len > ESP_PAYLOAD_MAX)
    {
        return 1u;
    } /*数据太长*/
    memcpy(esp.payload, data, len);
    esp.payload_len = len;
    esp.pending = 1u;
    return 0u;
}

uint32_t esp_get_ok_count(void)  { return esp.ok_count; }
uint32_t esp_get_err_count(void) { return esp.err_count; }

