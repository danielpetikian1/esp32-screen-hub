#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t i2c_write_u8(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val,
					   int timeout_ms);
esp_err_t i2c_read_u8(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *out,
					  int timeout_ms);
esp_err_t i2c_probe_addr(i2c_master_bus_handle_t bus, uint8_t addr,
						 int timeout_ms);

#ifdef __cplusplus
}
#endif