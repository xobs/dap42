#include <assert.h>
#include <libusb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct bmda_probe
{
    // uint16_t vid;
    // uint16_t pid;
    uint8_t interface_num;
    uint8_t in_ep;
    uint8_t out_ep;
    uint16_t max_packet_length;
    libusb_context *libusb_ctx;
    libusb_device *libusb_dev;
    libusb_device_handle *handle;
} bmda_probe_s;
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static bool check_cmsis_interface_type(libusb_device *const device, bmda_probe_s *const info)
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

static bmda_probe_s *find_cmsis_dap(struct libusb_context *libusb_ctx)
{
    bmda_probe_s *probe = malloc(sizeof(bmda_probe_s));
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

int main()
{
    struct libusb_context *libusb_ctx;
    libusb_init(&libusb_ctx);
    printf("Finding...\n");
    bmda_probe_s *probe = find_cmsis_dap(libusb_ctx);
    assert(probe);

    // ID_DAP_Info
    uint8_t request[] = {0x9f /* ID_DAP_Vendor31 */,'D', 'F', 'U'};
    uint8_t response[64];
    memset(response, 0, sizeof(response));

    int bytes_transferred = 0;
    int error;

    error = libusb_bulk_transfer(probe->handle, probe->out_ep, request, sizeof(request), &bytes_transferred, 0);
    if (error != 0)
    {
        fprintf(stderr, "Unable to send data: %s (%d)\n", libusb_error_name(error), error);
        assert(error);
    }
    printf("Transferred %d bytes out (ID_DAP_Vendor31)\n", bytes_transferred);

    // request[1] = 4;
    // error = libusb_bulk_transfer(probe->handle, probe->out_ep, request, sizeof(request), &bytes_transferred, 0);
    // if (error != 0)
    // {
    //     fprintf(stderr, "Unable to send data: %s (%d)\n", libusb_error_name(error), error);
    //     assert(error);
    // }
    // printf("Transferred %d bytes out (DAP_ID_FW_VER)\n", bytes_transferred);

    error = libusb_bulk_transfer(probe->handle, probe->in_ep, response, sizeof(response), &bytes_transferred, 0);
    if (error != 0)
    {
        fprintf(stderr, "Unable to receive data: %s (%d)\n", libusb_error_name(error), error);
        assert(error);
    }

    // if (bytes_transferred > 64)
    // {
    //     fprintf(stderr, "Transferred too many bytes! Expected 64 bytes max, got %d bytes (0x%08x)", bytes_transferred, bytes_transferred);
    //     assert(bytes_transferred);
    // }
    // printf("Response was %d bytes: ", bytes_transferred);
    // for (int i = 0; i < bytes_transferred; i += 1)
    // {
    //     printf(" 0x%02x", response[i]);
    // }
    // printf("\n");

    // error = libusb_bulk_transfer(probe->handle, probe->in_ep, response, sizeof(response), &bytes_transferred, 0);
    // if (error != 0)
    // {
    //     fprintf(stderr, "Unable to receive data: %s (%d)\n", libusb_error_name(error), error);
    //     assert(error);
    // }

    // if (bytes_transferred > 64)
    // {
    //     fprintf(stderr, "Transferred too many bytes! Expected 64 bytes max, got %d bytes (0x%08x)", bytes_transferred, bytes_transferred);
    //     assert(bytes_transferred);
    // }
    printf("Response was %d bytes: ", bytes_transferred);
    for (int i = 0; i < bytes_transferred; i += 1)
    {
        printf(" 0x%02x", response[i]);
    }
    printf("\n");

    libusb_close(probe->handle);
    free(probe);
}
