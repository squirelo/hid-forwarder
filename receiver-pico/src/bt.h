#ifndef _BT_H_
#define _BT_H_

#ifdef BLUETOOTH_ENABLED

#include <stdbool.h>
#include <stdint.h>

// Bluetooth mode enumeration
typedef enum {
    BT_MODE_CLASSIC = 0,
    BT_MODE_BLE = 1
} bt_mode_t;

// Bluetooth initialization and management
void bt_init(void);
void bt_deinit(void);
bt_mode_t bt_get_current_mode(void);

// Connection management
bool bt_is_connected(void);
void bt_disconnect(void);

// Pairing management
void bt_set_pairing_mode(bool enable);
bool bt_get_pairing_mode(void);
void bt_forget_all_devices(void);

// Data transmission
int bt_send_data(const uint8_t* data, uint16_t length);

#endif

#endif
