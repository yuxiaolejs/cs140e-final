#ifndef KEYBOARD_H
#define KEYBOARD_H
#include "piusb.h"
void keyboard_init(usb_context_t *ctx);
uint32_t keyboard_poll_once(); // will return an key event encoding or -1 if no key
#endif