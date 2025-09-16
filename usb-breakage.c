// Demonstrate the off-by-one error with the ARM CMSIS-DAP DAP_SWD_SEQUENCE command. On projects
// using ARM-supplied code, the IDR will be shifted by one.
//
// Compile with:
//
// Bash:
// gcc $(pkg-config libusb-1.0 --libs --cflags) usb-breakage.c -o usb-breakage; ./usb-breakage
//
// Fish:
// gcc (pkg-config libusb-1.0 --libs --cflags | string split -n " ") usb-breakage.c -o usb-breakage; and ./usb-breakage
//
// Based on the CMSIS-DAP code from the Blackmagic Project: https://github.com/blackmagic-debug/blackmagic

#include <assert.h>
#include <libusb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct probe
{
    uint8_t interface_num;
    uint8_t in_ep;
    uint8_t out_ep;
    uint16_t max_packet_length;
    libusb_context *libusb_ctx;
    libusb_device *libusb_dev;
    libusb_device_handle *handle;
} probe_s;
#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef enum dap_command {
	DAP_CONNECT = 0x02U,
	DAP_DISCONNECT = 0x03U,
	DAP_TRANSFER_CONFIGURE = 0x04U,
	DAP_SWJ_PINS = 0x10U,
	DAP_SWJ_SEQUENCE = 0x12U,
	DAP_SWD_CONFIGURE = 0x13U,
	DAP_SWD_SEQUENCE = 0x1dU,
} dap_command_e;
static bool check_cmsis_interface_type(libusb_device *const device, probe_s *const info)
{
    bool found = false;

    /* Try to get the active configuration descriptor for the device */
    struct libusb_config_descriptor *config;
    if (libusb_get_active_config_descriptor(device, &config) != LIBUSB_SUCCESS)
        return false;

    /* Try to open the device */
    libusb_device_handle *handle;
    if (libusb_open(device, &handle) != LIBUSB_SUCCESS)
    {
        libusb_free_config_descriptor(config);
        return false;
    }

    /* Enumerate the device's interfaces and all their alt modes */
    for (uint8_t iface = 0; iface < config->bNumInterfaces; ++iface)
    {
        const struct libusb_interface *interface = &config->interface[iface];
        for (int altmode = 0; altmode < interface->num_altsetting; ++altmode)
        {
            const struct libusb_interface_descriptor *descriptor = &interface->altsetting[altmode];
            /* If we've found an interface without a description string, ignore it */
            if (descriptor->iInterface == 0)
                continue;
            char interface_string[128U];
            /* Read out the string */
            if (libusb_get_string_descriptor_ascii(
                    handle, descriptor->iInterface, (unsigned char *)interface_string, sizeof(interface_string)) < 0)
                continue;

            /* Check if it's a CMSIS-DAP interface */
            if (strstr(interface_string, "CMSIS") == NULL)
                continue;

            /* Scan through the endpoints finding the one with the narrowest max transfer length */
            info->max_packet_length = UINT16_MAX;
            for (uint8_t index = 0; index < descriptor->bNumEndpoints; ++index)
                info->max_packet_length = MIN(descriptor->endpoint[index].wMaxPacketSize, info->max_packet_length);

            /* Check if it's a CMSIS-DAP v2 interface */
            if (descriptor->bInterfaceClass == 0xffU && descriptor->bNumEndpoints == 2U)
            {
                info->interface_num = descriptor->bInterfaceNumber;
                /* Extract the endpoints required */
                for (uint8_t index = 0; index < descriptor->bNumEndpoints; ++index)
                {
                    const uint8_t ep = descriptor->endpoint[index].bEndpointAddress;
                    if (ep & 0x80U)
                        info->in_ep = ep;
                    else
                        info->out_ep = ep;
                }
                /* If we've found a CMSIS-DAP v2 interface, look no further - we want to prefer these to v1. */
                found = true;
                break;
            }
        }
    }
    libusb_close(handle);
    libusb_free_config_descriptor(config);
    return found;
}

static probe_s *find_cmsis_dap(struct libusb_context *libusb_ctx)
{
    probe_s *probe = malloc(sizeof(probe_s));
    libusb_device **device_list;
    libusb_device_handle *handle = NULL;
    const ssize_t cnt = libusb_get_device_list(libusb_ctx, &device_list);
    assert(cnt >= 0);
    for (size_t device_index = 0; device_list[device_index]; ++device_index)
    {
        memset(probe, 0, sizeof(*probe));
        libusb_device *const device = device_list[device_index];
        struct libusb_device_descriptor device_descriptor;
        const int result = libusb_get_device_descriptor(device, &device_descriptor);
        if (result < 0)
        {
            fprintf(stderr, "Failed to get device descriptor (%d): %s\n", result, libusb_error_name(result));
            return NULL;
        }
        if (!check_cmsis_interface_type(device, probe))
        {
            continue;
        }
        printf("In EP: 0x%02x  Out EP: 0x%02x  Interface: %d  Max packet length: %d\n",
               probe->in_ep, probe->out_ep, probe->interface_num, probe->max_packet_length);
        printf("Opening...\n");
        int error;
        error = libusb_open(device, &handle);
        if (error != 0)
        {
            fprintf(stderr, "Unable to open: %s (%d)\n", libusb_error_name(error), error);
            assert(error);
        }
        printf("Claiming...\n");
        error = libusb_claim_interface(handle, probe->interface_num);
        if (error != 0)
        {
            fprintf(stderr, "Unable to claim: %s (%d)\n", libusb_error_name(error), error);
            assert(error);
        }
        probe->libusb_ctx = libusb_ctx;
        probe->libusb_dev = device;
        probe->handle = handle;
        break;
    }
    libusb_free_device_list(device_list, (int)cnt);
    if (probe->handle)
    {
        return probe;
    }
    else
    {
        free(probe);
        return NULL;
    }
}

#define SWD_SEQUENCE_IN(len) ((len) | (1 << 7))
#define SWD_SEQUENCE_OUT(len) (len)

static void probe_transaction(probe_s *probe, const uint8_t request[64], uint8_t response[64]) {
    int bytes_transferred = 0;
    int error;

    error = libusb_bulk_transfer(probe->handle, probe->out_ep, (void *)request, 64, &bytes_transferred, 0);
    if (error != 0)
    {
        fprintf(stderr, "Unable to send data: %s (%d)\n", libusb_error_name(error), error);
        assert(error);
    }
    printf("Transferred %d bytes out (%02x)\n", bytes_transferred, request[0]);

    memset(response, 0, 64);

    error = libusb_bulk_transfer(probe->handle, probe->in_ep, response, 64, &bytes_transferred, 0);
    if (error != 0)
    {
        fprintf(stderr, "Unable to receive data: %s (%d)\n", libusb_error_name(error), error);
        assert(error);
    }

    if (bytes_transferred > 64)
    {
        fprintf(stderr, "Transferred too many bytes! Expected 64 bytes max, got %d bytes (0x%08x)", bytes_transferred, bytes_transferred);
        assert(bytes_transferred);
    }
    printf("Response was %d bytes: ", bytes_transferred);
    for (int i = 0; i < bytes_transferred; i += 1)
    {
        printf(" 0x%02x", response[i]);
    }
    printf("\n");
}

static void swd_activate(probe_s *probe) {
    uint8_t response[64];
    const uint8_t requests[][64] = {
        {DAP_DISCONNECT},
        // Set nRST and wait 10 uS for it to settle
        {DAP_SWJ_PINS, 0x80, 0x80, 0x0a, 0x00, 0x00, 0x00},
        {DAP_DISCONNECT},
        // Switch to SWD mode
        {DAP_SWD_CONFIGURE, 0x00},
        {DAP_TRANSFER_CONFIGURE, 2, 0x80, 0x00, 0x80, 0x00},
        {DAP_CONNECT, 1},
    
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(32), 0xff, 0xff, 0xff, 0xff},
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(28), 0xff, 0xff, 0xff, 0x0f},

        // jtag_to_dormant1
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(5), 0x1f},

        // jtag_to_dormant2
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(31), 0xba, 0xbb, 0xbb, 0x33},

        // jtag_to_dormant3
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(8), 0xff},

        // swd_alert_1
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(32), 0x92, 0xf3, 0x09, 0x62},

        // swd_alert_2
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(32), 0x95, 0x2d, 0x85, 0x86},

        // swd_alert_3
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(32), 0xe9, 0xaf, 0xdd, 0xe3},

        // swd_alert_4
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(32), 0xa2, 0x0e, 0xbc, 0x19},

        // query_1
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(12), 0xa0, 0x01},

        // query_2
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(32), 0xff, 0xff, 0xff, 0xff},

        // query_3
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(32), 0xff, 0xff, 0xff, 0x0f},

        // deprecated jtag-to-swd sequence (required for some devices)
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(32), 0xff, 0xff, 0xff, 0xff},
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(28), 0xff, 0xff, 0xff, 0x0f},
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(16), 0x9e, 0xe7},
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(32), 0xff, 0xff, 0xff, 0xff},
        {DAP_SWD_SEQUENCE, 1, SWD_SEQUENCE_OUT(32), 0xff, 0xff, 0xff, 0x0f},

        {0xff},
    };

    for (int i = 0; requests[i][0] != 0xff; i += 1)
        probe_transaction(probe, requests[i], response);
}

int main(void)
{
    struct libusb_context *libusb_ctx;
    libusb_init(&libusb_ctx);
    printf("Finding...\n");
    probe_s *probe = find_cmsis_dap(libusb_ctx);
    assert(probe);

    swd_activate(probe);

    // construct a DAP_SWD_Sequence packet that does a DP IDR read,
    // 8 bits for request, 3 bits for ACK read,
    // 33 bits read for the IDR and parity bit, and 8 more bits write for
    // the bus idle - it will either be correct or it will be shifted by 1 bit
    uint8_t request[] = {
        DAP_SWD_SEQUENCE,
        5 /* Sequence Count */,

        // DP IDR READ
        SWD_SEQUENCE_OUT(8),
        0xa5,

        // 1 bit of turnaround
        SWD_SEQUENCE_IN(1),

        // 3 bits of ACK
        SWD_SEQUENCE_IN(3),

        // 32 bits of data + parity
        SWD_SEQUENCE_IN(33),

        // 8 idle bits
        SWD_SEQUENCE_OUT(8),
        0,
    };
    uint8_t response[64];

    int bytes_transferred = 0;
    int error;

    probe_transaction(probe, request, response);

    if (response[1] != 0) {
        fprintf(stderr, "Unable to make request! %02x\n", response[1]);
        assert(false);
    }
    fprintf(stderr, "Ack: %02x  DP IDR: 0x%02x%02x%02x%02x\n", response[3], response[7], response[6], response[5], response[4]);

    libusb_close(probe->handle);
    free(probe);
}
