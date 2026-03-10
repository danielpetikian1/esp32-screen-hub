#include "http_service.h"
#include "common/app_events.h"
#include "freertos/FreeRTOS.h"
#include "net_manager.h"
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "http_service";

static QueueHandle_t s_http_q;

/**
 * @brief Per-request receive context used by the HTTP event handler.
 *
 * This structure lives on the stack inside do_get() and is passed to the
 * event handler via esp_http_client_config_t.user_data.
 *
 * Notes:
 *  - The buffer pointer is owned by the requester (req->rx_buf).
 *  - The event handler appends data chunks and keeps the buffer NUL-terminated.
 */
typedef struct {
	char *buf;
	size_t cap;		/* total capacity of buf */
	size_t len;		/* bytes currently written */
	bool truncated; /* ran out of room */
} http_rx_ctx_t;

/**
 * @brief esp_http_client event handler used to capture response bodies.
 *
 * If the requester provided an RX buffer (req->rx_buf / req->rx_cap),
 * this handler appends HTTP_EVENT_ON_DATA chunks into that buffer and
 * ensures it stays NUL-terminated.
 *
 * Notes:
 *  - The handler does not log body content to avoid noisy logs.
 *  - If the response does not fit, rx->truncated is set true.
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
	http_rx_ctx_t *rx = (http_rx_ctx_t *)evt->user_data;

	/* If caller did not supply a buffer, ignore body data. */
	if (!rx || !rx->buf || rx->cap == 0) {
		return ESP_OK;
	}

	if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data_len > 0) {
		/* Leave room for NUL terminator so buf is always a C string. */
		size_t room = 0;
		if (rx->len < rx->cap) {
			room = (rx->cap - 1) - rx->len;
		}

		size_t copy =
			(evt->data_len < (int)room) ? (size_t)evt->data_len : room;

		if (copy > 0) {
			memcpy(rx->buf + rx->len, evt->data, copy);
			rx->len += copy;
			rx->buf[rx->len] = '\0';
		}

		if (copy < (size_t)evt->data_len) {
			rx->truncated = true;
		}
	}

	return ESP_OK;
}

/**
 * @brief Execute an HTTP GET request.
 *
 * The function runs synchronously inside the HTTP owner task context.
 * If the caller provided an RX buffer, the response body is captured
 * via the http_event_handler().
 *
 * @param[in] req Request description (URL, optional RX buffer).
 * @return http_resp_t Response status + optional body capture info.
 */
static http_resp_t do_get(const http_req_t *req) {
	http_resp_t resp = {
		.request_id = req->request_id,
		.err = ESP_FAIL,
		.http_status = -1,
		.content_length = -1,
		.rx_len = 0,
		.truncated = false,
	};

	http_rx_ctx_t rx = {
		.buf = req->rx_buf,
		.cap = req->rx_cap,
		.len = 0,
		.truncated = false,
	};

	/* Ensure caller sees empty string on failures too. */
	if (rx.buf && rx.cap) {
		rx.buf[0] = '\0';
	}

	esp_http_client_config_t config = {
		.url = req->url,
		.event_handler = http_event_handler,
		.user_data = &rx,
		.timeout_ms = 8000,
		.crt_bundle_attach = esp_crt_bundle_attach, /* HTTPS support */
	};

	esp_http_client_handle_t client = esp_http_client_init(&config);
	if (!client) {
		ESP_LOGE(TAG, "esp_http_client_init failed");
		return resp;
	}

	resp.err = esp_http_client_perform(client);
	if (resp.err == ESP_OK) {
		resp.http_status = esp_http_client_get_status_code(client);
		resp.content_length = esp_http_client_get_content_length(client);

		resp.rx_len = rx.len;
		resp.truncated = rx.truncated;

		ESP_LOGI(TAG, "status=%d len=%d rx=%u trunc=%d", resp.http_status,
				 resp.content_length, (unsigned)resp.rx_len,
				 (int)resp.truncated);
	} else {
		ESP_LOGE(TAG, "request failed: %s", esp_err_to_name(resp.err));
	}

	esp_http_client_cleanup(client);
	return resp;
}

/**
 * @brief FreeRTOS worker task that performs HTTP requests on behalf of clients.
 *
 * Multiple instances of this task run concurrently, each pulling requests
 * from the shared queue independently.  Because do_get() creates its own
 * esp_http_client handle on the stack there is no shared mutable state
 * between workers, so concurrent execution is safe.
 *
 * Each worker:
 *  - Waits for IP connectivity (IP_READY_BIT) before processing requests
 *  - Receives http_req_t messages from the shared HTTP request queue
 *  - Executes the transaction and sends an http_resp_t to the caller's queue
 *
 * Notes:
 *  - Callers must keep rx_buf valid until the matching response is received.
 */
static void http_worker_task(void *arg) {
	/* Wait for network ready */
	EventGroupHandle_t ev = net_manager_events();
	xEventGroupWaitBits(ev, IP_READY_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

	http_req_t req;
	for (;;) {
		if (xQueueReceive(s_http_q, &req, portMAX_DELAY) == pdTRUE) {
			http_resp_t resp;

			switch (req.method) {
			case HTTP_REQ_GET:
			default:
				resp = do_get(&req);
				break;
			}

			if (req.reply_queue) {
				(void)xQueueSend(req.reply_queue, &resp, pdMS_TO_TICKS(50));
			}
		}
	}
}

/**
 * @brief Initialize the HTTP service.
 *
 * Creates the shared request queue (once) and starts HTTP_SERVICE_NUM_WORKERS
 * worker tasks.  All workers pull from the same queue, allowing multiple
 * requests to be in-flight concurrently.
 */
void http_service_start(void) {
	if (!s_http_q) {
		s_http_q = xQueueCreate(CONFIG_HTTP_SERVICE_NUM_WORKERS * 2,
								sizeof(http_req_t));
	}
	for (int i = 0; i < CONFIG_HTTP_SERVICE_NUM_WORKERS; i++) {
		xTaskCreatePinnedToCore(http_worker_task, "http_svc", 6144, NULL, 5,
								NULL, 0);
	}
}

/**
 * @brief Return the shared HTTP request queue.
 */
QueueHandle_t http_service_queue(void) { return s_http_q; }