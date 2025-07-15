cmake_minimum_required(VERSION 3.13)

add_compile_definitions(PICO_DEFAULT_UART_BAUD_RATE=921600)

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk")

include(pico_sdk_import.cmake)

project(receiver)

pico_sdk_init()

# Compile GATT file
find_package(Python3 REQUIRED)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/hid_receiver.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/pico-sdk/lib/btstack/tool/compile_gatt.py
            ${CMAKE_CURRENT_SOURCE_DIR}/src/hid_receiver.gatt
            ${CMAKE_CURRENT_BINARY_DIR}/hid_receiver.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/hid_receiver.gatt
    COMMENT "Compiling GATT file"
)

add_executable(receiver
    src/receiver.c
    src/crc.c
    src/descriptors.c
    src/globals.c
    src/bt.c
    ${CMAKE_CURRENT_BINARY_DIR}/hid_receiver.h
)
target_include_directories(receiver PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(receiver
    pico_stdlib
    tinyusb_device
    tinyusb_board
    $<$<BOOL:${PICO_CYW43_SUPPORTED}>:pico_cyw43_arch_lwip_poll>
    $<$<BOOL:${PICO_CYW43_SUPPORTED}>:pico_btstack_cyw43>
    $<$<BOOL:${PICO_CYW43_SUPPORTED}>:pico_btstack_classic>
    $<$<BOOL:${PICO_CYW43_SUPPORTED}>:pico_btstack_ble>
)
pico_add_extra_outputs(receiver)

if (PICO_CYW43_SUPPORTED)
add_compile_definitions(NETWORK_ENABLED)
add_compile_definitions(BLUETOOTH_ENABLED)
endif()
