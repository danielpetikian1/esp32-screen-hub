#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#include "http_service.h"
#include "stocks_task.h"

static const char *TAG = "stocks";

/**
 * @brief Compile-time list of configured stock symbols.
 *
 * Empty strings (symbols left blank in menuconfig) are skipped at runtime.
 */
static const char *s_configured_symbols[STOCKS_MAX_SYMBOLS] = {
	CONFIG_FINNHUB_SYMBOL_1, CONFIG_FINNHUB_SYMBOL_2, CONFIG_FINNHUB_SYMBOL_3,
	CONFIG_FINNHUB_SYMBOL_4, CONFIG_FINNHUB_SYMBOL_5, CONFIG_FINNHUB_SYMBOL_6,
};

/**
 * @brief Shared stocks snapshot consumed by the UI.
 *
 * The stocks task updates this after each full polling cycle.
 * The UI reads via stocks_get_snapshot() (mutex-protected copy).
 */
static stocks_snapshot_t s_stocks;
static SemaphoreHandle_t s_stocks_mu;

bool stocks_get_snapshot(stocks_snapshot_t *out) {
	if (!out || !s_stocks_mu) {
		return false;
	}

	if (xSemaphoreTake(s_stocks_mu, pdMS_TO_TICKS(20)) != pdTRUE) {
		return false;
	}
	*out = s_stocks;
	xSemaphoreGive(s_stocks_mu);

	/* Report valid if at least one quote has been fetched. */
	for (int i = 0; i < out->count; i++) {
		if (out->quotes[i].valid) {
			return true;
		}
	}
	return false;
}

/**
 * @brief Extract a float value from a flat JSON object string.
 *
 * Searches for "key": followed by a numeric value. Handles integer and
 * floating-point values. Does NOT support nested objects or arrays.
 *
 * @param json NUL-terminated JSON string.
 * @param key  Field name to locate (without quotes).
 * @param val  Output float value on success.
 * @return true if found and parsed successfully.
 */
static bool json_get_float(const char *json, const char *key, float *val) {
	/* Build search pattern: "key": */
	char pattern[32];
	snprintf(pattern, sizeof(pattern), "\"%s\":", key);

	const char *p = strstr(json, pattern);
	if (!p) {
		return false;
	}

	p += strlen(pattern);
	/* Skip optional whitespace after the colon. */
	while (*p == ' ' || *p == '\t') {
		p++;
	}

	char *end;
	float f = strtof(p, &end);
	if (end == p) {
		return false; /* No numeric conversion */
	}

	*val = f;
	return true;
}

/**
 * @brief Parse a Finnhub /quote JSON response into a stock_quote_t.
 *
 * Expected fields:
 *   "c"  – current price
 *   "d"  – absolute change from previous close
 *   "dp" – percent change from previous close
 *
 * @param json   NUL-terminated JSON response body.
 * @param symbol Ticker symbol string (copied into the result).
 * @param out    Output quote on success (valid=true).
 * @return true on successful parse of all required fields.
 */
static bool parse_finnhub_quote(const char *json, const char *symbol,
								stock_quote_t *out) {
	if (!json || !symbol || !out) {
		return false;
	}

	float c = 0.0f, d = 0.0f, dp = 0.0f;
	if (!json_get_float(json, "c", &c) || !json_get_float(json, "d", &d) ||
		!json_get_float(json, "dp", &dp)) {
		return false;
	}

	/* A price of 0.0 usually means the market is closed or symbol invalid. */
	if (c == 0.0f) {
		return false;
	}

	snprintf(out->symbol, sizeof(out->symbol), "%s", symbol);
	out->price = c;
	out->change = d;
	out->change_pct = dp;
	out->valid = true;
	return true;
}

/**
 * @brief FreeRTOS task that periodically fetches stock quotes from Finnhub.
 *
 * For each non-empty configured symbol the task:
 *  1. Builds the Finnhub /quote URL with the API key
 *  2. Submits the request to the shared HTTP service queue
 *  3. Waits for the response (up to 15 s)
 *  4. Parses the JSON body with json_get_float()
 *  5. Updates the shared snapshot under the mutex
 *
 * After all symbols have been fetched, the task sleeps for the configured
 * poll interval before starting the next cycle.
 *
 * Notes:
 *  - Requests are submitted one at a time (serialized by the HTTP owner task).
 *  - The UI always reads a consistent copy via stocks_get_snapshot().
 */
static void stocks_task(void *arg) {
	/* Collect valid (non-empty) symbols at startup. */
	const char *symbols[STOCKS_MAX_SYMBOLS];
	int count = 0;
	for (int i = 0; i < STOCKS_MAX_SYMBOLS; i++) {
		if (s_configured_symbols[i] && s_configured_symbols[i][0] != '\0') {
			symbols[count++] = s_configured_symbols[i];
		}
	}

	if (count == 0) {
		ESP_LOGW(TAG, "No symbols configured; task exiting");
		vTaskDelete(NULL);
		return;
	}

	/* Initialise the snapshot with the known symbol list. */
	if (xSemaphoreTake(s_stocks_mu, portMAX_DELAY) == pdTRUE) {
		s_stocks.count = count;
		for (int i = 0; i < count; i++) {
			snprintf(s_stocks.quotes[i].symbol,
					 sizeof(s_stocks.quotes[i].symbol), "%s", symbols[i]);
			s_stocks.quotes[i].valid = false;
		}
		xSemaphoreGive(s_stocks_mu);
	}

	QueueHandle_t http_q = http_service_queue();
	QueueHandle_t reply_q = xQueueCreate(2, sizeof(http_resp_t));
	static char rx[512]; /* Finnhub quote response is ~100 bytes */

	const TickType_t period =
		pdMS_TO_TICKS((uint32_t)CONFIG_FINNHUB_POLL_INTERVAL_SEC * 1000U);
	TickType_t last = xTaskGetTickCount();
	uint32_t rid = 0;

	for (;;) {
		for (int i = 0; i < count; i++) {
			char url[256];
			snprintf(url, sizeof(url),
					 "https://finnhub.io/api/v1/quote"
					 "?symbol=%s&token=%s",
					 symbols[i], CONFIG_FINNHUB_API_KEY);

			http_req_t req = {0};
			req.method = HTTP_REQ_GET;
			req.reply_queue = reply_q;
			req.request_id = ++rid;
			snprintf(req.url, sizeof(req.url), "%s", url);
			req.rx_buf = rx;
			req.rx_cap = sizeof(rx);

			rx[0] = '\0';
			ESP_LOGI(TAG, "fetch %s id=%" PRIu32, symbols[i], req.request_id);
			xQueueSend(http_q, &req, portMAX_DELAY);

			http_resp_t resp;
			if (xQueueReceive(reply_q, &resp, pdMS_TO_TICKS(15000)) != pdTRUE) {
				ESP_LOGW(TAG, "%s: timeout", symbols[i]);
				continue;
			}

			ESP_LOGI(TAG, "%s id=%" PRIu32 " err=%s http=%d rx=%u trunc=%d",
					 symbols[i], resp.request_id, esp_err_to_name(resp.err),
					 resp.http_status, (unsigned)resp.rx_len,
					 (int)resp.truncated);

			if (resp.err != ESP_OK || resp.http_status != 200 ||
				resp.rx_len == 0) {
				continue;
			}

			stock_quote_t q = {0};
			if (parse_finnhub_quote(rx, symbols[i], &q)) {
				if (xSemaphoreTake(s_stocks_mu, pdMS_TO_TICKS(50)) == pdTRUE) {
					s_stocks.quotes[i] = q;
					xSemaphoreGive(s_stocks_mu);
				}
				ESP_LOGI(TAG, "%s $%.2f d=%.2f dp=%.2f%%", q.symbol, q.price,
						 q.change, q.change_pct);
			} else {
				ESP_LOGW(TAG, "%s: parse failed: %s", symbols[i], rx);
			}
		}

		vTaskDelayUntil(&last, period);
	}
}

/**
 * @brief Start the stocks polling task and initialise shared state.
 */
void stocks_task_start(void) {
	if (!s_stocks_mu) {
		s_stocks_mu = xSemaphoreCreateMutex();
	}

	memset(&s_stocks, 0, sizeof(s_stocks));

	xTaskCreatePinnedToCore(stocks_task, "stocks", 4096, NULL, 5, NULL, 1);
}
