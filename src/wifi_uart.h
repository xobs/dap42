/*
 * Second UART, bridged to the second USB CDC.
 *
 * The console driver in console.c is a single global instance -- one set of
 * buffers, one DMA channel, one ISR named by the board config -- so a board
 * that needs two UARTs needs a second implementation rather than a second
 * instance. This is that, kept separate so the boards with one UART are
 * untouched.
 */

#ifndef WIFI_UART_H_INCLUDED
#define WIFI_UART_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

#if WIFI_UART_AVAILABLE

/* Brings up the UART and starts its receive DMA. */
extern void wifi_uart_setup(void);

/* Moves bytes both ways between the UART and the second CDC. Returns true if
 * it moved any. Called from the main loop. */
extern bool wifi_uart_update(void);

/* Reprograms the line rate. Returns false if the rate is not representable,
 * leaving the current setting alone. */
extern bool wifi_uart_set_baudrate(uint32_t baudrate);

extern uint32_t wifi_uart_get_baudrate(void);

#endif /* WIFI_UART_AVAILABLE */

#endif
