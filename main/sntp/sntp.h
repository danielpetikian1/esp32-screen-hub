// sntp.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file sntp.h
 * @brief Simple SNTP time sync service for ESP-IDF.
 *
 * This module:
 *  - Starts SNTP (NTP client) using ESP-IDF's esp_netif_sntp APIs.
 *  - Optionally sets a timezone (so localtime is correct).
 *  - Provides helpers to check whether time is set and to format time for
 * display.
 *
 * Typical usage:
 *  1) Wait until network is ready (Wi-Fi connected + got IP).
 *  2) Call sntp_service_init_and_start("PST8PDT,M3.2.0,M11.1.0");
 *  3) Call sntp_service_wait_for_sync(pdMS_TO_TICKS(15000));
 *  4) Periodically call sntp_service_format_local_time(...)
 */

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

/**
 * @brief Start SNTP and set timezone.
 *
 * Must be called after the network is ready (you can’t sync without internet).
 *
 * @param tz_posix  POSIX TZ string, e.g.:
 *                   - Pacific: "PST8PDT,M3.2.0,M11.1.0"
 *                   - UTC:     "UTC0"
 *                 If NULL, timezone will not be changed (defaults to UTC unless
 * set elsewhere).
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t sntp_service_init_and_start(const char *tz_posix);

/**
 * @brief Stop SNTP service if it is running.
 *
 * Safe to call multiple times.
 */
void sntp_service_stop(void);

/**
 * @brief Returns true if the system time looks "set" (i.e., not 1970).
 *
 * This does not guarantee the time is perfectly accurate, just that SNTP has
 * likely synced at least once.
 */
bool sntp_service_time_is_set(void);

/**
 * @brief Wait until the system time is set or a timeout occurs.
 *
 * @param timeout_ticks  How long to wait in FreeRTOS ticks.
 *
 * @return ESP_OK if time was set before timeout, ESP_ERR_TIMEOUT otherwise.
 */
esp_err_t sntp_service_wait_for_sync(TickType_t timeout_ticks);

/**
 * @brief Get current local time into a struct tm.
 *
 * @param out  Output timeinfo (must not be NULL).
 *
 * @return ESP_OK if time was set and out is filled, otherwise an error.
 */
esp_err_t sntp_service_get_local_timeinfo(struct tm *out);

/**
 * @brief Format local time as a string for UI display (LVGL label, logs, etc.).
 *
 * Uses strftime() with your provided format string.
 * Common formats:
 *  - "%H:%M:%S"         -> 23:59:59
 *  - "%a %b %d %H:%M"   -> Sat Feb 21 14:30
 *
 * @param fmt   strftime() format string (must not be NULL).
 * @param out   Output buffer (must not be NULL).
 * @param out_len Output buffer length.
 *
 * @return ESP_OK on success, otherwise an error.
 */
esp_err_t sntp_service_format_local_time(const char *fmt, char *out,
										 size_t out_len);

#ifdef __cplusplus
}
#endif