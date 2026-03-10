/**
 * @file ui_stocks.c
 * @brief Stocks screen: header bar + up to 6 stock quote rows.
 *
 * Fixed layout on a 320×240 display:
 *
 *   Header bar (y=0, h=32):
 *     – Column headings "SYMBOL", "PRICE", "CHANGE" in muted blue
 *
 *   Quote rows (y=32, h=34 each, up to 6 rows):
 *     – Column 1 x=0..89    (90px):  symbol, white, montserrat_16
 *     – Column 2 x=90..199  (110px): price "$xxx.xx", white, montserrat_16
 *     – Column 3 x=200..319 (120px): two stacked right-aligned labels:
 *         top    "+$1.23"  – absolute dollar change, green/red
 *         bottom "+0.83%"  – percent change, same colour
 *
 * Background: deep navy (#0D111F), matching the weather screen.
 * When a quote is not yet fetched both change labels show "--".
 */

#include "bsp/m5stack_core_s3.h"
#include "stocks_task.h"
#include "ui_screens.h"
#include <stdio.h>

#define ROW_HEIGHT 34
#define HEADER_H 32
#define COL1_X 0
#define COL1_W 90
#define COL2_X 90
#define COL2_W 110
#define COL3_X 200
#define COL3_W 120
#define BG_COLOR 0x0D111F
#define TITLE_COLOR 0x6A8FAF
#define VALUE_COLOR 0xFFFFFF
#define POS_COLOR 0x4CAF50
#define NEG_COLOR 0xF44336

static struct {
	lv_obj_t *lbl_symbol[STOCKS_MAX_SYMBOLS];
	lv_obj_t *lbl_price[STOCKS_MAX_SYMBOLS];
	lv_obj_t *lbl_dollar[STOCKS_MAX_SYMBOLS]; /* absolute $ change, top line */
	lv_obj_t *lbl_pct[STOCKS_MAX_SYMBOLS]; /* percent change,   bottom line  */
} s_stocks;

/**
 * @brief Create a clipped label with common styling.
 */
static lv_obj_t *make_label(lv_obj_t *parent, int x, int y, int w,
							lv_color_t color, const lv_font_t *font,
							lv_text_align_t align) {
	lv_obj_t *lbl = lv_label_create(parent);
	lv_obj_set_pos(lbl, x, y);
	lv_obj_set_width(lbl, w);
	lv_obj_set_style_text_color(lbl, color, LV_PART_MAIN);
	lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
	lv_obj_set_style_text_align(lbl, align, LV_PART_MAIN);
	lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
	lv_label_set_text(lbl, "--");
	return lbl;
}

/**
 * @brief Populate the stocks tile with the header bar and quote rows.
 *
 * @param tile Tileview tile provided by ui_init().
 */
void ui_stocks_build(lv_obj_t *tile) {
	lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(tile, lv_color_hex(BG_COLOR), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_pad_all(tile, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(tile, 0, LV_PART_MAIN);

	/* ---- Header bar ---- */
	lv_obj_t *header = lv_obj_create(tile);
	lv_obj_set_pos(header, 0, 0);
	lv_obj_set_size(header, 320, HEADER_H);
	lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(header, lv_color_hex(0x111827), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);

	static const struct {
		int x, w;
		const char *text;
		lv_text_align_t align;
	} cols[] = {
		{COL1_X, COL1_W, "SYMBOL", LV_TEXT_ALIGN_LEFT},
		{COL2_X, COL2_W, "PRICE", LV_TEXT_ALIGN_LEFT},
		{COL3_X, COL3_W, "CHANGE", LV_TEXT_ALIGN_RIGHT},
	};
	for (int c = 0; c < 3; c++) {
		lv_obj_t *lbl = lv_label_create(header);
		lv_obj_set_pos(lbl, cols[c].x + 6, (HEADER_H - 16) / 2);
		lv_obj_set_width(lbl, cols[c].w - 12);
		lv_obj_set_style_text_color(lbl, lv_color_hex(TITLE_COLOR),
									LV_PART_MAIN);
		lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, LV_PART_MAIN);
		lv_obj_set_style_text_align(lbl, cols[c].align, LV_PART_MAIN);
		lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
		lv_label_set_text(lbl, cols[c].text);
	}

	/* ---- Quote rows ---- */
	for (int i = 0; i < STOCKS_MAX_SYMBOLS; i++) {
		int row_y = HEADER_H + i * ROW_HEIGHT;

		lv_obj_t *row = lv_obj_create(tile);
		lv_obj_set_pos(row, 0, row_y);
		lv_obj_set_size(row, 320, ROW_HEIGHT);
		lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_set_style_bg_color(
			row, (i % 2 == 0) ? lv_color_hex(0x0D111F) : lv_color_hex(0x131929),
			LV_PART_MAIN);
		lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
		lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
		lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);

		/* Symbol and price: vertically centred in the row */
		int mid_y = (ROW_HEIGHT - 16) / 2;

		s_stocks.lbl_symbol[i] = make_label(
			row, COL1_X + 6, mid_y, COL1_W - 12, lv_color_hex(VALUE_COLOR),
			&lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT);

		s_stocks.lbl_price[i] = make_label(
			row, COL2_X + 6, mid_y, COL2_W - 12, lv_color_hex(VALUE_COLOR),
			&lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT);

		/* Change column: two stacked labels (dollar top, percent bottom).
		 * Each is 16px tall; together they fill the 34px row with 1px padding.
		 */
		s_stocks.lbl_dollar[i] =
			make_label(row, COL3_X, 1, COL3_W - 6, lv_color_hex(VALUE_COLOR),
					   &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);

		s_stocks.lbl_pct[i] =
			make_label(row, COL3_X, 17, COL3_W - 6, lv_color_hex(TITLE_COLOR),
					   &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
	}
}

/**
 * @brief Refresh the stocks screen with the latest quote data.
 *
 * For valid quotes: dollar change (top, green/red) and percent change
 * (bottom, same colour but slightly dimmer via the title palette).
 * Pending quotes show "--" in both change labels.
 *
 * @param s Latest snapshot from stocks_get_snapshot().
 */
void ui_update_stocks(const stocks_snapshot_t *s) {
	if (!s) {
		return;
	}

	char buf[24];

	bsp_display_lock(0);

	for (int i = 0; i < STOCKS_MAX_SYMBOLS; i++) {
		if (i >= s->count) {
			lv_label_set_text(s_stocks.lbl_symbol[i], "");
			lv_label_set_text(s_stocks.lbl_price[i], "");
			lv_label_set_text(s_stocks.lbl_dollar[i], "");
			lv_label_set_text(s_stocks.lbl_pct[i], "");
			continue;
		}

		const stock_quote_t *q = &s->quotes[i];

		lv_label_set_text(s_stocks.lbl_symbol[i], q->symbol);

		if (!q->valid) {
			lv_label_set_text(s_stocks.lbl_price[i], "--");
			lv_label_set_text(s_stocks.lbl_dollar[i], "--");
			lv_label_set_text(s_stocks.lbl_pct[i], "");
			lv_obj_set_style_text_color(s_stocks.lbl_dollar[i],
										lv_color_hex(VALUE_COLOR),
										LV_PART_MAIN);
			lv_obj_set_style_text_color(
				s_stocks.lbl_pct[i], lv_color_hex(TITLE_COLOR), LV_PART_MAIN);
		} else {
			snprintf(buf, sizeof(buf), "$%.2f", q->price);
			lv_label_set_text(s_stocks.lbl_price[i], buf);

			lv_color_t chg_color = (q->change_pct >= 0.0f)
									   ? lv_color_hex(POS_COLOR)
									   : lv_color_hex(NEG_COLOR);

			/* Top line: absolute dollar change */
			snprintf(buf, sizeof(buf), "%+.2f", q->change);
			lv_label_set_text(s_stocks.lbl_dollar[i], buf);
			lv_obj_set_style_text_color(s_stocks.lbl_dollar[i], chg_color,
										LV_PART_MAIN);

			/* Bottom line: percent change */
			snprintf(buf, sizeof(buf), "%+.2f%%", q->change_pct);
			lv_label_set_text(s_stocks.lbl_pct[i], buf);
			lv_obj_set_style_text_color(s_stocks.lbl_pct[i], chg_color,
										LV_PART_MAIN);
		}
	}

	bsp_display_unlock();
}
