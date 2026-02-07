#include "i2c_utils.h"

esp_err_t i2c_write_u8(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val,
					   int timeout_ms) {
	uint8_t data[2] = {reg, val};
	return i2c_master_transmit(dev, data, sizeof(data), timeout_ms);
}

esp_err_t i2c_read_u8(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *out,
					  int timeout_ms) {
	return i2c_master_transmit_receive(dev, &reg, 1, out, 1, timeout_ms);
}

esp_err_t i2c_probe_addr(i2c_master_bus_handle_t bus, uint8_t addr,
						 int timeout_ms) {
	// Create a temporary device handle just to probe
	i2c_master_dev_handle_t dev = NULL;
	i2c_device_config_t cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = addr,
		.scl_speed_hz = 100000, // probe slow & safe
	};

	esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &dev);
	if (err != ESP_OK)
		return err;

	uint8_t dummy = 0;
	err = i2c_master_transmit(
		dev, &dummy, 0, timeout_ms); // 0-byte write acts like address probe

	i2c_master_bus_rm_device(dev);
	return err;
}
