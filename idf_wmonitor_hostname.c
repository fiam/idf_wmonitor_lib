#include <stdint.h>
#include <stdio.h>

#include <esp_wifi.h>

#include "idf_wmonitor_hostname.h"

esp_err_t idf_wmonitor_get_hostname(char *buf, size_t size)
{
    esp_err_t err;
    uint8_t mac[6];

    if ((err = esp_wifi_get_mac(ESP_IF_WIFI_STA, mac)) != ESP_OK)
    {
        return err;
    }

    snprintf(buf, size, "ESP32-%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return ESP_OK;
}
