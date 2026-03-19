#ifndef FB_H
#define FB_H
#include "types.h"
void printToScreen(char *str, uint32_t *x, uint32_t *y);
void framebuffer_init(char *_font);
void framebuffer_free(void);
void printToScreenColored(char *str, uint32_t *x, uint32_t *y, uint32_t color_code);
void fb_clear();
void fb_invert(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void fb_invert_char(uint16_t x, uint16_t y);
#endif