#include "clock.h"

void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI("NTP", "Time synchronized!");
}

bool obtain_time() {
    ESP_LOGI("NTP", "Initializing SNTP...");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");  // Use global NTP server
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int max_retries = 10;
    
    while (timeinfo.tm_year < (2024 - 1900) && ++retry < max_retries) {
        ESP_LOGI("NTP", "Waiting for time synchronization...");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Wait 2 seconds
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (timeinfo.tm_year >= (2024 - 1900)) {
        ESP_LOGI("NTP", "UTC Time synchronized: %s", asctime(&timeinfo));

        // Set Timezone to GMT-4
        setenv("TZ", "EST4", 1);  // EST = Eastern Standard Time (GMT-4)
        tzset();

        // Get local time after timezone adjustment
        time(&now);
        localtime_r(&now, &timeinfo);
        ESP_LOGI("NTP", "Local Time (GMT-4): %s", asctime(&timeinfo));
        esp_sntp_stop();
        return true;
    } else {
        ESP_LOGE("NTP", "Failed to obtain time!");
        esp_sntp_stop();
        return false;
    }
}