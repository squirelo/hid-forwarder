import crc32 from './crc.js';

const dpad_lut = [15, 6, 2, 15, 0, 7, 1, 0, 4, 5, 3, 4, 15, 6, 2, 15];

// Nordic SPP Service UUIDs
const NORDIC_SPP_SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const NORDIC_SPP_TX_CHAR_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'; // Write (RX from device perspective)
const NORDIC_SPP_RX_CHAR_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'; // Notify (TX from device perspective)

// SLIP protocol constants
const END = 0o300;     /* indicates end of packet */
const ESC = 0o333;     /* indicates byte stuffing */
const ESC_END = 0o334; /* ESC ESC_END means END data byte */
const ESC_ESC = 0o335; /* ESC ESC_ESC means ESC data byte */

let device = null;
let server = null;
let service = null;
let txCharacteristic = null; // For sending data to device
let rxCharacteristic = null; // For receiving data from device
let prev_report = new Uint8Array([0, 0, 15, 0, 0, 0, 0, 0, 0]);
let output;
let statusElement;
let connectButton;
let disconnectButton;
let isConnected = false;

document.addEventListener("DOMContentLoaded", function () {
    output = document.getElementById("output");
    statusElement = document.getElementById("status");
    connectButton = document.getElementById("connect_device");
    disconnectButton = document.getElementById("disconnect_device");
    
    connectButton.addEventListener("click", connectToDevice);
    disconnectButton.addEventListener("click", disconnectFromDevice);
    
    // Check if Web Bluetooth is supported
    if (!navigator.bluetooth) {
        updateStatus("Web Bluetooth not supported in this browser", false);
        connectButton.disabled = true;
        return;
    }
    
    setInterval(loop, 8);
});

async function connectToDevice() {
    try {
        connectButton.disabled = true;
        updateStatus("Scanning for devices...", false);
        
        // Request device with more permissive filtering
        device = await navigator.bluetooth.requestDevice({
            // Accept all devices - let user choose
            acceptAllDevices: true,
            optionalServices: [
                NORDIC_SPP_SERVICE_UUID,
                'battery_service',
                'device_information',
                '12345678-1234-1234-1234-123456789abc', // Generic custom service UUID pattern
                '0000180f-0000-1000-8000-00805f9b34fb', // Battery service
                '0000180a-0000-1000-8000-00805f9b34fb', // Device information service
                'generic_access',
                'generic_attribute',
                'be1e8a45-5971-8f2c-39c8-51735e59efa5'  // Add the detected UUID
            ]
        });
        
        updateStatus("Connecting to " + device.name + "...", false);
        write("Selected device: " + device.name + " (ID: " + device.id + ")\n");
        
        // Connect to GATT server
        server = await device.gatt.connect();
        write("GATT server connected successfully\n");
        
        // Discover all available services
        write("\n=== Discovering services ===\n");
        const services = await server.getPrimaryServices();
        
        write("Found " + services.length + " service(s):\n");
        let nordicSppService = null;
        let allServices = [];
        
        for (const srv of services) {
            const uuid = srv.uuid.toLowerCase();
            write("📡 Service: " + uuid + "\n");
            
            let serviceInfo = {
                service: srv,
                uuid: uuid,
                characteristics: []
            };
            
            // Check if this is the Nordic SPP service
            const normalizedSppUuid = NORDIC_SPP_SERVICE_UUID.toLowerCase();
            if (uuid === normalizedSppUuid) {
                write("  ✅ Found Nordic SPP service!\n");
                nordicSppService = srv;
            }
            
            // Try to get characteristics for each service
            try {
                const characteristics = await srv.getCharacteristics();
                write("  📋 " + characteristics.length + " characteristic(s):\n");
                
                for (const char of characteristics) {
                    const charUuid = char.uuid.toLowerCase();
                    write("    🔧 " + charUuid + " → ");
                    const props = [];
                    if (char.properties.read) props.push("read");
                    if (char.properties.write) props.push("write");
                    if (char.properties.writeWithoutResponse) props.push("writeWithoutResponse");
                    if (char.properties.notify) props.push("notify");
                    if (char.properties.indicate) props.push("indicate");
                    if (char.properties.authenticatedSignedWrites) props.push("authenticatedSignedWrites");
                    if (char.properties.reliableWrite) props.push("reliableWrite");
                    if (char.properties.writableAuxiliaries) props.push("writableAuxiliaries");
                    
                    write("(" + props.join(", ") + ")\n");
                    
                    serviceInfo.characteristics.push({
                        uuid: charUuid,
                        properties: char.properties,
                        characteristic: char
                    });
                    
                    // Check if this matches Nordic SPP characteristics
                    if (charUuid === NORDIC_SPP_TX_CHAR_UUID.toLowerCase()) {
                        write("      🎯 This is Nordic SPP TX (write to device)\n");
                    }
                    if (charUuid === NORDIC_SPP_RX_CHAR_UUID.toLowerCase()) {
                        write("      🎯 This is Nordic SPP RX (notifications from device)\n");
                    }
                }
                
            } catch (e) {
                write("  ❌ Could not read characteristics: " + e.message + "\n");
            }
            
            allServices.push(serviceInfo);
            write("\n");
        }
        
        // Use Nordic SPP service if found
        if (!nordicSppService) {
            write("❌ Nordic SPP service not found!\n");
            write("Expected UUID: " + NORDIC_SPP_SERVICE_UUID + "\n\n");
            write("Summary of all services:\n");
            for (const serviceInfo of allServices) {
                write("  🔸 " + serviceInfo.uuid + " - " + serviceInfo.characteristics.length + " characteristics\n");
            }
            write("\n🔍 Troubleshooting:\n");
            write("1. Make sure your HID receiver is built with BLUETOOTH_ENABLED\n");
            write("2. Ensure it's running in BLE mode (not Classic Bluetooth)\n");
            write("3. Check if the GATT profile compiled correctly\n");
            write("4. Verify the device is actually a HID receiver with Nordic SPP service\n\n");
            throw new Error("Device does not have Nordic SPP service. Check debug output above.");
        }
        
        // Use the Nordic SPP service
        service = nordicSppService;
        write("🔗 Using Nordic SPP service: " + NORDIC_SPP_SERVICE_UUID + "\n");
        
        // Get Nordic SPP characteristics
        try {
            txCharacteristic = await service.getCharacteristic(NORDIC_SPP_TX_CHAR_UUID);
            write("✅ Found TX characteristic: " + NORDIC_SPP_TX_CHAR_UUID + "\n");
        } catch (e) {
            throw new Error("Failed to get TX characteristic: " + e.message);
        }
        
        try {
            rxCharacteristic = await service.getCharacteristic(NORDIC_SPP_RX_CHAR_UUID);
            write("✅ Found RX characteristic: " + NORDIC_SPP_RX_CHAR_UUID + "\n");
            
            // Subscribe to notifications from RX characteristic
            try {
                await rxCharacteristic.startNotifications();
                rxCharacteristic.addEventListener('characteristicvaluechanged', handleNotification);
                write("🔔 Subscribed to notifications\n");
            } catch (e) {
                write("⚠️ Could not subscribe to notifications: " + e.message + "\n");
            }
        } catch (e) {
            write("⚠️ RX characteristic not available: " + e.message + "\n");
            rxCharacteristic = null;
        }
        
        // Handle disconnection
        device.addEventListener('gattserverdisconnected', onDisconnected);
        
        isConnected = true;
        updateStatus("Connected to " + device.name, true);
        connectButton.disabled = true;
        disconnectButton.disabled = false;
        
        write("🎉 Successfully connected to " + device.name + "!\n");
        // write("Using service: " + detectedServiceUuid + "\n"); // This line is removed as per the new_code
        write("TX characteristic: " + NORDIC_SPP_TX_CHAR_UUID + "\n");
        write("RX characteristic: " + NORDIC_SPP_RX_CHAR_UUID + "\n");
        write("Ready to send gamepad data...\n\n");
        
    } catch (error) {
        console.error('Connection failed:', error);
        updateStatus("Connection failed: " + error.message, false);
        connectButton.disabled = false;
        disconnectButton.disabled = true;
        isConnected = false;
        write("💥 Connection failed: " + error.message + "\n");
    }
}

async function disconnectFromDevice() {
    try {
        if (server && server.connected) {
            await server.disconnect();
        }
    } catch (error) {
        console.error('Disconnect error:', error);
    }
}

function onDisconnected() {
    isConnected = false;
    updateStatus("Disconnected", false);
    connectButton.disabled = false;
    disconnectButton.disabled = true;
    write("Device disconnected\n");
}

function handleNotification(event) {
    const value = event.target.value;
    const data = new Uint8Array(value.buffer);
    
    // Handle incoming data from device (if needed)
    write("Received: ");
    for (let i = 0; i < data.length; i++) {
        write(data[i].toString(16).padStart(2, '0') + " ");
    }
    write("\n");
}

function updateStatus(message, connected) {
    statusElement.textContent = message;
    statusElement.className = connected ? "status connected" : "status disconnected";
}

function encodeSlipPacket(data) {
    let encoded = [END];
    
    for (let i = 0; i < data.length; i++) {
        const byte = data[i];
        switch (byte) {
            case END:
                encoded.push(ESC, ESC_END);
                break;
            case ESC:
                encoded.push(ESC, ESC_ESC);
                break;
            default:
                encoded.push(byte);
        }
    }
    
    encoded.push(END);
    return new Uint8Array(encoded);
}

async function sendReport(report) {
    if (!isConnected || !txCharacteristic) {
        return;
    }

    try {
        // Create packet with header and CRC
        let data = new Uint8Array(4 + 8 + 4);
        data[0] = 1;   // Packet type
        data[1] = 2;   // Version
        data[2] = 8;   // Payload length
        data[3] = 0;   // Reserved
        data.set(report, 4);
        
        // Calculate CRC32
        const crc = crc32(new DataView(data.buffer), 12);
        data[12] = (crc >> 0) & 0xFF;
        data[13] = (crc >> 8) & 0xFF;
        data[14] = (crc >> 16) & 0xFF;
        data[15] = (crc >> 24) & 0xFF;

        // Encode with SLIP protocol
        const encodedData = encodeSlipPacket(data);
        
        // Send via BLE
        await txCharacteristic.writeValue(encodedData);
        
    } catch (error) {
        console.error('Failed to send report:', error);
        write("Send error: " + error.message + "\n");
    }
}

async function loop() {
    try {
        clear_output();
        
        if (isConnected) {
            write("🟢 CONNECTED to " + (device?.name || "HID Receiver") + "\n\n");
        } else {
            write("🔴 NOT CONNECTED\n\n");
        }

        // Initialize button states
        let b = false, a = false, y = false, x = false;
        let l = false, r = false, zl = false, zr = false;
        let minus = false, plus = false, ls = false, rs = false;
        let home = false, capture = false;
        let dpad_left = false, dpad_right = false, dpad_up = false, dpad_down = false;
        let lx = 128, ly = 128, rx = 128, ry = 128;

        // Process all connected gamepads
        for (const gamepad of navigator.getGamepads()) {
            if (!gamepad) {
                continue;
            }
            
            write("🎮 " + gamepad.id + "\n");
            
            if ((gamepad.mapping === 'standard') && !gamepad.id.includes('HID Receiver')) {
                // Display current button/axis values
                write("Buttons: ");
                for (const button of gamepad.buttons) {
                    write(button.value.toFixed(2) + " ");
                }
                write("\nAxes: ");
                for (const axis of gamepad.axes) {
                    write(axis.toFixed(2) + " ");
                }
                write("\n");

                // Map buttons to Nintendo Switch Pro Controller layout
                b |= gamepad.buttons[0].value > 0.5;      // A button
                a |= gamepad.buttons[1].value > 0.5;      // B button  
                y |= gamepad.buttons[2].value > 0.5;      // X button
                x |= gamepad.buttons[3].value > 0.5;      // Y button
                l |= gamepad.buttons[4].value > 0.5;      // L button
                r |= gamepad.buttons[5].value > 0.5;      // R button
                zl |= gamepad.buttons[6].value > 0.25;    // ZL trigger
                zr |= gamepad.buttons[7].value > 0.25;    // ZR trigger
                minus |= gamepad.buttons[8].value > 0.5;  // Select/Back
                plus |= gamepad.buttons[9].value > 0.5;   // Start/Menu
                ls |= gamepad.buttons[10].value > 0.5;    // Left stick
                rs |= gamepad.buttons[11].value > 0.5;    // Right stick
                home |= gamepad.buttons[16]?.value > 0.5; // Home button
                
                // D-pad
                dpad_up |= gamepad.buttons[12]?.value > 0.5;
                dpad_down |= gamepad.buttons[13]?.value > 0.5;
                dpad_left |= gamepad.buttons[14]?.value > 0.5;
                dpad_right |= gamepad.buttons[15]?.value > 0.5;
                
                // Analog sticks (convert from -1..1 to 0..255)
                lx = Math.max(0, Math.min(255, Math.round((gamepad.axes[0] + 1) * 127.5)));
                ly = Math.max(0, Math.min(255, Math.round((gamepad.axes[1] + 1) * 127.5)));
                rx = Math.max(0, Math.min(255, Math.round((gamepad.axes[2] + 1) * 127.5)));
                ry = Math.max(0, Math.min(255, Math.round((gamepad.axes[3] + 1) * 127.5)));
                
            } else {
                write("⚠️ IGNORED (not standard mapping or HID Receiver)\n");
            }
            write("\n");
        }

        // Build HID report
        let report = new Uint8Array(8);
        
        report[0] = (y << 0) | (b << 1) | (a << 2) | (x << 3) | (l << 4) | (r << 5) | (zl << 6) | (zr << 7);
        report[1] = (minus << 0) | (plus << 1) | (ls << 2) | (rs << 3) | (home << 4) | (capture << 5);
        report[2] = dpad_lut[(dpad_left << 0) | (dpad_right << 1) | (dpad_up << 2) | (dpad_down << 3)];
        report[3] = lx;
        report[4] = ly;
        report[5] = rx;
        report[6] = ry;
        report[7] = 0; // Reserved

        write("📤 HID REPORT: ");
        for (let i = 0; i < 8; i++) {
            write(report[i].toString(16).padStart(2, '0') + " ");
        }
        write("\n");

        // Send report if it changed
        if (!reportsEqual(prev_report, report)) {
            await sendReport(report);
            prev_report = report.slice(); // Create copy
            write("✅ Report sent\n");
        } else {
            write("⏸️ No change\n");
        }
        
    } catch (error) {
        console.error('Loop error:', error);
        write("❌ Error: " + error.message + "\n");
    }
}

function write(text) {
    output.textContent += text;
}

function clear_output() {
    output.textContent = '';
}

function reportsEqual(a, b) {
    if (a.length !== b.length) {
        return false;
    }
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i]) {
            return false;
        }
    }
    return true;
}