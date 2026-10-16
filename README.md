# STM32 智能温湿度传感器（WiFi 联网 + Qt 上位机）

基于 **STM32F103C8T6** 的温湿度采集与远程监控系统。

**SHT30 采集** → **OLED 本地显示** → **ESP-01S 通过 WiFi 上传** → **PC 端 Qt 上位机**实时显示曲线、统计丢包、落盘 CSV。

---

## 系统架构

```
        ┌──────────────┐
        │    SHT30     │  温湿度传感器
        └──────┬───────┘
               │ 软件 IIC (PB6/PB7)
               ▼
┌──────────────────────────────┐
│        STM32F103C8T6         │  ← 非阻塞状态机调度
│                              │
│  ┌────────────┐              │
│  │ SSD1306 OLED│  本地显示    │
│  └────────────┘  软件 IIC    │
│                   (PB8/PB9)  │
│                              │
│  USART1 (PA9/PA10) ──→ 调试日志（printf）
│  USART2 (PA2/PA3)  ──→ ESP-01S
└──────────────┬───────────────┘
               │ AT 指令
               ▼
        ┌──────────────┐
        │   ESP-01S    │  ESP8266 + AT 固件
        └──────┬───────┘
               │ WiFi (TCP Client)
               ▼
        ┌──────────────┐
        │  手机热点/路由器 │
        └──────┬───────┘
               │ TCP
               ▼
┌──────────────────────────────┐
│      Qt 上位机 (TCP Server)   │
│                              │
│  数值面板 │ 实时曲线 │ 丢包统计 │ CSV
└──────────────────────────────┘
```

---

## 功能特性

### 固件侧（STM32）

- **SHT30 温湿度采集** —— 软件 IIC + CRC8 校验，非阻塞状态机（不占用 CPU 等待）
- **SSD1306 OLED 显示** —— 128×64，带脏页标记，只刷新变化的页
- **ESP-01S 联网** —— 纯 AT 指令驱动，全非阻塞状态机
  - 连 WiFi（`AT+CWJAP`）
  - TCP 客户端（`AT+CIPSTART`）
  - 数据发送（`AT+CIPSEND=<len>`）
  - **失败分级处理**：临时性失败（服务器没开）指数退避重试，配置性错误才从头重来
  - **断线分级重连**：WiFi 掉线走完整流程，TCP 掉线只重连 TCP
  - **异步事件处理**：`WIFI DISCONNECT` / `CLOSED` 在任意状态都能被捕获
- **1 秒一包上传**，JSON 格式，`seq` 包号无条件递增（方便上位机算丢包率）

### 上位机（Qt 6）

- **TCP 服务器**，自动接受连接
- **数值面板** —— 温度 / 湿度大字显示
- **实时曲线** —— Qt Charts，双 Y 轴（温度左 / 湿度右），X 轴为时间
- **统计** —— 收包数 / 丢包数（按 `seq` 缺口算）/ 坏帧数
- **CSV 落盘** —— 每次运行一个文件，可直接用 Excel 打开
- **连接状态显示** —— 客户端连上 / 断开时实时更新
- **应用层心跳** —— 每秒一包本身就是心跳。距上次收到数据超过 3 秒判定为「数据超时」，
  超过 30 秒主动断开连接（腾出位置给新连接）

---

## 硬件清单

| 部件 | 型号 | 说明 |
|---|---|---|
| 主控 | STM32F103C8T6 | Blue Pill 最小系统板，72MHz |
| 温湿度传感器 | SHT30 | IIC，地址 0x88 |
| 显示屏 | SSD1306 OLED | 128×64，IIC，地址 0x78 |
| WiFi 模块 | ESP-01S | ESP8266 + AT 固件 |
| 调试器 | STLINK V2.1 | SWD 烧录 + 虚拟串口当调试口 |

### 引脚分配

| 引脚 | 功能 |
|---|---|
| PA9 / PA10 | USART1 TX/RX —— 调试日志（接 STLINK 虚拟串口） |
| PA2 / PA3 | USART2 TX/RX —— ESP-01S |
| PB6 / PB7 | 软件 IIC —— SHT30 |
| PB8 / PB9 | 软件 IIC —— OLED |
| PA13 / PA14 | SWD —— STLINK |
| PC13 | LED 心跳 |

---

## 数据协议

单行 JSON，以 `\n` 结尾，每秒一包：

```json
{"seq":123,"t":25.34,"h":60.12}
```

| 字段 | 含义 |
|---|---|
| `seq` | 包号，`uint32`，**无条件递增**（发不出去也加一） |
| `t` | 温度，单位 °C，两位小数 |
| `h` | 湿度，单位 %，两位小数 |

> **关于 `seq` 无条件递增**：它记录的是"设备认为该发出的第几包"，不是"成功发出的第几包"。
> 这样上位机一算差值就知道**设备侧掉了多久的线**——把网络问题变成了可以直接观测的数字。

---

## 目录结构

```
PJ1/
├── Core/                  CubeMX 生成的 HAL 初始化代码
│   ├── Inc/
│   └── Src/               main.c / usart.c / gpio.c / stm32f1xx_it.c ...
├── BSP/                   板级驱动
│   ├── bsp_dwt.c/h        DWT 微秒延时
│   ├── bsp_i2c.c/h        软件 IIC（含总线恢复）
│   ├── bsp_oled.c/h       SSD1306 驱动（脏页刷新）
│   ├── bsp_sht30.c/h      SHT30 驱动（状态机版）
│   ├── bsp_esp_uart.c/h   USART2 环形缓冲 + 中断收发
│   └── bsp_esp.c/h        ESP-01S AT 状态机
├── App/                   应用层
│   └── app_frame.c/h      JSON 组帧 + 定点格式化
├── Drivers/               STM32 HAL 库 + CMSIS
├── MDK-ARM/               Keil 工程
└── QtServer/untitled/     Qt 6 上位机
    ├── CMakeLists.txt
    └── src/
        ├── tcpserver.*    网络接收 + 拆包
        ├── jsonparse.*    JSON 解析（纯函数）
        ├── chartwidget.*  曲线（Qt Charts）
        ├── datastore.*    CSV 落盘
        └── widget.*       主界面 + 协调
```

---

## 编译运行

### 固件

1. 用 **Keil MDK** 打开 `MDK-ARM/PJ1.uvprojx`
2. 修改 `BSP/bsp_esp.h` 顶部的配置：

```c
#define ESP_WIFI_SSID "你的WiFi名"      /* 注意 ESP8266 只支持 2.4G */
#define ESP_WIFI_PASS "你的WiFi密码"
#define ESP_SERVER_IP "上位机的IPv4地址"
#define ESP_SERVER_PORT 8888
```

3. 编译、用 STLINK 烧录

**环境要求**：Keil MDK + ARM Compiler 6 + MicroLIB（已配置在工程里）

> ⚠️ 工程启用了 **MicroLIB**，它**不支持 `%f` 浮点格式符**。所以温湿度是用**定点整数**（×100）拼装成 JSON 的，代码里找不到 `%f`。

### 上位机

1. 用 **Qt Creator** 打开 `QtServer/untitled/CMakeLists.txt`
2. 需要 **Qt 6 + Widgets + Network + Charts** 模块
3. 编译运行，监听 8888 端口

**环境要求**：Qt 6（开发时使用 6.11.1 + MinGW 64-bit）

---

## 关键技术点

### 固件侧

| 问题 | 做法 |
|---|---|
| WiFi 连接要 3~7 秒，不能阻塞主循环 | 全非阻塞状态机，超时用 `HAL_GetTick()` 差值比较 |
| 串口接收必须尽快返回中断 | 单字节中断 + 环形缓冲（单生产者单消费者，无需临界区） |
| MicroLIB 不支持 `%f` | 浮点 ×100 转定点整数，只打印整数 |
| TCP 连不上时疯狂重试刷屏 | 失败分级 + **指数退避**（2→4→8→16 秒封顶） |
| WiFi 掉线 vs TCP 掉线 | 靠 `WIFI DISCONNECT` / `CLOSED` 异步事件区分，**TCP 掉线不重连 WiFi** |
| 模块卡死后 RX 永久失效 | 重写 `HAL_UART_ErrorCallback`，清 ORE 标志并重新武装 |

### 上位机侧

| 问题 | 做法 |
|---|---|
| TCP 是字节流，会粘包/半包 | 累积缓冲 + 按 `\n` 切分 + 保留余量 |
| 曲线跑久了越来越卡 | 限制保留点数（超出窗口的旧点定期删除） |
| 设备重启后丢包统计算出巨大负数 | `seq` 回退时判定为设备重启，统计清零 |
| TCP 层发现不了对方"猝死"（断电不发 FIN） | **应用层心跳**：每秒一包，超 3 秒没数据即判定异常 |
| 「连接状态」与「数据状态」互相覆盖 | 两个标签分开：连接状态由 connect/disconnect 信号驱动，数据状态由心跳定时器驱动 |
| 分层解耦 | `TcpServer`(网络) / `parseSample`(纯函数) / `ChartWidget`(曲线) / `DataStore`(存储) / `Widget`(界面+协调) |

---

## 已知限制

- ESP-01S 的 WiFi 账号密码**硬编码**在 `bsp_esp.h`，换网络需要重新编译烧录
- 只支持**单个客户端**连接
- 上位机时间戳取自 PC 本地时间（STM32 无 RTC）
- server.py是临时测试服务器

---

## 备注

- 固件工程使用 **STM32CubeMX 6.3 + FW_F1 1.8.7** 生成
- 上位机工程由 Qt Creator 的 Widget 模板起步
