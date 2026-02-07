#include "http_service.h"
#include "common/app_events.h"
#include "freertos/FreeRTOS.h"
#include "net_manager.h"
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_http_client.h"

static QueueHandle_t s_http_q;

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
	if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data_len > 0) {
		printf("%.*s", evt->data_len, (char *)evt->data);
	}
	return ESP_OK;
}

static http_resp_t do_get(const http_req_t *req) {
	http_resp_t resp = {
		.request_id = req->request_id,
		.err = ESP_FAIL,
		.http_status = -1,
		.content_length = -1,
	};

	esp_http_client_config_t config = {
		.url = req->url,
		.event_handler = http_event_handler,
		.timeout_ms = 8000,
	};

	esp_http_client_handle_t client = esp_http_client_init(&config);
	if (!client)
		return resp;

	resp.err = esp_http_client_perform(client);
	if (resp.err == ESP_OK) {
		resp.http_status = esp_http_client_get_status_code(client);
		resp.content_length = esp_http_client_get_content_length(client);
		printf("\nHTTP: status=%d len=%d\n", resp.http_status,
			   resp.content_length);
	} else {
		printf("\nHTTP: failed: %s\n", esp_err_to_name(resp.err));
	}

	esp_http_client_cleanup(client);
	return resp;
}

static void http_owner_task(void *arg) {
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

void http_service_start(void) {
	if (!s_http_q) {
		s_http_q = xQueueCreate(8, sizeof(http_req_t));
	}
	xTaskCreatePinnedToCore(http_owner_task, "http_service", 6144, NULL, 5,
							NULL, 1);
}

QueueHandle_t http_service_queue(void) { return s_http_q; }
