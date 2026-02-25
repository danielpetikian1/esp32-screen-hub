#include "port_a_i2c.h"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

// -----------------------------------------------------------------------------
// Default board mapping (M5Stack CoreS3 Grove Ports)
// -----------------------------------------------------------------------------
// If you later want this to be board-agnostic, move these into Kconfig or a
// bsp_* component. The API remains generic either way.
#define PORTA_SCL_GPIO 1
#define PORTA_SDA_GPIO 2
#define PORTB_SCL_GPIO 8
#define PORTB_SDA_GPIO 9
#define PORTC_SCL_GPIO 18
#define PORTC_SDA_GPIO 17

/* -------------------------------------------------------------------------- */
/* Default config helper                                                      */
/* -------------------------------------------------------------------------- */

esp_err_t port_i2c_get_default_bus_config(port_i2c_port_t port,
										  port_i2c_bus_config_t *out) {
	if (!out)
		return ESP_ERR_INVALID_ARG;

	// Common defaults that work well for Grove-style I2C
	port_i2c_bus_config_t cfg = {
		.sda_io_num = -1,
		.scl_io_num = -1,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.intr_priority = 0,
		.trans_queue_depth = 0,
		.enable_internal_pullup = false,
	};

	switch (port) {
	case PORT_I2C_PORT_A:
		cfg.sda_io_num = PORTA_SDA_GPIO;
		cfg.scl_io_num = PORTA_SCL_GPIO;
		break;
	case PORT_I2C_PORT_B:
		cfg.sda_io_num = PORTB_SDA_GPIO;
		cfg.scl_io_num = PORTB_SCL_GPIO;
		break;
	case PORT_I2C_PORT_C:
		cfg.sda_io_num = PORTC_SDA_GPIO;
		cfg.scl_io_num = PORTC_SCL_GPIO;
		break;
	default:
		return ESP_ERR_NOT_FOUND;
	}

	*out = cfg;
	return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/* Bus init/deinit                                                            */
/* -------------------------------------------------------------------------- */

esp_err_t port_i2c_bus_init(const port_i2c_bus_config_t *cfg,
							i2c_master_bus_handle_t *out_bus) {
	if (!cfg || !out_bus)
		return ESP_ERR_INVALID_ARG;
	if (cfg->sda_io_num < 0 || cfg->scl_io_num < 0)
		return ESP_ERR_INVALID_ARG;

	*out_bus = NULL;

	i2c_master_bus_config_t idf_cfg = {
		.i2c_port = -1, // let ESP-IDF pick a port (new driver supports this)
		.sda_io_num = cfg->sda_io_num,
		.scl_io_num = cfg->scl_io_num,
		.clk_source = cfg->clk_source,
		.glitch_ignore_cnt = cfg->glitch_ignore_cnt,
		.intr_priority = cfg->intr_priority,
		.trans_queue_depth = cfg->trans_queue_depth,
		.flags.enable_internal_pullup = cfg->enable_internal_pullup ? 1 : 0,
	};

	return i2c_new_master_bus(&idf_cfg, out_bus);
}

void port_i2c_bus_deinit(i2c_master_bus_handle_t bus) {
	if (!bus)
		return;
	i2c_del_master_bus(bus);
}

/* -------------------------------------------------------------------------- */
/* Device management                                                          */
/* -------------------------------------------------------------------------- */

esp_err_t port_i2c_add_device(i2c_master_bus_handle_t bus, uint8_t addr,
							  uint32_t scl_hz,
							  i2c_master_dev_handle_t *out_dev) {
	if (!bus || !out_dev)
		return ESP_ERR_INVALID_ARG;

	i2c_device_config_t dev_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = addr,
		.scl_speed_hz = scl_hz,
	};

	return i2c_master_bus_add_device(bus, &dev_cfg, out_dev);
}

esp_err_t port_i2c_rm_device(i2c_master_dev_handle_t dev) {
	if (!dev)
		return ESP_ERR_INVALID_ARG;
	return i2c_master_bus_rm_device(dev);
}

/* -------------------------------------------------------------------------- */
/* Transaction helpers                                                        */
/* -------------------------------------------------------------------------- */

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

esp_err_t port_i2c_xfer(const port_i2c_req_t *req, port_i2c_resp_t *out) {
	if (!req || !out)
		return ESP_ERR_INVALID_ARG;
	if (!req->dev)
		return ESP_ERR_INVALID_ARG;
	if (req->rx_len > sizeof(out->data))
		return ESP_ERR_INVALID_ARG;

	out->request_id = req->request_id;
	out->err = ESP_OK;
	memset(out->data, 0, sizeof(out->data));

	// Build command bytes explicitly (endianness-safe)
	uint8_t cmd_buf[2];

	if (req->cmd_len == 0) {
		// no write phase
	} else if (req->cmd_len == 1) {
		cmd_buf[0] = (uint8_t)(req->cmd & 0xFF);
	} else if (req->cmd_len == 2) {
		// 16-bit command, big-endian on the wire
		cmd_buf[0] = (uint8_t)((req->cmd >> 8) & 0xFF);
		cmd_buf[1] = (uint8_t)(req->cmd & 0xFF);
	} else {
		out->err = ESP_ERR_INVALID_ARG;
		return out->err;
	}

	// Write phase
	if (req->cmd_len > 0) {
		out->err = i2c_master_transmit(req->dev, cmd_buf, req->cmd_len, 200);
		if (out->err != ESP_OK)
			return out->err;
	}

	// Optional conversion delay
	if (req->post_cmd_delay_ticks > 0) {
		vTaskDelay(req->post_cmd_delay_ticks);
	}

	// Read phase with retry/backoff
	if (req->rx_len > 0) {
		for (int i = 0; i < 8; i++) {
			out->err =
				i2c_master_receive(req->dev, out->data, req->rx_len, 200);
			if (out->err == ESP_OK)
				return ESP_OK;
			vTaskDelay(rx_backoff_ticks(i));
		}
		return out->err;
	}

	return ESP_OK;
}