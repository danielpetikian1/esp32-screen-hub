# ESP32 Utility Device

A modular ESP32-based utility device built with **ESP-IDF** and **LVGL**.  
The system aggregates environmental sensor data, weather information, and is designed to expand into additional features like stock tracking and smart home integration.

---

## 📌 Overview

This project runs on an ESP32 (currently targeting M5Stack S3 hardware) and provides:

- Environmental sensor readings over I2C
- Weather data via HTTP API
- On-device graphical UI using LVGL
- Modular, task-based service architecture

The system is structured with clear separation of concerns:

- Sensor drivers are isolated
- Shared readings are centrally managed
- Network operations are handled by a dedicated HTTP service
- UI updates are synchronized using LVGL display locking
- Inter-task communication is handled via FreeRTOS queues

---

## ✅ What Has Been Implemented

### I2C Sensor Modules
- Dedicated I2C service task that owns the bus
- SHT40 temperature & humidity integration
- SGP30 air quality integration (eCO2 / TVOC)
- CRC validation and proper timing handling
- Thread-safe updates to shared readings

### HTTP Service
- Dedicated HTTP service task
- Request/response queue architecture
- Periodic background refresh support
- Safe inter-task communication

### Weather Module
- Periodic polling of Open-Meteo API
- Configurable location via Kconfig
- JSON parsing and structured data storage
- Integration into shared readings snapshot

### UI (LVGL)
- LVGL integration with ESP-IDF
- Display initialization and brightness control
- Thread-safe display locking
- Modular UI components (sensor cards)
- Dynamic label updates
- Custom layout tuning
- Style adjustments (text color, font sizing, scroll control)

### Shared Readings API
- Central snapshot structure
- Mutex-protected updates
- Clean public getter interface
- Timestamp support

---

## 🚧 What Has Not Been Implemented Yet

### Rust Module Migration
Planned migration of selected modules from C to Rust for:
- Improved memory safety
- Better ownership semantics
- Stronger concurrency guarantees
- Safer hardware abstraction

This is currently in the design phase.

---

### Stock Tracker Integration
Planned features:
- Stock market API integration
- Configurable tracked symbols
- Periodic updates
- UI display module
- Potential MQTT or mobile configuration support

---

### UI Improvements
Planned enhancements:
- Improved typography and spacing
- Cleaner visual hierarchy
- Theme consistency
- Card layout refinement
- Screen transitions and animations
- Reduced visual graininess
- Dynamic unit switching (F/C)

---

## 🏗 Architecture Overview

The system follows a modular, task-based architecture:

- **I2C Service Task**
  - Owns the I2C bus
  - Processes queued sensor requests

- **Sensor Tasks**
  - Periodically request readings
  - Update shared snapshot

- **HTTP Service Task**
  - Handles outbound network requests
  - Implements request/reply pattern via queues

- **Weather Task**
  - Periodic API polling
  - Updates shared readings

- **UI Loop**
  - Reads shared snapshot
  - Updates LVGL components
  - Uses display locking to ensure thread safety

---

## 🎯 Design Goals

- Clean module boundaries
- No hidden global state
- Deterministic sensor timing
- Safe inter-task communication
- Scalable service-based architecture
- Expandable feature set

---

## 🚀 Future Vision

This device is intended to evolve into:

- A customizable smart desktop dashboard
- A smart home data endpoint
- A configurable personal information display
- A hybrid Rust + C embedded platform

---

## 📍 Current Status

The system is stable and functional for:

- Environmental sensor readings
- Weather updates
- Basic UI display

Core architecture is complete.  
Next phase focuses on feature expansion and UI refinement.
