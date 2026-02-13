#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Port A I2C master bus.
 *
 * Allocates and configures an ESP-IDF I2C master bus for Port A.
 * The created bus handle is written to `bus` on success.
 *
 * @param[out] bus Pointer to a bus handle that will receive
 *                 the allocated I2C master bus.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if bus is NULL
 *      - Other ESP-IDF error codes from i2c_new_master_bus()
 */
esp_err_t port_a_i2c_init(i2c_master_bus_handle_t *bus);

/**
 * @brief Deinitialize Port A I2C master bus.
 *
 * Deletes the I2C master bus associated with the given handle.
 * Safe to call with NULL (no action taken).
 *
 * @param[in] bus I2C master bus handle returned by
 *                port_a_i2c_init().
 */
void port_a_i2c_deinit(i2c_master_bus_handle_t bus);

/**
 * @brief Add an I2C device to the Port A bus.
 *
 * Creates a device handle for a peripheral on the specified
 * Port A I2C bus.
 *
 * @param[in]  bus      Valid I2C master bus handle.
 * @param[in]  addr     7-bit I2C device address.
 * @param[in]  scl_hz   Clock speed for this device (Hz).
 * @param[out] out_dev  Pointer to receive the created device handle.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameters are invalid
 *      - Other ESP-IDF error codes from i2c_master_bus_add_device()
 */
esp_err_t port_a_add_device(i2c_master_bus_handle_t bus, uint8_t addr,
							uint32_t scl_hz, i2c_master_dev_handle_t *out_dev);

/**
 * @brief Remove an I2C device from the Port A bus.
 *
 * Deletes the specified device handle from the bus.
 *
 * @param[in] dev Device handle returned by port_a_add_device().
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if dev is NULL
 *      - Other ESP-IDF error codes from i2c_master_bus_rm_device()
 */
esp_err_t port_a_rem_device(i2c_master_dev_handle_t dev);

/**
 * @brief Perform a command-based I2C read transaction.
 *
 * Sends a single-byte command to the device, waits briefly,
 * and then reads data into the provided buffer.
 *
 * This is useful for sensors like the SHT40 that require
 * a command trigger before returning measurement data.
 *
 * @param[in]  dev      Valid I2C device handle.
 * @param[out] buf      Buffer to store received data.
 * @param[in]  buf_len  Number of bytes to read.
 * @param[in]  cmd      Single-byte command to transmit first.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameters are invalid
 *      - Other ESP-IDF error codes from transmit/receive operations
 */
esp_err_t port_a_i2c_read(i2c_master_dev_handle_t dev, uint8_t *buf,
						  size_t buf_len, uint8_t cmd);

#ifdef __cplusplus
}
#endif
