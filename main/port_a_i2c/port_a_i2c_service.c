#include "port_a_i2c_service.h"
#include "freertos/FreeRTOS.h"
#include "port_a_i2c.h"
#include <stdio.h>
#include <string.h>

#include "esp_err.h"

/* -------------------------------------------------------------------------- */
/* Static State                                                               */
/* -------------------------------------------------------------------------- */

/* Port A service request queue (created on first start). */
static QueueHandle_t port_a_q; // static queue for port A

/* -------------------------------------------------------------------------- */
/* Internal Owner Task                                                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief Port A I2C owner task.
 *
 * Serializes access to the Port A I2C bus by:
 *  - Blocking on the request queue
 *  - Executing the I2C operation for each request
 *  - Replying (optionally) to the requester's reply queue
 *
 * Notes:
 *  - This task must be the only code path that touches Port A I2C devices
 *    if you want strict single-owner arbitration.
 *  - If reply_queue is NULL, the response is effectively fire-and-forget.
 */
static void port_a_i2c_owner_task(void *arg) {

	/* arg currently unused (common FreeRTOS pattern) */
	port_a_i2c_req_t req;
	for (;;) {
		/* Block indefinitely until a request arrives */
		if (xQueueReceive(port_a_q, &req, portMAX_DELAY) == pdTRUE) {

			/* Populate response with correlation ID and pessimistic default */
			port_a_i2c_resp_t resp = {
				.request_id = req.request_id,
				.err = ESP_FAIL,
			};

			/* Execute transaction: command -> delay -> read bytes */
			resp.err = port_a_i2c_read(&req, &resp);

			/* Return response if the requester provided a reply queue */
			if (req.reply_queue) {
				(void)xQueueSend(req.reply_queue, &resp, pdMS_TO_TICKS(50));
			}
		}
	}
}

/* -------------------------------------------------------------------------- */
/* Public Service API                                                         */
/* -------------------------------------------------------------------------- */

void port_a_i2c_service_start(void) {
	/* Lazy-create request queue on first start */
	if (!port_a_q) {
		/* Depth chosen for expected request burst; tune as needed */
		port_a_q = xQueueCreate(8, sizeof(port_a_i2c_req_t));
	}

	/* Dedicated owner task pinned to core 1 (tune stack/priority as needed) */
	xTaskCreatePinnedToCore(port_a_i2c_owner_task, "port_a_i2c_service", 4096,
							NULL, 6, NULL, 1);
}

QueueHandle_t port_a_i2c_service_queue(void) { return port_a_q; }
