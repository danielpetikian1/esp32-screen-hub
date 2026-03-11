#include "bsp/m5stack_core_s3.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_private/i2c_platform.h"
#include "lvgl.h"
#include "nvs_flash.h"

#include "http_service.h"
#include "i2c_utils.h"
#include "net_manager.h"
#include "port_i2c.h"		  // provides port_i2c_* and port_i2c_port_t
#include "port_i2c_service.h" // provides port_i2c_service_start()
#include "power_aw9523.h"
#include "sgp30.h"
#include "sht40.h"
#include "sntp.h"
#include "stocks_task.h"
#include "ui.h"
#include "ui_task.h"
#include "weather_task.h"

#define TAG "main"

#define SHT40_ADDR 0x44
#define SGP30_ADDR 0x58

/* -------------------------------------------------------------------------- */
/* Compile-time external bus selection (one bus for all external I2C devices) */
/* -------------------------------------------------------------------------- */

static inline port_i2c_port_t external_i2c_selected_port(void) {
#if CONFIG_EXT_I2C_BUS_PORT_A
	return PORT_I2C_PORT_A;
#elif CONFIG_EXT_I2C_BUS_PORT_B
	return PORT_I2C_PORT_B;
#elif CONFIG_EXT_I2C_BUS_PORT_C
	return PORT_I2C_PORT_C;
#else
#error "External I2C bus not selected in menuconfig"
#endif
}

void app_main(void) {
	/* --- System init for Wi-Fi --- */
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

	/* --- BSP/system I2C (for AW9523, etc) --- */
	ESP_ERROR_CHECK(bsp_i2c_init());

	i2c_master_bus_handle_t sys_bus = NULL;
	ESP_ERROR_CHECK(i2c_master_get_bus_handle(BSP_I2C_NUM, &sys_bus));
	ESP_LOGI(TAG, "sys bus=%p (port=%d)", sys_bus, BSP_I2C_NUM);

	ESP_ERROR_CHECK(aw9523_enable_grove_5v(sys_bus));
	vTaskDelay(pdMS_TO_TICKS(50));
	ESP_LOGI(TAG, "Grove 5V enabled.");

	/* ---------------------------------------------------------------------- */
	/* External I2C bus init (single bus chosen from Kconfig) */
	/* ---------------------------------------------------------------------- */
	const port_i2c_port_t ext_port = external_i2c_selected_port();
	ESP_LOGI(TAG, "External I2C bus configured on port %d", (int)ext_port);

	i2c_master_bus_handle_t ext_bus = NULL;
	port_i2c_bus_config_t cfg;

	ESP_ERROR_CHECK(port_i2c_get_default_bus_config(ext_port, &cfg));
	ESP_ERROR_CHECK(port_i2c_bus_init(&cfg, &ext_bus));

	/* ---------------------------------------------------------------------- */
	/* Add external devices (share same bus) */
	/* ---------------------------------------------------------------------- */
	i2c_master_dev_handle_t sht_dev = NULL;
	ESP_ERROR_CHECK(port_i2c_add_device(ext_bus, SHT40_ADDR, 400000, &sht_dev));

	i2c_master_dev_handle_t sgp_dev = NULL;
	ESP_ERROR_CHECK(port_i2c_add_device(ext_bus, SGP30_ADDR, 100000, &sgp_dev));

	/* ---------------------------------------------------------------------- */
	/* Start services/tasks */
	/* ---------------------------------------------------------------------- */
	readings_store_init();
	net_manager_start();

	sntp_service_init_and_start(CONFIG_TIMEZONE);
	sntp_service_wait_for_sync(pdMS_TO_TICKS(5000));

	http_service_start();
	weather_task_start();
	stocks_task_start();

	/* Start the (single) external I2C owner/service */
	port_i2c_service_start();

	/* Start sensor tasks */
	sht40_task_start(sht_dev);
	sgp30_task_start(sgp_dev);

	/* ---------------------------------------------------------------------- */
	/* UI init + loop */
	/* ---------------------------------------------------------------------- */
	/* Use a 20-line DMA buffer (320×20×2 = 12.8 KB) instead of the default
	 * 50-line (64 KB). WiFi DMA descriptors consume most of the internal
	 * DMA-capable SRAM before we reach this point; 12.8 KB fits comfortably.
	 * Rendering is marginally slower per flush but still smooth at 300 ms
	 * update intervals. */
	const bsp_display_cfg_t disp_cfg = {
		.lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
		.buffer_size = BSP_LCD_H_RES * 20,
		.double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
		.flags =
			{
				.buff_dma = true,
				.buff_spiram = false,
			},
	};
	lv_disp_t *disp = bsp_display_start_with_config(&disp_cfg);
	assert(disp);
	bsp_display_brightness_set(100);

	ESP_ERROR_CHECK(ui_init(disp));

	ui_task_start();
}
