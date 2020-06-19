#include "arch/amd64/hw/rs232.h"
#include "arch/amd64/hw/con.h"
#include "arch/amd64/hw/ps2.h"
#include "user/termios.h"
#include "user/signum.h"
#include "sys/char/input.h"
#include "user/errno.h"
#include "sys/char/line.h"
#include "sys/char/ring.h"
#include "arch/amd64/cpu.h"
#include "sys/char/tty.h"
#include "sys/char/chr.h"
#include "sys/console.h"
#include "sys/string.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/ctype.h"
#include "sys/debug.h"
#include "sys/sched.h"
#include "sys/panic.h"
#include "sys/heap.h"
#include "sys/dev.h"
#include "sys/mm.h"

static ssize_t tty_write(struct chrdev *tty, const void *buf, size_t pos, size_t lim);
static int tty_ioctl(struct chrdev *tty, unsigned int cmd, void *arg);

static const struct termios default_termios = TERMIOS_DEFAULT;

//void tty_control_write(struct chrdev *tty, char c) {
//    struct tty_data *data = tty->dev_data;
//    _assert(data);
//
//    switch (c) {
//    case 'd':
//        ring_signal(&tty->buffer, RING_SIGNAL_EOF);
//        break;
//    case '.':
//        //ring_signal(&tty->buffer, RING_SIGNAL_BRK);
//        thread_signal_pgid(data->fg_pgid, SIGUSR1);
//        break;
//    case 'c':
//        //ring_signal(&tty->buffer, RING_SIGNAL_BRK);
//        thread_signal_pgid(data->fg_pgid, SIGINT);
//        break;
//    default:
//        panic("Unhandled control to TTY: ^%c\n", c);
//    }
//}

void tty_data_write(struct chrdev *tty, char c) {
    _assert(tty && tty->type == CHRDEV_TTY);
    ring_putc(NULL, &tty->buffer, c, 0);

    switch (c) {
    case '\n':
        if (tty->tc.c_lflag & ECHONL) {
            tty_putc(tty, c);
        }
        if (tty->tc.c_iflag & ICANON) {
            // Trigger line flush
            ring_signal(&tty->buffer, RING_SIGNAL_RET);
        }
        break;
    case '\b':
        ring_signal(&tty->buffer, 0);
        break;
    case '\033':
        if ((tty->tc.c_lflag & ECHO) || (tty->tc.c_iflag & ICANON)) {
            tty_puts(tty, "^[");
        }
        break;
    default:
        // This ignores ICANON
        if (tty->tc.c_lflag & ECHO) {
            tty_putc(tty, c);
        }
    }
}

void tty_puts(struct chrdev *tty, const char *s) {
    for (; *s; ++s) {
        tty_putc(tty, *s);
    }
}

int tty_create(struct console *master) {
    struct chrdev *tty = kmalloc(sizeof(struct chrdev));
    _assert(tty);

    struct tty_data *data = kmalloc(sizeof(struct tty_data));
    _assert(data);

    data->fg_pgid = 1;
    data->master = NULL;
    data->buffer = NULL;
    list_head_init(&data->list);

    tty->type = CHRDEV_TTY;
    memcpy(&tty->tc, &default_termios, sizeof(struct termios));
    tty->write = tty_write;
    tty->ioctl = tty_ioctl;
    tty->read = line_read;
    tty->dev_data = data;

    ring_init(&tty->buffer, 16);

    console_attach(master, tty);

    dev_add(DEV_CLASS_CHAR, DEV_CHAR_TTY, tty, "tty0");

    return 0;
}

void tty_putc(struct chrdev *tty, char c) {
    struct tty_data *data = tty->dev_data;
    _assert(data);
    _assert(data->master);

    console_putc(data->master, tty, c);
}

void tty_init(void) {
    // tty0
//    ring_init(&_dev_tty0.buffer, 16);
//    g_keyboard_tty = &_dev_tty0;
//    dev_add(DEV_CLASS_CHAR, DEV_CHAR_TTY, &_dev_tty0, "tty0");

    // ttyS0
    //ring_init(&_dev_ttyS0.buffer, 16);
    //rs232_set_tty(RS232_COM1, &_dev_ttyS0);
    //dev_add(DEV_CLASS_CHAR, DEV_CHAR_TTY, &_dev_ttyS0, "ttyS0");
}

static ssize_t tty_write(struct chrdev *tty, const void *buf, size_t pos, size_t lim) {
    struct tty_data *data = tty->dev_data;
    _assert(data);

    for (size_t i = 0; i < lim; ++i) {
        console_putc(data->master, tty, ((const char *) buf)[i]);
    }

    return lim;
}

static int tty_ioctl(struct chrdev *tty, unsigned int cmd, void *arg) {
    struct tty_data *data = tty->dev_data;
    _assert(data);

    switch (cmd) {
    case TIOCGWINSZ:
        {
            _assert(arg);
            struct winsize *winsz = arg;
            _assert(data->master);

            winsz->ws_col = data->master->width_chars;
            winsz->ws_row = data->master->height_chars;
        }
        return 0;
    case TCGETS:
        memcpy(arg, &tty->tc, sizeof(struct termios));
        return 0;
    case TCSETS:
        memcpy(&tty->tc, arg, sizeof(struct termios));
        if (tty->tc.c_iflag & ICANON) {
            tty->buffer.flags &= ~RING_RAW;
        } else {
            tty->buffer.flags |= RING_RAW;
        }
        return 0;
    case TIOCSPGRP:
        // Clear interrupts and stuff
        tty->buffer.flags = 0;
        data->fg_pgid = *(pid_t *) arg;
        return 0;
    }
    return -EINVAL;
}
