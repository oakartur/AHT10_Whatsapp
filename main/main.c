#include "network.h"
#include "aht10.h"
#include "esp_log.h"
#include "clock.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "cJSON.h"

#define I2C_PORT I2C_NUM_0
#define SSID "Iwakura Carvalho"
#define PASSWORD "internet007"
#define OWNER_NAME "CREATOR"
#define HARDWARE_NAME "LAB"
#define SENSOR_PERIOD 5 // in minutes

static const char *TAG = "main";

// Event group for WiFi connection
#define WIFI_CONNECTED_BIT BIT0

// Event group for time synchronization
EventGroupHandle_t s_time_event_group;
#define TIME_SYNCED_BIT BIT0

// WiFi Manager Task: Tries to connect and sets WIFI_CONNECTED_BIT.
void wifi_manager_task(void *pvParameter) {
    while (1) {
        // Check if already connected.
        EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
        if (!(bits & WIFI_CONNECTED_BIT)) {
            ESP_LOGI(TAG, "WiFi Manager: Not connected. Attempting to connect...");
            wifi_init_sta(SSID, PASSWORD);
            // Wait for the connection for up to 10 seconds.
            bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                       pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
            if (bits & WIFI_CONNECTED_BIT) {
                ESP_LOGI(TAG, "WiFi Manager: Connected to WiFi.");
            } else {
                ESP_LOGE(TAG, "WiFi Manager: Failed to connect, retrying...");
            }
        } else {
            ESP_LOGI(TAG, "WiFi Manager: Already connected.");
        }
        // Stay connected (or check periodically) for 30 seconds.
        vTaskDelay(pdMS_TO_TICKS(5*60000));
    }
}

// NTP Sync Task: Waits for WiFi then synchronizes time and sets TIME_SYNCED_BIT.
void ntp_sync_task(void *pvParameter) {
    while (1) {
        // Wait for WiFi connection before syncing time.
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                                 pdFALSE, pdFALSE, portMAX_DELAY);
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "NTP Task: WiFi connected, synchronizing time...");
            if (obtain_time()) {  // assume obtain_time() returns true on success
                ESP_LOGI(TAG, "NTP Task: Time synchronized.");
                // Signal that time sync has been completed.
                xEventGroupSetBits(s_time_event_group, TIME_SYNCED_BIT);
            } else {
                ESP_LOGE(TAG, "NTP Task: Time synchronization failed.");
            }
        }
        // Resync periodically if needed (adjust delay as required).
        vTaskDelay(pdMS_TO_TICKS(60*24*60000));
    }
}

// Sensor Task: Waits for an initial time sync, then reads sensor data.
void sensor_task(void *pvParameter) {  
    // Block until time is synchronized.
    ESP_LOGI(TAG, "Sensor Task: Waiting for initial NTP sync...");
    EventBits_t bits = xEventGroupWaitBits(s_time_event_group, TIME_SYNCED_BIT,
                                             pdFALSE, pdTRUE, portMAX_DELAY);
    if (bits & TIME_SYNCED_BIT) {
         ESP_LOGI(TAG, "Sensor Task: NTP sync complete, starting sensor readings.");
    }

    if (!aht10_init(I2C_PORT)) {
        ESP_LOGE(TAG, "Sensor Task: Failed to initialize AHT10 sensor");
        vTaskDelete(NULL);
    }
    
    while (1) {
        if (aht10_read(I2C_PORT)) {
            // aht10_read() stores the sensor reading (with timestamp from time(NULL))
            // into the circular buffer.
        } else {
            ESP_LOGE(TAG, "Sensor Task: Failed to read sensor data");
        }
        vTaskDelay(pdMS_TO_TICKS(SENSOR_PERIOD * 60 * 1000));
    }
}

// MQTT Publisher Task: Waits until the next half-hour boundary, then sends all data.
void mqtt_publish_task(void *pvParameter) {
    sensor_data_t data;
    char sensorTimeStr[64];
    time_t now;
    struct tm timeinfo;
    
    // Wait until the time is synchronized.
    ESP_LOGI(TAG, "MQTT Publisher: Waiting for initial NTP sync...");
    EventBits_t bits = xEventGroupWaitBits(s_time_event_group, TIME_SYNCED_BIT,
                                           pdFALSE, pdTRUE, portMAX_DELAY);
    if (bits & TIME_SYNCED_BIT) {
        ESP_LOGI(TAG, "MQTT Publisher: NTP sync complete. Starting publishing loop.");
    }
    
    while (1) {
        // Calculate delay until the next half-hour (e.g., 08:00, 08:30, etc.).
        time(&now);
        localtime_r(&now, &timeinfo);
        int minutes = timeinfo.tm_min;
        int seconds = timeinfo.tm_sec;
        int delay_seconds = (minutes < 30)
                                ? ((30 - minutes) * 60 - seconds)
                                : ((60 - minutes) * 60 - seconds);

        ESP_LOGI(TAG, "MQTT Publisher: Waiting %d seconds until next interval...", delay_seconds);
        vTaskDelay(pdMS_TO_TICKS(delay_seconds * 1000));

        // Check that WiFi is connected.
        bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                   pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
        if (!(bits & WIFI_CONNECTED_BIT)) {
            ESP_LOGE(TAG, "MQTT Publisher: WiFi not connected. Skipping this cycle.");
            continue;
        }

        // Start MQTT client and allow time for connection.
        mqtt_app_start();
        vTaskDelay(pdMS_TO_TICKS(2000));

        int count = 0;
        // Retrieve and publish each sensor reading as its own JSON message.
        while (buffer_retrieve(&data)) {
            // Convert timestamp to a formatted string.
            struct tm sensorTimeInfo;
            localtime_r(&data.timestamp, &sensorTimeInfo);
            strftime(sensorTimeStr, sizeof(sensorTimeStr), "%Y-%m-%dT%H:%M:%S", &sensorTimeInfo);

            // Create a JSON object for this single reading.
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "timestamp",   sensorTimeStr);
            cJSON_AddNumberToObject(root, "temperature", data.temperature);
            cJSON_AddNumberToObject(root, "humidity",    data.humidity);
            cJSON_AddStringToObject(root, "owner",       OWNER_NAME);
            cJSON_AddStringToObject(root, "hardware",    HARDWARE_NAME);

            // Convert JSON object to string.
            char *json_str = cJSON_PrintUnformatted(root);
            ESP_LOGI(TAG, "MQTT Publisher: Publishing sensor data: %s", json_str);

            // Publish the message. If your mqtt_publish_data() now returns bool, check it.
            if (mqtt_publish_data(json_str) != ESP_OK) {
                ESP_LOGE(TAG, "MQTT Publisher: Failed to publish sensor data.");
                // Optional: implement a retry mechanism here.
            }

            // Clean up
            free(json_str);
            cJSON_Delete(root);

            count++;
        }

        if (count == 0) {
            ESP_LOGW(TAG, "MQTT Publisher: No sensor data available to publish.");
        }

        // Disconnect MQTT client (and optionally WiFi) after publishing.
        ESP_LOGI(TAG, "MQTT Publisher: Disconnecting MQTT");
        esp_mqtt_client_stop(mqtt_client);
        // Optionally disconnect WiFi here if desired: wifi_disconnect();
    }
}


void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (!i2c_master_init()) {
        ESP_LOGE(TAG, "Main: Failed to initialize I2C");
        return;
    }

    if (!buffer_init()) {
        ESP_LOGE(TAG, "Main: Failed to initialize circular buffer");
        return;
    }

    // Create event groups.
    s_wifi_event_group = xEventGroupCreate();
    s_time_event_group = xEventGroupCreate();

    // Create tasks.
    xTaskCreate(&wifi_manager_task, "wifi_manager_task", 4096, NULL, 5, NULL);
    xTaskCreate(&ntp_sync_task, "ntp_sync_task", 4096, NULL, 5, NULL);
    xTaskCreate(&sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(&mqtt_publish_task, "mqtt_publish_task", 4096, NULL, 5, NULL);
}
