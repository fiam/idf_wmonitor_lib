#pragma once

#include <stddef.h>

#include <esp_err.h>

#define IDF_WMONITOR_HOSTNAME_SIZE (6 + (6 * 2) + 1) // See format in .c file

esp_err_t idf_wmonitor_get_hostname(char *buf, size_t size);