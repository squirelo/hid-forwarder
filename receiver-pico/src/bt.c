#ifdef BLUETOOTH_ENABLED
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bt.h"
#include "btstack.h"
#include "receiver.h"
#include "ble/gatt-service/nordic_spp_service_server.h"
#include "hid_receiver.h"

#define RFCOMM_SERVER_CHANNEL 1

// Bluetooth state
static bt_mode_t current_mode = BT_MODE_CLASSIC;
static bool bt_initialized = false;
static bool pairing_mode_enabled = false;

// Classic Bluetooth 
static uint16_t rfcomm_channel_id = 0;
static bool classic_connected = false;

static bool ble_connected = false;
static uint16_t ble_connection_handle = 0;

// Packet handler for Classic Bluetooth
static void classic_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    bd_addr_t event_addr;
    uint8_t rfcomm_channel_nr;
    uint16_t mtu;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_PIN_CODE_REQUEST:
                    printf("HCI_EVENT_PIN_CODE_REQUEST\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
                    break;
                    
                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    printf("HCI_EVENT_USER_CONFIRMATION_REQUEST '%06" PRIu32 "'\n", little_endian_read_32(packet, 8));
                    break;
                    
                case RFCOMM_EVENT_INCOMING_CONNECTION:
                    rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);
                    rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
                    rfcomm_channel_id = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
                    printf("RFCOMM_EVENT_INCOMING_CONNECTION %s channel %u\n", bd_addr_to_str(event_addr), rfcomm_channel_nr);
                    rfcomm_accept_connection(rfcomm_channel_id);
                    break;
                    
                case RFCOMM_EVENT_CHANNEL_OPENED:
                    if (rfcomm_event_channel_opened_get_status(packet)) {
                        printf("RFCOMM_EVENT_CHANNEL_OPENED failed 0x%02x\n", rfcomm_event_channel_opened_get_status(packet));
                        classic_connected = false;
                    } else {
                        rfcomm_channel_id = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                        mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        printf("RFCOMM_EVENT_CHANNEL_OPENED success %u, mtu %u\n", rfcomm_channel_id, mtu);
                        classic_connected = true;
                    }
                    break;
                    
                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    printf("RFCOMM_EVENT_CHANNEL_CLOSED\n");
                    rfcomm_channel_id = 0;
                    classic_connected = false;
                    break;
                    
                case GAP_EVENT_PAIRING_COMPLETE:
                    printf("GAP_EVENT_PAIRING_COMPLETE\n");
                    bt_set_pairing_mode(false);
                    break;
                    
                default:
                    break;
            }
            break;
            
        case RFCOMM_DATA_PACKET:
            for (int i = 0; i < size; i++) {
                serial_read_byte(packet[i], 0);
            }
            break;
            
        default:
            break;
    }
}

static void ble_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    bd_addr_t event_addr;
    
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            ble_connection_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                            hci_subevent_le_connection_complete_get_peer_address(packet, event_addr);
                            printf("BLE connected to %s\n", bd_addr_to_str(event_addr));
                            ble_connected = true;
                            break;
                            
                        case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
                            printf("BLE connection updated\n");
                            break;
                            
                        default:
                            break;
                    }
                    break;
                    
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    if (hci_event_disconnection_complete_get_connection_handle(packet) == ble_connection_handle) {
                        printf("BLE disconnected\n");
                        ble_connected = false;
                        ble_connection_handle = 0;
                    }
                    break;
                    
                case HCI_EVENT_GATTSERVICE_META:
                    switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
                        case GATTSERVICE_SUBEVENT_SPP_SERVICE_CONNECTED:
                            ble_connection_handle = gattservice_subevent_spp_service_connected_get_con_handle(packet);
                            printf("Nordic SPP service connected\n");
                            ble_connected = true;
                            break;
                            
                        case GATTSERVICE_SUBEVENT_SPP_SERVICE_DISCONNECTED:
                            printf("Nordic SPP service disconnected\n");
                            ble_connected = false;
                            ble_connection_handle = 0;
                            break;
                            
                        default:
                            break;
                    }
                    break;
                    
                default:
                    break;
            }
            break;
            
        case RFCOMM_DATA_PACKET:
            for (int i = 0; i < size; i++) {
                serial_read_byte(packet[i], 0);
            }
            break;
            
        default:
            break;
    }
}

// Classic Bluetooth setup
static void classic_setup(void) {
    static uint8_t spp_service_buffer[150];
    static btstack_packet_callback_registration_t hci_event_callback_registration;

    // Register for HCI events
    hci_event_callback_registration.callback = &classic_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager for cross-transport key derivation
    sm_init();
#endif

    rfcomm_init();
    rfcomm_register_service(classic_packet_handler, RFCOMM_SERVER_CHANNEL, 0xffff);

    // Initialize SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    spp_create_sdp_record(spp_service_buffer, sdp_create_service_record_handle(), RFCOMM_SERVER_CHANNEL, "HID Receiver");
    btstack_assert(de_get_len(spp_service_buffer) <= sizeof(spp_service_buffer));
    sdp_register_service(spp_service_buffer);
}

static void ble_setup(void) {
    static btstack_packet_callback_registration_t hci_event_callback_registration;

    // Register for HCI events
    hci_event_callback_registration.callback = &ble_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();
    sm_init();
    
    // Setup ATT server with compiled GATT profile
    att_server_init(profile_data, NULL, NULL);
    
    nordic_spp_service_server_init(&ble_packet_handler);
    
    gap_advertisements_set_params(0x0020, 0x0020, 0, 0, NULL, 0x07, 0x00);
    
    uint8_t adv_data[] = {
        2, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
        12, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'H', 'I', 'D', ' ', 'R', 'e', 'c', 'e', 'i', 'v', 'e', 'r',
        17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS, 
        0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e,
    };
    gap_advertisements_set_data(sizeof(adv_data), adv_data);
    gap_advertisements_enable(1);
}

// Main Bluetooth initialization
void bt_init(void) {
    if (bt_initialized) {
        return;
    }
    
    // Get mode from receiver.c configuration - need to declare extern
    extern uint8_t our_bt_mode;
    current_mode = (bt_mode_t)our_bt_mode;
    
    // Initialize based on mode
    switch (current_mode) {
        case BT_MODE_CLASSIC:
            classic_setup();
            break;
            
        case BT_MODE_BLE:
            ble_setup();
            break;
    }
    
    // Set device name
    gap_set_local_name("HID Receiver");
    
    // Set pairing mode
    bt_set_pairing_mode(false);
    
    // Power on Bluetooth
    hci_power_control(HCI_POWER_ON);
    
    bt_initialized = true;
}

// Get current mode
bt_mode_t bt_get_current_mode(void) {
    return current_mode;
}

// Check if connected
bool bt_is_connected(void) {
    switch (current_mode) {
        case BT_MODE_CLASSIC:
            return classic_connected;
        case BT_MODE_BLE:
            return ble_connected;
        default:
            return false;
    }
}

// Send data
int bt_send_data(const uint8_t* data, uint16_t length) {
    if (!data || length == 0) {
        return -1;
    }
    
    switch (current_mode) {
        case BT_MODE_CLASSIC:
            if (classic_connected && rfcomm_channel_id) {
                return rfcomm_send(rfcomm_channel_id, (uint8_t*)data, length);
            }
            break;
            
        case BT_MODE_BLE:
            if (ble_connected && ble_connection_handle) {
                return nordic_spp_service_server_send(ble_connection_handle, (uint8_t*)data, length);
            }
            break;
            
        default:
            return -1;
    }
    
    return -1;
}

// Pairing mode functions
void bt_set_pairing_mode(bool enabled) {
    pairing_mode_enabled = enabled;
    gap_discoverable_control(enabled);
    gap_ssp_set_auto_accept(enabled);
}

bool bt_get_pairing_mode(void) {
    return pairing_mode_enabled;
}

void bt_forget_all_devices(void) {
    gap_delete_all_link_keys();
}

void bt_disconnect(void) {
    if (classic_connected && rfcomm_channel_id) {
        rfcomm_disconnect(rfcomm_channel_id);
    }
    
    if (ble_connected && ble_connection_handle) {
        // Use hci_send_cmd with HCI_OPCODE_HCI_DISCONNECT
        hci_send_cmd(&hci_disconnect, ble_connection_handle, ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION);
    }
}

void bt_deinit(void) {
    if (!bt_initialized) {
        return;
    }
    
    // Disconnect any active connections
    bt_disconnect();
    
    // Power off Bluetooth
    hci_power_control(HCI_POWER_OFF);
    
    bt_initialized = false;
    classic_connected = false;
    ble_connected = false;
    rfcomm_channel_id = 0;
    ble_connection_handle = 0;
}

#endif
