#include "tusb.h"

#include "descriptors.h"
#include "globals.h"

#define USB_VID 0xCAFE
#define USB_PID 0xBAF5

typedef struct {
    const uint8_t* configuration_descriptor;
    const uint8_t* report_descriptor;
    uint16_t vid;
    uint16_t pid;
} our_descriptor_t;

tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x00,

    .bNumConfigurations = 0x01,
};

#define REPORT_ID_MOUSE 1
#define REPORT_ID_KEYBOARD 2
#define REPORT_ID_CONSUMER 3
#define REPORT_ID_LEDS 98
#define REPORT_ID_MULTIPLIER 99
#define RESOLUTION_MULTIPLIER 120

const uint8_t our_report_descriptor_kb_mouse[] = {
    0x05, 0x01,             // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,             // Usage (Mouse)
    0xA1, 0x01,             // Collection (Application)
    0x05, 0x01,             //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,             //   Usage (Mouse)
    0xA1, 0x02,             //   Collection (Logical)
    0x85, REPORT_ID_MOUSE,  //     Report ID (REPORT_ID_MOUSE)
    0x09, 0x01,             //     Usage (Pointer)
    0xA1, 0x00,             //     Collection (Physical)
    0x05, 0x09,             //       Usage Page (Button)
    0x19, 0x01,             //       Usage Minimum (0x01)
    0x29, 0x08,             //       Usage Maximum (0x08)
    0x95, 0x08,             //       Report Count (8)
    0x75, 0x01,             //       Report Size (1)
    0x25, 0x01,             //       Logical Maximum (1)
    0x81, 0x02,             //       Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,             //       Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,             //       Usage (X)
    0x09, 0x31,             //       Usage (Y)
    0x95, 0x02,             //       Report Count (2)
    0x75, 0x10,             //       Report Size (16)
    0x16, 0x00, 0x80,       //       Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,       //       Logical Maximum (32767)
    0x81, 0x06,             //       Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xA1, 0x02,             //       Collection (Logical)
                            /*
                                0x85, REPORT_ID_MULTIPLIER,   //         Report ID (REPORT_ID_MULTIPLIER)
                                0x09, 0x48,                   //         Usage (Resolution Multiplier)
                                0x95, 0x01,                   //         Report Count (1)
                                0x75, 0x02,                   //         Report Size (2)
                                0x15, 0x00,                   //         Logical Minimum (0)
                                0x25, 0x01,                   //         Logical Maximum (1)
                                0x35, 0x01,                   //         Physical Minimum (1)
                                0x45, RESOLUTION_MULTIPLIER,  //         Physical Maximum (RESOLUTION_MULTIPLIER)
                                0xB1, 0x02,                   //         Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                                0x85, REPORT_ID_MOUSE,        //         Report ID (REPORT_ID_MOUSE)
                            */
    0x09, 0x38,             //         Usage (Wheel)
    0x35, 0x00,             //         Physical Minimum (0)
    0x45, 0x00,             //         Physical Maximum (0)
    0x16, 0x00, 0x80,       //         Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,       //         Logical Maximum (32767)
    0x75, 0x10,             //         Report Size (16)
    0x81, 0x06,             //         Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                   //       End Collection
    0xA1, 0x02,             //       Collection (Logical)
                            /*
                                0x85, REPORT_ID_MULTIPLIER,   //         Report ID (REPORT_ID_MULTIPLIER)
                                0x09, 0x48,                   //         Usage (Resolution Multiplier)
                                0x75, 0x02,                   //         Report Size (2)
                                0x15, 0x00,                   //         Logical Minimum (0)
                                0x25, 0x01,                   //         Logical Maximum (1)
                                0x35, 0x01,                   //         Physical Minimum (1)
                                0x45, RESOLUTION_MULTIPLIER,  //         Physical Maximum (RESOLUTION_MULTIPLIER)
                                0xB1, 0x02,                   //         Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                                0x35, 0x00,                   //         Physical Minimum (0)
                                0x45, 0x00,                   //         Physical Maximum (0)
                                0x75, 0x04,                   //         Report Size (4)
                                0xB1, 0x03,                   //         Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                                0x85, REPORT_ID_MOUSE,        //         Report ID (REPORT_ID_MOUSE)
                            */
    0x05, 0x0C,             //         Usage Page (Consumer)
    0x16, 0x00, 0x80,       //         Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,       //         Logical Maximum (32767)
    0x75, 0x10,             //         Report Size (16)
    0x0A, 0x38, 0x02,       //         Usage (AC Pan)
    0x81, 0x06,             //         Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                   //       End Collection
    0xC0,                   //     End Collection
    0xC0,                   //   End Collection
    0xC0,                   // End Collection

    0x05, 0x01,                // Usage Page (Generic Desktop Ctrls)
    0x09, 0x06,                // Usage (Keyboard)
    0xA1, 0x01,                // Collection (Application)
    0x85, REPORT_ID_KEYBOARD,  //   Report ID (REPORT_ID_KEYBOARD)
    0x05, 0x07,                //   Usage Page (Kbrd/Keypad)
    0x19, 0xE0,                //   Usage Minimum (0xE0)
    0x29, 0xE7,                //   Usage Maximum (0xE7)
    0x15, 0x00,                //   Logical Minimum (0)
    0x25, 0x01,                //   Logical Maximum (1)
    0x75, 0x01,                //   Report Size (1)
    0x95, 0x08,                //   Report Count (8)
    0x81, 0x02,                //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x19, 0x04,                //   Usage Minimum (0x04)
    0x29, 0x73,                //   Usage Maximum (0x73)
    0x95, 0x70,                //   Report Count (112)
    0x81, 0x02,                //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x19, 0x87,                //   Usage Minimum (0x87)
    0x29, 0x8B,                //   Usage Maximum (0x8B)
    0x95, 0x05,                //   Report Count (5)
    0x81, 0x02,                //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x90,                //   Usage (0x90)
    0x09, 0x91,                //   Usage (0x91)
    0x95, 0x02,                //   Report Count (2)
    0x81, 0x02,                //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01,                //   Report Count (1)
    0x81, 0x03,                //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, REPORT_ID_LEDS,      //   Report ID (REPORT_ID_LEDS)
    0x05, 0x08,                //   Usage Page (LEDs)
    0x95, 0x05,                //   Report Count (5)
    0x19, 0x01,                //   Usage Minimum (Num Lock)
    0x29, 0x05,                //   Usage Maximum (Kana)
    0x91, 0x02,                //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x01,                //   Report Count (1)
    0x75, 0x03,                //   Report Size (3)
    0x91, 0x03,                //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,                      // End Collection

    0x05, 0x0C,                // Usage Page (Consumer)
    0x09, 0x01,                // Usage (Consumer Control)
    0xA1, 0x01,                // Collection (Application)
    0x85, REPORT_ID_CONSUMER,  //   Report ID (REPORT_ID_CONSUMER)
    0x15, 0x00,                //   Logical Minimum (0)
    0x25, 0x01,                //   Logical Maximum (1)
    0x09, 0xB5,                //   Usage (Scan Next Track)
    0x09, 0xB6,                //   Usage (Scan Previous Track)
    0x09, 0xB7,                //   Usage (Stop)
    0x09, 0xCD,                //   Usage (Play/Pause)
    0x09, 0xE2,                //   Usage (Mute)
    0x09, 0xE9,                //   Usage (Volume Increment)
    0x09, 0xEA,                //   Usage (Volume Decrement)
    0x75, 0x01,                //   Report Size (1)
    0x95, 0x07,                //   Report Count (7)
    0x81, 0x02,                //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0B,                //   Usage Page (Telephony)
    0x09, 0x2F,                //   Usage (Phone Mute)
    0x95, 0x01,                //   Report Count (1)
    0x81, 0x02,                //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                      // End Collection
};

const uint8_t our_report_descriptor_absolute[] = {
    0x05, 0x01,             // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,             // Usage (Mouse)
    0xA1, 0x01,             // Collection (Application)
    0x05, 0x01,             //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,             //   Usage (Mouse)
    0xA1, 0x02,             //   Collection (Logical)
    0x85, REPORT_ID_MOUSE,  //     Report ID (REPORT_ID_MOUSE)
    0x09, 0x01,             //     Usage (Pointer)
    0xA1, 0x00,             //     Collection (Physical)
    0x05, 0x09,             //       Usage Page (Button)
    0x19, 0x01,             //       Usage Minimum (0x01)
    0x29, 0x08,             //       Usage Maximum (0x08)
    0x95, 0x08,             //       Report Count (8)
    0x75, 0x01,             //       Report Size (1)
    0x25, 0x01,             //       Logical Maximum (1)
    0x81, 0x02,             //       Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,             //       Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,             //       Usage (X)
    0x09, 0x31,             //       Usage (Y)
    0x95, 0x02,             //       Report Count (2)
    0x75, 0x10,             //       Report Size (16)
    0x16, 0x00, 0x00,       //       Logical Minimum (0)
    0x26, 0xFF, 0x7F,       //       Logical Maximum (32767)
    0x81, 0x02,             //       Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xA1, 0x02,             //       Collection (Logical)
                            /*
                                0x85, REPORT_ID_MULTIPLIER,   //         Report ID (REPORT_ID_MULTIPLIER)
                                0x09, 0x48,                   //         Usage (Resolution Multiplier)
                                0x95, 0x01,                   //         Report Count (1)
                                0x75, 0x02,                   //         Report Size (2)
                                0x15, 0x00,                   //         Logical Minimum (0)
                                0x25, 0x01,                   //         Logical Maximum (1)
                                0x35, 0x01,                   //         Physical Minimum (1)
                                0x45, RESOLUTION_MULTIPLIER,  //         Physical Maximum (RESOLUTION_MULTIPLIER)
                                0xB1, 0x02,                   //         Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                                0x85, REPORT_ID_MOUSE,        //         Report ID (REPORT_ID_MOUSE)
                            */
    0x09, 0x38,             //         Usage (Wheel)
    0x35, 0x00,             //         Physical Minimum (0)
    0x45, 0x00,             //         Physical Maximum (0)
    0x16, 0x00, 0x80,       //         Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,       //         Logical Maximum (32767)
    0x75, 0x10,             //         Report Size (16)
    0x81, 0x06,             //         Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                   //       End Collection
    0xA1, 0x02,             //       Collection (Logical)
                            /*
                                0x85, REPORT_ID_MULTIPLIER,   //         Report ID (REPORT_ID_MULTIPLIER)
                                0x09, 0x48,                   //         Usage (Resolution Multiplier)
                                0x75, 0x02,                   //         Report Size (2)
                                0x15, 0x00,                   //         Logical Minimum (0)
                                0x25, 0x01,                   //         Logical Maximum (1)
                                0x35, 0x01,                   //         Physical Minimum (1)
                                0x45, RESOLUTION_MULTIPLIER,  //         Physical Maximum (RESOLUTION_MULTIPLIER)
                                0xB1, 0x02,                   //         Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                                0x35, 0x00,                   //         Physical Minimum (0)
                                0x45, 0x00,                   //         Physical Maximum (0)
                                0x75, 0x04,                   //         Report Size (4)
                                0xB1, 0x03,                   //         Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
                                0x85, REPORT_ID_MOUSE,        //         Report ID (REPORT_ID_MOUSE)
                            */
    0x05, 0x0C,             //         Usage Page (Consumer)
    0x16, 0x00, 0x80,       //         Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,       //         Logical Maximum (32767)
    0x75, 0x10,             //         Report Size (16)
    0x0A, 0x38, 0x02,       //         Usage (AC Pan)
    0x81, 0x06,             //         Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                   //       End Collection
    0xC0,                   //     End Collection
    0xC0,                   //   End Collection
    0xC0,                   // End Collection

    0x05, 0x01,                // Usage Page (Generic Desktop Ctrls)
    0x09, 0x06,                // Usage (Keyboard)
    0xA1, 0x01,                // Collection (Application)
    0x85, REPORT_ID_KEYBOARD,  //   Report ID (REPORT_ID_KEYBOARD)
    0x05, 0x07,                //   Usage Page (Kbrd/Keypad)
    0x19, 0xE0,                //   Usage Minimum (0xE0)
    0x29, 0xE7,                //   Usage Maximum (0xE7)
    0x15, 0x00,                //   Logical Minimum (0)
    0x25, 0x01,                //   Logical Maximum (1)
    0x75, 0x01,                //   Report Size (1)
    0x95, 0x08,                //   Report Count (8)
    0x81, 0x02,                //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x19, 0x04,                //   Usage Minimum (0x04)
    0x29, 0x73,                //   Usage Maximum (0x73)
    0x95, 0x70,                //   Report Count (112)
    0x81, 0x02,                //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x19, 0x87,                //   Usage Minimum (0x87)
    0x29, 0x8B,                //   Usage Maximum (0x8B)
    0x95, 0x05,                //   Report Count (5)
    0x81, 0x02,                //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x90,                //   Usage (0x90)
    0x09, 0x91,                //   Usage (0x91)
    0x95, 0x02,                //   Report Count (2)
    0x81, 0x02,                //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01,                //   Report Count (1)
    0x81, 0x03,                //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, REPORT_ID_LEDS,      //   Report ID (REPORT_ID_LEDS)
    0x05, 0x08,                //   Usage Page (LEDs)
    0x95, 0x05,                //   Report Count (5)
    0x19, 0x01,                //   Usage Minimum (Num Lock)
    0x29, 0x05,                //   Usage Maximum (Kana)
    0x91, 0x02,                //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x01,                //   Report Count (1)
    0x75, 0x03,                //   Report Size (3)
    0x91, 0x03,                //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,                      // End Collection

    0x05, 0x0C,                // Usage Page (Consumer)
    0x09, 0x01,                // Usage (Consumer Control)
    0xA1, 0x01,                // Collection (Application)
    0x85, REPORT_ID_CONSUMER,  //   Report ID (REPORT_ID_CONSUMER)
    0x15, 0x00,                //   Logical Minimum (0)
    0x25, 0x01,                //   Logical Maximum (1)
    0x09, 0xB5,                //   Usage (Scan Next Track)
    0x09, 0xB6,                //   Usage (Scan Previous Track)
    0x09, 0xB7,                //   Usage (Stop)
    0x09, 0xCD,                //   Usage (Play/Pause)
    0x09, 0xE2,                //   Usage (Mute)
    0x09, 0xE9,                //   Usage (Volume Increment)
    0x09, 0xEA,                //   Usage (Volume Decrement)
    0x75, 0x01,                //   Report Size (1)
    0x95, 0x07,                //   Report Count (7)
    0x81, 0x02,                //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0B,                //   Usage Page (Telephony)
    0x09, 0x2F,                //   Usage (Phone Mute)
    0x95, 0x01,                //   Report Count (1)
    0x81, 0x02,                //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                      // End Collection
};

const uint8_t our_report_descriptor_horipad[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x35, 0x00,        //   Physical Minimum (0)
    0x45, 0x01,        //   Physical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x0E,        //   Report Count (14)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (0x01)
    0x29, 0x0E,        //   Usage Maximum (0x0E)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
    0x25, 0x07,        //   Logical Maximum (7)
    0x46, 0x3B, 0x01,  //   Physical Maximum (315)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x65, 0x14,        //   Unit (System: English Rotation, Length: Centimeter)
    0x09, 0x39,        //   Usage (Hat switch)
    0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x65, 0x00,        //   Unit (None)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x46, 0xFF, 0x00,  //   Physical Maximum (255)
    0x09, 0x30,        //   Usage (X)
    0x09, 0x31,        //   Usage (Y)
    0x09, 0x32,        //   Usage (Z)
    0x09, 0x35,        //   Usage (Rz)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x04,        //   Report Count (4)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // End Collection
};

uint8_t const our_report_descriptor_ps4[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x09, 0x30,        //   Usage (X)
    0x09, 0x31,        //   Usage (Y)
    0x09, 0x32,        //   Usage (Z)
    0x09, 0x35,        //   Usage (Rz)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x04,        //   Report Count (4)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x39,        //   Usage (Hat switch)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x07,        //   Logical Maximum (7)
    0x35, 0x00,        //   Physical Minimum (0)
    0x46, 0x3B, 0x01,  //   Physical Maximum (315)
    0x65, 0x14,        //   Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x65, 0x00,        //   Unit (None)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (0x01)
    0x29, 0x0E,        //   Usage Maximum (0x0E)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x0E,        //   Report Count (14)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x20,        //   Usage (0x20)
    0x75, 0x06,        //   Report Size (6)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x33,        //   Usage (Rx)
    0x09, 0x34,        //   Usage (Ry)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x21,        //   Usage (0x21)
    0x95, 0x36,        //   Report Count (54)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x05,        //   Report ID (5)
    0x09, 0x22,        //   Usage (0x22)
    0x95, 0x1F,        //   Report Count (31)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x03,        //   Report ID (3)
    0x0A, 0x21, 0x27,  //   Usage (0x2721)
    0x95, 0x2F,        //   Report Count (47)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x06, 0x80, 0xFF,  //   Usage Page (Vendor Defined 0xFF80)
    0x85, 0xE0,        //   Report ID (-32)
    0x09, 0x57,        //   Usage (0x57)
    0x95, 0x02,        //   Report Count (2)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              // End Collection
    0x06, 0xF0, 0xFF,  // Usage Page (Vendor Defined 0xFFF0)
    0x09, 0x40,        // Usage (0x40)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0xF0,        //   Report ID (-16)
    0x09, 0x47,        //   Usage (0x47)
    0x95, 0x3F,        //   Report Count (63)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF1,        //   Report ID (-15)
    0x09, 0x48,        //   Usage (0x48)
    0x95, 0x3F,        //   Report Count (63)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF2,        //   Report ID (-14)
    0x09, 0x49,        //   Usage (0x49)
    0x95, 0x0F,        //   Report Count (15)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF3,        //   Report ID (-13)
    0x0A, 0x01, 0x47,  //   Usage (0x4701)
    0x95, 0x07,        //   Report Count (7)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              // End Collection
};

uint8_t const our_report_descriptor_stadia[] = {
    0x05, 0x01,                    // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,                    // Usage (Game Pad)
    0xA1, 0x01,                    // Collection (Application)
    0x85, 0x03,                    //   Report ID (3)
    0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
    0x75, 0x04,                    //   Report Size (4)
    0x95, 0x01,                    //   Report Count (1)
    0x25, 0x07,                    //   Logical Maximum (7)
    0x46, 0x3B, 0x01,              //   Physical Maximum (315)
    0x65, 0x14,                    //   Unit (System: English Rotation, Length: Centimeter)
    0x09, 0x39,                    //   Usage (Hat switch)
    0x81, 0x42,                    //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x45, 0x00,                    //   Physical Maximum (0)
    0x65, 0x00,                    //   Unit (None)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x04,                    //   Report Count (4)
    0x81, 0x01,                    //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,                    //   Usage Page (Button)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x0F,                    //   Report Count (15)
    0x09, 0x12,                    //   Usage (0x12)
    0x09, 0x11,                    //   Usage (0x11)
    0x09, 0x14,                    //   Usage (0x14)
    0x09, 0x13,                    //   Usage (0x13)
    0x09, 0x0D,                    //   Usage (0x0D)
    0x09, 0x0C,                    //   Usage (0x0C)
    0x09, 0x0B,                    //   Usage (0x0B)
    0x09, 0x0F,                    //   Usage (0x0F)
    0x09, 0x0E,                    //   Usage (0x0E)
    0x09, 0x08,                    //   Usage (0x08)
    0x09, 0x07,                    //   Usage (0x07)
    0x09, 0x05,                    //   Usage (0x05)
    0x09, 0x04,                    //   Usage (0x04)
    0x09, 0x02,                    //   Usage (0x02)
    0x09, 0x01,                    //   Usage (0x01)
    0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x01,                    //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
    0x15, 0x01,                    //   Logical Minimum (1)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x09, 0x01,                    //   Usage (Pointer)
    0xA1, 0x00,                    //   Collection (Physical)
    0x09, 0x30,                    //     Usage (X)
    0x09, 0x31,                    //     Usage (Y)
    0x75, 0x08,                    //     Report Size (8)
    0x95, 0x02,                    //     Report Count (2)
    0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                          //   End Collection
    0x09, 0x01,                    //   Usage (Pointer)
    0xA1, 0x00,                    //   Collection (Physical)
    0x09, 0x32,                    //     Usage (Z)
    0x09, 0x35,                    //     Usage (Rz)
    0x75, 0x08,                    //     Report Size (8)
    0x95, 0x02,                    //     Report Count (2)
    0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                          //   End Collection
    0x05, 0x02,                    //   Usage Page (Sim Ctrls)
    0x75, 0x08,                    //   Report Size (8)
    0x95, 0x02,                    //   Report Count (2)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x09, 0xC5,                    //   Usage (Brake)
    0x09, 0xC4,                    //   Usage (Accelerator)
    0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0C,                    //   Usage Page (Consumer)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x09, 0xE9,                    //   Usage (Volume Increment)
    0x09, 0xEA,                    //   Usage (Volume Decrement)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x02,                    //   Report Count (2)
    0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0xCD,                    //   Usage (Play/Pause)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x05,                    //   Report Count (5)
    0x81, 0x01,                    //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x05,                    //   Report ID (5)
    0x06, 0x0F, 0x00,              //   Usage Page (PID Page)
    0x09, 0x97,                    //   Usage (0x97)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x02,                    //   Report Count (2)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  //   Logical Maximum (65535)
    0x91, 0x02,                    //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,                          // End Collection
};

uint8_t const our_report_descriptor_xac_compat[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
    0x09, 0x30,        //   Usage (X)
    0x09, 0x31,        //   Usage (Y)
    0x09, 0x32,        //   Usage (Z)
    0x09, 0x35,        //   Usage (Rz)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x04,        //   Report Count (4)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x39,        //   Usage (Hat switch)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x07,        //   Logical Maximum (7)
    0x35, 0x00,        //   Physical Minimum (0)
    0x46, 0x3B, 0x01,  //   Physical Maximum (315)
    0x65, 0x14,        //   Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x65, 0x00,        //   Unit (None)
    0x45, 0x00,        //   Physical Maximum (0)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (0x01)
    0x29, 0x0C,        //   Usage Maximum (0x0C)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x0C,        //   Report Count (12)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // End Collection
};

uint8_t const config_report_descriptor[] = {
    0x06, 0x00, 0xFF,         // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x22,               // Usage (0x22)
    0xA1, 0x01,               // Collection (Application)
    0x85, REPORT_ID_CONFIG,   //   Report ID (REPORT_ID_CONFIG)
    0x09, 0x22,               //   Usage (0x22)
    0x75, 0x08,               //   Report Size (8)
    0x95, 0x3F,               //   Report Count (63)
    0xB1, 0x02,               //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, REPORT_ID_COMMAND,  //   Report ID (REPORT_ID_COMMAND)
    0x09, 0x22,               //   Usage (0x22)
    0x75, 0x08,               //   Report Size (8)
    0x95, 0x3F,               //   Report Count (63)
    0xB1, 0x02,               //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,                     // End Collection
};

const uint8_t configuration_descriptor0[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN, 0, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_KEYBOARD, sizeof(our_report_descriptor_kb_mouse), 0x81, CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, sizeof(config_report_descriptor), 0x83, CFG_TUD_HID_EP_BUFSIZE, 1),
};

const uint8_t configuration_descriptor1[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN, 0, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_KEYBOARD, sizeof(our_report_descriptor_absolute), 0x81, CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, sizeof(config_report_descriptor), 0x83, CFG_TUD_HID_EP_BUFSIZE, 1),
};

const uint8_t configuration_descriptor2[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN + TUD_HID_DESC_LEN, 0, 100),
    TUD_HID_INOUT_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(our_report_descriptor_horipad), 0x02, 0x81, CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, sizeof(config_report_descriptor), 0x83, CFG_TUD_HID_EP_BUFSIZE, 1),
};

const uint8_t configuration_descriptor3[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN, 0, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(our_report_descriptor_ps4), 0x81, CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, sizeof(config_report_descriptor), 0x83, CFG_TUD_HID_EP_BUFSIZE, 1),
};

const uint8_t configuration_descriptor4[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN + TUD_HID_DESC_LEN, 0, 100),
    TUD_HID_INOUT_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(our_report_descriptor_stadia), 0x02, 0x81, CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, sizeof(config_report_descriptor), 0x83, CFG_TUD_HID_EP_BUFSIZE, 1),
};

const uint8_t configuration_descriptor5[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN, 0, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(our_report_descriptor_xac_compat), 0x81, CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_HID_DESCRIPTOR(1, 0, HID_ITF_PROTOCOL_NONE, sizeof(config_report_descriptor), 0x83, CFG_TUD_HID_EP_BUFSIZE, 1),
};

our_descriptor_t our_descriptors[NOUR_DESCRIPTORS] = {
    {
        .configuration_descriptor = configuration_descriptor0,
        .report_descriptor = our_report_descriptor_kb_mouse,
        .vid = USB_VID,
        .pid = USB_PID,
    },
    {
        .configuration_descriptor = configuration_descriptor1,
        .report_descriptor = our_report_descriptor_absolute,
        .vid = USB_VID,
        .pid = USB_PID,
    },
    {
        .configuration_descriptor = configuration_descriptor2,
        .report_descriptor = our_report_descriptor_horipad,
        .vid = 0x0F0D,
        .pid = 0x00C1,
    },
    {
        .configuration_descriptor = configuration_descriptor3,
        .report_descriptor = our_report_descriptor_ps4,
        .vid = 0x054C,
        .pid = 0x1234,
    },
    {
        .configuration_descriptor = configuration_descriptor4,
        .report_descriptor = our_report_descriptor_stadia,
        .vid = 0x18D1,
        .pid = 0x9400,
    },
    {
        .configuration_descriptor = configuration_descriptor4,
        .report_descriptor = our_report_descriptor_xac_compat,
        .vid = USB_VID,
        .pid = USB_PID,
    },
};

char const* string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 },  // 0: is supported language is English (0x0409)
    "PlayAbility",                     // 1: Manufacturer
    "PlayAbility Receiver",                // 2: Product
};

uint8_t const* tud_descriptor_device_cb(void) {
    desc_device.idVendor = our_descriptors[our_descriptor_number].vid;
    desc_device.idProduct = our_descriptors[our_descriptor_number].pid;
    return (uint8_t const*) &desc_device;
}

uint8_t const* tud_hid_descriptor_report_cb(uint8_t itf) {
    if (itf == 0) {
        return our_descriptors[our_descriptor_number].report_descriptor;
    } else if (itf == 1) {
        return config_report_descriptor;
    }

    return NULL;
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    return our_descriptors[our_descriptor_number].configuration_descriptor;
}

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
            return NULL;

        const char* str = string_desc_arr[index];

        // Cap at max char
        chr_count = strlen(str);
        if (chr_count > 31)
            chr_count = 31;

        // Convert ASCII string into UTF-16
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}
