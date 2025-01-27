import crc32 from './crc.js';

const CONFIG_VERSION = 1;
const CONFIG_SIZE = 64;

// Nordic SPP (Serial Port Profile) Service UUIDs
const NORDIC_SPP_SERVICE = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const NORDIC_SPP_DATA_IN = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';  // RX Characteristic
const NORDIC_SPP_DATA_OUT = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'; // TX Characteristic

// Standard Bluetooth services that might be available
const HID_SERVICE = 0x1812;
const DEVICE_INFO_SERVICE = 0x180a;
const BATTERY_SERVICE = 0x180f;

class BluetoothTerminal {
    constructor() {
        this.device = null;
        this.characteristic = null;
        this.connected = false;
        this.onReceive = null;
    }

    async connect() {
        try {
            // Request the device with Nordic SPP service
            this.device = await navigator.bluetooth.requestDevice({
                filters: [{ name: 'PicoWG' }],
                optionalServices: [NORDIC_SPP_SERVICE]
            });

            console.log('Device selected:', this.device.name);
            const server = await this.device.gatt.connect();
            console.log('Connected to GATT server');

            // List all services
            const services = await server.getPrimaryServices();
            console.log('Available services:');
            
            let servicesText = 'Available Services:\n\n';
            
            for (const service of services) {
                console.log('Service UUID:', service.uuid);
                servicesText += `Service: ${service.uuid}\n`;
                
                // List characteristics for each service
                const characteristics = await service.getCharacteristics();
                console.log('Characteristics:');
                for (const characteristic of characteristics) {
                    const props = Object.keys(characteristic.properties)
                        .filter(p => characteristic.properties[p])
                        .join(', ');
                    console.log('- Characteristic UUID:', characteristic.uuid);
                    console.log('  Properties:', props);
                    servicesText += `  └─ Characteristic: ${characteristic.uuid}\n`;
                    servicesText += `     Properties: ${props}\n`;
                }
                servicesText += '\n';
            }
            
            // Display in UI
            document.getElementById('services_output').textContent = servicesText;

            // Get Nordic SPP service
            const service = await server.getPrimaryService(NORDIC_SPP_SERVICE);
            
            // Get TX characteristic (notifications)
            const txCharacteristic = await service.getCharacteristic(NORDIC_SPP_DATA_OUT);
            await txCharacteristic.startNotifications();
            txCharacteristic.addEventListener('characteristicvaluechanged', this.handleNotification.bind(this));
            
            // Get RX characteristic (write)
            this.characteristic = await service.getCharacteristic(NORDIC_SPP_DATA_IN);
            
            this.connected = true;
            this.device.addEventListener('gattserverdisconnected', this.handleDisconnection.bind(this));
        } catch (error) {
            console.error('Detailed error:', error);
            throw error;
        }
    }

    handleNotification(event) {
        const value = event.target.value;
        const text = new TextDecoder().decode(value);
        if (this.onReceive) {
            this.onReceive(text);
        }
    }

    handleDisconnection() {
        this.connected = false;
        this.device = null;
        this.characteristic = null;
    }

    async send(data) {
        if (!this.connected) {
            throw new Error('Not connected to device');
        }
        const encoder = new TextEncoder();
        const encoded = encoder.encode(data + '\n');
        await this.characteristic.writeValue(encoded);
    }
}

let terminal = null;
let bluetoothDevice = null;

document.addEventListener("DOMContentLoaded", function () {
    document.getElementById("open_device").addEventListener("click", open_device);
    document.getElementById("load_from_device").addEventListener("click", load_from_device);
    document.getElementById("save_to_device").addEventListener("click", save_to_device);
    document.getElementById("send_uart_command").addEventListener("click", send_uart_command);
    document.getElementById("uart_command_input").addEventListener("keypress", function(e) {
        if (e.key === "Enter") {
            send_uart_command();
        }
    });

    device_buttons_set_disabled_state(true);

    if (!("bluetooth" in navigator)) {
        display_error("Your browser doesn't support Web Bluetooth. Try Chrome or Edge.");
    }
    
    // Initialize terminal
    terminal = new BluetoothTerminal();
    
    terminal.onReceive = function(data) {
        const output = document.getElementById("uart_output");
        output.textContent += data + "\n";
        output.scrollTop = output.scrollHeight;
    };
});

async function open_device() {
    clear_error();
    try {
        await terminal.connect();
        device_buttons_set_disabled_state(false);
    } catch (error) {
        console.error('Connection error:', error);
        display_error(error);
        device_buttons_set_disabled_state(true);
    }
}

async function send_uart_command() {
    if (!terminal.connected) {
        display_error("Device not connected");
        return;
    }
    clear_error();

    try {
        const input = document.getElementById("uart_command_input");
        const command = input.value;
        if (!command) return;

        await terminal.send(command);
        input.value = "";
    } catch (e) {
        display_error(e);
    }
}

async function load_from_device() {
    if (!terminal.connected) {
        display_error("Device not connected");
        return;
    }
    clear_error();

    try {
        // Send a command to request current config
        await terminal.send("get_config");
    } catch (e) {
        display_error(e);
    }
}

async function save_to_device() {
    if (!terminal.connected) {
        display_error("Device not connected");
        return;
    }
    clear_error();

    try {
        const descriptor = document.getElementById("our_descriptor_number_dropdown").value;
        await terminal.send(`set_descriptor ${descriptor}`);
    } catch (e) {
        display_error(e);
    }
}

function clear_error() {
    document.getElementById("error").classList.add("d-none");
}

function display_error(message) {
    document.getElementById("error").innerText = message;
    document.getElementById("error").classList.remove("d-none");
}

function device_buttons_set_disabled_state(state) {
    document.getElementById("load_from_device").disabled = state;
    document.getElementById("save_to_device").disabled = state;
    document.getElementById("send_uart_command").disabled = state;
    document.getElementById("uart_command_input").disabled = state;
}