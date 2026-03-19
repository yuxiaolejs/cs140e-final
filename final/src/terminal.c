#include "terminal.h"
#include "fb.h"
#include "serial.h"
#define ANSI_OFF 0
#define DEBUG_TERMINAL_ANSI 0

// Screen cursor position (by pixel)
static uint32_t _x = 0;
static uint32_t _y = 0;
static uint32_t *x = &_x;
static uint32_t *y = &_y;

static uint32_t ANSI_MAX_X = 640;
static uint32_t ANSI_MAX_Y = 480;

static void move_cursor_to(uint32_t row, uint32_t col)
{
    fb_invert_char(*x, *y);
    uint32_t new_x = col * 8;
    uint32_t new_y = row * 16;
    if (new_x >= ANSI_MAX_X)
        new_x = ANSI_MAX_X - 8;
    if (new_y >= ANSI_MAX_Y)
        new_y = ANSI_MAX_Y - 16;
    printk("moving cur to %d, %d\n", new_x, new_y);
    *x = new_x;
    *y = new_y;
    fb_invert_char(*x, *y);
}

static void terminal_handle_backspace()
{
    if (*x >= 8)
        *x -= 8;
    else if (*y >= 16)
    {
        *y -= 16;
        *x = ANSI_MAX_X - 8;
    }
}

enum
{
    ANSI_NORM = 0,
    ANSI_ESC,
    ANSI_CSI,
};

static int ansi_state = ANSI_NORM;
static int ansi_params[] = {0, 0, 0, 0, 0}; // support up to 5 params, should be enough
static int ansi_param_count = 0;

static uint32_t ANSI_COLOR_CODE = 0xFFFFFFFF; // white

static void ansi_handle_final(char c)
{
    if (DEBUG_TERMINAL_ANSI)
        printk("  ++++ Got ANSI final byte: %c, param count: %d\n", c, ansi_param_count);
    for (int i = 0; i < ansi_param_count; i++)
        if (DEBUG_TERMINAL_ANSI)
            printk("    param %d: %d\n", i, ansi_params[i]);
    switch (c)
    {
    case 'A': // up
        if (*y >= 16)
            *y -= 16;
        break;
    case 'B': // down
        *y += 16;
        break;
    case 'C': // right
        *x += 8;
        break;
    case 'D': // left
        if (*x >= 8)
            *x -= 8;
        break;

    case 'H': // home OR CUP
        if (ansi_param_count == 1)
            move_cursor_to(ansi_params[0] - 1, 0);
        else if (ansi_param_count == 2)
            move_cursor_to(ansi_params[0] - 1, ansi_params[1] - 1);
        else
        {
            *x = 0;
            *y = 0;
        }
        break;

    case 'J':
    {
        uint32_t param = (ansi_param_count >= 1) ? ansi_params[0] : 0;
        if (param == 0 || param == 2 || param == 3)
        {
            fb_clear();
            *x = 0;
            *y = 0;
        }
        break;
    }

    case 'm': // color
        if (ansi_param_count == 0 || (ansi_param_count == 1 && ansi_params[0] == 0) || (ansi_param_count == 2 && ansi_params[0] == 0 && ansi_params[1] == 0))
        {
            ANSI_COLOR_CODE = 0xFFFFFFFF;
            break;
        }
        uint32_t target_index = ansi_param_count - 1;
        if (ansi_params[target_index] == 30)
            ANSI_COLOR_CODE = 0xFF000000;
        else if (ansi_params[target_index] == 31)
            ANSI_COLOR_CODE = 0xFFFF0000;
        else if (ansi_params[target_index] == 32)
            ANSI_COLOR_CODE = 0xFF00FF00;
        else if (ansi_params[target_index] == 33)
            ANSI_COLOR_CODE = 0xFFFFFF00;
        else if (ansi_params[target_index] == 34)
            ANSI_COLOR_CODE = 0xFF0000FF;
        else if (ansi_params[target_index] == 35)
            ANSI_COLOR_CODE = 0xFFFF00FF;
        else if (ansi_params[target_index] == 36)
            ANSI_COLOR_CODE = 0xFF00FFFF;
        else if (ansi_params[target_index] == 37)
            ANSI_COLOR_CODE = 0xFFFFFFFF;
        break;

    default:
        break;
    }

    ansi_param_count = 0;
    for (int i = 0; i < 5; i++)
        ansi_params[i] = 0;
    ansi_state = ANSI_NORM;
}
static int ANSI_STATE = 0; // 0 = normal, 1 = got ESC, 2 = got [

void terminal_putc(char c)
{
    char str[2] = {c, 0};

    switch (ansi_state)
    {
    case ANSI_NORM:
        if (c == '\x1b' && !ANSI_OFF)
        {
            ansi_state = ANSI_ESC;
            return;
        }
#ifdef USE_PHYSICAL_TTY
        fb_invert_char(*x, *y);
        if (c == '\b') // backspace...
            terminal_handle_backspace();
        else
            printToScreenColored(str, x, y, ANSI_COLOR_CODE);
        fb_invert_char(*x, *y);
#else
        uart_putc(c);
#endif
        return;

    case ANSI_ESC:
        if (c == '[')
        {
            ansi_state = ANSI_CSI;
            ansi_param_count = 0;
            for (int i = 0; i < 5; i++)
                ansi_params[i] = 0;
            return;
        }
        ansi_state = ANSI_NORM;
        return;

    case ANSI_CSI:
        if (DEBUG_TERMINAL_ANSI)
            printk("Got ANSI CSI char: %c\n", c);
        if (ansi_param_count == 0)
            ansi_param_count = 1;
        if (c >= '0' && c <= '9')
        {
            ansi_params[ansi_param_count - 1] = ansi_params[ansi_param_count - 1] * 10 + (c - '0');
            if (DEBUG_TERMINAL_ANSI)
                printk("  Updated param %d to %d\n", ansi_param_count, ansi_params[ansi_param_count - 1]);
            return;
        }
        else if (c == '?')
        {
            return;
        }
        else if (c == ';')
        {
            ansi_param_count++;
            if (ansi_param_count >= 5)
                ansi_param_count = 4; // just clamp it, should be enough
            return;
        }
        ansi_handle_final(c);
        return;
    }
}
