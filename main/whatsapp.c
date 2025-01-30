#include "esp_http_client.h"
#include "esp_log.h"
#include "whatsapp.h"

static const char *TAG = "WhatsApp";

// Send a message to WhatsApp
void send_to_whatsapp(const char *phone_number, const char *api_key, const char *message) {
    if (phone_number == NULL || api_key == NULL || message == NULL) {
        ESP_LOGE(TAG, "Invalid parameters: phone number, API key, or message is NULL.");
        return;
    }
    
    // URL-encode the message
    char encoded_message[256];
    url_encode(message, encoded_message, sizeof(encoded_message));

    // Prepare the URL
    char url[512];
    snprintf(url, sizeof(url),
             "http://api.callmebot.com/whatsapp.php?phone=%s&text=%s&apikey=%s",
             phone_number, encoded_message, api_key);

    ESP_LOGI(TAG, "URL: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = NULL,  
        .skip_cert_common_name_check = true,  
        .use_global_ca_store = false,
    };


    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Perform HTTP GET request
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Message sent successfully");
    } else {
        ESP_LOGE(TAG, "Failed to send message: %s", esp_err_to_name(err));
    }

    // Clean up
    esp_http_client_cleanup(client);
}

void url_encode(const char *src, char *dest, size_t dest_size) {
    const char *hex = "0123456789ABCDEF";
    while (*src && dest_size > 3) {
        if (isalnum((unsigned char)*src) || *src == '-' || *src == '_' || *src == '.' || *src == '~') {
            *dest++ = *src;
            dest_size--;
        } else {
            *dest++ = '%';
            *dest++ = hex[(*src >> 4) & 0xF];
            *dest++ = hex[*src & 0xF];
            dest_size -= 3;
        }
        src++;
    }
    *dest = '\0';
}