import crc32 from './crc.js';

const CONFIG_VERSION = 2;
const CONFIG_SIZE = 63;
const REPORT_ID_COMMAND = 2;
const COMMAND_PAIR_NEW_DEVICE = 1;
const COMMAND_FORGET_ALL_DEVICES = 2;

let device = null;

document.addEventListener("DOMContentLoaded", function () {
    document.getElementById("open_device").addEventListener("click", open_device);
    document.getElementById("pair_new_device").addEventListener("click", pair_new_device);
    document.getElementById("forget_all_devices").addEventListener("click", forget_all_devices);

    device_buttons_set_disabled_state(true);
    update_device_status();

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
    update_device_status();

    if (!success) {
        device = null;
    }
}

function clear_error() {
    document.getElementById("error").classList.add("d-none");
}

function display_error(message) {
    document.getElementById("error").innerText = message;
    document.getElementById("error").classList.remove("d-none");
}

function add_crc(data) {
    data.setUint32(CONFIG_SIZE - 4, crc32(data, CONFIG_SIZE - 4), true);
}

function hid_on_disconnect(event) {
    if (event.device === device) {
        device = null;
        device_buttons_set_disabled_state(true);
        update_device_status();
    }
}

function device_buttons_set_disabled_state(state) {
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

function update_device_status() {
    const deviceStatusElement = document.getElementById("device_status");
    const deviceNameElement = document.getElementById("device_name");

    if (device && device.opened) {
        deviceNameElement.textContent = device.productName || "Unknown Device";
        deviceStatusElement.classList.remove("d-none");
    } else {
        deviceStatusElement.classList.add("d-none");
    }
}