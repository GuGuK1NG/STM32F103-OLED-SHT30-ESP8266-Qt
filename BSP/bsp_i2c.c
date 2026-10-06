#include "bsp_i2c.h"
#include "bsp_dwt.h"

static void i2c_delay(void)
{
    dwt_delay_us(I2C_DELAY_US);
}

void i2c_soft_init(i2c_soft_t *bus)
{
    GPIO_InitTypeDef gi = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    gi.Mode = GPIO_MODE_OUTPUT_OD;
    gi.Pull = GPIO_PULLUP;
    gi.Speed = GPIO_SPEED_FREQ_HIGH;

    gi.Pin = bus->scl_pin;
    HAL_GPIO_Init(bus->scl_port, &gi);

    gi.Pin = bus->sda_pin;
    HAL_GPIO_Init(bus->sda_port, &gi);

    SCL_H(bus);
    SDA_H(bus);
    i2c_soft_bus_recover(bus);
}

void i2c_soft_bus_recover(i2c_soft_t *bus)
{
    uint8_t i;

    SDA_H(bus);
    SCL_H(bus);
    i2c_delay();

    /* 只有 SDA 被从机拉死时才需要恢复 */
    if (SDA_IN(bus) == GPIO_PIN_RESET)
    {
        for (i = 0; i < 9; i++)
        {
            SCL_L(bus);
            i2c_delay();
            SCL_H(bus);
            i2c_delay();
        }
        SDA_L(bus);
        i2c_delay();
        SCL_H(bus);
        i2c_delay();
        SDA_H(bus);
        i2c_delay();
    }
}

void i2c_soft_start(i2c_soft_t *bus)
{
    SDA_H(bus);
    SCL_H(bus);
    i2c_delay();
    SDA_L(bus); // SCL高时 SDA下降沿 代表开始
    i2c_delay();
    SCL_L(bus);
    i2c_delay();
}

void i2c_soft_stop(i2c_soft_t *bus)
{
    SDA_L(bus);
    SCL_L(bus);
    i2c_delay();
    SCL_H(bus);
    i2c_delay();
    SDA_H(bus); /* SCL 高时 SDA上升沿代表停止 */
    i2c_delay();
}

uint8_t i2c_soft_send_byte(i2c_soft_t *bus, uint8_t byte)
{
    uint8_t i, ack;
    for (i = 0; i < 8; ++i)
    {
        if (byte & 0x80)
            SDA_H(bus);
        else
            SDA_L(bus);
        i2c_delay();
        SCL_H(bus);
        i2c_delay();
        SCL_L(bus);
        i2c_delay();

        byte <<= 1;
    }

    SDA_H(bus);
    i2c_delay();
    SCL_H(bus);
    i2c_delay();
    ack = SDA_IN(bus) == GPIO_PIN_RESET ? 0 : 1;
    SCL_L(bus);
    i2c_delay();

    return ack; // 0=ack  1=no ack
}

uint8_t i2c_soft_read_byte(i2c_soft_t *bus, uint8_t ack)
{
    uint8_t i, byte = 0;
    SDA_H(bus); // 给从机驱动
    for (i = 0; i < 8; ++i)
    {
        byte <<= 1;
        i2c_delay();

        SCL_H(bus);
        i2c_delay();
        if (SDA_IN(bus) == GPIO_PIN_SET)
            byte |= 0x01;
        SCL_L(bus);
        i2c_delay();
    }

    if (ack)
        SDA_L(bus);
    else
        SDA_H(bus);
    i2c_delay();
    SCL_H(bus);
    i2c_delay();
    SCL_L(bus);
    i2c_delay();
    SDA_H(bus);

    return byte;
}
// 0=成功  1=从机没应答  2=访问寄存器地址/命令没应答  3=data没应答
uint8_t i2c_soft_write_reg(i2c_soft_t *bus, uint8_t dev, uint8_t reg, uint8_t data)
{
    i2c_soft_start(bus);
    if (i2c_soft_send_byte(bus, dev & 0xFE))
    {
        i2c_soft_stop(bus);
        return 1;
    }
    if (i2c_soft_send_byte(bus, reg))
    {
        i2c_soft_stop(bus);
        return 2;
    }
    if (i2c_soft_send_byte(bus, data))
    {
        i2c_soft_stop(bus);
        return 3;
    }
    i2c_soft_stop(bus);
    return 0;
}

uint8_t i2c_soft_read_regs(i2c_soft_t *bus, uint8_t dev, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    i2c_soft_start(bus);
    uint16_t i;
    if (i2c_soft_send_byte(bus, dev & 0xFE))
    {
        i2c_soft_stop(bus);
        return 1;
    }
    if (i2c_soft_send_byte(bus, reg))
    {
        i2c_soft_stop(bus);
        return 2;
    }

    i2c_soft_start(bus);
    if (i2c_soft_send_byte(bus, dev |= 0x01))
    {
        i2c_soft_stop(bus);
        return 3;
    }

    for (i = 0; i < len; ++i)
    {
        buf[i] = i2c_soft_read_byte(bus, (i < len - 1) ? 1 : 0); // 最后一个字节发 NACK
    }
    i2c_soft_stop(bus);
    return 0;
}

uint8_t i2c_soft_write_cmd16(i2c_soft_t *bus, uint8_t dev, uint16_t cmd)
{
    i2c_soft_start(bus);
    if (i2c_soft_send_byte(bus, dev & 0xFE))
    {
        i2c_soft_stop(bus);
        return 1;
    }
    if (i2c_soft_send_byte(bus, (uint8_t)(cmd >> 8)))
    {
        i2c_soft_stop(bus);
        return 2;
    }
    if (i2c_soft_send_byte(bus, (uint8_t)(cmd & 0xFF)))
    {
        i2c_soft_stop(bus);
        return 3;
    }

    i2c_soft_stop(bus);
    return 0;
}

uint8_t i2c_soft_read_bytes(i2c_soft_t *bus, uint8_t dev, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    i2c_soft_start(bus);
    if (i2c_soft_send_byte(bus, dev | 0x01))
    {
        i2c_soft_stop(bus);
        return 1;
    }
    for (i = 0; i < len; i++){
        buf[i] = i2c_soft_read_byte(bus, (i < len - 1) ? 1 : 0); /* 最后一字节 NACK */
    }
    i2c_soft_stop(bus);
    return 0;
}
