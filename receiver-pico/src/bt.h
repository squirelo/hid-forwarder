#ifndef _BT_H_
#define _BT_H_

#ifdef BLUETOOTH_ENABLED

#include <stdbool.h>
#include <stdint.h>
#include "btstack_wrapper.h"

// BLE connection state
void bt_init(void);
bool bt_is_connected(void);
void bt_set_pairing_mode(bool enable);
bool bt_get_pairing_mode(void);
void bt_forget_all_devices(void);

// HID Report functions
void bt_send_hid_report(const uint8_t* report, uint16_t size);
void bt_send_uart_data(const uint8_t* data, uint16_t size);

// Connection handle
extern hci_con_handle_t bt_connection_handle;

#endif // BLUETOOTH_ENABLED

#endif // _BT_H_
