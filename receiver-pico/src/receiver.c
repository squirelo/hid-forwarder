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

// Add UART buffer and handler
#define RX_BUFFER_SIZE 100
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint16_t rx_buffer_len = 0;
static msg_recv_cb_t uart_rx_handler = NULL;

// Function to handle received data
static void handle_uart_rx() {
    if (uart_rx_handler) {
        uart_rx_handler(rx_buffer, rx_buffer_len);
        rx_buffer_len = 0;
    }
}

// Function to write data to connected devices
static void uart_write(const uint8_t* data, uint16_t len) {
    if (con_handle != HCI_CON_HANDLE_INVALID) {
        // Request to send notification
        att_server_request_can_send_now_event(con_handle);
        // Send notification
        att_server_notify(con_handle, uart_tx_characteristic_handle, data, len);
    }
}

// Update the packet handler to handle UART data
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    uint8_t event = hci_event_packet_get_type(packet);
    
    switch (event) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = HCI_CON_HANDLE_INVALID;
            printf("Disconnected\n");
            // Restart advertising
            gap_advertisements_enable(1);
            break;
            
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    printf("Connected\n");
                    break;
            }
            break;
    }
}

// Update the write callback to handle UART RX
static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size) {
    if (att_handle == uart_rx_characteristic_handle) {
        // Check if we have space in the buffer
        if (rx_buffer_len + buffer_size <= RX_BUFFER_SIZE) {
            memcpy(rx_buffer + rx_buffer_len, buffer, buffer_size);
            rx_buffer_len += buffer_size;
            handle_uart_rx();
        }
        return 0;
    }
    return 0;
}

// Function to set the RX handler
void uart_set_rx_handler(msg_recv_cb_t handler) {
    uart_rx_handler = handler;
}

// Update advertising data to show as HID gamepad but implement Nordic UART
static uint8_t adv_data[] = {
    // Flags (3 bytes)
    0x02, 0x01, 0x06,
    // List of 16-bit Service UUIDs (3 bytes)
    0x03, 0x03, 0x12, 0x18,  // HID Service UUID (0x1812)
    // Appearance (4 bytes) - Gamepad (0x03C4)
    0x03, 0x19, 0xC4, 0x03,
    // Local name (8 bytes)
    0x07, 0x09, 'P', 'i', 'c', 'o', 'W', 'G'
};

static uint8_t scan_resp_data[] = {
    // Complete local name
    0x07, 0x09, 'P', 'i', 'c', 'o', 'W', 'G'
};

// Update ble_init to include both HID appearance and Nordic UART service
void ble_init(void) {
    printf("Starting BLE initialization...\n");

    // Initialize CYW43 driver
    if (cyw43_arch_init()) {
        printf("Failed to initialize CYW43439\n");
        return;
    }

    // Configure LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    // Initialize L2CAP
    l2cap_init();
    
    // Initialize security manager
    sm_init();
    
    // Set security parameters
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
    
    // Register for SM events
    sm_event_callback_registration.callback = &sm_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);
    
    // Setup ATT DB
    att_db_util_init();

    // Add Nordic UART Service (but advertise as HID)
    uart_service_handle = att_db_util_add_service_uuid128(nordic_uart_service_uuid);

    // Add TX Characteristic
    uart_tx_characteristic_handle = att_db_util_add_characteristic_uuid128(
        nordic_uart_tx_uuid,
        ATT_PROPERTY_NOTIFY,
        ATT_SECURITY_NONE,
        ATT_SECURITY_NONE,
        NULL,
        0);

    // Add RX Characteristic
    uart_rx_characteristic_handle = att_db_util_add_characteristic_uuid128(
        nordic_uart_rx_uuid,
        ATT_PROPERTY_WRITE | ATT_PROPERTY_WRITE_WITHOUT_RESPONSE,
        ATT_SECURITY_NONE,
        ATT_SECURITY_NONE,
        NULL,
        0);

    // Initialize ATT Server
    att_server_init(profile_data, att_read_callback, att_write_callback);    

    // Register packet handler
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // Set advertisement data (HID Gamepad appearance)
    gap_advertisements_set_data(sizeof(adv_data), adv_data);
    gap_scan_response_set_data(sizeof(scan_resp_data), scan_resp_data);
    gap_advertisements_enable(1);

    // Turn on Bluetooth
    hci_power_control(HCI_POWER_ON);

    printf("BLE initialization completed\n");
}

// Example message handler
void handle_message(const uint8_t* data, uint16_t len) {
    // Convert data to null-terminated string for easier handling
    char message[256] = {0};
    memcpy(message, data, len < 255 ? len : 255);
    
    printf("rx: %s\n", message);
    
    // Add your message handling logic here
    // This is where you can interpret the received data and respond accordingly
}

int main() {
    stdio_init_all();
    printf("Starting BLE UART (appearing as HID Gamepad)\n");

    ble_init();
    uart_set_rx_handler(handle_message);

    // BTstack run loop
    btstack_run_loop_execute();

    return 0;
}