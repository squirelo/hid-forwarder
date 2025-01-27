#ifndef BTSTACK_WRAPPER_H
#define BTSTACK_WRAPPER_H

// First include TinyUSB to get its HID definitions
#include "tusb.h"

// Block all BTstack HID-related headers
#define BTSTACK_HID_H
#define HID_H_
#define _BTSTACK_HID_H
#define __BTSTACK_HID_H__
#define BTSTACK_HID_PARSER_H
#define _BTSTACK_HID_PARSER_H
#define __BTSTACK_HID_PARSER_H__

// Define BLE advertising types
#define ADV_IND 0x00

// Only include the BTstack headers we actually need, in correct dependency order
#include <stdint.h>
#include "btstack_config.h"
#include "btstack_defines.h"
#include "btstack_linked_list.h"
#include "btstack_bool.h"
#include "btstack_event.h"
#include "hci.h"
#include "hci_cmd.h"
#include "gap.h"
#include "l2cap.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "ble/att_db_util.h"
#include "ble/sm.h"
#include "btstack_util.h"
#include "btstack_debug.h"
#include "ble/gatt-service/nordic_spp_service_server.h"

#endif // BTSTACK_WRAPPER_H