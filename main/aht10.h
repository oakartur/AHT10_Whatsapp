#ifndef AHT10_H
#define AHT10_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"
#include <time.h>

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
#define SENSOR_BUFFER_SIZE 512


typedef struct {
    float temperature;
    float humidity;
    time_t timestamp;
} sensor_data_t;

extern sensor_data_t *sensor_buffer;
extern int buffer_head;
extern int buffer_tail;


//functions prototypes
bool i2c_master_init();
bool aht10_init(i2c_port_t i2c_port);
bool aht10_read(i2c_port_t i2c_port);

// Circular buffer functions
bool buffer_init();
void buffer_free();
void buffer_insert(sensor_data_t data);
bool buffer_retrieve(sensor_data_t *data);

#endif //AHT10_H