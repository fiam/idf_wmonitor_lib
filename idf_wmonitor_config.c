#include <nvs_flash.h>

#include "idf_wmonitor_config.h"

#define STORAGE_NAMESPACE "iw"
#define STORAGE_CONFIG_KEY "cfg"

bool idf_wmonitor_get_stored_config(idf_wmonitor_stored_config_t *cfg)
{
    nvs_handle handle;

    if (nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
    {
        return false;
    }

    size_t s = sizeof(*cfg);
    bool found = false;
    if (nvs_get_blob(handle, STORAGE_CONFIG_KEY, cfg, &s) == ESP_OK && s == sizeof(*cfg))
    {
        found = cfg->v == IDF_WMONITOR_CONFIG_VERSION;
    }
    nvs_close(handle);
    return found;
}

bool idf_wmonitor_set_stored_config(idf_wmonitor_stored_config_t *cfg)
{
    nvs_handle handle;

    if (nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
    {
        return false;
    }

    bool stored = nvs_set_blob(handle, STORAGE_CONFIG_KEY, cfg, sizeof(*cfg)) == ESP_OK &&
                  nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    return stored;
}