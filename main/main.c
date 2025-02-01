#include "wifi.h"
#include "aht10.h"
#include "esp_log.h"
#include "clock.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "cJSON.h"


#define I2C_PORT I2C_NUM_0
#define SSID "Iwakura Carvalho"
#define PASSWORD "internet007"
#define HARDWARE_NAME "PINTEIRO"

static const char *TAG = "main";
static QueueHandle_t sensor_queue;

typedef struct {
    float temperature;
    float humidity;
} sensor_data_t;

void sensor_task(void *pvParameter) {
    sensor_data_t data;
    
    if (!aht10_init(I2C_PORT)) {
        ESP_LOGE(TAG, "Failed to initialize AHT10 sensor");
        vTaskDelete(NULL);
    }
    
    while (1) {
        if (aht10_read(I2C_PORT, &data.temperature, &data.humidity)) {
            ESP_LOGI(TAG, "Temperature: %.2f°C, Humidity: %.2f%%", data.temperature, data.humidity);
            xQueueSend(sensor_queue, &data, portMAX_DELAY);
        } else {
            ESP_LOGE(TAG, "Failed to read sensor data");
        }
        vTaskDelay(pdMS_TO_TICKS(60000)); // Envia dados a cada 60 segundos
    }
}

void wifi_task(void *pvParameter) {
    sensor_data_t data;
    char message[256];

    while (1) {
        if (xQueueReceive(sensor_queue, &data, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Connecting to WiFi...");
            wifi_init_sta(SSID, PASSWORD);
            
            EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
            if (bits & WIFI_CONNECTED_BIT) {
                ESP_LOGI(TAG, "Connected to WiFi!");
                mqtt_app_start(); // Start MQTT client after Wi-Fi connection
                // Wait for MQTT connection
                obtain_time();
                vTaskDelay(pdMS_TO_TICKS(2000)); // Adjust delay as needed
                
                time_t now;
                struct tm timeinfo;
                time(&now);
                localtime_r(&now, &timeinfo);
                char timeStr[64];
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%S", &timeinfo);

                // Format the JSON message
                snprintf(message, sizeof(message),
                         "{\"hardware\": \"%s\", \"timestamp\": \"%s\", \"temperature\": %.2f, \"humidity\": %.2f}",
                         HARDWARE_NAME, timeStr, data.temperature, data.humidity);

                mqtt_publish_data(message);
            } else {
                ESP_LOGE(TAG, "Failed to connect to WiFi.");
            }
            
            ESP_LOGI(TAG, "Disconnecting from MQTT...");
            esp_mqtt_client_stop(mqtt_client);

            ESP_LOGI(TAG, "Disconnecting from WiFi...");
            wifi_disconnect();
        }
    }
}

void app_main(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (!i2c_master_init()) {
        ESP_LOGE(TAG, "Failed to initialize I2C");
        return;
    }

    sensor_queue = xQueueCreate(5, sizeof(sensor_data_t));
    if (sensor_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }

    xTaskCreate(&sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(&wifi_task, "wifi_task", 4096, NULL, 5, NULL);
}