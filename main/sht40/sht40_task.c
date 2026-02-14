#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "port_a_i2c_service.h" // port_a_i2c_service_queue(), req/resp types
#include "port_a_i2c_types.h"

#include "sensirion_utils.h" // sensirion_crc8()

// SHT40 "measure high precision, no heater" command (8-bit)
#define SHT40_CMD_MEAS_HIGH_PREC_NO_HEAT 0xFD

// Logging tag for this module/task
#define TAG "sht40"

/**
 * @brief Validate and decode a 6-byte SHT40 measurement frame.
 *
 * SHT40 returns two 16-bit words, each followed by a CRC byte:
 *   [0..1] Temperature raw (MSB..LSB), [2] CRC
 *   [3..4] Humidity raw (MSB..LSB),    [5] CRC
 *
 * This function:
 *  - Verifies both CRC bytes using Sensirion CRC-8
 *  - Converts the raw words into temperature (°C) and relative humidity (%RH)
 *  - Logs the decoded values
 *
 * @param buf 6-byte measurement buffer from the sensor.
 * @return ESP_OK on success, ESP_ERR_INVALID_CRC if CRC fails.
 */
static esp_err_t process_buf(const uint8_t buf[6]) {
	// CRC is computed over each 2-byte word independently
	uint8_t c0 = sensirion_crc8(&buf[0], 2);
	uint8_t c1 = sensirion_crc8(&buf[3], 2);

	// Reject data if either CRC byte does not match
	if (c0 != buf[2])
		return ESP_ERR_INVALID_CRC;
	if (c1 != buf[5])
		return ESP_ERR_INVALID_CRC;

	// Raw values are big-endian words (MSB first)
	uint16_t raw_t = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
	uint16_t raw_rh = (uint16_t)(((uint16_t)buf[3] << 8) | buf[4]);

	// Convert according to SHT4x datasheet formulas
	float temperature_c = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
	float humidity_rh = -6.0f + 125.0f * ((float)raw_rh / 65535.0f);

	// Clamp RH to [0,100] since the formula can produce slight out-of-range
	// values
	if (humidity_rh < 0.0f)
		humidity_rh = 0.0f;
	if (humidity_rh > 100.0f)
		humidity_rh = 100.0f;

	// Report decoded measurement
	ESP_LOGI(TAG, "Temperature: %.2f C, %.2f %%RH", temperature_c, humidity_rh);

	return ESP_OK;
}

/**
 * @brief FreeRTOS task that periodically reads SHT40 via the Port A I2C
 * service.
 *
 * The task:
 *  - Creates a private reply queue for responses from the owner task
 *  - Periodically sends an I2C request (command + delay + 6-byte read)
 *  - Validates/decodes the returned bytes and logs the measurement
 *
 * Notes:
 *  - All actual I2C transactions are executed by the Port A owner task.
 *  - This task only submits requests and processes replies.
 */
static void sht40_task(void *arg) {
	// Device handle (created by port_a_add_device) passed in via task arg
	i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)arg;

	// Port A service request queue (shared by all Port A requesters)
	QueueHandle_t port_a_q = port_a_i2c_service_queue();

	// Per-task reply queue: owner sends exactly one response per request
	QueueHandle_t reply_q = xQueueCreate(2, sizeof(port_a_i2c_resp_t));
	if (!reply_q) {
		ESP_LOGE(TAG, "Failed to create reply queue");
		vTaskDelete(NULL);
	}

	// Periodic scheduling state
	TickType_t last = xTaskGetTickCount();
	const TickType_t period = pdMS_TO_TICKS(2 * 1000); // 2 seconds

	// Request correlation ID (increments each request)
	uint32_t rid = 0;

	for (;;) {
		// Build a Port A transaction request:
		//  - Send 1-byte command (0xFD)
		//  - Wait 25ms for conversion
		//  - Read 6 bytes (T word+CRC, RH word+CRC)
		port_a_i2c_req_t req = {
			.request_id = ++rid,
			.sensor = SHT40,
			.cmd = SHT40_CMD_MEAS_HIGH_PREC_NO_HEAT,
			.cmd_len = 1,
			.rx_len = 6,
			.post_cmd_delay_ticks = pdMS_TO_TICKS(25),
			.dev = dev,
			.reply_queue = reply_q,
		};

		// Submit request to the Port A owner task (blocks until queued)
		(void)xQueueSend(port_a_q, &req, portMAX_DELAY);

		// Wait for response from the owner task
		port_a_i2c_resp_t resp = {0};
		if (xQueueReceive(reply_q, &resp, pdMS_TO_TICKS(500)) == pdTRUE) {
			// Transport-level failure (NACK/timeout/etc.)
			if (resp.err != ESP_OK) {
				ESP_LOGW(TAG, "I2C read failed id=%" PRIu32 ": %s",
						 resp.request_id, esp_err_to_name(resp.err));
			} else {
				// CRC/format validation + unit conversion
				esp_err_t err = process_buf(resp.data);
				if (err != ESP_OK) {
					ESP_LOGW(TAG, "Bad CRC id=%" PRIu32, resp.request_id);
				}
			}
		} else {
			// No response received within timeout window
			ESP_LOGW(TAG, "Timeout waiting for response id=%" PRIu32, rid);
		}

		// Sleep until next period boundary (prevents drift)
		vTaskDelayUntil(&last, period);
	}
}

/**
 * @brief Create and start the SHT40 task.
 *
 * @param dev I2C device handle for the SHT40.
 */
void sht40_task_start(i2c_master_dev_handle_t dev) {
	// Pin task to core 1; adjust stack/priority as needed
	xTaskCreatePinnedToCore(sht40_task, "sht40", 4096, (void *)dev, 5, NULL, 1);
}
