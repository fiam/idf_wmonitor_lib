#pragma once

#include <stddef.h>
#include <stdint.h>

typedef int (*idf_wmonitor_coredump_read_f)(const void *data, size_t size, void *user_data);

uint32_t idf_wmonitor_coredump_size(void);
void idf_wmonitor_coredump_read(idf_wmonitor_coredump_read_f fn, void *user_data);
void idf_wmonitor_coredump_erase(void);