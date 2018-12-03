#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <idf_wmonitor/idf_wmonitor.h>

#define IDF_WMONITOR_CONFIG_VERSION 1

typedef struct
{
    uint8_t v; // Config version
    char wifi_ssid[IDF_WMONITOR_WIFI_SSID_SIZE];
    char wifi_password[IDF_WMONITOR_WIFI_PASSWORD_SIZE];
    uint8_t wifi_mode; // From idf_monitor_wifi_mode_t
} __attribute__((packed)) idf_wmonitor_stored_config_t;

bool idf_wmonitor_get_stored_config(idf_wmonitor_stored_config_t *cfg);
bool idf_wmonitor_set_stored_config(idf_wmonitor_stored_config_t *cfg);