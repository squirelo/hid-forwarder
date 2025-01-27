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
    0x6E, 0x40, 0x00, 0x01, 0xB5, 0xA3, 0xF3, 0x93,
    0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E
};

// Nordic UART RX UUID
static const uint8_t nordic_uart_rx_uuid[] = {
    0x6E, 0x40, 0x00, 0x02, 0xB5, 0xA3, 0xF3, 0x93,
    0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E
};

// Nordic UART TX UUID
static const uint8_t nordic_uart_tx_uuid[] = {
    0x6E, 0x40, 0x00, 0x03, 0xB5, 0xA3, 0xF3, 0x93,
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
        return 0;
    }
    return 0;
}

// Add new advertising data definitions
static uint8_t adv_data[] = {
    // Flags (3 bytes)
    0x02, 0x01, 0x06,
    // Service UUID (17 bytes)
    0x11, 0x07, 
    0x6E, 0x40, 0x00, 0x01, 0xB5, 0xA3, 0xF3, 0x93,
    0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E,
    // Local name (7 bytes)
    0x06, 0x09, 'P', 'i', 'c', 'o', 'W'
};

static uint8_t scan_resp_data[] = {
    // Complete local name
    0x06, 0x09, 'P', 'i', 'c', 'o', 'W'
};

// Update ble_init function
void ble_init(void) {
    printf("Starting BLE initialization...\n");

    // Initialize CYW43 driver
    if (cyw43_arch_init()) {
        printf("Failed to initialize CYW43439\n");
        return;
    }

    // Initialize BTstack with simplified setup
    l2cap_init();
    sm_init();

    // Setup ATT DB
    att_db_util_init();

    // Add Nordic UART Service
    uart_service_handle = att_db_util_add_service_uuid128(nordic_uart_service_uuid);

    // Add RX Characteristic
    uart_rx_characteristic_handle = att_db_util_add_characteristic_uuid128(
        nordic_uart_rx_uuid,
        ATT_PROPERTY_WRITE | ATT_PROPERTY_WRITE_WITHOUT_RESPONSE,
        ATT_SECURITY_NONE,
        ATT_SECURITY_NONE,
        NULL,
        0);

    // Add TX Characteristic
    uart_tx_characteristic_handle = att_db_util_add_characteristic_uuid128(
        nordic_uart_tx_uuid,
        ATT_PROPERTY_NOTIFY,
        ATT_SECURITY_NONE,
        ATT_SECURITY_NONE,
        NULL,
        0);

    // Initialize ATT Server
    att_server_init(profile_data, att_read_callback, att_write_callback);    

    // Setup advertisements with complete parameters
    gap_advertisements_set_params(0x0100,    // adv_int_min
                                0x0100,      // adv_int_max
                                0,           // ADV_IND type (0 in BTstack)
                                0,           // Public address type
                                NULL,        // No direct address
                                0x07,        // Primary advertising channels (all)
                                0);          // No filter policy

    // Set advertisement data and scan response
    gap_advertisements_set_data(sizeof(adv_data), adv_data);
    gap_scan_response_set_data(sizeof(scan_resp_data), scan_resp_data);
    
    // Enable advertisements
    gap_advertisements_enable(1);

    // Register for ATT events
    att_server_register_packet_handler(gatt_event_handler);

    // Turn on Bluetooth
    hci_power_control(HCI_POWER_ON);
}

int main() {
    stdio_init_all();

    ble_init();

    // BTstack run loop
    btstack_run_loop_execute();

    return 0;
}