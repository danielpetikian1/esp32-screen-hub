#include "power_aw9523.h"
#include "i2c_utils.h"

#include "esp_check.h"
#include "esp_log.h"

#define TAG "aw9523"

#define AW9523_ADDR 0x58

esp_err_t aw9523_enable_grove_5v(i2c_master_bus_handle_t sys_bus) {
	i2c_master_dev_handle_t aw = NULL;

	i2c_device_config_t cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = AW9523_ADDR,
		.scl_speed_hz = 400000,
	};

	ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(sys_bus, &cfg, &aw), TAG,
						"add AW9523 failed");

	// 1) P0 push-pull mode
	ESP_RETURN_ON_ERROR(i2c_write_u8(aw, 0x11, 0x10, 1000), TAG, "write 0x11");

	// 2) P0: BUS_OUT_EN=1, USB_OTG_EN=0
	uint8_t p0 = 0;
	ESP_RETURN_ON_ERROR(i2c_read_u8(aw, 0x02, &p0, 1000), TAG, "read 0x02");
	p0 |= (1u << 1);
	p0 &= (uint8_t)~(1u << 5);
	ESP_RETURN_ON_ERROR(i2c_write_u8(aw, 0x02, p0, 1000), TAG, "write 0x02");

	// 3) P1: BOOST_EN=1
	uint8_t p1 = 0;
	ESP_RETURN_ON_ERROR(i2c_read_u8(aw, 0x03, &p1, 1000), TAG, "read 0x03");
	p1 |= (1u << 7);
	ESP_RETURN_ON_ERROR(i2c_write_u8(aw, 0x03, p1, 1000), TAG, "write 0x03");

	i2c_master_bus_rm_device(aw);
	return ESP_OK;
}
