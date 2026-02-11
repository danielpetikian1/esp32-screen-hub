#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	i2c_master_bus_handle_t bus;
} port_a_i2c_t;

esp_err_t port_a_i2c_init(port_a_i2c_t *out);
void port_a_i2c_deinit(port_a_i2c_t *ctx);

// Convenience: add a device on Port A
esp_err_t port_a_add_device(port_a_i2c_t *ctx, uint8_t addr, uint32_t scl_hz,
							i2c_master_dev_handle_t *out_dev);
esp_err_t port_a_rem_device(const i2c_master_dev_handle_t *dev);

#ifdef __cplusplus
}
#endif