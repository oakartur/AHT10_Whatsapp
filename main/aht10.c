#include "aht10.h"
#include "esp_log.h"
#include "driver/i2c.h"

static const char *TAG = "AHT10"; //Tag for logging (esp_log)

bool i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };

    esp_err_t err = i2c_param_config(I2C_PORT, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C config failed: %s", esp_err_to_name(err));
        return false;
    }

    err = i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "I2C initialized successfully");
    return true;
}

//initialize AHT10
bool aht10_init(i2c_port_t i2c_port){
    uint8_t init_cmd[3] = {AHT10_INIT, 0x08, 0x00};
    esp_err_t ret = i2c_master_write_to_device(i2c_port, AHT10_I2C_ADDRESS, init_cmd, sizeof(init_cmd), pdMS_TO_TICKS(1000));

    if(ret != ESP_OK){
        ESP_LOGE(TAG, "Failed to initialize AHT10: %s", esp_err_to_name(ret));
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    return true;
}

// Read temperature and humidity from the AHT10 sensor
bool aht10_read(i2c_port_t i2c_port, float *temperature, float *humidity) {
    uint8_t trigger_cmd[3] = {AHT10_TRIGGER, 0x33, 0x00}; // Trigger measurement command
    esp_err_t ret = i2c_master_write_to_device(i2c_port, AHT10_I2C_ADDRESS, trigger_cmd, sizeof(trigger_cmd), pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to trigger measurement: %s", esp_err_to_name(ret));
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(80)); // Wait for measurement to complete

    uint8_t data[6]; // Buffer to store sensor data
    ret = i2c_master_read_from_device(i2c_port, AHT10_I2C_ADDRESS, data, sizeof(data), pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read data: %s", esp_err_to_name(ret));
        return false;
    }

    // Convert raw data to temperature and humidity
    uint32_t raw_humidity = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | ((data[3] & 0xF0) >> 4);
    uint32_t raw_temperature = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

    *humidity = (raw_humidity * 100.0) / 0x100000; // Convert to percentage
    *temperature = (raw_temperature * 200.0 / 0x100000) - 50; // Convert to Celsius

    return true;
}