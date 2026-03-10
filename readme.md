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

- **M5Stack Core S3** (ESP32-S3, 320×240 touchscreen)
- External I2C sensors via Grove port (Port A/B/C, configurable in menuconfig):
  - **SHT40** — temperature & humidity
  - **SGP30** — CO₂ & TVOC air quality

---

## Configuration

Run `idf.py menuconfig` and fill in:

**Wi-Fi**
- `WIFI_SSID` / `WIFI_PASSWORD`

**Location** (for weather)
- `LOCATION_LATITUDE` / `LOCATION_LONGITUDE` (decimal degrees)

**Time**
- `TIMEZONE` — POSIX timezone string, e.g. `PST8PDT,M3.2.0,M11.1.0`

**External I2C Bus**
- Choose the Grove port your sensors are on (Port A, B, or C)

**Finnhub Stock Ticker**
- `FINNHUB_API_KEY` — get a free key at [finnhub.io](https://finnhub.io)
- `FINNHUB_POLL_INTERVAL_SEC` — poll interval in seconds (default 60, min 15)
- `FINNHUB_SYMBOL_1` through `FINNHUB_SYMBOL_6` — tickers to display (e.g. `AAPL`, `SPY`, `QQQ`); leave blank to disable

---

## Build & Flash

```bash
idf.py set-target esp32s3
idf.py menuconfig      # fill in Wi-Fi, location, API keys
idf.py build flash monitor
```

Requires [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/) v5.x.

---

## Architecture

The system follows a modular, task-based architecture with clear separation of concerns. All inter-task communication uses FreeRTOS queues; shared data is mutex-protected.

```
app_main
├── net_manager          Wi-Fi connection (core 1)
├── sntp_service         Time sync
├── http_service         HTTP/HTTPS owner task — serialises all network requests (core 1)
├── weather_task         Open-Meteo polling every 3 min (core 1)
├── stocks_task          Finnhub quote polling on configurable interval (core 1)
├── port_i2c_service     I2C owner task — serialises bus access (core 1)
├── sht40_task           Temperature & humidity polling (core 1)
├── sgp30_task           CO₂ & TVOC polling (core 1)
└── ui_task              150ms LVGL refresh loop (core 0)
```

### Key patterns

- **Owner task pattern** — HTTP and I2C each have a single owner task. Clients submit requests via a queue and receive responses on a private reply queue. Prevents concurrent bus/network conflicts.
- **Snapshot pattern** — Each service exposes a `*_get_snapshot()` function that returns a mutex-protected copy of the latest data. The UI holds no pointers into task memory.
- **Fixed-size buffers** — All HTTP response buffers are stack-allocated by the requester. No heap allocation in steady state.
- **Core separation** — UI runs on core 0; Wi-Fi stack and all sensors/network tasks run on core 1.

---

## Project Structure

```
main/
├── app_main.c              # System init and task startup sequence
├── Kconfig.projbuild       # User-facing configuration (Wi-Fi, location, stocks, etc.)
├── http/                   # Shared HTTP owner task (HTTP + HTTPS via mbedTLS)
├── net/                    # Wi-Fi manager
├── sntp/                   # SNTP time sync service
├── weather/                # Open-Meteo polling task + snapshot
├── stocks/                 # Finnhub stock quote polling task + snapshot
├── port_i2c/               # I2C owner task, sensor data store
├── sht40/                  # SHT40 temperature & humidity driver task
├── sgp30/                  # SGP30 CO₂ & TVOC driver task
├── common/                 # App event bits, CRC utilities
└── ui/
    ├── ui.c                # Tileview init, temperature unit state
    ├── ui_clock.c          # Tile 0: clock + CPU gauge
    ├── ui_stats.c          # Tile 1: indoor sensor cards
    ├── ui_weather.c        # Tile 2: outdoor weather
    ├── ui_stocks.c         # Tile 3: stock quotes
    └── ui_task.c           # 150ms refresh loop, pinned to core 0
```

---

## Design Goals

- Clean module boundaries with no hidden global state
- Deterministic sensor timing via the I2C owner pattern
- Safe inter-task communication (queues + mutexes, no shared mutable globals)
- Fixed-memory-footprint steady state (no heap fragmentation)
- Scalable, service-based architecture for easy feature addition
