#include <stdio.h>
#include <string.h>

#include <esp_log.h>
#include <esp_wifi.h>

#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>

#include "idf_wmonitor_config.h"
#include "idf_wmonitor_coredump.h"
#include "idf_wmonitor_mdns.h"
#include "idf_wmonitor_ota.h"
#include "idf_wmonitor_private.h"
#include "idf_wmonitor_wifi.h"

// Compatibility with IDF < 3.2
#if !defined(WIFI_TASK_CORE_ID)
#define WIFI_TASK_CORE_ID 0
#endif

#define CMD_PRINT_STDOUT 0
#define CMD_PRINT_STDERR 1
// Leave some cmd space for potential stderr/stdin future implementation
#define CMD_PONG 5
#define CMD_OTA_PROGRESS 6
#define CMD_OTA_OK 7
#define CMD_OTA_FAILED 8
#define CMD_CONFIG 9

#define CMD_PING 128
#define CMD_REBOOT 129
#define CMD_OTA 130
#define CMD_CONTINUE 131
#define CMD_COREDUMP_READ 132
#define CMD_COREDUMP_ERASE 133
#define CMD_GET_CONFIG 134
#define CMD_SET_CONFIG 135

static const char *TAG = "wmonitor";

typedef struct
{
    uint8_t cmd;
    uint32_t offset;
} __attribute__((packed)) idf_wmonitor_ota_progress_t;

typedef struct
{
    uint8_t cmd;
    uint16_t size;
    idf_wmonitor_stored_config_t config;
} __attribute__((packed)) idf_wmonitor_config_payload_t;

static idf_wmonitor_state_t state = {
    .original_vprintf = NULL,
    .remote_output = -1,
    .wait_sema = NULL,
    .socket_sema = NULL,
    .connected = false,
};

static int send_all(int s, const void *buf, size_t size)
{
    int rem = size;
    const uint8_t *ptr = buf;
    while (rem > 0)
    {
        int r = send(s, ptr, rem, 0);
        if (r <= 0)
        {
            return r;
        }
        rem -= r;
        ptr += r;
    }
    return size;
}

static int idf_wmonitor_vprintf(const char *fmt, va_list ap)
{
    xSemaphoreTake(state.socket_sema, portMAX_DELAY);
    int output = state.remote_output;
    xSemaphoreGive(state.socket_sema);
    if (output < 0)
    {
        return state.original_vprintf(fmt, ap);
    }

    va_list cpy1; // For length calculation
    va_list cpy2; // For actual writing

    va_copy(cpy1, ap);
    va_copy(cpy2, ap);

    int ret = state.original_vprintf(fmt, ap);

    uint8_t cmd = CMD_PRINT_STDOUT;
    xSemaphoreTake(state.socket_sema, portMAX_DELAY);
    write(output, &cmd, sizeof(cmd));
    uint32_t length = vsnprintf(NULL, 0, fmt, cpy1);
    length = htonl(length);
    write(output, &length, sizeof(length));
    vdprintf(output, fmt, cpy2);
    xSemaphoreGive(state.socket_sema);

    va_end(cpy1);
    va_end(cpy2);
    return ret;
}

static int idf_wmonitor_sendall(int s, const void *buf, size_t size)
{
    xSemaphoreTake(state.socket_sema, portMAX_DELAY);
    int ret = send_all(s, buf, size);
    xSemaphoreGive(state.socket_sema);
    return ret;
}

static uint8_t idf_wmonitor_do_ota(int s)
{
    idf_wmonitor_ota_progress_t progress = {
        .cmd = CMD_OTA_PROGRESS,
    };
    uint32_t ota_size;
    if (read(s, &ota_size, sizeof(ota_size)) < 0)
    {
        return CMD_OTA_FAILED;
    }
    ota_size = htonl(ota_size);
    wmonitor_ota_t handle;
    if (!wmonitor_ota_begin(ota_size, &handle))
    {
        return CMD_OTA_FAILED;
    }
    uint8_t buf[32];
    uint32_t done = 0;
    int ii = 0;
    while (done < ota_size)
    {
        size_t rsize = sizeof(buf);
        size_t rem = ota_size - done;
        if (rem < rsize)
        {
            rsize = rem;
        }
        int n = read(s, buf, rsize);
        if (n < 0)
        {
            wmonitor_ota_discard(&handle);
            return CMD_OTA_FAILED;
        }
        if (!wmonitor_ota_write(&handle, buf, n))
        {
            wmonitor_ota_discard(&handle);
            return CMD_OTA_FAILED;
        }
        done += n;
        progress.offset = htonl(done);
        if (++ii == 30 || done == ota_size)
        {
            idf_wmonitor_sendall(s, &progress, sizeof(progress));
            vTaskDelay(1 / portTICK_PERIOD_MS);
            ii = 0;
        }
    }
    if (!wmonitor_ota_commit(&handle))
    {
        return CMD_OTA_FAILED;
    }
    return CMD_OTA_OK;
}

static void idf_wmonitor_ota(int s)
{
    uint8_t resp = idf_wmonitor_do_ota(s);
    idf_wmonitor_sendall(s, &resp, sizeof(resp));
    if (CMD_OTA_OK)
    {
        esp_restart();
    }
}

static int idf_wmonitor_coredump_reader(const void *data, size_t size, void *user_data)
{
    if (size > 0)
    {
        int *s = user_data;
        return send_all(*s, data, size);
    }
    return size;
}

static void idf_wmonitor_do_coredump_read(int s)
{
    uint32_t coredump_size = idf_wmonitor_coredump_size();
    uint8_t resp = CMD_COREDUMP_READ;
    coredump_size = htonl(coredump_size);
    xSemaphoreTake(state.socket_sema, portMAX_DELAY);
    write(s, &resp, sizeof(resp));
    write(s, &coredump_size, sizeof(&coredump_size));
    idf_wmonitor_coredump_read(idf_wmonitor_coredump_reader, &s);
    xSemaphoreGive(state.socket_sema);
}

static void idf_wmonitor_send_config(int s)
{
    idf_wmonitor_config_payload_t p = {
        .cmd = CMD_CONFIG,
    };
    idf_wmonitor_flag_t mask = IDF_WMONITOR_DISALLOW_WIFI_CONFIG | IDF_WMONITOR_IGNORE_SAVED_WIFI_CONFIG;
    if (((state.opts.flags & mask) == 0) && idf_wmonitor_get_stored_config(&p.config))
    {
        p.size = sizeof(p.config);
    }
    else
    {
        p.size = 0;
    }
    p.size = htons(p.size);
    idf_wmonitor_sendall(s, &p, sizeof(p));
}

static void idf_wmonitor_read_config(int s)
{
    idf_wmonitor_config_payload_t p;

    if (read(s, &p.size, sizeof(p.size)) < 0)
    {
        return;
    }

    p.size = ntohs(p.size);

    if (read(s, &p.config.v, sizeof(p.config.v)) < 0)
    {
        return;
    }

    if (read(s, p.config.wifi_ssid, sizeof(p.config.wifi_ssid)) < 0)
    {
        return;
    }

    if (read(s, p.config.wifi_password, sizeof(p.config.wifi_password)) < 0)
    {
        return;
    }

    if (read(s, &p.config.wifi_mode, sizeof(p.config.wifi_mode)) < 0)
    {
        return;
    }

    if (strnlen(p.config.wifi_ssid, sizeof(p.config.wifi_ssid)) == sizeof(p.config.wifi_ssid) ||
        strnlen(p.config.wifi_password, sizeof(p.config.wifi_password)) == sizeof(p.config.wifi_password) ||
        p.config.wifi_mode >= IDF_WMONITOR_WIFI_COUNT)
    {
        return;
    }

    if ((state.opts.flags & IDF_WMONITOR_DISALLOW_WIFI_CONFIG) == 0)
    {
        idf_wmonitor_set_stored_config(&p.config);
    }

    idf_wmonitor_send_config(s);
}

static void idf_monitor_server_task(void *arg)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(60414);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        ESP_LOGE(TAG, "Failed to open server socket");
        vTaskDelete(NULL);
        return;
    }

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(s);
        ESP_LOGE(TAG, "Failed to bind(): %s", strerror(errno));
        vTaskDelete(NULL);
        return;
    }

    if (listen(s, 1) < 0)
    {
        close(s);
        ESP_LOGE(TAG, "Failed to listen(): %s", strerror(errno));
        vTaskDelete(NULL);
        return;
    }

    // Retrieve address and port
    socklen_t size = sizeof(addr);
    getsockname(s, (struct sockaddr *)&addr, &size);

    in_port_t port = htons(addr.sin_port);
    tcpip_adapter_if_t iface = (tcpip_adapter_if_t)arg;
    tcpip_adapter_ip_info_t info;
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(iface, &info));
    ESP_LOGI(TAG, "starting server on %s:%u", ip4addr_ntoa(&info.ip), (unsigned)port);

    ESP_ERROR_CHECK(idf_wmonitor_mdns_start(port));

    for (;;)
    {
        struct sockaddr_in caddr;
        socklen_t csize = sizeof(caddr);
        int cs = accept(s, (struct sockaddr *)&caddr, &csize);
        if (cs < 0)
        {
            continue;
        }

        ESP_LOGI(TAG, "Got connection from %s: %u", inet_ntoa(caddr.sin_addr), htons(caddr.sin_port));

        int val = 1;
        setsockopt(cs, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(int));

        xSemaphoreTake(state.socket_sema, portMAX_DELAY);
        state.remote_output = cs;
        xSemaphoreGive(state.socket_sema);
        for (;;)
        {
            // Process commands
            uint8_t cmd;
            if (read(cs, &cmd, sizeof(cmd)) < 0)
            {
                // Connection was closed
                break;
            }
            switch (cmd)
            {
            case CMD_PING:
                cmd = CMD_PONG;
                idf_wmonitor_sendall(cs, &cmd, sizeof(cmd));
                break;
            case CMD_REBOOT:
                esp_restart();
                break;
            case CMD_OTA:
                idf_wmonitor_ota(cs);
                break;
            case CMD_CONTINUE:
                if (state.wait_sema)
                {
                    xSemaphoreGive(state.wait_sema);
                    idf_wmonitor_sendall(cs, &cmd, sizeof(cmd));
                }
                break;
            case CMD_COREDUMP_READ:
                idf_wmonitor_do_coredump_read(cs);
                break;
            case CMD_COREDUMP_ERASE:
                idf_wmonitor_coredump_erase();
                idf_wmonitor_sendall(cs, &cmd, sizeof(cmd));
                break;
            case CMD_GET_CONFIG:
                idf_wmonitor_send_config(cs);
                break;
            case CMD_SET_CONFIG:
                idf_wmonitor_read_config(cs);
                break;
            default:
                ESP_LOGW(TAG, "Unknown cmd %02x", cmd);
            }
        }
        xSemaphoreTake(state.socket_sema, portMAX_DELAY);
        state.remote_output = -1;
        xSemaphoreGive(state.socket_sema);
        close(cs);
        ESP_LOGI(TAG, "Connection from %s:%u was closed", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
    }
    close(s);
}

static void idf_wmonitor_start_task(tcpip_adapter_if_t iface)
{
    xTaskCreatePinnedToCore(idf_monitor_server_task, "WMONITOR", 4096,
                            (void *)iface, tskIDLE_PRIORITY + 1,
                            NULL, WIFI_TASK_CORE_ID);
}

esp_err_t idf_wmonitor_event_handler(void *ctx, system_event_t *event)
{
    esp_err_t err;

    if ((err = idf_wmonitor_mdns_handle_system_event(ctx, event)) != ESP_OK)
    {
        return err;
    }

    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        idf_wmonitor_start_task(TCPIP_ADAPTER_IF_STA);
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        state.connected = true;
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        if (state.connected)
        {
            state.connected = false;
            ESP_ERROR_CHECK(esp_wifi_connect());
        }
        else
        {
            // If we get a DISCONNECTED without connecting, it means
            // either the AP can't be found or either the network
            // password is incorrect.
            if (state.opts.config.wifi.mode == IDF_WMONITOR_WIFI_AUTO)
            {
                ESP_LOGW(TAG, "Could not connect to network %s, falling back to AP mode...", state.opts.config.wifi.ssid);
                ESP_ERROR_CHECK(idf_wmonitor_start_wifi_ap(&state.opts, true));
            }
            else
            {
                ESP_LOGW(TAG, "Could not connect to network %s", state.opts.config.wifi.ssid);
            }
        }
        break;
    case SYSTEM_EVENT_AP_START:
    {
        ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
        tcpip_adapter_ip_info_t info;
        memset(&info, 0, sizeof(info));
        IP4_ADDR(&info.ip, 192, 168, 10, 1);
        IP4_ADDR(&info.gw, 192, 168, 10, 1);
        IP4_ADDR(&info.netmask, 255, 255, 255, 0);
        ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
        ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
        if (state.connected)
        {
            break;
        }
        state.connected = true;
        idf_wmonitor_start_task(TCPIP_ADAPTER_IF_AP);
        break;
    }
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t idf_wmonitor_start(idf_wmonitor_opts_t *opts)
{
    esp_err_t err;

    if (opts)
    {
        memcpy(&state.opts, opts, sizeof(state.opts));
    }
    else
    {
        memset(&state.opts, 0, sizeof(state.opts));
    }

    if ((state.opts.flags & IDF_WMONITOR_IGNORE_SAVED_WIFI_CONFIG) == 0)
    {
        idf_wmonitor_stored_config_t cfg;
        if (idf_wmonitor_get_stored_config(&cfg))
        {
            ESP_LOGI(TAG, "Using stored configuration");
            strlcpy(state.opts.config.wifi.ssid, cfg.wifi_ssid, sizeof(state.opts.config.wifi.ssid));
            strlcpy(state.opts.config.wifi.password, cfg.wifi_password, sizeof(state.opts.config.wifi.password));
            state.opts.config.wifi.mode = cfg.wifi_mode;
        }
    }

    if ((err = idf_wmonitor_start_wifi(&state.opts)))
    {
        return err;
    }

    state.socket_sema = xSemaphoreCreateBinary();
    xSemaphoreGive(state.socket_sema);
    state.original_vprintf = esp_log_set_vprintf(idf_wmonitor_vprintf);

    if ((state.opts.flags & IDF_WMONITOR_WAIT_FOR_CLIENT) ||
        ((state.opts.flags & IDF_WMONITOR_WAIT_FOR_CLIENT_IF_COREDUMP) && idf_wmonitor_coredump_size() > 0))
    {
        ESP_LOGI(TAG, "Waiting for client to connect...");
        state.wait_sema = xSemaphoreCreateBinary();
        xSemaphoreTake(state.wait_sema, portMAX_DELAY);
        vQueueDelete(state.wait_sema);
        state.wait_sema = NULL;
    }

    return ESP_OK;
}