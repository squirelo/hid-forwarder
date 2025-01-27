import crc32 from './crc.js';

const CONFIG_VERSION = 1;
const CONFIG_SIZE = 64;

// Nordic UART Service UUIDs in lowercase for Web Bluetooth compatibility
const NORDIC_UART_SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const NORDIC_UART_RX_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';
const NORDIC_UART_TX_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';

// Standard Bluetooth services that might be available
const HID_SERVICE = 0x1812;
const DEVICE_INFO_SERVICE = 0x180a;
const BATTERY_SERVICE = 0x180f;

let bluetoothDevice = null;
let configCharacteristic = null;

document.addEventListener("DOMContentLoaded", function () {
    document.getElementById("open_device").addEventListener("click", open_device);
    document.getElementById("load_from_device").addEventListener("click", load_from_device);
    document.getElementById("save_to_device").addEventListener("click", save_to_device);

    device_buttons_set_disabled_state(true);

    if ("bluetooth" in navigator) {
        navigator.bluetooth.addEventListener('disconnected', ble_on_disconnect);
    } else {
        display_error("Your browser doesn't support Web Bluetooth. Try Chrome or Edge.");
    }
});

async function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

async function connectWithRetry(server, maxAttempts = 5) {
    for (let attempt = 1; attempt <= maxAttempts; attempt++) {
        try {
            // Ensure we're still connected
            if (!bluetoothDevice.gatt.connected) {
                console.log('Reconnecting to GATT server...');
                await server.connect();
            }

            console.log(`Attempt ${attempt} to get services...`);
            
            // Wait a bit before trying to get services
            await sleep(attempt * 500); // Increase wait time with each attempt
            
            const services = await server.getPrimaryServices();
            console.log('Available services:', services.map(s => s.uuid));
            return services;
        } catch (error) {
            console.log(`Attempt ${attempt} failed:`, error);
            if (attempt < maxAttempts) {
                console.log(`Waiting ${attempt}s before retry...`);
                await sleep(attempt * 1000); // Increase wait time with each attempt
            } else {
                throw error;
            }
        }
    }
}

async function open_device() {
    clear_error();
    try {
        // Disconnect existing device if any
        if (bluetoothDevice) {
            console.log('Disconnecting existing device...');
            if (bluetoothDevice.gatt.connected) {
                await bluetoothDevice.gatt.disconnect();
            }
            bluetoothDevice = null;
        }

        // Try to find the device by name first
        console.log('Requesting Bluetooth device...');
        bluetoothDevice = await navigator.bluetooth.requestDevice({
            filters: [
                { name: 'playAbility' }
            ],
            optionalServices: [NORDIC_UART_SERVICE_UUID]
        });

        console.log('Device selected:', bluetoothDevice.name);

        // Connect to the device
        console.log('Connecting to GATT server...');
        const server = await bluetoothDevice.gatt.connect();
        console.log('Connected to GATT server');

        // Initial delay after connection
        console.log('Waiting for services to initialize...');
        await sleep(2000);

        // Try to discover services
        console.log('Discovering services...');
        const services = await server.getPrimaryServices();
        
        if (!services || services.length === 0) {
            throw new Error('No services found on device');
        }

        // Log all available services
        console.log('Available services:', services.map(s => s.uuid));

        // Try to find our service
        const service = services.find(s => s.uuid === NORDIC_UART_SERVICE_UUID);
        if (!service) {
            throw new Error('Nordic UART service not found. Available services: ' + 
                          services.map(s => s.uuid).join(', '));
        }
        
        console.log('Got Nordic UART service');
        
        // Get both RX and TX characteristics
        const rxCharacteristic = await service.getCharacteristic(NORDIC_UART_RX_UUID);
        const txCharacteristic = await service.getCharacteristic(NORDIC_UART_TX_UUID);
        console.log('Got RX and TX characteristics');
        
        // We'll use RX characteristic for writing (from our perspective)
        configCharacteristic = rxCharacteristic;
        
        // Enable notifications on TX characteristic if needed
        await txCharacteristic.startNotifications();
        txCharacteristic.addEventListener('characteristicvaluechanged', handleNotifications);
        
        device_buttons_set_disabled_state(false);
        bluetoothDevice.addEventListener('gattserverdisconnected', ble_on_disconnect);
        
        console.log('Device setup complete');
    } catch (error) {
        console.error('Connection error:', error);
        display_error(error);
        device_buttons_set_disabled_state(true);
        
        // Clean up on error
        if (bluetoothDevice) {
            try {
                if (bluetoothDevice.gatt.connected) {
                    await bluetoothDevice.gatt.disconnect();
                }
            } catch (e) {
                console.error('Error during cleanup:', e);
            }
        }
        bluetoothDevice = null;
    }
}

function handleNotifications(event) {
    const value = event.target.value;
    // Handle incoming notifications if needed
    console.log('Received notification:', value);
}

async function load_from_device() {
    if (!configCharacteristic) {
        return;
    }
    clear_error();

    try {
        const data = await configCharacteristic.readValue();
        check_crc(data);
        let pos = 0;

        const config_version = data.getUint8(pos++);
        check_received_version(config_version);

        document.getElementById("our_descriptor_number_dropdown").value = data.getUint8(pos++);

        let ssid = '';
        for (let i = 0; i < 20; i++) {
            const c = data.getUint8(pos+i);
            if (c == 0) {
                break;
            }
            ssid += String.fromCharCode(c);
        }
        document.getElementById("wifi_ssid_input").value = ssid;
        pos += 20;

        pos += 24;
        document.getElementById("wifi_password_input").value = '';
    } catch (e) {
        display_error(e);
    }
}

async function save_to_device() {
    if (!configCharacteristic) {
        return;
    }
    clear_error();

    try {
        let buffer = new ArrayBuffer(CONFIG_SIZE);
        let dataview = new DataView(buffer);
        dataview.setUint8(0, CONFIG_VERSION);
        let pos = 1;
        dataview.setUint8(pos++, get_int("our_descriptor_number_dropdown", "our_descriptor_number"));

        const ssid = document.getElementById("wifi_ssid_input").value;
        if (ssid.length > 19) {
            throw new Error('Maximum wifi network name length is 19 characters.');
        }
        for (let i = 0; i < 20; i++) {
            const c = ssid.charCodeAt(i);
            dataview.setUint8(pos++, isNaN(c) ? 0 : c);
        }

        const password = document.getElementById("wifi_password_input").value;
        if (password.length > 23) {
            throw new Error('Maximum wifi password length is 23 characters.');
        }
        for (let i = 0; i < 24; i++) {
            const c = password.charCodeAt(i);
            dataview.setUint8(pos++, isNaN(c) ? 0 : c);
        }

        for (let i = 0; i < 14; i++) {
            dataview.setUint8(pos++, 0);
        }

        add_crc(dataview);

        await configCharacteristic.writeValue(buffer);
    } catch (e) {
        display_error(e);
    }
}

function get_int(element_id, description) {
    const value = parseInt(document.getElementById(element_id).value, 10);
    if (isNaN(value)) {
        throw new Error("Invalid "+description+".");
    }
    return value;
}

function clear_error() {
    document.getElementById("error").classList.add("d-none");
}

function display_error(message) {
    document.getElementById("error").innerText = message;
    document.getElementById("error").classList.remove("d-none");
}

function check_crc(data) {
    if (data.getUint32(CONFIG_SIZE - 4, true) != crc32(data, CONFIG_SIZE - 4)) {
        throw new Error('CRC error.');
    }
}

function add_crc(data) {
    data.setUint32(CONFIG_SIZE - 4, crc32(data, CONFIG_SIZE - 4), true);
}

function check_received_version(config_version) {
    if (config_version != CONFIG_VERSION) {
        throw new Error("Incompatible version.");
    }
}

function ble_on_disconnect() {
    bluetoothDevice = null;
    configCharacteristic = null;
    device_buttons_set_disabled_state(true);
}

function device_buttons_set_disabled_state(state) {
    document.getElementById("load_from_device").disabled = state;
    document.getElementById("save_to_device").disabled = state;
}