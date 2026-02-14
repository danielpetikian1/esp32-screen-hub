#include "port_a_i2c.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

// -----------------------------------------------------------------------------
// Port A I2C Pin Configuration (M5Stack CoreS3 Grove Port A)
// -----------------------------------------------------------------------------

// Grove Port A on CoreS3 is routed to GPIO2 (SDA) and GPIO1 (SCL)
#define PORTA_SDA_GPIO 2
#define PORTA_SCL_GPIO 1

// Example sensor command (SHT40 high precision measurement, no heater)
#define SHT40_CMD_MEAS_HIGH_PREC_NO_HEAT 0xFD

/* -------------------------------------------------------------------------- */
/* Port A Bus Initialization                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize the Port A I2C master bus.
 *
 * Configures GPIO2/1 as I2C SDA/SCL and creates a new master bus instance.
 * This must be called before adding any devices to the bus.
 *
 * @param bus Output handle to the created I2C master bus.
 * @return ESP_OK on success, otherwise error code.
 */
esp_err_t port_a_i2c_init(i2c_master_bus_handle_t *bus) {
	if (!bus) {
		return ESP_ERR_INVALID_ARG;
	}

	// Ensure caller doesn't accidentally use a stale handle
	*bus = NULL;

	// Configure I2C master bus parameters
	i2c_master_bus_config_t cfg = {
		.i2c_port = -1, // Let ESP-IDF auto-select hardware I2C port
		.sda_io_num = PORTA_SDA_GPIO,
		.scl_io_num = PORTA_SCL_GPIO,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7, // Filter small glitches on lines
		.intr_priority = 0,
		.trans_queue_depth = 0, // No internal transaction queue
		// Grove bus provides external pull-ups
		.flags.enable_internal_pullup = 0,
	};

	return i2c_new_master_bus(&cfg, bus);
}

/**
 * @brief Deinitialize and delete the Port A I2C bus.
 *
 * Safe to call with NULL handle.
 */
void port_a_i2c_deinit(i2c_master_bus_handle_t bus) {
	if (!bus)
		return;

	i2c_del_master_bus(bus);
}

/* -------------------------------------------------------------------------- */
/* Device Management                                                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Add a 7-bit I2C device to the Port A bus.
 *
 * This associates a device address and SCL speed with the master bus,
 * returning a device handle used for future transactions.
 *
 * @param bus     Initialized I2C master bus handle
 * @param addr    7-bit I2C device address
 * @param scl_hz  Desired clock speed (e.g., 100000 or 50000)
 * @param out_dev Output handle to created device
 */
esp_err_t port_a_add_device(i2c_master_bus_handle_t bus, uint8_t addr,
							uint32_t scl_hz, i2c_master_dev_handle_t *out_dev) {
	if (!bus)
		return ESP_ERR_INVALID_ARG;

	i2c_device_config_t dev_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = addr,
		.scl_speed_hz = scl_hz,
	};

	return i2c_master_bus_add_device(bus, &dev_cfg, out_dev);
}

/**
 * @brief Remove a device from the Port A bus.
 */
esp_err_t port_a_rem_device(const i2c_master_dev_handle_t dev) {
	if (!dev) {
		return ESP_ERR_INVALID_ARG;
	}

	return i2c_master_bus_rm_device(dev);
}

/* -------------------------------------------------------------------------- */
/* Transaction Helpers                                                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief Compute progressive backoff delay between RX retry attempts.
 *
 * Prevents hammering the bus repeatedly if the peripheral is temporarily busy.
 * Backoff pattern:
 *   2ms → 5ms → 10ms → 20ms → 30ms → 50ms (then stays at 50ms)
 *
 * @param attempt Retry attempt index
 * @return Tick delay before next attempt
 */
static TickType_t rx_backoff_ticks(int attempt) {
	switch (attempt) {
	case 0:
		return pdMS_TO_TICKS(2);
	case 1:
		return pdMS_TO_TICKS(5);
	case 2:
		return pdMS_TO_TICKS(10);
	case 3:
		return pdMS_TO_TICKS(20);
	case 4:
		return pdMS_TO_TICKS(30);
	default:
		return pdMS_TO_TICKS(50);
	}
}

/**
 * @brief Execute a complete Port A I2C transaction.
 *
 * Transaction phases:
 *   1) Optional command transmit
 *   2) Optional delay (sensor conversion time)
 *   3) Optional read with retry/backoff
 *
 * This function is intended to be called only by the Port A owner task,
 * which serializes bus access.
 *
 * @param req  Request descriptor
 * @param out  Response structure (filled with result + raw data)
 */
esp_err_t port_a_i2c_read(const port_a_i2c_req_t *req, port_a_i2c_resp_t *out) {
	if (!req || !out)
		return ESP_ERR_INVALID_ARG;
	if (!req->dev)
		return ESP_ERR_INVALID_ARG;
	if (req->rx_len > sizeof(out->data))
		return ESP_ERR_INVALID_ARG;

	out->request_id = req->request_id;
	out->err = ESP_OK;
	memset(out->data, 0, sizeof(out->data));

	// -------------------------------------------------------------------------
	// Build command bytes explicitly.
	// Never transmit &req->cmd directly (endianness matters).
	// -------------------------------------------------------------------------
	uint8_t cmd_buf[2];

	if (req->cmd_len == 0) {
		// Write-less transaction (read-only)
	} else if (req->cmd_len == 1) {
		// 8-bit command (e.g., SHT40)
		cmd_buf[0] = (uint8_t)(req->cmd & 0xFF);
	} else if (req->cmd_len == 2) {
		// 16-bit command, big-endian on wire (e.g., SGP30)
		cmd_buf[0] = (uint8_t)((req->cmd >> 8) & 0xFF);
		cmd_buf[1] = (uint8_t)(req->cmd & 0xFF);
	} else {
		out->err = ESP_ERR_INVALID_ARG;
		return out->err;
	}

	// -------------------------------------------------------------------------
	// Write phase
	// -------------------------------------------------------------------------
	if (req->cmd_len > 0) {
		out->err = i2c_master_transmit(req->dev, cmd_buf, req->cmd_len, 200);
		if (out->err != ESP_OK)
			return out->err;
	}

	// -------------------------------------------------------------------------
	// Sensor-specific conversion delay (if requested)
	// -------------------------------------------------------------------------
	if (req->post_cmd_delay_ticks > 0) {
		vTaskDelay(req->post_cmd_delay_ticks);
	}

	// -------------------------------------------------------------------------
	// Read phase with backoff retry
	// -------------------------------------------------------------------------
	if (req->rx_len > 0) {
		for (int i = 0; i < 8; i++) {
			out->err =
				i2c_master_receive(req->dev, out->data, req->rx_len, 200);

			if (out->err == ESP_OK)
				return ESP_OK;

			// If device NACKs or bus is busy, wait progressively longer
			vTaskDelay(rx_backoff_ticks(i));
		}

		// All attempts failed
		return out->err;
	}

	return ESP_OK;
}
