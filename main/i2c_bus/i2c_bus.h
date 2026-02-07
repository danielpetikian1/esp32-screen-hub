#pragma once
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	i2c_port_t port; // I2C_NUM_0 / I2C_NUM_1
	gpio_num_t sda_io;
	gpio_num_t scl_io;
	uint32_t clk_hz; // e.g. 100000
} i2c_bus_cfg_t;

esp_err_t i2c_bus_init(const i2c_bus_cfg_t *cfg);

// Convenience helpers (legacy I2C driver)
esp_err_t i2c_bus_write(i2c_port_t port, uint8_t addr, const uint8_t *data,
						size_t len, TickType_t timeout);
esp_err_t i2c_bus_read(i2c_port_t port, uint8_t addr, uint8_t *data, size_t len,
					   TickType_t timeout);
esp_err_t i2c_bus_write_read(i2c_port_t port, uint8_t addr, const uint8_t *tx,
							 size_t tx_len, uint8_t *rx, size_t rx_len,
							 TickType_t timeout);

#ifdef __cplusplus
}
#endif
