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
// #include "hermes_mcp_server.h"

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

    // Note: esp_netif_init() is NOT called here because the ESP32-P4 uses
    // the C5 SDIO coprocessor (ESP-Hosted) for WiFi networking. The C5
    // handles its own TCP/IP stack, so calling esp_netif_init() on the P4
    // would create a conflicting lwIP instance.

    // Launch the application (may block for WiFi provisioning)
    auto& app = Application::GetInstance();
    app.Start();
}
