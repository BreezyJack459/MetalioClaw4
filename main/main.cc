#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#include "board.h"
#include "system_info.h"
#include "hermes_mcp_server.h"

#define TAG "main"

extern "C" void app_main(void)
{
    // Initialize the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Force board init early (this establishes the board singleton so that
    // subsequent Application::GetInstance().Start() reuses it)
    Board::GetInstance();

    // Initialize TCP/IP network stack (required by httpd_start() for MCP server).
    // This is normally called inside the board constructor for NT26 boards,
    // but the MetalioClaw-4 board does NOT call it during construction.
    ESP_LOGI(TAG, "Initializing TCP/IP network stack...");
    ESP_ERROR_CHECK(esp_netif_init());

    // Start Hermes MCP HTTP server BEFORE the blocking app.Start()
    // (network stack is ready, but we don't need WiFi to serve MCP)
    ESP_LOGI(TAG, "Starting Hermes MCP HTTP server on port 8090...");
    esp_err_t mcp_err = hermes_mcp_server_start(8090);
    if (mcp_err != ESP_OK) {
        ESP_LOGW(TAG, "Hermes MCP server start failed (non-fatal): %s", esp_err_to_name(mcp_err));
    }

    // Launch the application (may block for WiFi provisioning)
    auto& app = Application::GetInstance();
    app.Start();
}
