import crc32 from './crc.js';

const CONFIG_VERSION = 2;
const CONFIG_SIZE = 63;
const REPORT_ID_CONFIG = 1;
const REPORT_ID_COMMAND = 2;
const COMMAND_PAIR_NEW_DEVICE = 1;
const COMMAND_FORGET_ALL_DEVICES = 2;
const BLUETOOTH_ENABLED_FLAG_MASK = (1 << 0);
const WIFI_ENABLED_FLAG_MASK = (1 << 1);

let device = null;

document.addEventListener("DOMContentLoaded", function () {
    document.getElementById("open_device").addEventListener("click", open_device);
    document.getElementById("load_from_device").addEventListener("click", load_from_device);
    document.getElementById("save_to_device").addEventListener("click", save_to_device);
    document.getElementById("pair_new_device").addEventListener("click", pair_new_device);
    document.getElementById("forget_all_devices").addEventListener("click", forget_all_devices);

    device_buttons_set_disabled_state(true);

    if ("hid" in navigator) {
        navigator.hid.addEventListener('disconnect', hid_on_disconnect);
    } else {
        display_error("Your browser doesn't support WebHID. Try Chrome (desktop version) or a Chrome-based browser.");
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
        document.getElementById("wifi_enabled_checkbox").checked = ((flags & WIFI_ENABLED_FLAG_MASK) != 0);
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

        let flags = 0;
        if (document.getElementById("bluetooth_enabled_checkbox").checked) {
            flags |= BLUETOOTH_ENABLED_FLAG_MASK;
        }
        if (document.getElementById("wifi_enabled_checkbox").checked) {
            flags |= WIFI_ENABLED_FLAG_MASK;
        }
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