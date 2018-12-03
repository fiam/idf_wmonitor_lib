#pragma once

#include <stdbool.h>

#include <esp_err.h>

#include <idf_wmonitor/idf_wmonitor.h>

esp_err_t idf_wmonitor_start_wifi(idf_wmonitor_opts_t *opts);
esp_err_t idf_wmonitor_start_wifi_ap(idf_wmonitor_opts_t *opts, bool fallback);
esp_err_t idf_wmonitor_start_wifi_sta(idf_wmonitor_opts_t *opts);