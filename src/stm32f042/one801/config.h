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

#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#define PRODUCT_NAME "oNE801"
#define REMAP_USB 1
#define USB_NVIC_LINE NVIC_USB_IRQ
#define USB_IRQ_NAME usb_isr

/* No CAN on this board. Also keeps SLCAN off the second CDC, which it would
 * otherwise claim. */
#define CAN_RX_AVAILABLE 0
#define CAN_TX_AVAILABLE 0
#define CAN_NVIC_LINE NVIC_CEC_CAN_IRQ

/* A second USB serial port.
 *
 * The first carries the Wi-Fi module's UART, and its modem-control lines gate
 * the module (below), so it cannot double as the STM32H5's console without
 * the two contending for one port. This is the second port for that console.
 *
 * This port carries the Wi-Fi module's UART, bridged to it by wifi_uart.c,
 * and its modem-control lines gate the module (below) -- so reading the log,
 * driving AT commands and reaching download mode all happen over one port,
 * the way the host tooling already expects.
 */
#define VCDC_AVAILABLE 1
#define VCDC_TX_BUFFER_SIZE 256
#define VCDC_RX_BUFFER_SIZE 256

#define CDC_AVAILABLE 1
#define DEFAULT_BAUDRATE 115200

/* USART2 on PA2/PA3 carries the STM32H5's console, on the first CDC, exactly
 * as on the current board.
 */
#define CONSOLE_USART USART2
#define CONSOLE_TX_BUFFER_SIZE 128

/* The STM32H5's console runs at 115200, where 256 bytes is over 20 ms of
 * slack against a main loop that drains every pass. The 2 Mbit side is the
 * Wi-Fi UART below, and that is where the buffer budget goes -- 6 KB of RAM
 * is shared with a stack the linker places at the top with no reservation
 * and no overflow check.
 */
#define CONSOLE_RX_BUFFER_SIZE 256

#define CONSOLE_USART_GPIO_PORT GPIOA
#define CONSOLE_USART_GPIO_PINS (GPIO2|GPIO3)
#define CONSOLE_USART_GPIO_AF   GPIO_AF1
#define CONSOLE_USART_MODE USART_MODE_TX_RX
#define CONSOLE_USART_CLOCK RCC_USART2

#define CONSOLE_USART_IRQ_NAME  usart2_isr
#define CONSOLE_USART_NVIC_LINE NVIC_USART2_IRQ
#define CONSOLE_RX_DMA_CONTROLLER DMA1
#define CONSOLE_RX_DMA_CLOCK RCC_DMA
#define CONSOLE_RX_DMA_CHANNEL DMA_CHANNEL5

#define DFU_AVAILABLE 1
#define nBOOT0_GPIO_CLOCK RCC_GPIOB
#define nBOOT0_GPIO_PORT GPIOB
#define nBOOT0_GPIO_PIN  GPIO8

#define BULK_AVAILABLE 1
#define HID_AVAILABLE 1
#define WINUSB_AVAILABLE 1

/* Wi-Fi module UART: USART1 on PB6/PB7, bridged to the second CDC.
 *
 * Not PF0/PF1 as first drawn: on this part PF0 and PF1 have no USART
 * capability at all -- their only signals are CRS_SYNC/I2C1_SDA/OSC_IN and
 * I2C1_SCL/OSC_OUT. The USART pins available are USART1 on PA9/PA10 or
 * PB6/PB7, and USART2 on PA2/PA14 and PA3/PA15, of which PA14 is SWCLK.
 * AF0 selects USART1 on PB6/PB7, per ST's pin data for this part.
 */
#define WIFI_UART_AVAILABLE 1
#define WIFI_USART USART1
#define WIFI_USART_CLOCK RCC_USART1
#define WIFI_USART_GPIO_CLOCK RCC_GPIOB
#define WIFI_USART_GPIO_PORT GPIOB
#define WIFI_USART_GPIO_PINS (GPIO6|GPIO7)
#define WIFI_USART_GPIO_AF GPIO_AF0

/* USART1_RX is DMA1 channel 3 in this family's default request map, where
 * USART2_RX -- used by the console above and by the other boards here -- is
 * channel 5. */
#define WIFI_RX_DMA_CONTROLLER DMA1
#define WIFI_RX_DMA_CLOCK RCC_DMA
#define WIFI_RX_DMA_CHANNEL DMA_CHANNEL3

/* 1 KB is 5 ms of slack at 2 Mbit, comfortably more than a pass of the main
 * loop, which is what this has to cover rather than the whole ~2 KB banner. */
#define WIFI_RX_BUFFER_SIZE 1024
#define WIFI_DEFAULT_BAUDRATE 2000000

/* Wi-Fi module power and boot mode, driven from the first CDC's modem-control
 * lines so that a host can reach download mode over the same port it reads the
 * log on.
 *
 * The polarity deliberately matches a TTL USB-serial adapter, which inverts:
 * asserting the line drives the pin low. That is what the existing host-side
 * tooling assumes -- assert RTS and release it to power-cycle, hold DTR to
 * pick the mode -- so it works against this firmware unchanged.
 *
 *   RTS asserted   -> WIFI_EN low    -> module powered down
 *   RTS released   -> WIFI_EN high   -> module running
 *   DTR asserted   -> WIFI_BOOT low  -> normal boot
 *   DTR released   -> WIFI_BOOT high -> factory download mode
 */
#define WIFI_CONTROL_AVAILABLE 1
#define WIFI_GPIO_CLOCK RCC_GPIOB
#define WIFI_GPIO_PORT  GPIOB
#define WIFI_EN_PIN     GPIO3
#define WIFI_BOOT_PIN   GPIO4

/* SWD only -- see this board's CMSIS_DAP_config.h: JTAG needs JTDO, which
 * would take PB7 away from the Wi-Fi module's UART. */
/* #define CONF_JTAG */

/* Word size for usart_recv and usart_send */
typedef uint8_t usart_word_t;

#define LED_OPEN_DRAIN         1

#endif
