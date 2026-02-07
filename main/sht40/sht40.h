#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_t i2c_port;   // I2C_NUM_0 or I2C_NUM_1
    uint8_t addr;          // usually 0x44
} sht40_t;

typedef struct {
    float temperature_c;
    float humidity_rh;
} sht40_reading_t;

esp_err_t sht40_init(sht40_t *dev, i2c_port_t port, uint8_t addr);
esp_err_t sht40_read(const sht40_t *dev, sht40_reading_t *out);

#ifdef __cplusplus
}
#endif
