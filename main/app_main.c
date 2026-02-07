#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"

#include "bsp/m5stack_core_s3.h"
#include "esp_private/i2c_platform.h"

#include "power_aw9523.h"
#include "port_a_i2c.h"
#include "i2c_utils.h"

#include "net_manager.h"
#include "http_service.h"
#include "weather_task.h"

#define TAG "main"

void app_main(void)
{
    // --- System init for Wi-Fi ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // --- BSP/system I2C (for AW9523, etc) ---
    ESP_ERROR_CHECK(bsp_i2c_init());

    i2c_master_bus_handle_t sys_bus = NULL;
    ESP_ERROR_CHECK(i2c_master_get_bus_handle(BSP_I2C_NUM, &sys_bus));
    ESP_LOGI(TAG, "sys bus=%p (port=%d)", sys_bus, BSP_I2C_NUM);

    ESP_ERROR_CHECK(aw9523_enable_grove_5v(sys_bus));
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "Grove 5V enabled.");

    // --- Port A I2C (GPIO2/1) ---
    port_a_i2c_t porta = {0};
    ESP_ERROR_CHECK(port_a_i2c_init(&porta));

    // Add two devices on Port A (example)
    i2c_master_dev_handle_t sht40 = NULL;
    ESP_ERROR_CHECK(port_a_add_device(&porta, 0x44, 100000, &sht40));

    i2c_master_dev_handle_t other = NULL;
    ESP_ERROR_CHECK(port_a_add_device(&porta, 0x40, 100000, &other)); // example addr

    // Optional: probe addresses before committing
    // ESP_LOGI(TAG, "probe 0x44: %s", esp_err_to_name(i2c_probe_addr(porta.bus, 0x44, 200)));
    // ESP_LOGI(TAG, "probe 0x45: %s", esp_err_to_name(i2c_probe_addr(porta.bus, 0x45, 200)));

    // Now your sensor modules can take `sht40` / `other` handles.

    net_manager_start();
    http_service_start();
    weather_task_start();
}
