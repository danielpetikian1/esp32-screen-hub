#include "port_a_i2c.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_rom_sys.h" // esp_rom_delay_us
#include "freertos/FreeRTOS.h"
#include <string.h>

// todo: clean up includes

#define PORTA_SDA_GPIO 2
#define PORTA_SCL_GPIO 1

#define SHT40_CMD_MEAS_HIGH_PREC_NO_HEAT 0xFD

esp_err_t port_a_i2c_init(port_a_i2c_t *out) {
	if (!out)
		return ESP_ERR_INVALID_ARG;
	memset(out, 0, sizeof(*out));

	i2c_master_bus_config_t cfg = {
		.i2c_port =
			-1, // let driver pick a port (IDF 5.x new driver supports this)
		.sda_io_num = PORTA_SDA_GPIO,
		.scl_io_num = PORTA_SCL_GPIO,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.intr_priority = 0,
		.trans_queue_depth = 0,
		.flags.enable_internal_pullup =
			0, // Port A should use external pullups on the bus
	};

	return i2c_new_master_bus(&cfg, &out->bus);
}

void port_a_i2c_deinit(port_a_i2c_t *ctx) {
	if (!ctx || !ctx->bus)
		return;
	i2c_del_master_bus(ctx->bus);
	ctx->bus = NULL;
}

esp_err_t port_a_add_device(port_a_i2c_t *ctx, uint8_t addr, uint32_t scl_hz,
							i2c_master_dev_handle_t *out_dev) {
	if (!ctx || !ctx->bus || !out_dev)
		return ESP_ERR_INVALID_ARG;

	i2c_device_config_t dev_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = addr,
		.scl_speed_hz = scl_hz,
	};
	return i2c_master_bus_add_device(ctx->bus, &dev_cfg, out_dev);
}

esp_err_t port_a_rem_device(const i2c_master_dev_handle_t *dev) {
	if (!dev) {
		return ESP_ERR_INVALID_ARG;
	}
	esp_err_t err = i2c_master_bus_rm_device(*dev);
	dev = NULL;
	return err;
}

esp_err_t port_a_i2c_read(const i2c_master_dev_handle_t *dev, uint8_t *buf,
						  size_t buf_len, const uint8_t cmd) {
	if (!dev || !dev || !buf)
		return ESP_ERR_INVALID_ARG;

	esp_err_t err = i2c_master_transmit(*dev, &cmd, 1, 200);
	if (err != ESP_OK)
		return err;

	// Give it a minimum time
	vTaskDelay(pdMS_TO_TICKS(25));

	// Try multiple times because the sensor can NACK while busy
	for (int i = 0; i < 8; i++) {
		err = i2c_master_receive(*dev, buf, buf_len, 200);
		if (err == ESP_OK)
			break;
		vTaskDelay(pdMS_TO_TICKS(3));
	}
	if (err != ESP_OK)
		return err;

	return ESP_OK;
}
