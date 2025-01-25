#ifndef _BT_H_
#define _BT_H_

#ifdef BLUETOOTH_ENABLED

#include <stdbool.h>

void bt_init();
bool bt_is_connected();
void bt_set_pairing_mode(bool enable);
bool bt_get_pairing_mode();
void bt_forget_all_devices();

#endif

#endif
