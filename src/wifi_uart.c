/*
 * Second UART, bridged to the second USB CDC. See wifi_uart.h.
 */

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>

#include "wifi_uart.h"

#if WIFI_UART_AVAILABLE

#include "USB/vcdc.h"

#define IS_POW_OF_TWO(X) (((X) & ((X)-1)) == 0)
_Static_assert(IS_POW_OF_TWO(WIFI_RX_BUFFER_SIZE),
               "Unmasked circular buffer size must be a power of two");

/* Bytes moved host-to-UART per poll.
 *
 * Bounded because a firmware image pushed at the module is over a megabyte,
 * and draining it all in one pass would stall the main loop -- which is also
 * what services USB and the debug probe. Sixty-four bytes is one USB packet's
 * worth, so the bridge keeps up with the host without ever owning the loop
 * for long.
 */
#define WIFI_TX_CHUNK 64

/* How long to wait for the transmit register to drain, in poll iterations of
 * a tight loop. At the rates this UART runs, a byte leaves in microseconds;
 * this only exists so a wedged peripheral cannot hang the main loop. */
#define WIFI_TX_SPIN_LIMIT 10000

static uint8_t wifi_rx_buffer[WIFI_RX_BUFFER_SIZE];
static volatile uint16_t wifi_rx_head = 0;
static uint32_t wifi_baudrate = WIFI_DEFAULT_BAUDRATE;

static void wifi_rx_dma_start(void) {
    dma_channel_reset(WIFI_RX_DMA_CONTROLLER, WIFI_RX_DMA_CHANNEL);
    dma_set_peripheral_address(WIFI_RX_DMA_CONTROLLER, WIFI_RX_DMA_CHANNEL,
                               (uint32_t)&USART_RDR(WIFI_USART));
    dma_set_memory_address(WIFI_RX_DMA_CONTROLLER, WIFI_RX_DMA_CHANNEL,
                           (uint32_t)wifi_rx_buffer);
    dma_set_number_of_data(WIFI_RX_DMA_CONTROLLER, WIFI_RX_DMA_CHANNEL,
                           WIFI_RX_BUFFER_SIZE);
    dma_set_read_from_peripheral(WIFI_RX_DMA_CONTROLLER, WIFI_RX_DMA_CHANNEL);
    dma_set_peripheral_size(WIFI_RX_DMA_CONTROLLER, WIFI_RX_DMA_CHANNEL,
                            DMA_CCR_PSIZE_8BIT);
    dma_set_memory_size(WIFI_RX_DMA_CONTROLLER, WIFI_RX_DMA_CHANNEL,
                        DMA_CCR_MSIZE_8BIT);
    dma_enable_memory_increment_mode(WIFI_RX_DMA_CONTROLLER, WIFI_RX_DMA_CHANNEL);
    dma_enable_circular_mode(WIFI_RX_DMA_CONTROLLER, WIFI_RX_DMA_CHANNEL);
    /* Ahead of USB's own traffic: a byte missed here is gone, while USB
     * retries. This is the 2 Mbit side. */
    dma_set_priority(WIFI_RX_DMA_CONTROLLER, WIFI_RX_DMA_CHANNEL, DMA_CCR_PL_HIGH);

    dma_enable_channel(WIFI_RX_DMA_CONTROLLER, WIFI_RX_DMA_CHANNEL);
    usart_enable_rx_dma(WIFI_USART);
}

static void wifi_rx_dma_stop(void) {
    usart_disable_rx_dma(WIFI_USART);
    dma_disable_channel(WIFI_RX_DMA_CONTROLLER, WIFI_RX_DMA_CHANNEL);
}

/* Where the DMA has written up to. The count reads as the full buffer size
 * for the instant after a wrap, which the mask folds back to zero -- the same
 * position, the buffer being a power of two. */
static uint16_t wifi_rx_dma_tail(void) {
    uint16_t remaining = dma_get_number_of_data(WIFI_RX_DMA_CONTROLLER,
                                                WIFI_RX_DMA_CHANNEL);
    return (uint16_t)(WIFI_RX_BUFFER_SIZE - remaining) & (WIFI_RX_BUFFER_SIZE - 1);
}

/* Clears the receive error flags. Once ORE latches the peripheral stops
 * presenting new data until it is cleared, so an overrun that nothing clears
 * takes receive down for good rather than costing a few bytes. There is no
 * receive interrupt here, so this is the only path that reaches them. */
static void wifi_clear_rx_errors(void) {
    USART_ICR(WIFI_USART) = USART_ICR_ORECF | USART_ICR_NCF
                          | USART_ICR_FECF | USART_ICR_PECF;
}

void wifi_uart_setup(void) {
    rcc_periph_clock_enable(WIFI_USART_CLOCK);
    rcc_periph_clock_enable(WIFI_RX_DMA_CLOCK);
    rcc_periph_clock_enable(WIFI_USART_GPIO_CLOCK);

    gpio_mode_setup(WIFI_USART_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE,
                    WIFI_USART_GPIO_PINS);
    gpio_set_af(WIFI_USART_GPIO_PORT, WIFI_USART_GPIO_AF, WIFI_USART_GPIO_PINS);

    usart_set_baudrate(WIFI_USART, wifi_baudrate);
    usart_set_databits(WIFI_USART, 8);
    usart_set_parity(WIFI_USART, USART_PARITY_NONE);
    usart_set_stopbits(WIFI_USART, USART_STOPBITS_1);
    usart_set_mode(WIFI_USART, USART_MODE_TX_RX);
    usart_set_flow_control(WIFI_USART, USART_FLOWCONTROL_NONE);

    usart_enable(WIFI_USART);
    wifi_rx_dma_start();
}

bool wifi_uart_set_baudrate(uint32_t baudrate) {
    /* The divisor is the peripheral clock over the rate and has to land in
     * [16, 65535] for 16x oversampling. Outside that a divisor truncates to
     * zero and stops the clock, or overflows to an unrelated rate. */
    const uint32_t clock = rcc_get_usart_clk_freq(WIFI_USART);
    if ((baudrate < (clock / 65535U)) || (baudrate > (clock / 16U))) {
        return false;
    }

    wifi_baudrate = baudrate;

    usart_disable(WIFI_USART);
    wifi_rx_dma_stop();
    /* Restarted rather than rewound: the write pointer lives in the DMA
     * channel's count register, so the buffer is only really empty once the
     * channel has been reloaded. */
    wifi_rx_head = 0;
    usart_set_baudrate(WIFI_USART, baudrate);
    usart_enable(WIFI_USART);
    wifi_rx_dma_start();
    return true;
}

uint32_t wifi_uart_get_baudrate(void) {
    return wifi_baudrate;
}

bool wifi_uart_update(void) {
    bool active = false;

    wifi_clear_rx_errors();

    /* UART to host. Contiguous runs only: the CDC takes a flat buffer, so a
     * wrap is simply left for the next pass. */
    uint16_t tail = wifi_rx_dma_tail();
    if (tail != wifi_rx_head) {
        size_t available = (tail > wifi_rx_head)
                         ? (size_t)(tail - wifi_rx_head)
                         : (size_t)(WIFI_RX_BUFFER_SIZE - wifi_rx_head);
        size_t room = vcdc_send_buffer_space();
        if (available > room) {
            available = room;
        }
        if (available > 0) {
            size_t sent = vcdc_send_buffered(&wifi_rx_buffer[wifi_rx_head], available);
            wifi_rx_head = (uint16_t)((wifi_rx_head + sent) & (WIFI_RX_BUFFER_SIZE - 1));
            active = active || (sent > 0);
        }
    }

    /* Host to UART. */
    uint8_t tx[WIFI_TX_CHUNK];
    size_t got = vcdc_recv_buffered(tx, sizeof(tx));
    for (size_t i = 0; i < got; i++) {
        uint32_t spins = 0;
        while (!usart_get_flag(WIFI_USART, USART_FLAG_TXE)) {
            if (++spins >= WIFI_TX_SPIN_LIMIT) {
                /* Give up rather than hang the loop that also services USB. */
                return true;
            }
        }
        usart_send(WIFI_USART, tx[i]);
    }
    active = active || (got > 0);

    return active;
}

#endif /* WIFI_UART_AVAILABLE */
