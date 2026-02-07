#include "i2c_bus.h"

esp_err_t i2c_bus_init(const i2c_bus_cfg_t *cfg) {
    if (!cfg) return ESP_ERR_INVALID_ARG;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = cfg->sda_io,
        .scl_io_num = cfg->scl_io,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = cfg->clk_hz,
        .clk_flags = 0,
    };

    esp_err_t err = i2c_param_config(cfg->port, &conf);
    if (err != ESP_OK) return err;

    // If already installed, i2c_driver_install returns ESP_ERR_INVALID_STATE.
    // Treat that as "already initialized".
    err = i2c_driver_install(cfg->port, conf.mode, 0, 0, 0);
    if (err == ESP_ERR_INVALID_STATE) return ESP_OK;

    return err;
}

esp_err_t i2c_bus_write(i2c_port_t port, uint8_t addr, const uint8_t *data, size_t len, TickType_t timeout) {
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;
    return i2c_master_write_to_device(port, addr, data, len, timeout);
}

esp_err_t i2c_bus_read(i2c_port_t port, uint8_t addr, uint8_t *data, size_t len, TickType_t timeout) {
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;
    return i2c_master_read_from_device(port, addr, data, len, timeout);
}

esp_err_t i2c_bus_write_read(i2c_port_t port, uint8_t addr,
                            const uint8_t *tx, size_t tx_len,
                            uint8_t *rx, size_t rx_len,
                            TickType_t timeout) {
    if (!tx || tx_len == 0 || !rx || rx_len == 0) return ESP_ERR_INVALID_ARG;
    return i2c_master_write_read_device(port, addr, tx, tx_len, rx, rx_len, timeout);
}
