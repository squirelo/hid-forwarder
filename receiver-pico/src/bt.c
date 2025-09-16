#ifdef BLUETOOTH_ENABLED
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bt.h"
#include "btstack.h"
#include "receiver.h"

#define RFCOMM_SERVER_CHANNEL 1

static uint16_t rfcomm_channel_id;
static bool pairing_mode_enabled;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
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
                    } else {
                        rfcomm_channel_id = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                        mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        printf("RFCOMM_EVENT_CHANNEL_OPENED success %u, mtu %u\n", rfcomm_channel_id, mtu);
                    }
                    break;
                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    printf("RFCOMM_EVENT_CHANNEL_CLOSED\n");
                    rfcomm_channel_id = 0;
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
                // printf("%02x ", packet[i]);
                serial_read_byte(packet[i], 0);
            }
            // printf("\n");
            break;
        default:
            break;
    }
}

static void spp_service_setup(void) {
    static uint8_t spp_service_buffer[150];
    static btstack_packet_callback_registration_t hci_event_callback_registration;

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    rfcomm_init();
    rfcomm_register_service(packet_handler, RFCOMM_SERVER_CHANNEL, 0xffff);  // reserved channel, mtu limited by l2cap

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    spp_create_sdp_record(spp_service_buffer, sdp_create_service_record_handle(), RFCOMM_SERVER_CHANNEL, "PlayAbility Receiver");
    btstack_assert(de_get_len(spp_service_buffer) <= sizeof(spp_service_buffer));
    sdp_register_service(spp_service_buffer);
}

void bt_init() {
    spp_service_setup();

    bt_set_pairing_mode(false);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
    gap_set_local_name("PlayAbility Receiver 00:00:00:00:00:00");

    hci_power_control(HCI_POWER_ON);
}

void bt_set_pairing_mode(bool enabled) {
    pairing_mode_enabled = enabled;
    gap_discoverable_control(enabled);
    gap_ssp_set_auto_accept(enabled);
}

bool bt_get_pairing_mode() {
    return pairing_mode_enabled;
}

bool bt_is_connected() {
    return rfcomm_channel_id != 0;
}

void bt_forget_all_devices() {
    gap_delete_all_link_keys();
}
#endif
