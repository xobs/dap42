/*
 * Copyright (c) 2016, Devan Lai
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef TARGET_H_INCLUDED
#define TARGET_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

extern void cpu_setup(void);
extern void clock_setup(void);
extern void gpio_setup(void);
extern void target_console_init(void);

/* Applies the CDC modem-control lines to whatever the board wires them to.
 *
 * Weakly defined as a no-op, so only boards that route these lines somewhere
 * need an implementation. `dtr` and `rts` are the logical states the host
 * asked for, not pin levels: a board inverts if its hardware does.
 */
extern void target_set_control_lines(bool dtr, bool rts);
extern void led_num(uint8_t value);
extern void led_bit(uint8_t position, bool state);

#endif
