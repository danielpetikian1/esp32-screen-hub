#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "port_i2c_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Logical port selector (optional convenience).
 *
 * Use PORT_I2C_PORT_CUSTOM if you want to supply pins manually.
 */
typedef enum {
	PORT_I2C_PORT_A = 0,
	PORT_I2C_PORT_B = 1,
	PORT_I2C_PORT_C = 2,
} port_i2c_port_t;

/**
 * @brief I2C bus initialization parameters.
 *
 * This struct is the single source of truth for how the bus is wired.
 * You can populate it from menuconfig or from a fixed board mapping.
 */
typedef struct {
	int sda_io_num; ///< SDA GPIO number
	int scl_io_num; ///< SCL GPIO number
	i2c_clock_source_t
		clk_source;				///< Clock source (usually I2C_CLK_SRC_DEFAULT)
	uint8_t glitch_ignore_cnt;	///< Glitch filter count (typical: 7)
	int intr_priority;			///< Interrupt priority (0 = default)
	uint32_t trans_queue_depth; ///< Internal transaction queue depth (0 = none)
	bool enable_internal_pullup; ///< true to enable internal pullups (often
								 ///< false on Grove)
} port_i2c_bus_config_t;

/**
 * @brief Fill a bus config from a known board port mapping (A/B/C).
 *
 * This is optional convenience. If you don't want "ports", skip this
 * and just populate port_i2c_bus_config_t directly.
 *
 * @param[in]  port   Logical port selector (A/B/C).
 * @param[out] out    Filled config.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG for bad args
 *      - ESP_ERR_NOT_FOUND if the port mapping isn't supported
 */
esp_err_t port_i2c_get_default_bus_config(port_i2c_port_t port,
										  port_i2c_bus_config_t *out);

/**
 * @brief Initialize an I2C master bus using the provided bus config.
 *
 * Allocates and configures an ESP-IDF I2C master bus instance.
 *
 * @param[in]  cfg  Bus configuration (pins, pullups, filter, etc.)
 * @param[out] out_bus Pointer to receive the allocated bus handle.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if arguments are invalid
 *      - Other ESP-IDF error codes from i2c_new_master_bus()
 */
esp_err_t port_i2c_bus_init(const port_i2c_bus_config_t *cfg,
							i2c_master_bus_handle_t *out_bus);

/**
 * @brief Deinitialize (delete) an I2C master bus.
 *
 * Safe to call with NULL.
 *
 * @param[in] bus Bus handle returned by port_i2c_bus_init().
 */
void port_i2c_bus_deinit(i2c_master_bus_handle_t bus);

/**
 * @brief Add an I2C device to a bus.
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
esp_err_t port_i2c_add_device(i2c_master_bus_handle_t bus, uint8_t addr,
							  uint32_t scl_hz,
							  i2c_master_dev_handle_t *out_dev);

/**
 * @brief Remove an I2C device from its bus.
 *
 * @param[in] dev Device handle returned by port_i2c_add_device().
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if dev is NULL
 *      - Other ESP-IDF error codes from i2c_master_bus_rm_device()
 */
esp_err_t port_i2c_rm_device(i2c_master_dev_handle_t dev);

/**
 * @brief Execute a command + optional delay + optional read transaction.
 *
 * This is generic (works for any I2C device handle).
 *
 * @param[in]  req  Request descriptor (dev, cmd_len, cmd, rx_len, delays, etc.)
 * @param[out] out  Response (err + data)
 *
 * @return ESP_OK on success, otherwise error code.
 */
esp_err_t port_i2c_xfer(const port_i2c_req_t *req, port_i2c_resp_t *out);

#ifdef __cplusplus
}
#endif