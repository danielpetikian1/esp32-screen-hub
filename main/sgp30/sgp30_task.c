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

// Logging tag for this module/task
#define TAG "sgp30"

// SGP30 I2C address is typically 0x58 (handled when creating dev handle)

// SGP30 commands are 16-bit values transmitted big-endian on the I2C bus
#define SGP30_CMD_IAQ_INIT 0x2003
#define SGP30_CMD_MEASURE_IAQ 0x2008

/**
 * @brief Validate and decode the SGP30 "Measure IAQ" response frame.
 *
 * The SGP30 returns 2 words, each followed by a CRC byte:
 *   [0..1] eCO2 word (MSB..LSB), [2] CRC
 *   [3..4] TVOC word (MSB..LSB), [5] CRC
 *
 * This function:
 *  - Computes Sensirion CRC-8 over each 2-byte word
 *  - Compares with the corresponding CRC byte
 *  - Decodes big-endian words into host-endian uint16_t values
 *
 * @param buf       6-byte response buffer (two words + CRCs)
 * @param eco2_ppm  Output: eCO2 concentration in ppm
 * @param tvoc_ppb  Output: TVOC concentration in ppb
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_CRC on CRC failure.
 */
static esp_err_t sgp30_process_iaq_buf(const uint8_t buf[6], uint16_t *eco2_ppm,
									   uint16_t *tvoc_ppb) {
	if (!buf || !eco2_ppm || !tvoc_ppb) {
		return ESP_ERR_INVALID_ARG;
	}

	// Sensirion CRC is computed per-word over the 2 data bytes
	uint8_t c0 = sensirion_crc8(&buf[0], 2);
	uint8_t c1 = sensirion_crc8(&buf[3], 2);

	// Reject frame if either CRC byte does not match
	if (c0 != buf[2]) {
		return ESP_ERR_INVALID_CRC;
	}
	if (c1 != buf[5]) {
		return ESP_ERR_INVALID_CRC;
	}

	// Words are big-endian in the response payload
	*eco2_ppm = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
	*tvoc_ppb = (uint16_t)(((uint16_t)buf[3] << 8) | buf[4]);

	return ESP_OK;
}

/**
 * @brief FreeRTOS task that periodically polls the SGP30 via the Port A I2C
 * service.
 *
 * High-level behavior:
 *  1) Send IAQ init (required once after boot)
 *  2) Every ~1s: send Measure IAQ command and read back 6 bytes
 *  3) Validate CRCs, decode eCO2/TVOC, and log the readings
 *
 * Notes:
 *  - All physical I2C operations are executed by the Port A owner task.
 *  - This task only submits requests and processes responses.
 *  - A small post-command delay is used before reading to avoid "busy" windows
 *    where the SGP30 may NACK reads.
 */
static void sgp30_task(void *arg) {
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

	// Request correlation ID (increments each request)
	uint32_t rid = 0;

	// -------------------------------------------------------------------------
	// 1) IAQ initialization (run once after power-up)
	// -------------------------------------------------------------------------
	{
		// IAQ init is a write-only command (no readback)
		port_a_i2c_req_t init_req = {
			.request_id = ++rid,
			.sensor = SGP30,
			.cmd = SGP30_CMD_IAQ_INIT,
			.cmd_len = 2,
			.rx_len = 0,
			.post_cmd_delay_ticks = pdMS_TO_TICKS(100),
			.dev = dev,
			.reply_queue = reply_q,
		};

		// Submit init request to Port A owner
		(void)xQueueSend(port_a_q, &init_req, portMAX_DELAY);

		// Wait for completion status from owner task
		port_a_i2c_resp_t init_resp = {0};
		if (xQueueReceive(reply_q, &init_resp, pdMS_TO_TICKS(500)) == pdTRUE) {
			if (init_resp.err != ESP_OK) {
				ESP_LOGW(TAG, "IAQ init failed id=%" PRIu32 ": %s",
						 init_resp.request_id, esp_err_to_name(init_resp.err));
			} else {
				ESP_LOGI(TAG, "IAQ init OK");
			}
		} else {
			ESP_LOGW(TAG, "Timeout waiting for IAQ init response id=%" PRIu32,
					 rid);
		}

		// Small guard time before starting periodic measurements
		vTaskDelay(pdMS_TO_TICKS(10));
	}

	// -------------------------------------------------------------------------
	// 2) Periodic measurement loop
	// -------------------------------------------------------------------------

	// SGP30 IAQ measurement is typically polled at 1 Hz
	TickType_t last = xTaskGetTickCount();
	const TickType_t period = pdMS_TO_TICKS(1000);

	for (;;) {
		// Measure IAQ is command + 6-byte response (two words + CRCs)
		port_a_i2c_req_t req = {
			.request_id = ++rid,
			.sensor = SGP30,
			.cmd = SGP30_CMD_MEASURE_IAQ,
			.cmd_len = 2,
			.rx_len = 6,
			// Delay before read helps avoid read attempts during internal
			// update windows
			.post_cmd_delay_ticks = pdMS_TO_TICKS(30),
			.dev = dev,
			.reply_queue = reply_q,
		};

		// Submit request to the Port A owner task
		(void)xQueueSend(port_a_q, &req, portMAX_DELAY);

		// Wait for response (I2C operation + optional retries in owner)
		port_a_i2c_resp_t resp = {0};
		if (xQueueReceive(reply_q, &resp, pdMS_TO_TICKS(800)) == pdTRUE) {
			// Transport-level failure (NACK/timeout/etc.)
			if (resp.err != ESP_OK) {
				ESP_LOGW(TAG, "I2C read failed id=%" PRIu32 ": %s",
						 resp.request_id, esp_err_to_name(resp.err));
			} else {
				// CRC validation + decode into eCO2 and TVOC units
				uint16_t eco2 = 0, tvoc = 0;
				esp_err_t perr = sgp30_process_iaq_buf(resp.data, &eco2, &tvoc);
				if (perr != ESP_OK) {
					ESP_LOGW(TAG, "Bad CRC id=%" PRIu32, resp.request_id);
				} else {
					ESP_LOGI(TAG,
							 "eCO2: %" PRIu16 " ppm, TVOC: %" PRIu16 " ppb",
							 eco2, tvoc);
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
 * @brief Create and start the SGP30 task.
 *
 * @param dev I2C device handle for the SGP30.
 */
void sgp30_task_start(i2c_master_dev_handle_t dev) {
	// Pin task to core 1; adjust stack/priority as needed
	xTaskCreatePinnedToCore(sgp30_task, "sgp30", 4096, (void *)dev, 5, NULL, 1);
}
