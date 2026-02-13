#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Logical sensor identifier for Port A requests.
 *
 * Used by requesters to indicate which sensor a command/transaction
 * is intended for. The owner task may use this to route behavior
 * (e.g., different read lengths, delays, or command formats).
 *
 * Note: naming is application-specific.
 */
typedef enum { SHT40 = 0, SGT30 } sensor_t;

/**
 * @brief Request message sent to the Port A I2C owner task.
 *
 * A requester sends one of these to the Port A service queue.
 * The owner task serializes access to the I2C bus by processing
 * requests one-at-a-time.
 */
typedef struct {
	/** Correlation ID assigned by the requester. */
	uint32_t request_id;

	/** Target sensor (application-level routing hint). */
	const sensor_t sensor;

	/** Command byte to transmit prior to reading (if applicable). */
	const uint8_t cmd;

	/** Device handle created from the Port A bus (i2c_master_dev_handle_t). */
	i2c_master_dev_handle_t dev;

	/**
	 * @brief Optional reply queue.
	 *
	 * If non-NULL, the owner task sends exactly one response message
	 * back to this queue when the transaction completes.
	 */
	QueueHandle_t reply_queue;
} port_a_i2c_req_t;

/**
 * @brief Response message sent back to the requester.
 *
 * Returned via the request's reply_queue (if provided). The payload
 * format is application-defined; `data` commonly contains raw sensor bytes.
 *
 * Note: `data[6]` matches sensors like SHT40 (6-byte measurement), but
 * other sensors may require different sizes (caller/owner must agree).
 */
typedef struct {
	/** Correlation ID copied from request_id. */
	uint32_t request_id;

	/** Result of the I2C transaction (ESP_OK on success). */
	esp_err_t err;

	/** Raw response bytes (application-defined). */
	uint8_t data[6];
} port_a_i2c_resp_t; // todo please edit this

/**
 * @brief Start the Port A I2C service.
 *
 * Creates the service request queue (if needed) and starts the owner task.
 * Must be called before using port_a_i2c_service_queue().
 */
void port_a_i2c_service_start(void);

/**
 * @brief Get the Port A I2C service request queue.
 *
 * Requesters send port_a_i2c_req_t messages to this queue.
 *
 * @return Queue handle for Port A service requests (NULL if service not started).
 */
QueueHandle_t port_a_i2c_service_queue(void);

#ifdef __cplusplus
}
#endif
