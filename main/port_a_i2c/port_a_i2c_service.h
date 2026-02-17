#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

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
 * @return Queue handle for Port A service requests (NULL if service not
 * started).
 */
QueueHandle_t port_a_i2c_service_queue(void);

#ifdef __cplusplus
}
#endif
