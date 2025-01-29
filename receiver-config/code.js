import crc32 from './crc.js';

const CONFIG_VERSION = 2;
const CONFIG_SIZE = 63;
const REPORT_ID_CONFIG = 1;
const REPORT_ID_COMMAND = 2;
const COMMAND_PAIR_NEW_DEVICE = 1;
const COMMAND_FORGET_ALL_DEVICES = 2;
const BLUETOOTH_ENABLED_FLAG_MASK = (1 << 0);

let device = null;
let bleDevice = null;
let bleServer = null;

document.addEventListener("DOMContentLoaded", function () {
    document.getElementById("open_device").addEventListener("click", open_device);
    document.getElementById("load_from_device").addEventListener("click", load_from_device);
    document.getElementById("save_to_device").addEventListener("click", save_to_device);
    document.getElementById("pair_new_device").addEventListener("click", pair_new_device);
    document.getElementById("forget_all_devices").addEventListener("click", forget_all_devices);
    document.getElementById("connect_ble").addEventListener("click", connect_ble_device);

    device_buttons_set_disabled_state(true);

    if ("hid" in navigator) {
        navigator.hid.addEventListener('disconnect', hid_on_disconnect);
    } else {
        display_error("Your browser doesn't support WebHID. Try Chrome (desktop version) or a Chrome-based browser.");
    }

    // Check if Web Bluetooth is supported
    if (!navigator.bluetooth) {
        display_error("Web Bluetooth is not supported in your browser");
    }
});

async function open_device() {
    clear_error();
    let success = false;
    const devices = await navigator.hid.requestDevice({
        filters: [{ usagePage: 0xFF00, usage: 0x0022 }]
    }).catch((err) => { display_error(err); });
    const config_interface = devices?.find(d => d.collections.some(c => c.usagePage == 0xff00));
    if (config_interface !== undefined) {
        device = config_interface;
        if (!device.opened) {
            await device.open().catch((err) => { display_error(err + "\nIf you're on Linux, you might need to give yourself permissions to the appropriate /dev/hidraw* device."); });
        }
        success = device.opened;
    }

    device_buttons_set_disabled_state(!success);

    if (!success) {
        device = null;
    }
}

async function load_from_device() {
    if (device == null) {
        return;
    }
    clear_error();

    try {
        const data_with_report_id = await device.receiveFeatureReport(REPORT_ID_CONFIG);
        const data = new DataView(data_with_report_id.buffer, 1);
        // console.log(data);
        check_crc(data);
        let pos = 0;

        const config_version = data.getUint8(pos++);
        check_received_version(config_version);

        document.getElementById("our_descriptor_number_dropdown").value = data.getUint8(pos++);

        let ssid = '';
        for (let i = 0; i < 20; i++) {
            const c = data.getUint8(pos + i);
            if (c == 0) {
                break;
            }
            ssid += String.fromCharCode(c);
        }
        document.getElementById("wifi_ssid_input").value = ssid;
        pos += 20;

        pos += 24;
        document.getElementById("wifi_password_input").value = '';

        const flags = data.getUint8(pos++);
        document.getElementById("bluetooth_enabled_checkbox").checked = ((flags & BLUETOOTH_ENABLED_FLAG_MASK) != 0);
    } catch (e) {
        display_error(e);
    }
}

async function save_to_device() {
    if (device == null) {
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

        const flags = document.getElementById("bluetooth_enabled_checkbox").checked ? BLUETOOTH_ENABLED_FLAG_MASK : 0;
        dataview.setUint8(pos++, flags);

        for (let i = 0; i < 12; i++) {
            dataview.setUint8(pos++, 0);
        }

        add_crc(dataview);

        // console.log(buffer);

        await device.sendFeatureReport(REPORT_ID_CONFIG, buffer);

    } catch (e) {
        display_error(e);
    }
}

function get_int(element_id, description) {
    const value = parseInt(document.getElementById(element_id).value, 10);
    if (isNaN(value)) {
        throw new Error("Invalid " + description + ".");
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

function hid_on_disconnect(event) {
    if (event.device === device) {
        device = null;
        device_buttons_set_disabled_state(true);
    }
}

function device_buttons_set_disabled_state(state) {
    document.getElementById("load_from_device").disabled = state;
    document.getElementById("save_to_device").disabled = state;
    document.getElementById("pair_new_device").disabled = state;
    document.getElementById("forget_all_devices").disabled = state;
}

async function send_feature_command(command) {
    let buffer = new ArrayBuffer(CONFIG_SIZE);
    let dataview = new DataView(buffer);
    dataview.setUint8(0, CONFIG_VERSION);
    dataview.setUint8(1, command);
    add_crc(dataview);
    await device.sendFeatureReport(REPORT_ID_COMMAND, buffer);
}

async function pair_new_device() {
    await send_feature_command(COMMAND_PAIR_NEW_DEVICE);
}

async function forget_all_devices() {
    await send_feature_command(COMMAND_FORGET_ALL_DEVICES);
}

async function connect_ble_device() {
    try {
        clear_error();
        
        // Request BLE devices with PicoWG in their name
        bleDevice = await navigator.bluetooth.requestDevice({
            filters: [{
                namePrefix: 'PicoWG'
            }],
            optionalServices: [
                '0000180a-0000-1000-8000-00805f9b34fb',
                '00001812-0000-1000-8000-00805f9b34fb',
                '9e400000-b5a3-f393-e0a9-e50e24dcca9e'
            ]
        });

        // Add event listener for disconnection
        bleDevice.addEventListener('gattserverdisconnected', onDisconnected);

        // Connect to the device
        console.log('Connecting to GATT Server...');
        bleServer = await bleDevice.gatt.connect();

        // Get all services
        const services = await bleServer.getPrimaryServices();
        
        // Create a list to show the services
        let serviceList = document.createElement('ul');
        for (const service of services) {
            const serviceLi = document.createElement('li');
            serviceLi.textContent = `Service: ${service.uuid}`;
            
            // Get characteristics for each service
            const characteristics = await service.getCharacteristics();
            if (characteristics.length > 0) {
                const charList = document.createElement('ul');
                for (const characteristic of characteristics) {
                    const charLi = document.createElement('li');
                    charLi.textContent = `Characteristic: ${characteristic.uuid}
                        Properties: ${Object.keys(characteristic.properties).filter(p => characteristic.properties[p]).join(', ')}`;
                    charList.appendChild(charLi);
                }
                serviceLi.appendChild(charList);
            }
            serviceList.appendChild(serviceLi);
        }

        // Add the list to the page
        const servicesDiv = document.getElementById('ble_services') || document.createElement('div');
        servicesDiv.id = 'ble_services';
        servicesDiv.innerHTML = '<h3>BLE Services:</h3>';
        servicesDiv.appendChild(serviceList);
        document.body.appendChild(servicesDiv);

        console.log('Connected to device:', bleDevice.name);
    } catch (error) {
        console.error('Error:', error);
        display_error(error);
    }
}

function onDisconnected() {
    console.log('Device disconnected');
    bleDevice = null;
    bleServer = null;
    
    // Clear the services list
    const servicesDiv = document.getElementById('ble_services');
    if (servicesDiv) {
        servicesDiv.remove();
    }
}