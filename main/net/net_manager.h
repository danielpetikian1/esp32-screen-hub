#pragma once
#include "freertos/FreeRTOS.h"      // MUST be first
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Start Wi-Fi in STA mode and manage reconnect/state */
void net_manager_start(void);

/* Event group handle to wait on network readiness bits */
EventGroupHandle_t net_manager_events(void);

#ifdef __cplusplus
}
#endif
