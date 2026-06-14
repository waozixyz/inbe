#ifndef FLINT_TEXT_H
#define FLINT_TEXT_H

#include "raylib.h"

#define FLINT_TEXT_12 12
#define FLINT_TEXT_14 14
#define FLINT_TEXT_16 16
#define FLINT_TEXT_18 18
#define FLINT_TEXT_20 20
#define FLINT_TEXT_24 24
#define FLINT_TEXT_32 32
#define FLINT_TEXT_BASE_SIZE 16

void flint_text_set_font(Font font);
Font flint_text_font(void);
Font flint_text_load_chopped_font(const char *png_path, const char *dat_path, int base_size);
Font flint_text_load_chopped_font_from_memory(const unsigned char *png_data, unsigned int png_size, const unsigned char *dat_data, unsigned int dat_size, int base_size);
void flint_text_unload_font(Font *font);
int flint_text_size(int preferred_size);
int flint_text_dpi_size(int base_size);
int flint_text_measure(const char *text, int font_size);
int flint_text_measure_scaled(const char *text, int scale);
void flint_text_draw(const char *text, int x, int y, int font_size, Color color);
void flint_text_draw_scaled(const char *text, int x, int y, int scale, Color color);
void flint_text_draw_centered(const char *text, int center_x, int center_y, int font_size, Color color);
void flint_text_draw_in_rect(const char *text, Rectangle rect, int font_size, Color color);
void flint_text_draw_fitted_in_rect(const char *text, Rectangle rect, int preferred_size, int min_size, Color color);
int flint_text_y(const char *text, int box_y, int box_h, int font_size);
int flint_text_y_scaled(const char *text, int box_y, int box_h, int scale);

#endif
