#include "sht40.h"
#include "esp_check.h"
#include "esp_rom_sys.h" // esp_rom_delay_us

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

esp_err_t sht40_read(const i2c_master_dev_handle_t *dev, sht40_reading_t *out) {
	if (!dev || !dev || !out)
		return ESP_ERR_INVALID_ARG;

	const uint8_t cmd = SHT40_CMD_MEAS_HIGH_PREC_NO_HEAT;
	esp_err_t err = i2c_master_transmit(*dev, &cmd, 1, 200);
	if (err != ESP_OK)
		return err;

	// Give it a minimum time
	vTaskDelay(pdMS_TO_TICKS(25));

	uint8_t buf[6];

	// Try multiple times because the sensor can NACK while busy
	for (int i = 0; i < 8; i++) {
		err = i2c_master_receive(*dev, buf, sizeof(buf), 200);
		if (err == ESP_OK)
			break;
		vTaskDelay(pdMS_TO_TICKS(3));
	}
	if (err != ESP_OK)
		return err;

	uint8_t c0 = crc8_sensirion(&buf[0], 2);
	uint8_t c1 = crc8_sensirion(&buf[3], 2);
	if (c0 != buf[2])
		return ESP_ERR_INVALID_CRC;
	if (c1 != buf[5])
		return ESP_ERR_INVALID_CRC;

	uint16_t raw_t = (uint16_t)((buf[0] << 8) | buf[1]);
	uint16_t raw_rh = (uint16_t)((buf[3] << 8) | buf[4]);

	out->temperature_c = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
	out->humidity_rh = -6.0f + 125.0f * ((float)raw_rh / 65535.0f);

	if (out->humidity_rh < 0.0f)
		out->humidity_rh = 0.0f;
	if (out->humidity_rh > 100.0f)
		out->humidity_rh = 100.0f;

	return ESP_OK;
}
