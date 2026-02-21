#include "bsp/m5stack_core_s3.h"
#include "driver/gpio.h"
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
#include "port_a_i2c.h"
#include "port_a_i2c_readings.h"
#include "port_a_i2c_service.h"
#include "power_aw9523.h"
#include "sgp30.h"
#include "sht40.h"
#include "sntp.h"
#include "weather_task.h"

#define TAG "main"

#define SHT40_ADDR 0x44
#define SGP30_ADDR 0x58

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
	ESP_ERROR_CHECK(port_a_add_device(porta, SHT40_ADDR, 400000, &sht_dev));

	i2c_master_dev_handle_t sgp_dev = NULL;
	ESP_ERROR_CHECK(port_a_add_device(porta, SGP30_ADDR, 100000, &sgp_dev));

	readings_store_init();
	net_manager_start();
	sntp_service_init_and_start(CONFIG_TIMEZONE);
	sntp_service_wait_for_sync(pdMS_TO_TICKS(5000));

	http_service_start();
	weather_task_start();
	port_a_i2c_service_start();
	sht40_task_start(sht_dev);
	sgp30_task_start(sgp_dev);
	lv_display_t *disp = bsp_display_start();
	assert(disp);
	bsp_display_brightness_set(100);

	bsp_display_lock(0);
	lv_obj_t *label = lv_label_create(lv_scr_act());
	lv_obj_center(label);
	bsp_display_unlock();

	readings_snapshot_t snapshot = {0};
	char buf[96];

	for (;;) {
		readings_get_snapshot(&snapshot);

		char tbuf[16];
		sntp_service_format_local_time("%H:%M:%S", tbuf, sizeof(tbuf));

		snprintf(buf, sizeof(buf),
				 "Time: %s\n"
				 "Temp: %.2f C\n"
				 "Humidity: %.2f%%",
				 tbuf, snapshot.temp_c, snapshot.rh_percent);

		bsp_display_lock(0);

		lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
		lv_label_set_text(label, buf);

		bsp_display_unlock();

		vTaskDelay(pdMS_TO_TICKS(500));
	}
}