#pragma once
#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	i2c_master_dev_handle_t dev;
	uint8_t addr; // optional, for logging/debug
} sht40_t;

typedef struct {
	float temperature_c;
	float humidity_rh;
} sht40_reading_t;

esp_err_t sht40_init(i2c_master_bus_handle_t bus, uint8_t addr, sht40_t *out);
esp_err_t sht40_read(const sht40_t *dev, sht40_reading_t *out);
void sht40_deinit(sht40_t *dev);

#ifdef __cplusplus
}
#endif
