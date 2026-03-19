#include <stdint.h>

#include "fb.h"
#include <stdarg.h>
#include "serial.h"
#include "mem.h"

#define PERIPH_BASE 0x20000000
#define MBOX_BASE (PERIPH_BASE + 0xB880)

#define MBOX_READ (*(volatile uint32_t *)(MBOX_BASE + 0x00))
#define MBOX_STATUS (*(volatile uint32_t *)(MBOX_BASE + 0x18))
#define MBOX_WRITE (*(volatile uint32_t *)(MBOX_BASE + 0x20))

#define MBOX_EMPTY 0x40000000
#define MBOX_FULL 0x80000000
#define MBOX_CH_PROP 8
#define ARM_TO_GPU_BUS(addr) ((addr) | 0x40000000)

__attribute__((aligned(16))) volatile uint32_t mbox[36];

char *font;
uint16_t font_rows;
uint16_t font_cols;

static int mailbox_call(uint8_t channel)
{
    uint32_t addr = (uint32_t)(uintptr_t)mbox;

    // must be 16-byte aligned
    if (addr & 0xF)
    {
        return 0;
    }

    uint32_t msg = (ARM_TO_GPU_BUS(addr) & ~0xF) | (channel & 0xF);

    while (MBOX_STATUS & MBOX_FULL)
        ;
    MBOX_WRITE = msg;

    while (1)
    {
        while (MBOX_STATUS & MBOX_EMPTY)
            ;
        uint32_t r = MBOX_READ;
        if (r == msg)
            return 1;
    }
}

volatile uint32_t *fb;
uint32_t pitch;

void framebuffer_init(char *_font)
{
    font = _font;
    font_rows = (uint16_t)font[0];
    font_cols = (uint16_t)font[1];

    mbox[0] = 30 * 4;
    mbox[1] = 0;

    mbox[2] = 0x48003; // set physical size
    mbox[3] = 8;
    mbox[4] = 8;
    mbox[5] = 640;
    mbox[6] = 480;

    mbox[7] = 0x48004; // set virtual size
    mbox[8] = 8;
    mbox[9] = 8;
    mbox[10] = 640;
    mbox[11] = 480;

    mbox[12] = 0x48005; // set depth
    mbox[13] = 4;
    mbox[14] = 4;
    mbox[15] = 32;

    mbox[16] = 0x48006; // set pixel order
    mbox[17] = 4;
    mbox[18] = 4;
    mbox[19] = 1; // RGB

    mbox[20] = 0x40001; // allocate framebuffer
    mbox[21] = 8;
    mbox[22] = 4;
    mbox[23] = 16;
    mbox[24] = 0;

    mbox[25] = 0x40008; // get pitch
    mbox[26] = 4;
    mbox[27] = 0;
    mbox[28] = 0;

    mbox[29] = 0;

    mailbox_call(MBOX_CH_PROP);

    fb = (volatile uint32_t *)(mbox[23] & 0x3FFFFFFF);
    pitch = mbox[28];
}

void framebuffer_free(void)
{
    mbox[0] = 8 * 4;
    mbox[1] = 0;

    mbox[2] = 0x48001; // release framebuffer
    mbox[3] = 0;
    mbox[4] = 0;

    mbox[5] = 0; // end tag

    // padding to 16-byte align (already fine at 8 words but just in case)
    mbox[6] = 0;
    mbox[7] = 0;

    mailbox_call(MBOX_CH_PROP);

    fb = NULL;
    pitch = 0;
}

static inline void putpixel(int x, int y, uint32_t rgb)
{
    fb[y * (pitch / 4) + x] = rgb;
}

void draw_char(char c, int x, int y, uint32_t color_code)
{
    char *map = font + (c - 0x20) * font_rows + 2; // skip metadata
    for (int i = 0; i < font_rows; i++)
        for (int j = 0; j < font_cols; j++)
            putpixel(x + j, y + i, ((map[i] >> ((font_cols - 1) - j)) & 1u) ? color_code : 0);
}

void handle_scroll(uint32_t *x, uint32_t *y)
{
    if (*y >= 480 - font_rows)
    {
        // scroll up by font_rows pixels
        // for (int i = 0; i < 480 - font_rows; i++)
        //     for (int j = 0; j < pitch / 4; j++)
        //         fb[i * (pitch / 4) + j] = fb[(i + font_rows) * (pitch / 4) + j];
        memmove_fast((void *)fb, (void *)(fb + font_rows * (pitch / 4)), (480 - font_rows) * pitch);
        // clear bottom font_rows pixels
        // for (int i = 480 - font_rows; i < 480; i++)
        //     for (int j = 0; j < pitch / 4; j++)
        //         fb[i * (pitch / 4) + j] = 0;
        memset_fast((void *)(fb + (480 - font_rows) * (pitch / 4)), 0, font_rows * pitch);
        *y -= font_rows;
    }
}

void printToScreenColored(char *str, uint32_t *x, uint32_t *y, uint32_t color_code)
{
    while (*str)
    {
        if ((*str < 0x20 || *str > 0x7E) && *str != '\n')
        {
            printk("Unsupported character '%x' in printToScreen\n", *str);
            str++;
            continue;
        }
        if (*str == '\n') // just do the \r\n thingy
        {
            *y += font_rows;
            *x = 0;
            handle_scroll(x, y);
        }
        else
        {
            draw_char(*str, *x, *y, color_code);
            *x += font_cols;
        }
        if (*x >= pitch / 4)
        {
            *x = 0;
            *y += font_rows;
            handle_scroll(x, y);
        }
        str++;
    }
}

void printToScreen(char *str, uint32_t *x, uint32_t *y)
{
    printToScreenColored(str, x, y, 0xFFFFFFFF);
}

void fb_clear()
{
    memset_fast((void *)fb, 0, 480 * pitch);
}

void fb_invert(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    for (uint16_t i = 0; i < height; i++)
        for (uint16_t j = 0; j < width; j++)
            putpixel(x + j, y + i, ~fb[(y + i) * (pitch / 4) + (x + j)]);
}

void fb_invert_char(uint16_t x, uint16_t y)
{
    fb_invert(x, y, font_cols, font_rows);
}

// void notmain(void)
// {
//     puts("INIT\n");
//     framebuffer_init();
//     puts("INIT OK\n");

//     uint32_t _x = 0, _y = 0;
//     uint32_t *x = &_x;
//     uint32_t *y = &_y;
//     printToScreen("Hello world\n cs140e", x, y);
// }