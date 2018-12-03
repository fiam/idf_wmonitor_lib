#pragma once

#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <esp_log.h>

#include <idf_wmonitor/idf_wmonitor.h>

typedef struct
{
    vprintf_like_t original_vprintf;
    int remote_output;
    SemaphoreHandle_t wait_sema;
    SemaphoreHandle_t socket_sema;
    idf_wmonitor_opts_t opts;
    bool connected;
} idf_wmonitor_state_t;