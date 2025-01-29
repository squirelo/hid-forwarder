#ifdef BLUETOOTH_ENABLED
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include our wrapper first to control BTstack includes
#include "btstack_wrapper.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"
#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"
#include "ble/le_device_db.h"
#include "ble/sm.h"  // For security manager functions
#include "ble/att_server.h"
#include "ble/att_db.h"
#include "hci.h"
#include "l2cap.h"

// Now include our own headers
#include "bt.h"
#include "receiver.h"
#include "../build/generated/receiver_gatt_header/receiver.h"  // Generated from receiver.gatt

// Connection handle
hci_con_handle_t bt_connection_handle = HCI_CON_HANDLE_INVALID;
static bool pairing_mode_enabled = false;

// Packet handler structure
static btstack_packet_callback_registration_t hci_event_callback_registration;

// Service handles
static uint16_t hid_service_handle;
static uint16_t uart_service_handle;
static uint16_t uart_rx_characteristic_handle;
static uint16_t uart_tx_characteristic_handle;
static uint16_t hid_report_characteristic_handle;

// ATT Read Callback for handling read requests
static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size) {
    // Handle reads for each characteristic
    if (att_handle == uart_tx_characteristic_handle) {
        return 0; // No static data to read
    }
    
    if (att_handle == hid_report_characteristic_handle) {
        return 0; // No static data to read
    }
    
    return 0;
}

// ATT Write Callback for handling write requests
static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size) {
    // Handle UART RX writes
    if (att_handle == uart_rx_characteristic_handle) {
        for (int i = 0; i < buffer_size; i++) {
            serial_read_byte(buffer[i], 0);
        }
        return 0;
    }
    
    return 0;
}

// Callback for GATT events
static void gatt_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CONNECTED:
            bt_connection_handle = att_event_connected_get_handle(packet);
            printf("ATT Connected, handle 0x%04x\n", bt_connection_handle);
            break;

        case ATT_EVENT_DISCONNECTED:
            printf("ATT Disconnected, handle 0x%04x\n", bt_connection_handle);
            bt_connection_handle = HCI_CON_HANDLE_INVALID;
            break;

        case ATT_EVENT_CAN_SEND_NOW:
            // Handle notification sending here if needed
            break;

        case ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE:
            // Handle indication complete if needed
            break;
    }
}

// Callback for HCI events
static void hci_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_PIN_CODE_REQUEST:
            printf("Pin code request\n");
            break;

        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
            printf("User confirmation request\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;

        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                printf("BTstack up and running\n");
            }
            break;

        case SM_EVENT_JUST_WORKS_REQUEST:
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;

        case SM_EVENT_PAIRING_COMPLETE:
            switch (sm_event_pairing_complete_get_status(packet)) {
                case ERROR_CODE_SUCCESS:
                    printf("Pairing complete\n");
                    bt_set_pairing_mode(false);
                    break;
                default:
                    printf("Pairing failed\n");
                    break;
            }
            break;
    }
}

// Callback for UART RX characteristic writes
static int uart_rx_callback(uint16_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size) {
    if (att_handle == uart_rx_characteristic_handle) {
        for (int i = 0; i < buffer_size; i++) {
            serial_read_byte(buffer[i], 0);
        }
        return 0;
    }
    return 0;
}

void bt_init(void) {
    // Initialize L2CAP
    l2cap_init();

    // Initialize Security Manager
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);

    // Initialize ATT Server
    att_server_init(profile_data, att_read_callback, att_write_callback);

    // Register for HCI events
    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    
    // Register for ATT events
    att_server_register_packet_handler(gatt_packet_handler);

    // Set device name
    uint8_t adv_data[] = {
        // Flags general discoverable
        0x02, 0x01, 0x06,
        // Name
        0x07, 0x09, 'P', 'i', 'c', 'o', 'W', 'G'
    };
    gap_advertisements_set_data(sizeof(adv_data), adv_data);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);

    // Set advertising parameters
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0060;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);

    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);

    // Start with pairing mode enabled
    bt_set_pairing_mode(true);

    // Turn on Bluetooth
    hci_power_control(HCI_POWER_ON);
}

void bt_set_pairing_mode(bool enabled) {
    pairing_mode_enabled = enabled;
    gap_advertisements_enable(enabled);
    sm_set_authentication_requirements(enabled ? SM_AUTHREQ_BONDING : SM_AUTHREQ_NO_BONDING);
}

bool bt_get_pairing_mode(void) {
    return pairing_mode_enabled;
}

bool bt_is_connected(void) {
    return bt_connection_handle != HCI_CON_HANDLE_INVALID;
}

void bt_forget_all_devices(void) {
    // Delete all LE device pairings
    uint16_t i;
    for (i = 0; i < le_device_db_max_count(); i++) {
        int addr_type;
        bd_addr_t addr;
        memset(addr, 0, 6);
        le_device_db_info(i, &addr_type, addr, NULL);
        if (addr_type != BD_ADDR_TYPE_UNKNOWN) {
            gap_delete_bonding(addr_type, addr);
        }
    }
}

void bt_send_hid_report(const uint8_t* report, uint16_t size) {
    if (!bt_is_connected()) return;
    
    att_server_notify(bt_connection_handle, hid_report_characteristic_handle, report, size);
}

void bt_send_uart_data(const uint8_t* data, uint16_t size) {
    if (!bt_is_connected()) return;
    
    att_server_notify(bt_connection_handle, uart_tx_characteristic_handle, data, size);
}

#endif // BLUETOOTH_ENABLED
