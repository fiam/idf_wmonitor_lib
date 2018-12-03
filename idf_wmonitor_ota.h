#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <esp_ota_ops.h>

typedef struct
{
    const esp_partition_t *partition;
    esp_ota_handle_t ota;
} wmonitor_ota_t;

bool wmonitor_ota_begin(uint32_t size, wmonitor_ota_t *handle);
bool wmonitor_ota_write(wmonitor_ota_t *handle, const void *buf, size_t size);
bool wmonitor_ota_discard(wmonitor_ota_t *handle);
bool wmonitor_ota_commit(wmonitor_ota_t *handle);