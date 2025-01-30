#ifndef AHT10_H
#define AHT10_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"

//I2C address of AHT10
#define AHT10_I2C_ADDRESS 0x38

//Setup commands for AHT10
#define AHT10_INIT 0xE1
#define AHT10_TRIGGER 0xAC
#define AHT10_SOFTRESET 0xBA
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_PORT I2C_NUM_0

//functions prototypes
bool i2c_master_init();
bool aht10_init(i2c_port_t i2c_port);
bool aht10_read(i2c_port_t i2c_port, float *temperature, float *humidity);

#endif //AHT10_H