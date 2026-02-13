#include "bsp/m5stack_core_s3.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_private/i2c_platform.h"
#include "nvs_flash.h"

#include "http_service.h"
#include "i2c_utils.h"
#include "net_manager.h"
#include "port_a_i2c.h"
#include "port_a_i2c_service.h"
#include "power_aw9523.h"
#include "sht40_task.h"
#include "weather_task.h"

#define TAG "main"

void app_main(void) {
	// --- System init for Wi-Fi ---
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
		ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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
	i2c_master_bus_handle_t porta = NULL;
	ESP_ERROR_CHECK(port_a_i2c_init(&porta));

	ESP_LOGI("porta", "idle SDA=%d SCL=%d", gpio_get_level(1),
			 gpio_get_level(2));

	i2c_master_dev_handle_t sht_dev = NULL;
	ESP_ERROR_CHECK(
		port_a_add_device(porta, 0x44, 400000, &sht_dev)); // example addr

	net_manager_start();
	http_service_start();
	weather_task_start();
	port_a_i2c_service_start();
	sht40_task_start(sht_dev);
}