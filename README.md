# ESP32 Utility Device

A modular ESP32-based utility dashboard built with **ESP-IDF** and **LVGL**, running on the **M5Stack Core S3**. Aggregates environmental sensor data, weather, and live stock quotes into a horizontally swipeable touchscreen UI.

---

## Screens

Swipe left/right to navigate between tiles:

| Tile | Screen | Description |
|------|--------|-------------|
| 0 | **Clock** | Large time display with CPU usage gauge and date |
| 1 | **Stats** | Indoor sensor dashboard — temperature, humidity, CO₂, TVOC |
| 2 | **Weather** | Outdoor conditions — temperature, humidity, wind, precipitation |
| 3 | **Stocks** | Live stock quotes — price, dollar change, percent change |

---

## Hardware

- **M5Stack Core S3** (ESP32-S3 dual-core 240 MHz, 512 KB SRAM, 8 MB PSRAM, 320×240 touchscreen)
- External I2C sensors via Grove port (Port A/B/C, configurable in menuconfig):
  - **SHT40** — temperature & humidity
  - **SGP30** — CO₂ & TVOC air quality

---

## Configuration

The fastest way to get started is to copy `sdkconfig.defaults`, fill in your credentials, then build. All required and optional settings are documented there with inline comments.

```bash
# Edit sdkconfig.defaults with your WiFi, API keys, location, and tickers
idf.py set-target esp32s3
idf.py build flash monitor
```

Or use `idf.py menuconfig` to configure interactively.

### Required settings

| Key | Description |
|-----|-------------|
| `WIFI_SSID` / `WIFI_PASSWORD` | Your WiFi network |
| `LOCATION_LATITUDE` / `LOCATION_LONGITUDE` | Decimal degrees for weather |
| `TIMEZONE` | POSIX timezone string, e.g. `PST8PDT,M3.2.0,M11.1.0` |
| `FINNHUB_API_KEY` | Free key at [finnhub.io](https://finnhub.io) |
| `FINNHUB_SYMBOL_1..6` | Tickers to display (e.g. `AAPL`, `SPY`); leave blank to disable |
| `FINNHUB_POLL_INTERVAL_SEC` | Poll interval in seconds (default 60, min 15) |

### External I2C Bus

Choose which Grove port your sensors are wired to under **External I2C Bus** in menuconfig (Port A, B, or C).

---

## Build & Flash

```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

Requires [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/) v5.x.

---

## Architecture

### Dual-core task layout

The system is explicitly partitioned across the two ESP32-S3 cores to prevent network and sensor work from causing UI jitter:

| Core | Tasks | Notes |
|------|-------|-------|
| **Core 1** | LVGL renderer (pri 4) | Pinned by M5Stack BSP — uncontested |
| **Core 0** | HTTP workers ×4 (pri 5), weather (pri 5), stocks (pri 5), sht40 (pri 5), sgp30 (pri 5), port_i2c (pri 6), ui_update (pri 5) | All background work |

TLS handshakes are the heaviest CPU work in the system. Running them on core 0 means they can never preempt the LVGL renderer on core 1, eliminating the main source of frame drops.

### HTTP service — concurrent worker pool

A shared FreeRTOS queue accepts `http_req_t` messages from any task. A pool of 4 worker tasks pulls from the queue concurrently. Because each worker creates its own `esp_http_client` handle on the stack, workers share no mutable state and are fully thread-safe.

```
[stocks_task]  ──┐
[weather_task] ──┼──▶  http_q  ──▶  [http_svc_0]  ──▶  Finnhub / Open-Meteo
                 │              ──▶  [http_svc_1]  ──▶  (concurrent)
                 │              ──▶  [http_svc_2]
                 └──▶           ──▶  [http_svc_3]
```

mbedTLS context memory is allocated from PSRAM (`CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y`) so concurrent TLS handshakes don't compete with the internal SRAM used by WiFi, LVGL, and task stacks. `CONFIG_MBEDTLS_DYNAMIC_BUFFER=y` is also enabled so the 16 KB RX buffer is held only during active data transfer.

### Stocks task — batch parallel fetch

Rather than fetching tickers one at a time (total latency = N × per-request latency), the stocks task submits all configured requests to the HTTP queue simultaneously, then collects all responses:

```
Phase 1 — submit all at once:  [DIA req] [SPY req] [QQQ req] ...  →  http_q
Phase 2 — collect all:         responses arrive in any order, matched back by request_id
```

Each ticker has its own 512-byte RX buffer (`rx[STOCKS_MAX_SYMBOLS][512]`) so HTTP workers fill them in parallel without any coordination. Responses are matched to symbols using a `batch_start` request ID offset recorded before submission.

With 4 workers and up to 6 tickers, all requests are enqueued at once and workers pick them up immediately — total cycle time is `max(per-request latency)` rather than `sum(per-request latency)`.

### Snapshot pattern

Each data-producing task exposes a `*_get_snapshot()` function that returns a mutex-protected copy of its latest state. The UI task holds no pointers into task memory — it works only on local copies, so there are no races between producers and the renderer.

```
[http worker] → parse JSON → mutex lock → update snapshot → mutex unlock
                                                                   ↓
                                          [ui_update] → mutex lock → copy snapshot → update LVGL labels
```

### I2C owner pattern

External sensors share a single I2C bus managed by a dedicated owner task (`port_i2c_service`). Sensor driver tasks submit requests via queue and block on a private reply queue — the same producer-consumer pattern as HTTP. This serialises bus access without any manual locking in sensor code.

---

## Project Structure

```
main/
├── app_main.c              # System init and task startup sequence
├── Kconfig.projbuild       # User-facing configuration (Wi-Fi, location, stocks, etc.)
├── http/                   # Concurrent HTTP worker pool (HTTPS via mbedTLS)
├── net/                    # Wi-Fi manager + IP event bits
├── sntp/                   # SNTP time sync service
├── weather/                # Open-Meteo polling task + snapshot
├── stocks/                 # Finnhub stock quote polling task + snapshot
├── port_i2c/               # I2C owner task + sensor data store
├── sht40/                  # SHT40 temperature & humidity driver task
├── sgp30/                  # SGP30 CO₂ & TVOC driver task
├── common/                 # App event bits, shared types
└── ui/
    ├── ui.c                # Tileview init
    ├── ui_clock.c          # Tile 0: clock + CPU usage
    ├── ui_stats.c          # Tile 1: indoor sensor cards
    ├── ui_weather.c        # Tile 2: outdoor weather
    ├── ui_stocks.c         # Tile 3: stock quotes
    └── ui_task.c           # 500ms refresh loop, pinned to core 0 pri 8
```

---

## Design Goals

- **Core isolation** — LVGL renderer on core 1, all I/O and sensor work on core 0; no contention between rendering and network
- **Concurrent HTTP** — 4-worker pool with shared queue enables parallel in-flight TLS connections
- **PSRAM-backed TLS** — mbedTLS allocates from PSRAM, keeping internal SRAM free for WiFi and the kernel
- **Batch fetching** — all stock requests submitted simultaneously; responses collected in any order by request ID
- **Snapshot pattern** — producers and the UI communicate through mutex-protected value copies, not shared pointers
- **Owner task pattern** — HTTP and I2C each serialised through a single owner task + queue; no manual locking in clients
- **Bounded memory** — per-request RX buffers stack-allocated by the requester; dynamic mbedTLS buffers freed after transfer
