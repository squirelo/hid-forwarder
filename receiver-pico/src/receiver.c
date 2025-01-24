#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"

#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"

#include "pico/stdio.h"

#ifdef NETWORK_ENABLED
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "pico/cyw43_arch.h"
#endif

#include "crc.h"
#include "descriptors.h"
#include "globals.h"

#define PERSISTED_CONFIG_SIZE 4096
#define CONFIG_OFFSET_IN_FLASH (PICO_FLASH_SIZE_BYTES - 16384)
#define FLASH_CONFIG_IN_MEMORY (((uint8_t*) XIP_BASE) + CONFIG_OFFSET_IN_FLASH)

#ifdef NETWORK_ENABLED
#define OUR_PORT 42734
#endif

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
    char wifi_ssid[20];
    char wifi_password[24];
    uint8_t reserved[14];
    uint32_t crc;
} config_t;

_Static_assert(sizeof(config_t) == 64);

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

#ifdef NETWORK_ENABLED

struct udp_pcb* pcb;
ip_addr_t server_address;

bool wifi_connected = false;

#endif

config_t config = {
    .config_version = CONFIG_VERSION,
    .our_descriptor_number = 2,
    .wifi_ssid = "",
    .wifi_password = "",
    .reserved = { 0 },
    .crc = 0,
};

#define OR_BUFSIZE 8
outgoing_report_t outgoing_reports[OR_BUFSIZE];
uint8_t or_head = 0;
uint8_t or_tail = 0;
uint8_t or_items = 0;

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

void handle_received_packet(const uint8_t* data, uint16_t len) {
    if (len < sizeof(packet_t)) {
        printf("packet to small\n");
        return;
    }
    packet_t* msg = (packet_t*) data;
    len = len - sizeof(packet_t);
    if ((msg->protocol_version != PROTOCOL_VERSION) ||
        (msg->len != len) ||
        (len > 64) ||
        (msg->our_descriptor_number >= NOUR_DESCRIPTORS) ||
        ((msg->report_id == 0) && (len >= 64))) {
        printf("ignoring packet\n");
        return;
    }
    if (msg->our_descriptor_number != our_descriptor_number) {
        config.our_descriptor_number = msg->our_descriptor_number;
        persist_config();
        watchdog_reboot(0, 0, 0);
    }
    if (tud_hid_n_ready(0)) {
        tud_hid_n_report(0, msg->report_id, msg->data, len);
    } else {
        queue_outgoing_report(msg->report_id, msg->data, len);
    }
}

#ifdef NETWORK_ENABLED

void net_recv(void* arg, struct udp_pcb* pcb, struct pbuf* p, const ip_addr_t* addr, u16_t port) {
    handle_received_packet(p->payload, p->len);
    pbuf_free(p);
}

void net_init() {
    cyw43_arch_init();
    cyw43_arch_enable_sta_mode();
    if (strlen(config.wifi_ssid) > 0) {
        cyw43_arch_wifi_connect_async(config.wifi_ssid, config.wifi_password, CYW43_AUTH_WPA2_AES_PSK);
    }

    pcb = udp_new();
    udp_bind(pcb, IP_ANY_TYPE, OUR_PORT);
    udp_recv(pcb, net_recv, NULL);
}

void net_task() {
    wifi_connected = CYW43_LINK_UP == cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
}

#endif

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
                    // this shouldn't happen
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
        return;
    }
    memcpy(&config, FLASH_CONFIG_IN_MEMORY, sizeof(config_t));
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    if (itf == 1) {
        if (reqlen != sizeof(config_t)) {
            return 0;
        }

        memcpy(buffer, &config, reqlen);
        config_t* c = (config_t*) buffer;
        memset(c->wifi_password, 0, sizeof(c->wifi_password));
        c->crc = crc32((uint8_t*) c, sizeof(config_t) - 4);

        return reqlen;
    }

    return 0;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    if (itf == 1) {
        if (bufsize != sizeof(config_t)) {
            return;
        }
        if (!config_ok((config_t*) buffer)) {
            return;
        }
        memcpy(&config, buffer, bufsize);
        config.wifi_ssid[sizeof(config.wifi_ssid) - 1] = 0;
        config.wifi_password[sizeof(config.wifi_password) - 1] = 0;
        if ((strlen(config.wifi_password) == 0) &&
            config_ok((config_t*) FLASH_CONFIG_IN_MEMORY)) {
            memcpy(config.wifi_password, ((config_t*) FLASH_CONFIG_IN_MEMORY)->wifi_password, sizeof(config.wifi_password));
        }
        persist_config();
    }
}

int main(void) {
    board_init();
    stdio_init_all();
    printf("HID Receiver\n");
    config_init();
    our_descriptor_number = config.our_descriptor_number;
    if (our_descriptor_number >= NOUR_DESCRIPTORS) {
        our_descriptor_number = 0;
    }
    serial_init();
#ifdef NETWORK_ENABLED
    net_init();
#endif
    tusb_init();

    while (true) {
        tud_task();
#ifdef NETWORK_ENABLED
        cyw43_arch_poll();
        net_task();
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, wifi_connected);
#endif
        serial_task();
        if ((or_items > 0) && (tud_hid_n_ready(0))) {
            tud_hid_n_report(0, outgoing_reports[or_head].report_id, outgoing_reports[or_head].data, outgoing_reports[or_head].len);
            or_head = (or_head + 1) % OR_BUFSIZE;
            or_items--;
        }
    }

    return 0;
}
