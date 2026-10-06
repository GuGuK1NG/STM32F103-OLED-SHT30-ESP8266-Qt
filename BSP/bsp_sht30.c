#include "bsp_sht30.h"

#define SHT30_CMD_MEASURE_HIGHREP 0x2400
#define SHT30_CMD_MEASURE_MEDREP 0x240B
#define SHT30_CMD_MEASURE_LOWREP 0x2416
#define SHT30_CMD_SOFT_RESET 0x30A2

static i2c_soft_t *sht30_bus = 0;
//状态机变量
static uint8_t  sht_st = 0;        /* 0=空闲  1=测量中 */
static uint32_t sht_t0;            /* 发命令的时刻 */
static float    sht_t, sht_h;
static uint8_t  sht_err = 0xFF;    /* 0xFF = 还没有数据 */

static uint8_t sht30_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF, i, b;
    for (b = 0; b < len; b++)
    {
        crc ^= data[b];
        for (i = 0; i < 8; i++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31)
                               : (uint8_t)(crc << 1);
    }
    return crc;
}

uint8_t sht30_init(i2c_soft_t *bus){
    sht30_bus = bus;
    return i2c_soft_write_cmd16(bus, SHT30_ADDR, SHT30_CMD_SOFT_RESET); //0成功 1失败
}
uint8_t sht30_read(float *temp, float *humi){
    uint8_t buf[6];
    uint16_t raw_t,raw_h;
    if(i2c_soft_write_cmd16(sht30_bus, SHT30_ADDR, SHT30_CMD_MEASURE_HIGHREP)!=0) return 1; //发送测量命令失败

    dwt_delay_ms(16); //等待测量完成
    if (i2c_soft_read_bytes(sht30_bus, SHT30_ADDR, buf, 6) != 0) return 2;

    if (sht30_crc8(&buf[0], 2) != buf[2]) return 3;
    if (sht30_crc8(&buf[3], 2) != buf[5]) return 4;

    raw_t = (uint16_t)((buf[0] << 8) | buf[1]);
    raw_h = (uint16_t)((buf[3] << 8) | buf[4]);

    *temp = -45.0f + 175.0f * (float)raw_t / 65535.0f;
    *humi = 100.0f * (float)raw_h / 65535.0f;

    return 0;
}

void sht30_start(void){
    if (sht_st != 0)
        return; // 忙碌中
    if(i2c_soft_write_cmd16(sht30_bus, SHT30_ADDR, SHT30_CMD_MEASURE_HIGHREP) == 0)
    {
        sht_t0=HAL_GetTick();
        sht_st=1;
    }
    else{
        sht_err=1;
    }
}
void sht30_task(void)
{
    uint8_t buf[6];
    uint16_t raw_t, raw_h;
    if (sht_st != 1)
        return; // 空闲中
    if (HAL_GetTick() - sht_t0 < 20)
        return; // 测量还没完成,CPU先去做别的事情
    sht_st = 0; // 无论成败都返回空闲
    if (i2c_soft_read_bytes(sht30_bus, SHT30_ADDR, buf, 6) != 0)
    {
        sht_st = 0;
        sht_err = 2;
        return;
    }
    if (sht30_crc8(&buf[0], 2) != buf[2])
    {
        sht_err = 3;
        return;
    }
    if (sht30_crc8(&buf[3], 2) != buf[5])
    {
        sht_err = 4;
        return;
    }

    raw_t = (uint16_t)((buf[0] << 8) | buf[1]);
    raw_h = (uint16_t)((buf[3] << 8) | buf[4]);

    sht_t = -45.0f + 175.0f * (float)raw_t / 65535.0f;
    sht_h = 100.0f * (float)raw_h / 65535.0f;
    sht_err = 0;
}
uint8_t sht30_get(float *temp, float *humi){
    *temp=sht_t;
    *humi=sht_h;
    return sht_err;
}