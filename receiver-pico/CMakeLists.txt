cmake_minimum_required(VERSION 3.13)

# Set Pico W as the board before including the SDK
set(PICO_BOARD pico_w)

add_compile_definitions(PICO_DEFAULT_UART_BAUD_RATE=921600)

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk")


# Create directories for generated files
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(GATT_HEADER_DIR "${GENERATED_DIR}/receiver_gatt_header")
file(REMOVE_RECURSE "${GATT_HEADER_DIR}")
file(MAKE_DIRECTORY "${GATT_HEADER_DIR}")

# # Enable TinyUSB
# set(PICO_ENABLE_USB 1)

include(pico_sdk_import.cmake)

project(receiver)

pico_sdk_init()

add_executable(receiver
    src/receiver.c
    src/crc.c
    src/globals.c
    src/bt.c
    src/descriptors.c
)

# Add GATT database generation after the executable is defined
pico_btstack_make_gatt_header(receiver PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/receiver.gatt")

target_include_directories(receiver PRIVATE src)
target_include_directories(receiver PRIVATE
    ${PICO_SDK_PATH}/lib/btstack/src
    ${PICO_SDK_PATH}/lib/btstack/3rd-party/micro-ecc
    ${PICO_SDK_PATH}/lib/btstack/platform/embedded
    ${GATT_HEADER_DIR}
    ${PICO_SDK_PATH}/lib/tinyusb/src
    ${PICO_SDK_PATH}/lib/tinyusb/hw
)
target_link_libraries(receiver
    pico_stdlib
    tinyusb_device
    tinyusb_board
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
    add_compile_definitions(BLUETOOTH_ENABLED)
endif()
