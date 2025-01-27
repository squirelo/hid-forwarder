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

// Fix Nordic UART Service UUID (correct byte order)
static const uint8_t nordic_uart_service_uuid[] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E
};

// Fix Nordic UART RX UUID (correct byte order)
static const uint8_t nordic_uart_rx_uuid[] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E
};

// Fix Nordic UART TX UUID (correct byte order)
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

#define END 0300     /* indicates end of packet */
#define ESC 0333     /* indicates byte stuffing */
#define ESC_END 0334 /* ESC ESC_END means END data byte */
#define ESC_ESC 0335 /* ESC ESC_ESC means ESC data byte */

// HID Report Descriptor
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

// HID Service and Characteristic UUIDs
static const uint8_t hid_service_uuid[] = {0x12, 0x18};  // 0x1812
static const uint8_t hid_report_map_uuid[] = {0x4B, 0x2A};  // 0x2A4B
static const uint8_t hid_report_uuid[] = {0x4D, 0x2A};  // 0x2A4D
static const uint8_t hid_protocol_mode_uuid[] = {0x4E, 0x2A};  // 0x2A4E

// Gamepad report structure
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

// Update the packet handler to handle UART data
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    if (packet_type != HCI_EVENT_PACKET) {
        printf("Received non-HCI packet type: %02x\n", packet_type);
        return;
    }

    uint8_t event = hci_event_packet_get_type(packet);
    printf("Received HCI event: %02x\n", event);
    
    switch (event) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = HCI_CON_HANDLE_INVALID;
            printf("Disconnected - Restarting advertisements\n");
            // Restart advertising
            gap_advertisements_enable(1);
            break;
            
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE: {
                    con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    uint8_t status = hci_subevent_le_connection_complete_get_status(packet);
                    printf("Connection complete - Status: %02x, Handle: %04x\n", status, con_handle);
                    
                    // Request encryption if connected
                    if (status == ERROR_CODE_SUCCESS) {
                        sm_request_pairing(con_handle);
                    }
                    break;
                }
                default:
                    printf("Unhandled LE Meta subevent: %02x\n", hci_event_le_meta_get_subevent_code(packet));
                    break;
            }
            break;
            
        case HCI_EVENT_ENCRYPTION_CHANGE:
            printf("Encryption change - Status: %02x, Handle: %04x, Enabled: %u\n",
                hci_event_encryption_change_get_status(packet),
                hci_event_encryption_change_get_connection_handle(packet),
                hci_event_encryption_change_get_encryption_enabled(packet));
            break;
            
        default:
            printf("Unhandled HCI event: %02x\n", event);
            break;
    }
}

// Update SM packet handler to handle available security events
static void sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just Works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
            
        case SM_EVENT_PAIRING_COMPLETE:
            switch (sm_event_pairing_complete_get_status(packet)) {
                case ERROR_CODE_SUCCESS:
                    printf("Pairing complete, success - Handle: %04x\n", 
                        sm_event_pairing_complete_get_handle(packet));
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
                    printf("Pairing failed, status = %u\n",
                        sm_event_pairing_complete_get_status(packet));
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

// Update advertising data to properly show both services
static uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, 0x01, 0x06,
    
    // Complete List of 16-bit Service UUIDs
    0x03, 0x03,
    0x12, 0x18,  // HID Service (0x1812)
    
    // Complete List of 128-bit Service UUIDs
    0x11, 0x07,  // Length (17) and Complete List of 128-bit Service UUIDs
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E,
    
    // Appearance (Gamepad)
    0x03, 0x19, 0xC4, 0x03,
    
    // Complete Local Name
    0x07, 0x09, 'P', 'i', 'c', 'o', 'W', 'G'
};

// Update scan response data to include full service UUID
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

    printf("CYW43 initialized successfully\n");

    // Configure LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    // Initialize L2CAP
    l2cap_init();
    printf("L2CAP initialized\n");
    
    // Initialize security manager
    sm_init();
    printf("Security Manager initialized\n");
    
    // Set security parameters with no bonding and no MITM protection for Just Works pairing
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_NO_BONDING);
    
    printf("Security parameters set\n");
    
    // Register for SM events
    sm_event_callback_registration.callback = &sm_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);
    
    // Setup ATT DB
    att_db_util_init();
    printf("ATT DB initialized\n");

    // Add HID Service (0x1812)
    uint16_t hid_service_handle = att_db_util_add_service_uuid16(0x1812);
    printf("HID Service added with handle: %04x\n", hid_service_handle);

    // Add Protocol Mode characteristic (mandatory)
    uint16_t protocol_mode_handle = att_db_util_add_characteristic_uuid16(
        0x2A4E, // Protocol Mode
        ATT_PROPERTY_READ | ATT_PROPERTY_WRITE_WITHOUT_RESPONSE,
        ATT_SECURITY_NONE,
        ATT_SECURITY_NONE,
        (uint8_t*)"\x01", // Report Protocol Mode
        1
    );
    printf("Protocol Mode characteristic added with handle: %04x\n", protocol_mode_handle);

    // Add HID Information characteristic (mandatory)
    uint16_t hid_info_handle = att_db_util_add_characteristic_uuid16(
        0x2A4A, // HID Information
        ATT_PROPERTY_READ,
        ATT_SECURITY_NONE,
        ATT_SECURITY_NONE,
        (uint8_t*)"\x11\x01\x00\x01", // HID spec 1.11, Not localized, Not removable
        4
    );
    printf("HID Information characteristic added with handle: %04x\n", hid_info_handle);

    // Add Report Map characteristic (mandatory)
    uint16_t report_map_handle = att_db_util_add_characteristic_uuid16(
        0x2A4B, // Report Map
        ATT_PROPERTY_READ,
        ATT_SECURITY_NONE,
        ATT_SECURITY_NONE,
        (uint8_t*)hid_report_descriptor,
        sizeof(hid_report_descriptor)
    );
    printf("Report Map characteristic added with handle: %04x\n", report_map_handle);

    // Add HID Report characteristic (mandatory)
    uint16_t report_handle = att_db_util_add_characteristic_uuid16(
        0x2A4D, // Report
        ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY | ATT_PROPERTY_WRITE,
        ATT_SECURITY_NONE,
        ATT_SECURITY_NONE,
        NULL,
        0
    );
    printf("Report characteristic added with handle: %04x\n", report_handle);

    // Add Report Reference descriptor for the Report characteristic
    uint16_t report_reference_handle = att_db_util_add_descriptor_uuid16(
        0x2908, // Report Reference
        ATT_PROPERTY_READ,
        ATT_SECURITY_NONE,  // read permission
        ATT_SECURITY_NONE,  // write permission
        (uint8_t*)"\x01\x01", // Report ID 1, Input report
        2  // data length
    );
    printf("Report Reference descriptor added with handle: %04x\n", report_reference_handle);

    // Add HID Control Point characteristic (mandatory)
    uint16_t control_point_handle = att_db_util_add_characteristic_uuid16(
        0x2A4C, // HID Control Point
        ATT_PROPERTY_WRITE_WITHOUT_RESPONSE,
        ATT_SECURITY_NONE,
        ATT_SECURITY_NONE,
        NULL,
        0
    );
    printf("Control Point characteristic added with handle: %04x\n", control_point_handle);

    // Add Nordic UART Service
    uart_service_handle = att_db_util_add_service_uuid128(nordic_uart_service_uuid);
    printf("UART Service added with handle: %04x\n", uart_service_handle);

    // Add TX Characteristic with notify property
    uart_tx_characteristic_handle = att_db_util_add_characteristic_uuid128(
        nordic_uart_tx_uuid,
        ATT_PROPERTY_NOTIFY,
        ATT_SECURITY_NONE,
        ATT_SECURITY_NONE,
        NULL,
        0);
    printf("UART TX characteristic added with handle: %04x\n", uart_tx_characteristic_handle);

    // Add RX Characteristic with write property
    uart_rx_characteristic_handle = att_db_util_add_characteristic_uuid128(
        nordic_uart_rx_uuid,
        ATT_PROPERTY_WRITE | ATT_PROPERTY_WRITE_WITHOUT_RESPONSE,
        ATT_SECURITY_NONE,
        ATT_SECURITY_NONE,
        NULL,
        0);
    printf("UART RX characteristic added with handle: %04x\n", uart_rx_characteristic_handle);

    // Initialize ATT Server
    att_server_init(profile_data, att_read_callback, att_write_callback);    
    printf("ATT Server initialized\n");

    // Register packet handler
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // Set advertisement data and scan response
    gap_advertisements_set_data(sizeof(adv_data), adv_data);
    gap_scan_response_set_data(sizeof(scan_resp_data), scan_resp_data);
    printf("Advertisement data set (adv: %d bytes, scan: %d bytes)\n", sizeof(adv_data), sizeof(scan_resp_data));

    // Create empty address for undirected advertising
    bd_addr_t empty_addr;
    memset(empty_addr, 0, sizeof(bd_addr_t));

    // Set advertisement data and parameters
    gap_advertisements_set_data(sizeof(adv_data), adv_data);
    
    // More aggressive advertising parameters
    gap_advertisements_set_params(
        0x0020,     // adv_int_min (20ms)
        0x0040,     // adv_int_max (40ms)
        ADV_IND,    // adv_type
        0,          // own_address_type
        empty_addr, // peer_address
        0x07,       // channel_map (all channels)
        0          // filter_policy
    );
    printf("Advertisement parameters set (interval: 20-40ms)\n");
    
    // Start advertising
    gap_advertisements_enable(1);
    printf("Advertisements enabled\n");

    // Turn on Bluetooth
    hci_power_control(HCI_POWER_ON);
    printf("Bluetooth powered on\n");

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
            gamepad_state.x = 0;
            gamepad_state.y = 0;
            gamepad_state.z = 0;
            gamepad_state.rz = 0;
            gamepad_state.rx = 0;
            gamepad_state.ry = 0;
            gamepad_state.hat = 0;
            gamepad_state.buttons = button_pressed ? 0 : 1; // Button A
            
            if (tud_hid_ready()) {
                tud_hid_report(0, &gamepad_state, sizeof(gamepad_state));
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