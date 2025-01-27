#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H
#define MAX_ATT_DB_SIZE 1024  // Set a reasonable size

// BTstack features that can be enabled
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LOG_INFO
#define ENABLE_LOG_ERROR
#define ENABLE_PRINTF_HEXDUMP

// Enable GAP features
#define ENABLE_GAP_RANDOM_ADDRESS
#define ENABLE_ATT_DELAYED_RESPONSE

// Enable Security Manager
#define ENABLE_LE_SECURE_CONNECTIONS
#define ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION
#define ENABLE_SM_JUST_WORKS
#define ENABLE_SM_BONDING
#define ENABLE_SM_SECURE_CONNECTIONS

// BTstack configuration
#define MAX_NR_WHITELIST_ENTRIES 1
#define MAX_NR_SM_LOOKUP_ENTRIES 4
#define MAX_NR_LE_DEVICE_DB_ENTRIES 4

#define NVM_NUM_DEVICE_DB_ENTRIES 4
#define HCI_OUTGOING_PRE_BUFFER_SIZE 1024
#define HCI_ACL_CHUNK_SIZE_ALIGNMENT 1024

// HCI configuration
#define HCI_ACL_PAYLOAD_SIZE 1024
#define MAX_NR_HCI_CONNECTIONS 1

// ATT Server
#define ATT_DB_MAX_DEVICE_NAME_LEN 20

// Enable additional services
#define ENABLE_GATT_SERVICE_CHANGED
#define ENABLE_LE_DATA_LENGTH_EXTENSION

// Optional: Enable debug logging for security manager
#define ENABLE_LOG_DEBUG
#define ENABLE_LOG_SM

#endif