#ifndef TERMIOS_H
#define TERMIOS_H
#include "types.h"
#define NCCS 19
typedef struct termios
{
    uint32_t c_iflag;   // input modes
    uint32_t c_oflag;   // output modes
    uint32_t c_cflag;   // control modes
    uint32_t c_lflag;   // local modes (ECHO flag is here!)
    uint8_t c_cc[NCCS]; // control characters
} termios_t;

// CMDS
// TCGETS/TCSETS family - work with struct termios
#define TERMIOS_CMD_TCGETS 0x5401  // Get terminal attributes
#define TERMIOS_CMD_TCSETS 0x5402  // Set terminal attributes immediately
#define TERMIOS_CMD_TCSETSW 0x5403 // Set after draining output
#define TERMIOS_CMD_TCSETSF 0x5404 // Set after drain + flush input

// TIOC family
#define TERMIOS_CMD_TIOCGWINSZ 0x5413 // Get window size (struct winsize)
#define TERMIOS_CMD_TIOCSWINSZ 0x5414 // Set window size
#define TERMIOS_CMD_TIOCGPGRP 0x540F  // Get foreground process group
#define TERMIOS_CMD_TIOCSPGRP 0x5410  // Set foreground process group
#define TERMIOS_CMD_TIOCNOTTY 0x5422  // Give up controlling terminal
#define TERMIOS_CMD_TIOCSCTTY 0x540E  // Become controlling terminal

// Input/output queue control
#define TERMIOS_CMD_TCIFLUSH 0    // Flush input queue
#define TERMIOS_CMD_TCOFLUSH 1    // Flush output queue
#define TERMIOS_CMD_TCIOFLUSH 2   // Flush both queues
#define TERMIOS_CMD_TCFLSH 0x540B // Flush queues (arg: TCIFLUSH/TCOFLUSH/TCIOFLUSH)

// Flow control
#define TERMIOS_CMD_TCXONC 0x540A // Flow control (TCOOFF/TCOON/TCIOFF/TCION)

#define TERMIOS_LF_ISIG 0x0001   // Enable signals (Ctrl-C -> SIGINT, etc.)
#define TERMIOS_LF_ICANON 0x0002 // Canonical mode (line buffering)
#define TERMIOS_LF_ECHO 0x0008   // Echo input characters
#define TERMIOS_LF_ECHOE 0x0010  // Echo erase as backspace-space-backspace
#define TERMIOS_LF_ECHOK 0x0020  // Echo NL after kill character
#define TERMIOS_LF_ECHONL 0x0040 // Echo NL even if ECHO is off
#define TERMIOS_LF_NOFLSH 0x0080 // Don't flush after interrupt
#define TERMIOS_LF_IEXTEN 0x8000 // Enable extended input processing

struct winsize
{
    unsigned short ws_row;    // number of rows (in characters)
    unsigned short ws_col;    // number of columns (in characters)
    unsigned short ws_xpixel; // horizontal size in pixels (unused, usually 0)
    unsigned short ws_ypixel; // vertical size in pixels (unused, usually 0)
};

#endif