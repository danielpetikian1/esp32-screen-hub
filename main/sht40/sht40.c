#include "sht40.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SHT40_CMD_MEAS_HIGH_PREC_NO_HEAT  0xFD

static uint8_t crc8_sensirion(const uint8_t *data, int len) {
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

esp_err_t sht40_init(sht40_t *dev, i2c_port_t port, uint8_t addr) {
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->i2c_port = port;
    dev->addr = addr;
    return ESP_OK;
}

esp_err_t sht40_read(const sht40_t *dev, sht40_reading_t *out) {
    if (!dev || !out) return ESP_ERR_INVALID_ARG;

    // 1) Trigger measurement
    uint8_t cmd = SHT40_CMD_MEAS_HIGH_PREC_NO_HEAT;
    esp_err_t err = i2c_master_write_to_device(dev->i2c_port, dev->addr, &cmd, 1,
                                               pdMS_TO_TICKS(50));
    if (err != ESP_OK) return err;

    // 2) Wait for conversion (~10ms typical; give it margin)
    vTaskDelay(pdMS_TO_TICKS(15));

    // 3) Read 6 bytes: T(2) + CRC + RH(2) + CRC
    uint8_t buf[6] = {0};
    err = i2c_master_read_from_device(dev->i2c_port, dev->addr, buf, sizeof(buf),
                                      pdMS_TO_TICKS(50));
    if (err != ESP_OK) return err;

    // 4) CRC check
    if (crc8_sensirion(&buf[0], 2) != buf[2]) return ESP_ERR_INVALID_CRC;
    if (crc8_sensirion(&buf[3], 2) != buf[5]) return ESP_ERR_INVALID_CRC;

    uint16_t raw_t  = (uint16_t)((buf[0] << 8) | buf[1]);
    uint16_t raw_rh = (uint16_t)((buf[3] << 8) | buf[4]);

    // 5) Convert (Sensirion formula)
    out->temperature_c = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
    out->humidity_rh   = -6.0f  + 125.0f * ((float)raw_rh / 65535.0f);

    // Clamp RH to [0,100]
    if (out->humidity_rh < 0.0f) out->humidity_rh = 0.0f;
    if (out->humidity_rh > 100.0f) out->humidity_rh = 100.0f;

    return ESP_OK;
}
