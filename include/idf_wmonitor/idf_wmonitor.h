#pragma once

#include <stdbool.h>

#include <esp_err.h>
#include <esp_event.h>

// These are defined in esp_wifi_types.h, but there are not constants for them.
// ESP32 API has an additional SSID length field to allow up to 32 characters,
// but since we only suport null termination we need an extra byte.
#define IDF_WMONITOR_WIFI_SSID_SIZE 33
#define IDF_WMONITOR_WIFI_PASSWORD_SIZE 64

typedef enum
{
    IDF_WMONITOR_WAIT_FOR_CLIENT = 1 << 0,             // block until a client connects
    IDF_WMONITOR_WAIT_FOR_CLIENT_IF_COREDUMP = 1 << 1, // block only if a coredump is present
    IDF_WMONITOR_DISALLOW_WIFI_CONFIG = 1 << 2,        // don't allow changing the wifi configuration via the network
    IDF_WMONITOR_IGNORE_SAVED_WIFI_CONFIG = 1 << 3,    // ignore saved wifi configuration
} idf_wmonitor_flag_t;

typedef enum
{
    IDF_WMONITOR_WIFI_AUTO = 0, // Start as STA, fallback to AP if cannot connect to network
    IDF_WMONITOR_WIFI_STA,
    IDF_WMONITOR_WIFI_AP,

    IDF_WMONITOR_WIFI_COUNT,
} idf_monitor_wifi_mode_t;

typedef struct
{
    char ssid[IDF_WMONITOR_WIFI_SSID_SIZE];
    char password[IDF_WMONITOR_WIFI_PASSWORD_SIZE];
    idf_monitor_wifi_mode_t mode;
} idf_wmonitor_wifi_config_t;

typedef struct
{
    idf_wmonitor_wifi_config_t wifi;
} idf_wmonitor_config_t;

typedef struct idf_wmonitor_opts
{
    idf_wmonitor_config_t config;
    idf_wmonitor_flag_t flags;
} idf_wmonitor_opts_t;

#define IDF_WMONITOR_CONFIG_DEFAULT() ((idf_wmonitor_config_t){ \
    .wifi.mode = IDF_WMONITOR_WIFI_AUTO,                        \
})

#define IDF_WMONITOR_CONFIG_DEFAULT_WITH_CREDENTIALS(s, p) _IDF_WMONITOR_CONFIG_WIFI(s, p, IDF_WMONITOR_WIFI_AUTO)

#define _IDF_WMONITOR_CONFIG_WIFI(s, p, m) ((idf_wmonitor_config_t){ \
    .wifi.ssid = s,                                                  \
    .wifi.password = p,                                              \
    .wifi.mode = m,                                                  \
})

esp_err_t idf_wmonitor_start(idf_wmonitor_opts_t *opts);
esp_err_t idf_wmonitor_event_handler(void *ctx, system_event_t *event);