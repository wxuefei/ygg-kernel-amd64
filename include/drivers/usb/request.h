#pragma once
#include "sys/types.h"

#define USB_REQUEST_TYPE_D2H            (1 << 7)
#define USB_REQUEST_TYPE_CLASS          (1 << 5)

#define USB_REQUEST_TO_INTERFACE        (1 << 0)

// General
#define USB_REQUEST_GET_STATUS          0
#define USB_REQUEST_SET_ADDRESS         5
#define USB_REQUEST_GET_DESCRIPTOR      6
#define USB_REQUEST_SET_CONFIGURATION   9

// USB HID
#define USB_REQUEST_SET_IDLE            0x0A

#define USB_DESCRIPTOR_DEVICE           1
#define USB_DESCRIPTOR_CONFIGURATION    2
#define USB_DESCRIPTOR_STRING           3
#define USB_DESCRIPTOR_INTERFACE        4
#define USB_DESCRIPTOR_ENDPOINT         5

struct usb_request {
    uint8_t type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
};

#define USB_TRANSFER_COMPLETE           (1 << 0)
#define USB_TRANSFER_SUCCESS            (1 << 1)

struct usb_transfer {
    struct usb_request *request;
    uint8_t endp;
    void *data;
    size_t length;
    uint32_t flags;
};

struct usb_desc_configuration {
    uint8_t length;
    uint8_t type;
    uint16_t total_length;
    uint8_t num_interfaces;
    uint8_t configuration_value;
    uint8_t idx_configuration;
    uint8_t attributes;
    uint8_t max_power;
} __attribute__((packed));

struct usb_desc_interface {
    uint8_t length;
    uint8_t type;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t num_endpoints;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t idx_interface;
} __attribute__((packed));

struct usb_desc_endpoint {
    uint8_t length;
    uint8_t type;
    uint8_t address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
} __attribute__((packed));

struct usb_desc_string {
    uint8_t len;
    uint8_t type;
    uint16_t data[0];
} __attribute__((packed));

struct usb_desc_device {
    uint8_t length;
    uint8_t type;
    uint16_t bcd_usb;
    uint8_t dev_class;
    uint8_t dev_subclass;
    uint8_t dev_protocol;
    uint8_t max_packet_zero;
    // After 8 bytes
    uint16_t id_vendor;
    uint16_t id_product;
    uint16_t bcd_device;
    uint8_t idx_manufacturer;
    uint8_t idx_product;
    uint8_t idx_serial_number;
    uint8_t num_configurations;
} __attribute__((packed));