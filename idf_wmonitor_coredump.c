#include <esp_partition.h>

#include "idf_wmonitor_coredump.h"

// ESP32 flash coredump format contains a header with the following info
// (all 32 bit little endian unsigned):
// | magic | total_dump_size | task_num | tcb_size
// total_dump_size indicates the number of bytes after the
// 4 magic bytes. The data has another magic marker at the end
// Found in espcoredump.py.
#define ESP_COREDUMP_MAGIC 0xE32C04ED

static const esp_partition_t *coredump_partition(void)
{
    const esp_partition_t *p = NULL;
    esp_partition_iterator_t iter = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
    if (iter)
    {
        p = esp_partition_get(iter);
        esp_partition_iterator_release(iter);
    }
    return p;
}

static uint32_t idf_wmonitor_coredump_size_from_partition(const esp_partition_t *p)
{
    if (p)
    {
        uint32_t val;
        if (esp_partition_read(p, 0, &val, sizeof(val)) == ESP_OK && val == ESP_COREDUMP_MAGIC)
        {
            if (esp_partition_read(p, 4, &val, sizeof(val)) == ESP_OK)
            {
                return val;
            }
        }
    }
    return 0;
}

uint32_t idf_wmonitor_coredump_size(void)
{
    const esp_partition_t *p = coredump_partition();
    return idf_wmonitor_coredump_size_from_partition(p);
}

void idf_wmonitor_coredump_read(idf_wmonitor_coredump_read_f fn, void *user_data)
{
    const esp_partition_t *p = coredump_partition();
    if (p)
    {
        uint32_t size = idf_wmonitor_coredump_size_from_partition(p);
        uint32_t off = 0; // Send everything including the initial magic bytes
        uint8_t buf[128];
        while (off < size)
        {
            size_t rs = sizeof(buf) < (size - off) ? sizeof(buf) : (size - off);
            if (esp_partition_read(p, off, buf, rs) != ESP_OK)
            {
                // Signal failure
                fn(NULL, -1, user_data);
                return;
            }
            fn(buf, rs, user_data);
            off += rs;
        }
    }
}

void idf_wmonitor_coredump_erase(void)
{
    const esp_partition_t *p = coredump_partition();
    if (p)
    {
        // Delete enough for the magic number check to fail
        // Note that that erases must be for full pages and
        // properly aligned (4K multiples)
        ESP_ERROR_CHECK(esp_partition_erase_range(p, 0, 4 * 1024));
    }
}