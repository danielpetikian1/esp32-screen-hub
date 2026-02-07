#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"

#include "http_service.h"
#include "sdkconfig.h"
#include "weather_task.h"

static void weather_task(void *arg) {
	double lat = strtod(CONFIG_LOCATION_LATITUDE, NULL);
	double lon = strtod(CONFIG_LOCATION_LONGITUDE, NULL);

	QueueHandle_t http_q = http_service_queue();
	QueueHandle_t reply_q = xQueueCreate(2, sizeof(http_resp_t));

	TickType_t last = xTaskGetTickCount();
	const TickType_t period = pdMS_TO_TICKS(15 * 60 * 1000);

	uint32_t rid = 1;
	char url[256];

	for (;;) {
		snprintf(url, sizeof(url),
				 "http://api.open-meteo.com/v1/forecast"
				 "?latitude=%.4f&longitude=%.4f"
				 "&current_weather=true"
				 "&temperature_unit=fahrenheit"
				 "&windspeed_unit=mph",
				 lat, lon);

		http_req_t req = {0};
		req.method = HTTP_REQ_GET;
		req.reply_queue = reply_q;
		req.request_id = ++rid;
		snprintf(req.url, sizeof(req.url), "%s", url);
		req.url[sizeof(req.url) - 1] = '\0';

		printf("\nWEATHER: enqueue id=%" PRIu32 "\n", req.request_id);
		xQueueSend(http_q, &req, portMAX_DELAY);

		http_resp_t resp;
		if (xQueueReceive(reply_q, &resp, pdMS_TO_TICKS(15000)) == pdTRUE) {
			printf("WEATHER: done id=%" PRIu32 " err=%s http=%d\n",
				   resp.request_id, esp_err_to_name(resp.err),
				   resp.http_status);
		} else {
			printf("WEATHER: timeout waiting for response\n");
		}

		vTaskDelayUntil(&last, period);
	}
}

void weather_task_start(void) {
	xTaskCreatePinnedToCore(weather_task, "weather", 4096, NULL, 5, NULL, 1);
}
