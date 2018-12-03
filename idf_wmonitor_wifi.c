#include <string.h>

#include <esp_wifi.h>

#include "idf_wmonitor_hostname.h"

#include "idf_wmonitor_wifi.h"

esp_err_t idf_wmonitor_start_wifi_ap(idf_wmonitor_opts_t *opts, bool fallback)
{
    esp_err_t err;

    tcpip_adapter_init();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if ((err = esp_wifi_init(&cfg)) != ESP_OK)
    {
        return err;
    }
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    if (fallback || !opts->config.wifi.ssid[0])
    {
        char hostname[IDF_WMONITOR_HOSTNAME_SIZE];
        if ((err = idf_wmonitor_get_hostname(hostname, sizeof(hostname))) != ESP_OK)
        {
            return err;
        }
        strlcpy((char *)wifi_config.ap.ssid, hostname, sizeof(wifi_config.ap.ssid));
        wifi_config.ap.password[0] = '\0';
    }
    else
    {
        strlcpy((char *)wifi_config.ap.ssid, opts->config.wifi.ssid, sizeof(wifi_config.ap.ssid));
        strlcpy((char *)wifi_config.ap.password, opts->config.wifi.password, sizeof(wifi_config.ap.password));
    }

    wifi_config.ap.ssid_len = strlen((char *)wifi_config.ap.ssid);

    if (strlen((char *)wifi_config.sta.password) > 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    }
    else
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    wifi_config.ap.max_connection = 1;

    if ((err = esp_wifi_set_mode(WIFI_MODE_AP)) != ESP_OK)
    {
        return err;
    }

    if ((err = esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config)) != ESP_OK)
    {
        return err;
    }
    if ((err = esp_wifi_start()) != ESP_OK)
    {
        return err;
    }
    return ESP_OK;
}

esp_err_t idf_wmonitor_start_wifi_sta(idf_wmonitor_opts_t *opts)
{
    esp_err_t err;

    tcpip_adapter_init();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if ((err = esp_wifi_init(&cfg)) != ESP_OK)
    {
        return err;
    }
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strlcpy((char *)wifi_config.sta.ssid, opts->config.wifi.ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, opts->config.wifi.password, sizeof(wifi_config.sta.password));

    if ((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK)
    {
        return err;
    }

    if ((err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config)) != ESP_OK)
    {
        return err;
    }
    if ((err = esp_wifi_start()) != ESP_OK)
    {
        return err;
    }
    return ESP_OK;
}

esp_err_t idf_wmonitor_start_wifi(idf_wmonitor_opts_t *opts)
{
    switch (opts->config.wifi.mode)
    {
    case IDF_WMONITOR_WIFI_AUTO:
        if (opts->config.wifi.ssid[0])
        {
            return idf_wmonitor_start_wifi_sta(opts);
        }
        return idf_wmonitor_start_wifi_ap(opts, true);
        break;
    case IDF_WMONITOR_WIFI_STA:
        return idf_wmonitor_start_wifi_sta(opts);
    case IDF_WMONITOR_WIFI_AP:
        return idf_wmonitor_start_wifi_ap(opts, false);
    case IDF_WMONITOR_WIFI_COUNT:
        break;
    }
    return ESP_OK;
}