#include <mdns.h>

#include "idf_wmonitor_hostname.h"

#include "idf_wmonitor_mdns.h"

esp_err_t idf_wmonitor_mdns_start(in_port_t port)
{
    esp_err_t err;
    if ((err = mdns_init()) != ESP_OK)
    {
        return err;
    }

    char hostname[IDF_WMONITOR_HOSTNAME_SIZE];
    if ((err = idf_wmonitor_get_hostname(hostname, sizeof(hostname))) != ESP_OK)
    {
        return err;
    }

    if ((err = mdns_hostname_set(hostname)) != ESP_OK)
    {
        return err;
    }

    mdns_txt_item_t txt[] = {
        {"v", "0.1"},
    };
    return mdns_service_add("ESP32-WirelessMonitor", "_esp32wmonitor", "_tcp", port, txt, sizeof(txt) / sizeof(txt[0]));
}

esp_err_t idf_wmonitor_mdns_handle_system_event(void *ctx, system_event_t *event)
{
    return mdns_handle_system_event(ctx, event);
}
