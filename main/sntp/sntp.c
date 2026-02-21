// sntp.c
#include "sntp.h"

#include <string.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"

static const char *TAG = "sntp_service";

/**
 * @brief Simple heuristic: if year >= 2023 then time is probably set.
 *
 * ESP32 starts at epoch (1970) on cold boot unless you have an RTC/battery.
 * After SNTP sync, the year becomes modern.
 */
static bool timeinfo_is_set(const struct tm *ti) {
	if (!ti)
		return false;
	return (ti->tm_year >= (2023 - 1900));
}

esp_err_t sntp_service_init_and_start(const char *tz_posix) {
	// Set timezone before calling localtime()/strftime() so UI shows local
	// time. This does not require SNTP, but you typically set it here.
	if (tz_posix && tz_posix[0] != '\0') {
		setenv("TZ", tz_posix, 1);
		tzset();
		ESP_LOGI(TAG, "Timezone set to: %s", tz_posix);
	} else {
		ESP_LOGI(TAG, "Timezone not changed (TZ not provided)");
	}

	// If SNTP was already initialized, stop it first to keep behavior
	// predictable. This is safe even if not running in many IDF versions, but
	// we keep it simple. NOTE: esp_netif_sntp_deinit() is the counterpart to
	// esp_netif_sntp_init().
	esp_netif_sntp_deinit();

	// Configure SNTP to use a default public pool. You can change this to your
	// org's NTP server. The ESP_NETIF_SNTP_DEFAULT_CONFIG macro sets reasonable
	// defaults.
	esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");

	// Optional: if you want faster initial sync, you can reduce this in some
	// designs. config.sync_interval = 3600 * 1000; // 1 hour in ms (example)
	//
	// Optional: if you want to be notified on sync:
	// config.sync_cb = your_callback;

	esp_err_t err = esp_netif_sntp_init(&config);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_netif_sntp_init failed: %s", esp_err_to_name(err));
		return err;
	}

	ESP_LOGI(TAG, "SNTP started (server=%s)", "pool.ntp.org");
	return ESP_OK;
}

void sntp_service_stop(void) {
	// Deinit stops SNTP and frees resources created by esp_netif_sntp_init().
	esp_netif_sntp_deinit();
	ESP_LOGI(TAG, "SNTP stopped");
}

bool sntp_service_time_is_set(void) {
	time_t now = 0;
	struct tm ti = {0};

	time(&now);
	localtime_r(&now, &ti);

	return timeinfo_is_set(&ti);
}

esp_err_t sntp_service_wait_for_sync(TickType_t timeout_ticks) {
	const TickType_t start = xTaskGetTickCount();
	const TickType_t poll = pdMS_TO_TICKS(250);

	while ((xTaskGetTickCount() - start) < timeout_ticks) {
		if (sntp_service_time_is_set()) {
			ESP_LOGI(TAG, "System time is set");
			return ESP_OK;
		}
		vTaskDelay(poll);
	}

	ESP_LOGW(TAG, "Timed out waiting for SNTP sync");
	return ESP_ERR_TIMEOUT;
}

esp_err_t sntp_service_get_local_timeinfo(struct tm *out) {
	if (!out)
		return ESP_ERR_INVALID_ARG;

	time_t now = 0;
	struct tm ti = {0};

	time(&now);
	localtime_r(&now, &ti);

	if (!timeinfo_is_set(&ti)) {
		// Caller can decide to show "--:--" or "Syncing..." in the UI.
		return ESP_ERR_INVALID_STATE;
	}

	*out = ti;
	return ESP_OK;
}

esp_err_t sntp_service_format_local_time(const char *fmt, char *out,
										 size_t out_len) {
	if (!fmt || !out || out_len == 0)
		return ESP_ERR_INVALID_ARG;

	struct tm ti = {0};
	esp_err_t err = sntp_service_get_local_timeinfo(&ti);
	if (err != ESP_OK) {
		// Provide a safe, readable placeholder for UI.
		// Caller can override if they want different behavior.
		snprintf(out, out_len, "--:--");
		return err;
	}

	// strftime returns 0 on failure (buffer too small or format issue).
	if (strftime(out, out_len, fmt, &ti) == 0) {
		// If formatting fails, return an error so caller can react.
		// Keep out as a useful placeholder.
		snprintf(out, out_len, "??:??");
		return ESP_ERR_INVALID_SIZE;
	}

	return ESP_OK;
}