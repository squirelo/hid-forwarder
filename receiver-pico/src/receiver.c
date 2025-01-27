#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define HCI_ACL_PAYLOAD_SIZE before any BTstack includes
#define HCI_ACL_PAYLOAD_SIZE 1024

// Add CYW43 include at the top with other includes
#include "pico/cyw43_arch.h"

#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"
#include "hardware/sync.h"
#include "pico/stdio.h"

// BLE headers
#include "btstack_run_loop.h"
#include "btstack_config.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "ble/sm.h"
#include "btstack.h"
#include "btstack_event.h"

// Correct includes for Pico W
#include "pico/stdlib.h"


// Nordic UART Service UUID
static const uint8_t nordic_uart_service_uuid[] = {
    0x9E, 0x40, 0x00, 0x00, 0xB5, 0xA3, 0xF3, 0x93,
    0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E
};

// Nordic UART RX UUID
static const uint8_t nordic_uart_rx_uuid[] = {
    0x9E, 0x40, 0x00, 0x02, 0xB5, 0xA3, 0xF3, 0x93,
    0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E
};

// Nordic UART TX UUID
static const uint8_t nordic_uart_tx_uuid[] = {
    0x9E, 0x40, 0x00, 0x03, 0xB5, 0xA3, 0xF3, 0x93,
    0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E
};

#define PERSISTED_CONFIG_SIZE 4096
#define CONFIG_OFFSET_IN_FLASH (PICO_FLASH_SIZE_BYTES - 16384)
#define FLASH_CONFIG_IN_MEMORY (((uint8_t*) XIP_BASE) + CONFIG_OFFSET_IN_FLASH)

#define CONFIG_VERSION 1
#define PROTOCOL_VERSION 1

#define SERIAL_UART uart1
#define SERIAL_BAUDRATE 921600
#define SERIAL_TX_PIN 4
#define SERIAL_RX_PIN 5
#define SERIAL_MAX_PACKET_SIZE 512

// Add missing definitions
#define NOUR_DESCRIPTORS 4

typedef struct __attribute__((packed)) {
    uint8_t config_version;
    uint8_t our_descriptor_number;
    uint8_t reserved[14];
    uint32_t crc;
} config_t;

_Static_assert(sizeof(config_t) == 20);

config_t config = {
    .config_version = CONFIG_VERSION,
    .our_descriptor_number = 2,
    .reserved = { 0 },
    .crc = 0,
};

// Forward declarations
typedef void (*msg_recv_cb_t)(const uint8_t* data, uint16_t len);
uint32_t crc32(const uint8_t* data, size_t len);  // Just declare it, don't implement
void serial_task(void);

// Add global variable
static uint8_t our_descriptor_number = 0;

void persist_config() {
    static uint8_t buffer[PERSISTED_CONFIG_SIZE];

    config.crc = crc32((uint8_t*) &config, sizeof(config_t) - 4);
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, &config, sizeof(config_t));
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(CONFIG_OFFSET_IN_FLASH, PERSISTED_CONFIG_SIZE);
    flash_range_program(CONFIG_OFFSET_IN_FLASH, buffer, PERSISTED_CONFIG_SIZE);
    restore_interrupts(ints);
}

bool config_ok(config_t* c) {
    if (crc32((uint8_t*) c, sizeof(config_t) - 4) != c->crc) {
        return false;
    }
    if (c->config_version != CONFIG_VERSION) {
        return false;
    }
    return true;
}

void config_init() {
    config.crc = crc32((uint8_t*) &config, sizeof(config_t) - 4);
    if (!config_ok((config_t*) FLASH_CONFIG_IN_MEMORY)) {
        return;
    }
    memcpy(&config, FLASH_CONFIG_IN_MEMORY, sizeof(config_t));
}

void serial_init() {
    uart_init(SERIAL_UART, SERIAL_BAUDRATE);
    uart_set_translate_crlf(SERIAL_UART, false);
    gpio_set_function(SERIAL_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(SERIAL_RX_PIN, GPIO_FUNC_UART);
}

// BLE connection handle
static hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
static uint16_t uart_service_handle;
static uint16_t uart_rx_characteristic_handle;
static uint16_t uart_tx_characteristic_handle;
static uint8_t profile_data[512];
static btstack_packet_callback_registration_t hci_event_callback_registration;

// Simplified BLE service and characteristic handles
static uint16_t service_handle;
static uint16_t rx_char_handle;
static uint16_t tx_char_handle;
static uint8_t characteristic_data = 0;

// Add these callback registrations at the top with other static declarations
static btstack_packet_callback_registration_t sm_event_callback_registration;

// Add the SM packet handler
static void sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
            
        case SM_EVENT_PAIRING_COMPLETE:
            switch (sm_event_pairing_complete_get_status(packet)) {
                case ERROR_CODE_SUCCESS:
                    printf("Pairing complete, success\n");
                    break;
                case ERROR_CODE_CONNECTION_TIMEOUT:
                    printf("Pairing failed, timeout\n");
                    break;
                case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                    printf("Pairing failed, disconnected\n");
                    break;
                case ERROR_CODE_AUTHENTICATION_FAILURE:
                    printf("Pairing failed, reason = %u\n", 
                        sm_event_pairing_complete_get_reason(packet));
                    break;
                default:
                    break;
            }
            break;
    }
}

// Callback for GATT events
static void gatt_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CAN_SEND_NOW:
            // Handle notification sending here
            break;
            
        case ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE:
            // Handle indication complete
            break;
    }
}

// Callback for read requests
static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size) {
    if (att_handle == rx_char_handle) {
        if (buffer) {
            buffer[0] = characteristic_data;
        }
        return 1;
    }
    return 0;
}

// Callback for write requests
static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size) {
    if (att_handle == tx_char_handle) {
        characteristic_data = buffer[0];
        printf("Received write: 0x%02x\n", characteristic_data);
        
        // Toggle LED state when data is received
        static bool led_state = false;
        led_state = !led_state;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
        
        return 0;
    }
    return 0;
}

// Add Device Information Service UUID
static const uint8_t device_information_service_uuid[] = {0x0A, 0x18};  // 0x180A

// Add manufacturer name string
static const char manufacturer_name[] = "Pico BLE Bridge";
static const char model_number[] = "PicoW-1.0";
static const char firmware_rev[] = "1.0.0";

// Update advertisement data to match Bluefruit format
static uint8_t adv_data[] = {
    // Flags
    0x02, 0x01, 0x06,  // General discoverable, BR/EDR not supported
    
    // Complete Local Name
    0x0B, 0x09, 'P', 'i', 'c', 'o', 'f', 'r', 'u', 'i', 't', ' ',
    
    // 128-bit Service UUID (Adafruit UART)
    0x11, 0x06,  // Length and Complete List of 128-bit Service UUIDs
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x00, 0x00, 0x40, 0x9E
};

static uint8_t scan_resp_data[] = {
    // Complete Local Name (repeated in scan response)
    0x0B, 0x09, 'P', 'i', 'c', 'o', 'f', 'r', 'u', 'i', 't', ' '
};

// Add HID Report Descriptor
static const uint8_t hid_report_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //   Usage (X)
    0x09, 0x31,        //   Usage (Y)
    0x09, 0x32,        //   Usage (Z)
    0x09, 0x35,        //   Usage (Rz)
    0x09, 0x33,        //   Usage (Rx)
    0x09, 0x34,        //   Usage (Ry)
    0x15, 0x81,        //   Logical Minimum (-127)
    0x25, 0x7F,        //   Logical Maximum (127)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x06,        //   Report Count (6)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x39,        //   Usage (Hat switch)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x07,        //   Logical Maximum (7)
    0x35, 0x00,        //   Physical Minimum (0)
    0x46, 0x3B, 0x01,  //   Physical Maximum (315)
    0x65, 0x14,        //   Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (0x01)
    0x29, 0x20,        //   Usage Maximum (0x20)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x20,        //   Report Count (32)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0xC0               // End Collection
};

// Add HID Service and Characteristic UUIDs
static const uint8_t hid_service_uuid[] = {0x12, 0x18};  // 0x1812
static const uint8_t hid_report_map_uuid[] = {0x4B, 0x2A};  // 0x2A4B
static const uint8_t hid_report_uuid[] = {0x4D, 0x2A};  // 0x2A4D
static const uint8_t hid_protocol_mode_uuid[] = {0x4E, 0x2A};  // 0x2A4E

// Add gamepad report structure
typedef struct __attribute__((packed)) {
    int8_t x;
    int8_t y;
    int8_t z;
    int8_t rz;
    int8_t rx;
    int8_t ry;
    uint8_t hat;
    uint32_t buttons;
} gamepad_report_t;

static gamepad_report_t gamepad_state = {0};

// Add security requirements
#define SECURITY_ENABLED 1
#define REQUIRE_BONDING 1

// Update ble_init function to include security
void ble_init(void) {
    printf("Starting BLE initialization...\n");

    // Initialize CYW43 driver
    if (cyw43_arch_init()) {
        printf("Failed to initialize CYW43439\n");
        return;
    }

    // Configure LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);  // Turn LED off initially

    // Initialize L2CAP
    l2cap_init();
    printf("L2CAP initialized\n");

    // Initialize Security Manager
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
    printf("Security Manager initialized\n");

    // Register for Security Manager events
    sm_event_callback_registration.callback = &sm_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // Setup ATT DB
    att_db_util_init();
    printf("ATT DB initialized\n");

    // Add Device Information Service
    uint16_t dev_info_handle = att_db_util_add_service_uuid16(0x180A);
    printf("Added Device Information Service: handle 0x%04x\n", dev_info_handle);
    
    att_db_util_add_characteristic_uuid16(0x2A29, ATT_PROPERTY_READ, ATT_SECURITY_NONE, ATT_SECURITY_NONE,
        (uint8_t*)manufacturer_name, strlen(manufacturer_name));
    att_db_util_add_characteristic_uuid16(0x2A24, ATT_PROPERTY_READ, ATT_SECURITY_NONE, ATT_SECURITY_NONE,
        (uint8_t*)model_number, strlen(model_number));
    att_db_util_add_characteristic_uuid16(0x2A26, ATT_PROPERTY_READ, ATT_SECURITY_NONE, ATT_SECURITY_NONE,
        (uint8_t*)firmware_rev, strlen(firmware_rev));

    // Add Nordic UART Service with Adafruit-compatible properties
    uart_service_handle = att_db_util_add_service_uuid128(nordic_uart_service_uuid);
    
    uart_tx_characteristic_handle = att_db_util_add_characteristic_uuid128(
        nordic_uart_tx_uuid,
        ATT_PROPERTY_NOTIFY | ATT_PROPERTY_READ,  // Add READ property
        ATT_SECURITY_NONE,
        ATT_SECURITY_NONE,
        NULL,
        0
    );
    
    uart_rx_characteristic_handle = att_db_util_add_characteristic_uuid128(
        nordic_uart_rx_uuid,
        ATT_PROPERTY_WRITE | ATT_PROPERTY_WRITE_WITHOUT_RESPONSE | ATT_PROPERTY_READ,  // Add READ property
        ATT_SECURITY_NONE,
        ATT_SECURITY_NONE,
        NULL,
        0
    );

    // Add HID Service
    uint16_t hid_handle = att_db_util_add_service_uuid16(0x1812);
    printf("Added HID Service: handle 0x%04x\n", hid_handle);
    
    att_db_util_add_characteristic_uuid16(0x2A4B, ATT_PROPERTY_READ,
        ATT_SECURITY_NONE, ATT_SECURITY_NONE,
        (uint8_t*)hid_report_descriptor, sizeof(hid_report_descriptor));
    att_db_util_add_characteristic_uuid16(0x2A4D,
        ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY,
        ATT_SECURITY_NONE, ATT_SECURITY_NONE,
        (uint8_t*)&gamepad_state, sizeof(gamepad_state));

    // Get the generated profile
    uint8_t * profile_data = att_db_util_get_address();
    uint16_t profile_data_len = att_db_util_get_size();
    printf("Generated ATT DB with %d bytes\n", profile_data_len);

    // Initialize ATT Server with the generated profile
    att_server_init(profile_data, att_read_callback, att_write_callback);    
    printf("ATT Server initialized\n");

    // Set advertisement parameters
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0060;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    printf("Advertisement parameters set\n");

    // Set advertisement data
    gap_advertisements_set_data(sizeof(adv_data), adv_data);
    gap_scan_response_set_data(sizeof(scan_resp_data), scan_resp_data);
    printf("Advertisement data set\n");

    // Register for ATT events
    att_server_register_packet_handler(gatt_event_handler);

    // Turn on Bluetooth and enable advertisements
    hci_power_control(HCI_POWER_ON);
    gap_advertisements_enable(1);

    printf("BLE initialization completed, advertisements enabled\n");
}

// Update main function to register security handler
int main() {
    stdio_init_all();

    ble_init();

    // Register for Security Manager events
    sm_event_callback_registration.callback = &sm_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // BTstack run loop
    btstack_run_loop_execute();

    return 0;
}