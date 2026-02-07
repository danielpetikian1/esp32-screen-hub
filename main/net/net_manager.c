#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "common/app_events.h"
#include "net_manager.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

#define TAG "net_manager"

static EventGroupHandle_t s_net_events;
static esp_netif_t *s_sta_netif;
static bool s_wifi_inited;
static bool s_handlers_registered;

static void wifi_ip_event_handler(void *arg, esp_event_base_t event_base,
								  int32_t event_id, void *event_data) {
	(void)arg;
	(void)event_data;

	if (!s_net_events)
		return;

	if (event_base == WIFI_EVENT) {
		if (event_id == WIFI_EVENT_STA_CONNECTED) {
			xEventGroupSetBits(s_net_events, WIFI_CONNECTED_BIT);
		} else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
			xEventGroupClearBits(s_net_events,
								 WIFI_CONNECTED_BIT | IP_READY_BIT);
			// simple reconnect; ignore error if not ready yet
			(void)esp_wifi_connect();
		}
	} else if (event_base == IP_EVENT) {
		if (event_id == IP_EVENT_STA_GOT_IP) {
			xEventGroupSetBits(s_net_events, IP_READY_BIT);
		} else if (event_id == IP_EVENT_STA_LOST_IP) {
			xEventGroupClearBits(s_net_events, IP_READY_BIT);
		}
	}
}

static void wifi_start_sta(void) {
	// Guard empty SSID so you don't abort with ESP_ERR_WIFI_SSID
	if (CONFIG_WIFI_SSID[0] == '\0') {
		ESP_LOGW(TAG, "CONFIG_WIFI_SSID is empty. Skipping Wi-Fi start.");
		return;
	}

	esp_err_t err;

	// Global init (safe if already done elsewhere)
	err = esp_netif_init();
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
		ESP_ERROR_CHECK(err);

	err = esp_event_loop_create_default();
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
		ESP_ERROR_CHECK(err);

	// Create default STA netif only once
	if (s_sta_netif == NULL) {
		s_sta_netif = esp_netif_create_default_wifi_sta();
		ESP_ERROR_CHECK(s_sta_netif ? ESP_OK : ESP_FAIL);
	}

	// Wi-Fi driver init only once
	if (!s_wifi_inited) {
		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		ESP_ERROR_CHECK(esp_wifi_init(&cfg));
		ESP_ERROR_CHECK(
			esp_wifi_set_storage(WIFI_STORAGE_FLASH)); // optional but typical
		s_wifi_inited = true;
	}

	// Register handlers only once
	if (!s_handlers_registered) {
		ESP_ERROR_CHECK(esp_event_handler_register(
			WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_ip_event_handler, NULL));
		ESP_ERROR_CHECK(esp_event_handler_register(
			IP_EVENT, ESP_EVENT_ANY_ID, &wifi_ip_event_handler, NULL));
		s_handlers_registered = true;
	}

	// Configure credentials
	wifi_config_t wifi_config = {0};

	strncpy((char *)wifi_config.sta.ssid, CONFIG_WIFI_SSID,
			sizeof(wifi_config.sta.ssid));
	wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';

	strncpy((char *)wifi_config.sta.password, CONFIG_WIFI_PASSWORD,
			sizeof(wifi_config.sta.password));
	wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';

	// Optional: reduce “can’t connect due to weak auth mode” surprises
	wifi_config.sta.threshold.authmode =
		WIFI_AUTH_WPA2_PSK; // change if you use open/WPA3
	wifi_config.sta.pmf_cfg.capable = true;
	wifi_config.sta.pmf_cfg.required = false;

	ESP_LOGI(TAG, "Starting Wi-Fi STA: SSID='%s'",
			 (const char *)wifi_config.sta.ssid);

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

	// Start/connect (be tolerant to already-started states)
	err = esp_wifi_start();
	if (err != ESP_OK && err != ESP_ERR_WIFI_CONN &&
		err != ESP_ERR_INVALID_STATE) {
		ESP_ERROR_CHECK(err);
	}

	err = esp_wifi_connect();
	if (err != ESP_OK && err != ESP_ERR_WIFI_CONN &&
		err != ESP_ERR_INVALID_STATE) {
		ESP_ERROR_CHECK(err);
	}
}

void net_manager_start(void) {
	if (!s_net_events) {
		s_net_events = xEventGroupCreate();
		ESP_ERROR_CHECK(s_net_events ? ESP_OK : ESP_FAIL);
	}
	wifi_start_sta();
}

EventGroupHandle_t net_manager_events(void) { return s_net_events; }
