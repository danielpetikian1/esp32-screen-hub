/**
 * @file ui_clock.c
 * @brief Clock screen: full-screen large time display with date and CPU usage.
 *
 * Dark navy background (#0D111F) matching the weather screen palette.
 * Layout (320×240, centred column):
 *   – Large HH:MM:SS label   (montserrat_40, white)
 *   – Date label              (montserrat_16, muted blue #6A8FAF)
 *   – Microchip icon + CPU%  (fa_microchip_16 + montserrat_16, muted blue)
 */

#include "bsp/m5stack_core_s3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_screens.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

LV_FONT_DECLARE(fa_microchip_16);

/* FontAwesome microchip U+F2DB as UTF-8 */
#define FA_MICROCHIP "\xEF\x8B\x9B"

/* Colour palette shared with the weather screen */
#define CLR_BG 0x0D111F
#define CLR_MUTED 0x6A8FAF
#define CLR_ACCENT 0x4A90D9

static lv_obj_t *s_lbl_clock_big;
static lv_obj_t *s_lbl_date;
static lv_obj_t *s_lbl_cpu_icon;
static lv_obj_t *s_lbl_cpu;

/* ---------- CPU usage ---------- */

static TaskStatus_t s_tasks[30];
static uint32_t s_prev_idle = 0;
static uint32_t s_prev_all = 0;

/**
 * @brief Compute CPU usage over the last sample interval.
 *
 * Sums per-task run-time counters for both idle tasks and all tasks.
 * Using task-level counters (not the wall-clock total) keeps the ratio
 * correct on dual-core ESP32 where idle can exceed wall time.
 */
static float get_total_cpu_usage(void) {
	UBaseType_t num_tasks = uxTaskGetSystemState(
		s_tasks, sizeof(s_tasks) / sizeof(s_tasks[0]), NULL);
	if (num_tasks == 0)
		return 0.0f;

	uint32_t idle_runtime = 0;
	uint32_t all_runtime = 0;
	for (UBaseType_t i = 0; i < num_tasks; i++) {
		all_runtime += s_tasks[i].ulRunTimeCounter;
		if (strncmp(s_tasks[i].pcTaskName, "IDLE", 4) == 0)
			idle_runtime += s_tasks[i].ulRunTimeCounter;
	}

	uint32_t delta_all = all_runtime - s_prev_all;
	uint32_t delta_idle = idle_runtime - s_prev_idle;
	s_prev_all = all_runtime;
	s_prev_idle = idle_runtime;

	if (delta_all == 0)
		return 0.0f;
	float usage = (1.0f - (float)delta_idle / delta_all) * 100.0f;
	if (usage < 0.0f)
		usage = 0.0f;
	if (usage > 100.0f)
		usage = 100.0f;
	return usage;
}

/* ---------- Build / update ---------- */

/**
 * @brief Populate the clock tile with time, date, and CPU widgets.
 *
 * @param tile Tileview tile provided by ui_init().
 */
void ui_clock_build(lv_obj_t *tile) {
	/* Dark background matching the weather screen */
	lv_obj_set_style_bg_color(tile, lv_color_hex(CLR_BG), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_PART_MAIN);

	/* Large HH:MM:SS — centred, shifted up to make room for date + CPU */
	s_lbl_clock_big = lv_label_create(tile);
	lv_label_set_text(s_lbl_clock_big, "--:--:--");
	lv_obj_set_style_text_font(s_lbl_clock_big, &lv_font_montserrat_40,
							   LV_PART_MAIN);
	lv_obj_set_style_text_color(s_lbl_clock_big, lv_color_white(),
								LV_PART_MAIN);
	lv_obj_align(s_lbl_clock_big, LV_ALIGN_CENTER, 0, -30);

	/* Date line — muted blue, montserrat_16 */
	s_lbl_date = lv_label_create(tile);
	lv_label_set_text(s_lbl_date, "--- --- --");
	lv_obj_set_style_text_font(s_lbl_date, &lv_font_montserrat_16,
							   LV_PART_MAIN);
	lv_obj_set_style_text_color(s_lbl_date, lv_color_hex(CLR_MUTED),
								LV_PART_MAIN);
	lv_obj_align(s_lbl_date, LV_ALIGN_CENTER, 0, 10);

	/* CPU % text — fixed width + left-aligned so the icon anchor never drifts
	 */
	s_lbl_cpu = lv_label_create(tile);
	lv_label_set_text(s_lbl_cpu, "CPU: --%");
	lv_obj_set_style_text_font(s_lbl_cpu, &lv_font_montserrat_16, LV_PART_MAIN);
	lv_obj_set_style_text_color(s_lbl_cpu, lv_color_hex(CLR_MUTED),
								LV_PART_MAIN);
	lv_obj_set_style_text_align(s_lbl_cpu, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
	lv_obj_set_width(s_lbl_cpu, 90);
	lv_obj_align(s_lbl_cpu, LV_ALIGN_CENTER, 12, 46);

	/* Microchip icon — accent blue, placed left of the CPU label */
	s_lbl_cpu_icon = lv_label_create(tile);
	lv_label_set_text(s_lbl_cpu_icon, FA_MICROCHIP);
	lv_obj_set_style_text_font(s_lbl_cpu_icon, &fa_microchip_16, LV_PART_MAIN);
	lv_obj_set_style_text_color(s_lbl_cpu_icon, lv_color_hex(CLR_ACCENT),
								LV_PART_MAIN);
	lv_obj_align_to(s_lbl_cpu_icon, s_lbl_cpu, LV_ALIGN_OUT_LEFT_MID, -8, 0);
}

/** Update the large time label. Called from ui_set_time_str() under display
 * lock. */
void ui_clock_set_time(const char *hhmmss) {
	lv_label_set_text(s_lbl_clock_big, hhmmss);
}

/**
 * @brief Refresh CPU usage and date labels.
 *
 * Called every 150 ms by ui_task. Acquires the display lock internally.
 * The date is read from the system RTC (set by SNTP) so no extra parameter
 * is needed — it updates automatically once time is synchronised.
 */
void ui_clock_update_cpu(void) {
	float cpu = get_total_cpu_usage();

	/* Format date from system time: "Mon Mar 09" */
	char date_buf[20];
	struct tm timeinfo = {0};
	time_t now = time(NULL);
	localtime_r(&now, &timeinfo);
	strftime(date_buf, sizeof(date_buf), "%a %b %d", &timeinfo);

	char cpu_buf[16];
	snprintf(cpu_buf, sizeof(cpu_buf), "CPU: %.1f%%", cpu);

	bsp_display_lock(0);
	lv_label_set_text(s_lbl_date, date_buf);
	lv_label_set_text(s_lbl_cpu, cpu_buf);
	bsp_display_unlock();
}
