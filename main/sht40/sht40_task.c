#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "port_a_i2c_service.h" // for port_a_i2c_service_queue(), req/resp types

#define SHT40_CMD_MEAS_HIGH_PREC_NO_HEAT 0xFD
#define TAG "sht40"

static uint8_t crc8_sensirion(const uint8_t *data, int len) {
	uint8_t crc = 0xFF;
	for (int i = 0; i < len; i++) {
		crc ^= data[i];
		for (int b = 0; b < 8; b++) {
			crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31)
							   : (uint8_t)(crc << 1);
		}
	}
	return crc;
}

static esp_err_t process_buf(const uint8_t buf[6]) {
	uint8_t c0 = crc8_sensirion(&buf[0], 2);
	uint8_t c1 = crc8_sensirion(&buf[3], 2);

	if (c0 != buf[2])
		return ESP_ERR_INVALID_CRC;
	if (c1 != buf[5])
		return ESP_ERR_INVALID_CRC;

	uint16_t raw_t = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
	uint16_t raw_rh = (uint16_t)(((uint16_t)buf[3] << 8) | buf[4]);

	float temperature_c = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
	float humidity_rh = -6.0f + 125.0f * ((float)raw_rh / 65535.0f);

	// Optional clamp (datasheet allows a little out-of-range due to formula)
	if (humidity_rh < 0.0f)
		humidity_rh = 0.0f;
	if (humidity_rh > 100.0f)
		humidity_rh = 100.0f;

	ESP_LOGI(TAG, "Temperature: %.2f C, %.2f %%RH", temperature_c, humidity_rh);

	return ESP_OK;
}

static void sht40_task(void *arg) {
	i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)arg;

	QueueHandle_t port_a_q = port_a_i2c_service_queue();

	QueueHandle_t reply_q = xQueueCreate(2, sizeof(port_a_i2c_resp_t));
	if (!reply_q) {
		ESP_LOGE(TAG, "Failed to create reply queue");
		vTaskDelete(NULL);
	}

	TickType_t last = xTaskGetTickCount();
	const TickType_t period = pdMS_TO_TICKS(2 * 1000);

	uint32_t rid = 0;

	for (;;) {
		port_a_i2c_req_t req = {.request_id = ++rid,
								.sensor = SHT40,
								.cmd = SHT40_CMD_MEAS_HIGH_PREC_NO_HEAT,
								.dev = dev,
								.reply_queue = reply_q};

		(void)xQueueSend(port_a_q, &req, portMAX_DELAY);

		port_a_i2c_resp_t resp;
		if (xQueueReceive(reply_q, &resp, pdMS_TO_TICKS(500)) == pdTRUE) {
			if (resp.err != ESP_OK) {
				ESP_LOGW(TAG, "I2C read failed id=%" PRIu32 ": %s",
						 resp.request_id, esp_err_to_name(resp.err));
			} else {
				// process returned 6-byte measurement
				esp_err_t err = process_buf(resp.data);
				if (err != ESP_OK) {
					ESP_LOGW(TAG, "Bad CRC id=%" PRIu32, resp.request_id);
				}
			}
		} else {
			ESP_LOGW(TAG, "Timeout waiting for response id=%" PRIu32, rid);
		}

		vTaskDelayUntil(&last, period);
	}
}

void sht40_task_start(i2c_master_dev_handle_t dev) {
	xTaskCreatePinnedToCore(sht40_task, "sht40", 4096, (void *)dev, 5, NULL, 1);
}
