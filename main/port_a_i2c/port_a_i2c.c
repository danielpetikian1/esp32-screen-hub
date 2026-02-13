#include "port_a_i2c.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

// Port A I2C pin configuration (CoreS3 Grove)
#define PORTA_SDA_GPIO 2
#define PORTA_SCL_GPIO 1

// SHT40 high precision measurement command (no heater)
#define SHT40_CMD_MEAS_HIGH_PREC_NO_HEAT 0xFD


/* -------------------------------------------------------------------------- */
/* Port A Bus Initialization                                                  */
/* -------------------------------------------------------------------------- */

esp_err_t port_a_i2c_init(i2c_master_bus_handle_t *bus)
{
	if (!bus) {
		return ESP_ERR_INVALID_ARG;
	}

	// Clear caller handle before allocation
	*bus = NULL;

	// Configure I2C master bus for Port A (GPIO2/1)
	i2c_master_bus_config_t cfg = {
		.i2c_port = -1,  // Auto-select port (IDF 5.x)
		.sda_io_num = PORTA_SDA_GPIO,
		.scl_io_num = PORTA_SCL_GPIO,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.intr_priority = 0,
		.trans_queue_depth = 0,
		// Port A relies on external pull-ups (Grove bus)
		.flags.enable_internal_pullup = 0,
	};

	return i2c_new_master_bus(&cfg, bus);
}


void port_a_i2c_deinit(i2c_master_bus_handle_t bus)
{
	// Safe no-op if bus is NULL
	if (!bus)
		return;

	i2c_del_master_bus(bus);
}


/* -------------------------------------------------------------------------- */
/* Device Management                                                          */
/* -------------------------------------------------------------------------- */

esp_err_t port_a_add_device(i2c_master_bus_handle_t bus,
							uint8_t addr,
							uint32_t scl_hz,
							i2c_master_dev_handle_t *out_dev)
{
	if (!bus)
		return ESP_ERR_INVALID_ARG;

	// Configure 7-bit I2C device
	i2c_device_config_t dev_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = addr,
		.scl_speed_hz = scl_hz,
	};

	return i2c_master_bus_add_device(bus, &dev_cfg, out_dev);
}


esp_err_t port_a_rem_device(const i2c_master_dev_handle_t dev)
{
	if (!dev) {
		return ESP_ERR_INVALID_ARG;
	}

	return i2c_master_bus_rm_device(dev);
}


/* -------------------------------------------------------------------------- */
/* Transaction Helpers                                                        */
/* -------------------------------------------------------------------------- */

esp_err_t port_a_i2c_read(const i2c_master_dev_handle_t dev,
						  uint8_t *buf,
						  size_t buf_len,
						  const uint8_t cmd)
{
	if (!dev || !buf)
		return ESP_ERR_INVALID_ARG;

	// Send command byte to trigger measurement/conversion
	esp_err_t err = i2c_master_transmit(dev, &cmd, 1, 200);
	if (err != ESP_OK)
		return err;

	// Allow device time to complete measurement (e.g., SHT40 ~20ms)
	vTaskDelay(pdMS_TO_TICKS(25));

	// Retry read in case device NACKs while busy
	for (int i = 0; i < 8; i++) {
		err = i2c_master_receive(dev, buf, buf_len, 200);
		if (err == ESP_OK)
			break;

		vTaskDelay(pdMS_TO_TICKS(3));
	}

	if (err != ESP_OK)
		return err;

	return ESP_OK;
}
