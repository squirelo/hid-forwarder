#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"
#include "class/hid/hid.h"

#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"
#include "hardware/sync.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "crc.h"
#include "descriptors.h"
#include "globals.h"

// BLE headers
#include "btstack_wrapper.h"
#include "btstack_run_loop.h"

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

typedef void (*msg_recv_cb_t)(const uint8_t* data, uint16_t len);

typedef struct __attribute__((packed)) {
    uint8_t config_version;
    uint8_t our_descriptor_number;
    uint8_t reserved[14];
    uint32_t crc;
} config_t;

_Static_assert(sizeof(config_t) == 20);

typedef struct __attribute__((packed)) {
    uint8_t protocol_version;
    uint8_t our_descriptor_number;
    uint8_t len;
    uint8_t report_id;
    uint8_t data[0];
} packet_t;

typedef struct {
    uint8_t report_id;
    uint8_t len;
    uint8_t data[64];
} outgoing_report_t;

#define OR_BUFSIZE 8
outgoing_report_t outgoing_reports[OR_BUFSIZE];
uint8_t or_head = 0;
uint8_t or_tail = 0;
uint8_t or_items = 0;

config_t config = {
    .config_version = CONFIG_VERSION,
    .our_descriptor_number = 2,
    .reserved = { 0 },
    .crc = 0,
};

// BLE connection handle
static hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
static uint16_t uart_service_handle;
static uint16_t uart_rx_characteristic_handle;
static uint16_t uart_tx_characteristic_handle;
static uint8_t profile_data[512];
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

// Add Nordic UART Service UUID
static const uint8_t nordic_uart_service_uuid[] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E
};

// Nordic UART RX UUID
static const uint8_t nordic_uart_rx_uuid[] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E
};

// Nordic UART TX UUID
static const uint8_t nordic_uart_tx_uuid[] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E
};

void queue_outgoing_report(uint8_t report_id, uint8_t* data, uint8_t len) {
    if (or_items == OR_BUFSIZE) {
        printf("overflow!\n");
        return;
    }
    outgoing_reports[or_tail].report_id = report_id;
    outgoing_reports[or_tail].len = len;
    memcpy(outgoing_reports[or_tail].data, data, len);
    or_tail = (or_tail + 1) % OR_BUFSIZE;
    or_items++;
}

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
        // No valid config in flash, save the default one
        persist_config();
        return;
    }
    memcpy(&config, FLASH_CONFIG_IN_MEMORY, sizeof(config_t));
}

void handle_received_packet(const uint8_t* data, uint16_t len) {
    if (len < sizeof(packet_t)) {
        printf("packet too small\n");
        return;
    }
    packet_t* msg = (packet_t*) data;
    len = len - sizeof(packet_t);
    if ((msg->protocol_version != PROTOCOL_VERSION) ||
        (msg->len != len) ||
        (len > 64) ||
        (msg->our_descriptor_number >= NOUR_DESCRIPTORS)) {
        printf("ignoring packet\n");
        return;
    }
    
    if (msg->our_descriptor_number != our_descriptor_number) {
        config.our_descriptor_number = msg->our_descriptor_number;
        persist_config();
        watchdog_reboot(0, 0, 0);
    }

    // Forward via TinyUSB HID
    if (tud_hid_n_ready(0)) {
        tud_hid_n_report(0, msg->report_id, msg->data, len);
    } else {
        queue_outgoing_report(msg->report_id, msg->data, len);
    }
}

void serial_init() {
    uart_init(SERIAL_UART, SERIAL_BAUDRATE);
    uart_set_translate_crlf(SERIAL_UART, false);
    gpio_set_function(SERIAL_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(SERIAL_RX_PIN, GPIO_FUNC_UART);
}

#define END 0300     /* indicates end of packet */
#define ESC 0333     /* indicates byte stuffing */
#define ESC_END 0334 /* ESC ESC_END means END data byte */
#define ESC_ESC 0335 /* ESC ESC_ESC means ESC data byte */

void serial_read(msg_recv_cb_t callback) {
    static uint8_t buffer[SERIAL_MAX_PACKET_SIZE];
    static uint16_t bytes_read = 0;
    static bool escaped = false;

    while (uart_is_readable(SERIAL_UART)) {
        bytes_read %= sizeof(buffer);

        char c = uart_getc(SERIAL_UART);

        if (escaped) {
            switch (c) {
                case ESC_END:
                    buffer[bytes_read++] = END;
                    break;
                case ESC_ESC:
                    buffer[bytes_read++] = ESC;
                    break;
                default:
                    buffer[bytes_read++] = c;
                    break;
            }
            escaped = false;
        } else {
            switch (c) {
                case END:
                    if (bytes_read > 4) {
                        uint32_t crc = crc32(buffer, bytes_read - 4);
                        uint32_t received_crc = 0;
                        for (int i = 0; i < 4; i++) {
                            received_crc = (received_crc << 8) | buffer[bytes_read - 1 - i];
                        }
                        if (crc == received_crc) {
                            callback(buffer, bytes_read - 4);
                            bytes_read = 0;
                            return;
                        } else {
                            printf("CRC error\n");
                        }
                    }
                    bytes_read = 0;
                    break;
                case ESC:
                    escaped = true;
                    break;
                default:
                    buffer[bytes_read++] = c;
                    break;
            }
        }
    }
}

void serial_task() {
    serial_read(handle_received_packet);
}

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

// Callback for write requests - this is where we receive HID reports via BLE
static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size) {
    if (att_handle == uart_rx_characteristic_handle) {
        // Process received HID report
        handle_received_packet(buffer, buffer_size);
        return 0;
    }
    return 0;
}

// Callback for read requests
static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size) {
    return 0;
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

// Update advertising data to include Nordic UART Service
static uint8_t adv_data[] = {
    // Flags (3 bytes)
    0x02, 0x01, 0x06,
    
    // Complete List of 128-bit Service UUIDs (17 bytes)
    0x11, 0x07,  // Length (17) and Complete List of 128-bit Service UUIDs
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E,
    
    // List of 16-bit Service UUIDs (3 bytes)
    0x03, 0x03, 0x12, 0x18,  // HID Service UUID (0x1812)
    
    // Appearance (4 bytes) - Gamepad (0x03C4)
    0x03, 0x19, 0xC4, 0x03,
    
    // Local name (8 bytes)
    0x07, 0x09, 'P', 'i', 'c', 'o', 'W', 'G'
};

static uint8_t scan_resp_data[] = {
    // Complete local name
    0x07, 0x09, 'P', 'i', 'c', 'o', 'W', 'G',
    
    // Complete List of 128-bit Service UUIDs (17 bytes)
    0x11, 0x07,  // Length (17) and Complete List of 128-bit Service UUIDs
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E
};

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

    // Add Nordic UART Service
    uart_service_handle = att_db_util_add_service_uuid128(nordic_uart_service_uuid);

    // Add TX Characteristic with notify property
    uart_tx_characteristic_handle = att_db_util_add_characteristic_uuid128(
        nordic_uart_tx_uuid,
        ATT_PROPERTY_NOTIFY,
        ATT_SECURITY_NONE,
        ATT_SECURITY_NONE,
        NULL,
        0);

    // Add RX Characteristic with write property
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

    // Set advertisement data and scan response
    gap_advertisements_set_data(sizeof(adv_data), adv_data);
    gap_scan_response_set_data(sizeof(scan_resp_data), scan_resp_data);

    // Create empty address for undirected advertising
    bd_addr_t empty_addr;
    memset(empty_addr, 0, sizeof(bd_addr_t));

    // Set advertising parameters
    gap_advertisements_set_params(
        0x0020,     // adv_int_min (20ms)
        0x0020,     // adv_int_max (20ms)
        0,          // adv_type (ADV_IND)
        0,          // own_address_type
        empty_addr, // peer_address (not used for undirected advertising)
        0x07,       // channel_map (all channels)
        0          // filter_policy
    );
    
    // Start advertising
    gap_advertisements_enable(1);

    // Turn on Bluetooth
    hci_power_control(HCI_POWER_ON);

    printf("BLE initialization completed\n");
}

// TinyUSB HID callbacks
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    // Not used for this application
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    // Not used for this application
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

int main(void) {
    board_init();
    stdio_init_all();
    printf("BLE to USB HID Bridge\n");
    config_init();
    our_descriptor_number = config.our_descriptor_number;
    if (our_descriptor_number >= NOUR_DESCRIPTORS) {
        our_descriptor_number = 0;
    }
    serial_init();
    ble_init();
    tusb_init();

    absolute_time_t next_button_press = make_timeout_time_ms(1000);
    bool button_pressed = false;

    while (true) {
        tud_task();
        btstack_run_loop_base_poll_data_sources();
        serial_task();
        
        // Handle periodic button A press
        if (time_reached(next_button_press)) {
            hid_gamepad_report_t report = {
                .x = 0, .y = 0, .z = 0, .rz = 0, .rx = 0, .ry = 0,
                .hat = 0, .buttons = button_pressed ? 0 : GAMEPAD_BUTTON_A
            };
            
            if (tud_hid_ready()) {
                tud_hid_report(0, &report, sizeof(report));
            }
            
            button_pressed = !button_pressed;
            next_button_press = make_timeout_time_ms(button_pressed ? 100 : 900); // Press for 100ms, release for 900ms
        }

        if ((or_items > 0) && (tud_hid_n_ready(0))) {
            tud_hid_n_report(0, outgoing_reports[or_head].report_id, outgoing_reports[or_head].data, outgoing_reports[or_head].len);
            or_head = (or_head + 1) % OR_BUFSIZE;
            or_items--;
        }
    }

    return 0;
}