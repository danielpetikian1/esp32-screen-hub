#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Snapshot of all latest sensor readings.
 *
 * This struct represents a coherent, point-in-time copy of the
 * most recent sensor values available in the system.
 *
 * It is safe for other modules (UI, HTTP, MQTT, etc.) to read
 * this struct after calling readings_get_snapshot().
 *
 * Each sensor group includes:
 *  - A validity flag (true once first successful reading arrives)
 *  - Latest decoded values in engineering units
 *  - Timestamp (ms since boot) of when the reading was stored
 *
 * Notes:
 *  - All fields are owned and updated by readings_store.c
 *  - External modules must treat this struct as read-only
 */
typedef struct {

	// -------------------------------------------------------------------------
	// SGP30 (Air Quality)
	// -------------------------------------------------------------------------

	bool sgp30_valid;	  ///< True after first successful IAQ measurement
	uint16_t eco2_ppm;	  ///< Equivalent CO2 concentration (ppm)
	uint16_t tvoc_ppb;	  ///< Total Volatile Organic Compounds (ppb)
	uint32_t sgp30_ts_ms; ///< Timestamp of last SGP30 update (ms since boot)

	// -------------------------------------------------------------------------
	// SHT40 (Temperature + Humidity)
	// -------------------------------------------------------------------------

	bool sht40_valid;	  ///< True after first successful reading
	float temp_c;		  ///< Temperature in Celsius
	float rh_percent;	  ///< Relative humidity (%)
	uint32_t sht40_ts_ms; ///< Timestamp of last SHT40 update (ms since boot)

} readings_snapshot_t;

/**
 * @brief Initialize the readings storage module.
 *
 * Must be called once at startup before any update or get calls.
 *
 * Responsibilities:
 *  - Create internal synchronization primitives (mutex)
 *  - Zero-initialize internal storage
 *
 * This function is typically called from app_main() or
 * from your system initialization sequence.
 */
void readings_store_init(void);

/**
 * @brief Update the latest SGP30 reading.
 *
 * Called by the SGP30 task after:
 *  - I2C transport success
 *  - CRC validation
 *  - Proper decoding into engineering units
 *
 * Thread-safe.
 *
 * @param eco2_ppm  Equivalent CO2 concentration (ppm)
 * @param tvoc_ppb  Total VOC concentration (ppb)
 * @param ts_ms     Timestamp (ms since boot)
 */
void readings_update_sgp30(uint16_t eco2_ppm, uint16_t tvoc_ppb,
						   uint32_t ts_ms);

/**
 * @brief Update the latest SHT40 reading.
 *
 * Called by the SHT40 task after:
 *  - I2C read success
 *  - CRC validation
 *  - Conversion to float units
 *
 * Thread-safe.
 *
 * @param temp_c     Temperature in Celsius
 * @param rh_percent Relative humidity in %
 * @param ts_ms      Timestamp (ms since boot)
 */
void readings_update_sht40(float temp_c, float rh_percent, uint32_t ts_ms);

/**
 * @brief Obtain a coherent snapshot of all sensor readings.
 *
 * This function:
 *  - Acquires internal mutex
 *  - Copies entire snapshot struct
 *  - Releases mutex
 *
 * The returned snapshot is safe to use without further locking.
 *
 * Thread-safe.
 *
 * @param out Pointer to struct that receives snapshot copy.
 */
void readings_get_snapshot(readings_snapshot_t *out);
