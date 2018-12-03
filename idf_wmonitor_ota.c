#include "idf_wmonitor_ota.h"

bool wmonitor_ota_begin(uint32_t size, wmonitor_ota_t *handle)
{
    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    if (!ota_partition)
    {
        return false;
    }
    if (esp_ota_begin(ota_partition, size, &handle->ota) != ESP_OK)
    {
        return false;
    }
    handle->partition = ota_partition;
    return true;
}

bool wmonitor_ota_write(wmonitor_ota_t *handle, const void *buf, size_t size)
{
    return esp_ota_write(handle->ota, buf, size) == ESP_OK;
}

bool wmonitor_ota_discard(wmonitor_ota_t *handle)
{
    return esp_ota_end(handle->ota) == ESP_OK;
}

bool wmonitor_ota_commit(wmonitor_ota_t *handle)
{
    if (esp_ota_end(handle->ota) == ESP_OK)
    {
        return esp_ota_set_boot_partition(handle->partition) == ESP_OK;
    }
    return false;
}