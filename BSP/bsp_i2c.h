#ifndef __BSP_I2C_H
#define __BSP_I2C_H
#include "gpio.h"


#define I2C_DELAY_US   5

#define SCL_H(b)   HAL_GPIO_WritePin((b)->scl_port, (b)->scl_pin, GPIO_PIN_SET)
#define SCL_L(b)   HAL_GPIO_WritePin((b)->scl_port, (b)->scl_pin, GPIO_PIN_RESET)
#define SDA_H(b)   HAL_GPIO_WritePin((b)->sda_port, (b)->sda_pin, GPIO_PIN_SET)
#define SDA_L(b)   HAL_GPIO_WritePin((b)->sda_port, (b)->sda_pin, GPIO_PIN_RESET)
#define SDA_IN(b)  HAL_GPIO_ReadPin((b)->sda_port,  (b)->sda_pin)

typedef struct {
    GPIO_TypeDef *scl_port;  uint16_t scl_pin;
    GPIO_TypeDef *sda_port;  uint16_t sda_pin;
} i2c_soft_t;



static void i2c_delay(void);
void i2c_soft_init(i2c_soft_t *bus);
void i2c_soft_bus_recover(i2c_soft_t *bus);
void i2c_soft_start(i2c_soft_t *bus);
void i2c_soft_stop(i2c_soft_t *bus);
uint8_t i2c_soft_send_byte(i2c_soft_t *bus,uint8_t byte);
uint8_t i2c_soft_read_byte(i2c_soft_t *bus,uint8_t ack);

uint8_t i2c_soft_write_reg(i2c_soft_t *bus, uint8_t dev, uint8_t reg, uint8_t data);

uint8_t i2c_soft_read_regs(i2c_soft_t *bus, uint8_t dev, uint8_t reg,
                           uint8_t *buf, uint16_t len);

uint8_t i2c_soft_write_cmd16(i2c_soft_t *bus, uint8_t dev, uint16_t cmd);
uint8_t i2c_soft_read_bytes(i2c_soft_t *bus, uint8_t dev, uint8_t *buf, uint16_t len);
#endif