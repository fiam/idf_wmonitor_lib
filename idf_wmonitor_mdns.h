#pragma once

#include <lwip/sockets.h>

#include <esp_err.h>

esp_err_t idf_wmonitor_mdns_start(in_port_t port);
esp_err_t idf_wmonitor_mdns_handle_system_event(void *ctx, system_event_t *event);