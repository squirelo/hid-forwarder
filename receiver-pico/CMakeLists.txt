cmake_minimum_required(VERSION 3.13)

# Set Pico W as the board before including the SDK
set(PICO_BOARD pico_w)

add_compile_definitions(PICO_DEFAULT_UART_BAUD_RATE=921600)

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk")

# Configure CYW43 architecture
set(PICO_CYW43_ARCH_THREADSAFE_BACKGROUND 1)

include(pico_sdk_import.cmake)

project(receiver)

pico_sdk_init()

add_executable(receiver
    src/receiver.c
    src/crc.c
    src/globals.c
)
target_include_directories(receiver PRIVATE src)
target_include_directories(receiver PRIVATE
    ${PICO_SDK_PATH}/lib/btstack/src
    ${PICO_SDK_PATH}/lib/btstack/3rd-party/micro-ecc
    ${PICO_SDK_PATH}/lib/btstack/platform/embedded
)
target_link_libraries(receiver
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    pico_btstack_ble
    pico_btstack_cyw43
    hardware_flash
    hardware_gpio
    hardware_uart
    hardware_sync
)
pico_add_extra_outputs(receiver)

if (PICO_CYW43_SUPPORTED)
    add_compile_definitions(NETWORK_ENABLED)
endif()
