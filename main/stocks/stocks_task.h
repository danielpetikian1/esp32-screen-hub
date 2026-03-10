#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STOCKS_MAX_SYMBOLS 6

/**
 * @brief Quote data for a single stock symbol.
 *
 * Fields match the Finnhub /quote response:
 *  - symbol:     ticker (e.g. "AAPL")
 *  - price:      current price (c)
 *  - change:     absolute change from previous close (d)
 *  - change_pct: percent change from previous close (dp)
 *  - valid:      true once a successful fetch+parse has completed
 */
typedef struct {
	char symbol[12];
	float price;
	float change;
	float change_pct;
	bool valid;
} stock_quote_t;

/**
 * @brief Snapshot of all configured stock quotes.
 *
 * count reflects the number of non-empty symbols from Kconfig.
 * Entries beyond count are zeroed and not displayed.
 */
typedef struct {
	stock_quote_t quotes[STOCKS_MAX_SYMBOLS];
	int count;
} stocks_snapshot_t;

/**
 * @brief Start the Finnhub stock polling task.
 *
 * Reads configured symbols from Kconfig and begins periodic fetching.
 * Must be called after http_service_start().
 */
void stocks_task_start(void);

/**
 * @brief Copy out the latest stocks snapshot for the UI.
 *
 * @param[out] out Destination struct to receive the snapshot.
 * @return true if at least one quote is valid, false otherwise.
 */
bool stocks_get_snapshot(stocks_snapshot_t *out);

#ifdef __cplusplus
}
#endif
