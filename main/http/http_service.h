#pragma once

#include <stdbool.h> /* bool */
#include <stddef.h>	 /* size_t */
#include <stdint.h>	 /* uint32_t */

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	HTTP_REQ_GET = 0,
	// later: HTTP_REQ_POST, etc
} http_method_t;

/**
 * @brief HTTP request message submitted to the http_service owner task.
 *
 * The requester may optionally provide a response buffer (rx_buf/rx_cap).
 * If provided, the HTTP service will copy the response body into rx_buf,
 * NUL-terminate it, and report rx_len/truncated in the response.
 *
 * Notes:
 *  - rx_buf is owned by the requester and must remain valid until the
 *    reply is received (or the request times out).
 *  - If rx_buf is NULL or rx_cap is 0, the body is discarded and only
 *    status/length are returned.
 */
typedef struct {
	uint32_t request_id;
	http_method_t method;
	char url[256];

	/* Optional response body sink (owned by requester) */
	char *rx_buf;  /* buffer to fill with response body */
	size_t rx_cap; /* capacity of rx_buf in bytes */

	QueueHandle_t reply_queue;
} http_req_t;

/**
 * @brief HTTP response message returned to the requester.
 *
 * Contains the esp_err_t result, HTTP status, and content length. If an RX
 * buffer was provided, rx_len and truncated describe the captured body.
 */
typedef struct {
	uint32_t request_id;
	esp_err_t err;
	int http_status;
	int content_length;

	/* Body info (valid only if requester provided rx_buf/rx_cap) */
	size_t rx_len;	/* bytes written into rx_buf (<= rx_cap-1) */
	bool truncated; /* true if body did not fit in rx_buf */
} http_resp_t;

/**
 * @brief Start the HTTP owner task and create the shared request queue.
 */
void http_service_start(void);

/**
 * @brief Return the queue used to submit HTTP requests to the owner task.
 *
 * @return QueueHandle_t request queue
 */
QueueHandle_t http_service_queue(void);

#ifdef __cplusplus
}
#endif