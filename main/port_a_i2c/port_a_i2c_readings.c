#include "port_a_i2c_readings.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Internal mutex protecting shared state
static SemaphoreHandle_t s_mu = NULL;

// Internal storage of latest readings
static readings_snapshot_t s_latest;

/**
 * @brief Initialize readings storage module.
 */
void readings_store_init(void) {
	// Create mutex used to protect shared snapshot
	s_mu = xSemaphoreCreateMutex();

	// Optional: could assert here if allocation fails
	// configASSERT(s_mu);

	// Ensure storage starts zeroed (valid flags false)
	// Static globals are zeroed automatically (.bss),
	// but this keeps intent explicit.
	memset(&s_latest, 0, sizeof(s_latest));
}

/**
 * @brief Update SGP30 fields in shared snapshot.
 *
 * Critical section is kept intentionally short:
 *  - Only simple assignments
 *  - No logging
 *  - No dynamic allocation
 */
void readings_update_sgp30(uint16_t eco2_ppm, uint16_t tvoc_ppb,
						   uint32_t ts_ms) {
	if (!s_mu) {
		return; // module not initialized
	}

	xSemaphoreTake(s_mu, portMAX_DELAY);

	s_latest.sgp30_valid = true;
	s_latest.eco2_ppm = eco2_ppm;
	s_latest.tvoc_ppb = tvoc_ppb;
	s_latest.sgp30_ts_ms = ts_ms;

	xSemaphoreGive(s_mu);
}

/**
 * @brief Update SHT40 fields in shared snapshot.
 */
void readings_update_sht40(float temp_c, float rh_percent, uint32_t ts_ms) {
	if (!s_mu) {
		return;
	}

	xSemaphoreTake(s_mu, portMAX_DELAY);

	s_latest.sht40_valid = true;
	s_latest.temp_c = temp_c;
	s_latest.rh_percent = rh_percent;
	s_latest.sht40_ts_ms = ts_ms;

	xSemaphoreGive(s_mu);
}

/**
 * @brief Copy latest readings into caller-provided struct.
 *
 * The entire struct is copied while holding the mutex
 * to guarantee a consistent snapshot.
 */
void readings_get_snapshot(readings_snapshot_t *out) {
	if (!s_mu || !out) {
		return;
	}

	xSemaphoreTake(s_mu, portMAX_DELAY);

	*out = s_latest; // struct copy

	xSemaphoreGive(s_mu);
}
