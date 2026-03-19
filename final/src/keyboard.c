#include "keyboard.h"
#include "piusb.h"
#include "hid.h"
#include "flags.h"
#include "serial.h"
#include "regs.h"
#include "pictl.h"
static usb_context_t usb_ctx;

void keyboard_init(usb_context_t *ctx)
{
    usb_ctx = *ctx;
}

void do_usb()
{
    PUT32(ADD_IRQ_DISABLE1, 0xFFFFFFFF);
    PUT32(ADD_IRQ_DISABLE2, 0xFFFFFFFF);
    usb_context_t *usb_ctx = kalloc(sizeof(usb_context_t));
    usb_context_init(usb_ctx);

    usb_hid_full_init(usb_ctx);

    udelay(200000);

    usb_ctx->hid.interrupt_toggle = PID_DATA0;
    usb_interrupt_init_ch(usb_ctx, CH_INTERRUPT, 0,
                          usb_ctx->device.address, &usb_ctx->hid);

    // Also arm aux endpoint (EP4) if present - needed for 2.4G transceiver reconnect
    if (usb_ctx->aux.endpoint)
    {
        usb_ctx->aux.interrupt_toggle = PID_DATA0;
        usb_interrupt_init_ch(usb_ctx, CH_INTERRUPT2, 1,
                              usb_ctx->device.address, &usb_ctx->aux);
    }

    printk("\n*** Keyboard ready! Type something: ***\n");
    keyboard_init(usb_ctx);
}



static uint8_t last_pressed_map[128];
static uint8_t pressed_map[128];

static uint32_t process_keyboard_report(struct hid_keyboard_report *report)
{
    uint8_t shift = (report->modifiers & 0x22); // Left or right shift
    uint8_t ctrl = (report->modifiers & 0x11);  // Left or right ctrl
    uint32_t ret = -1;
    if (DEBUG_KEYBOARD)
        printk("NEW USB FRAME\n");
    for (int i = 0; i < 6; i++)
    {
        // printk("Checking key slot %d: last=%d, current=%d\n", i, last_keys[i], report->keys[i]);
        uint8_t key = report->keys[i];
        if (key != 0)
        {
            if (DEBUG_KEYBOARD)
                printk(" Pressed code: %d, previously_pressed: %d\n", key, last_pressed_map[key]);
            pressed_map[key] = 1;
            if (!last_pressed_map[key])
            {
                char c = shift ? hid_scancode_to_ascii_shift[key] : hid_scancode_to_ascii[key];
                if (ctrl && c >= 0x40 && c <= 0x7F)
                    c = c & 0x1F;
                if (c != 0)
                    ret = (uint32_t)c;
            }
            // printk("Key pressed: %d\n", key);
        }
    }

    for (int i = 0; i < 128; i++)
    {
        last_pressed_map[i] = pressed_map[i];
        pressed_map[i] = 0;
    }
    // printk("Returning key code: %d\n", ret);
    return ret;
}
static struct hid_keyboard_report report __attribute__((aligned(32)));
static uint8_t aux_report[24] __attribute__((aligned(32)));
#ifdef USE_PHYSICAL_TTY
uint32_t keyboard_poll_once()
{
    // Drain aux endpoint (EP4) - transceiver sends reconnect/status data here.
    // Without polling this, the transceiver won't resume sending keyboard data
    // on EP2 after a wireless keyboard sleep/wake cycle.
    if (usb_ctx.aux.endpoint)
        usb_interrupt_poll_ch(&usb_ctx, CH_INTERRUPT2, 1,
                              usb_ctx.device.address, &usb_ctx.aux,
                              aux_report, usb_ctx.aux.ep_mps);

    int ret = usb_interrupt_poll_ch(&usb_ctx, CH_INTERRUPT, 0,
                                    usb_ctx.device.address, &usb_ctx.hid,
                                    &report, sizeof(report));

    if (ret == 0)
        return process_keyboard_report(&report);
    else if (ret == -3 || ret == -5 || ret == -6)
        return -1;
    else
    {
        printk("USB: error %d, re-enumerating\n", ret);
        do_usb();
    }
    return -1;
}
#else
uint32_t keyboard_poll_once()
{
    uint32_t c;
    int r = uart_getc_timeout((char *)&c, 10);
    if (r == 1)
        return c;
    else
        return -1;
}
#endif
