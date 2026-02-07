#include "port_a_i2c.h"
#include <string.h>

#define PORTA_SDA_GPIO 2
#define PORTA_SCL_GPIO 1

esp_err_t port_a_i2c_init(port_a_i2c_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));

    i2c_master_bus_config_t cfg = {
        .i2c_port = -1, // let driver pick a port (IDF 5.x new driver supports this)
        .sda_io_num = PORTA_SDA_GPIO,
        .scl_io_num = PORTA_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 16,
        .flags.enable_internal_pullup = 0, // Port A should use external pullups on the bus
    };

    return i2c_new_master_bus(&cfg, &out->bus);
}

void port_a_i2c_deinit(port_a_i2c_t *ctx)
{
    if (!ctx || !ctx->bus) return;
    i2c_del_master_bus(ctx->bus);
    ctx->bus = NULL;
}

esp_err_t port_a_add_device(port_a_i2c_t *ctx, uint8_t addr, uint32_t scl_hz,
                            i2c_master_dev_handle_t *out_dev)
{
    if (!ctx || !ctx->bus || !out_dev) return ESP_ERR_INVALID_ARG;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = scl_hz,
    };
    return i2c_master_bus_add_device(ctx->bus, &dev_cfg, out_dev);
}
