import crc32 from './crc.js';

const dpad_lut = [15, 6, 2, 15, 0, 7, 1, 0, 4, 5, 3, 4, 15, 6, 2, 15];

// BLE Service and Characteristic UUIDs
// Using Nordic UART Service UUIDs as they're commonly supported
const UART_SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const UART_TX_CHARACTERISTIC_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'; // Write
const UART_RX_CHARACTERISTIC_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'; // Notify

const END = 0o300;     /* indicates end of packet */
const ESC = 0o333;     /* indicates byte stuffing */
const ESC_END = 0o334; /* ESC ESC_END means END data byte */
const ESC_ESC = 0o335; /* ESC ESC_ESC means ESC data byte */

// Virtual button states
let virtualButtonA = false;

document.addEventListener("DOMContentLoaded", function () {
    document.getElementById("connect_ble").addEventListener("click", connect_ble);
    document.getElementById("disconnect_ble").addEventListener("click", disconnect_ble);
    
    // Virtual button A event handlers - toggle mode
    const buttonA = document.getElementById("button_a");
    buttonA.addEventListener("click", function(e) {
        e.preventDefault();
        virtualButtonA = !virtualButtonA; // Toggle the state
        
        if (virtualButtonA) {
            buttonA.classList.add("pressed");
            buttonA.textContent = "Button A (ON)";
        } else {
            buttonA.classList.remove("pressed");
            buttonA.textContent = "Button A";
        }
    });
    
    output = document.getElementById("output");
    setInterval(loop, 8);
});

// Check if Web Bluetooth is supported
if (!navigator.bluetooth) {
    document.body.innerHTML = '<h1>Web Bluetooth API not supported</h1><p>Please use Chrome/Edge with HTTPS or enable experimental features.</p>';
}

let device = null;
let server = null;
let service = null;
let txCharacteristic = null;
let rxCharacteristic = null;
let prev_report = new Uint8Array([0, 0, 15, 0, 0, 0, 0, 0, 0]);
let output;

async function connect_ble() {
    try {
        write("Searching for BLE devices...\n");
        
        // Request a device with more flexible filtering
        // This allows connecting to devices like "playAbility" that might not advertise the service UUID
        device = await navigator.bluetooth.requestDevice({
            // Accept all devices but prefer those with UART service or specific names
            acceptAllDevices: true,
            optionalServices: [UART_SERVICE_UUID]
        });

        write(`Selected device: ${device.name || 'Unknown'}\n`);
        
        // Connect to GATT server
        server = await device.gatt.connect();
        write("Connected to GATT server\n");
        
        // Try to get the UART service
        try {
            service = await server.getPrimaryService(UART_SERVICE_UUID);
            write("Found UART service\n");
        } catch (serviceError) {
            write(`UART service not found: ${serviceError.message}\n`);
            throw new Error("Device doesn't have Nordic UART Service. Please select a compatible device.");
        }
        
        // Get the TX characteristic (for writing data to device)
        try {
            txCharacteristic = await service.getCharacteristic(UART_TX_CHARACTERISTIC_UUID);
            write("Got TX characteristic\n");
        } catch (txError) {
            write(`TX characteristic error: ${txError.message}\n`);
            throw new Error("Cannot find TX characteristic");
        }
        
        // Get the RX characteristic (for reading data from device)
        try {
            rxCharacteristic = await service.getCharacteristic(UART_RX_CHARACTERISTIC_UUID);
            await rxCharacteristic.startNotifications();
            rxCharacteristic.addEventListener('characteristicvaluechanged', handleNotification);
            write("Got RX characteristic with notifications\n");
        } catch (rxError) {
            write("RX characteristic not available (write-only mode)\n");
            // This is OK, the device might be write-only
        }
        
        // Update UI
        document.getElementById("connect_ble").style.display = "none";
        document.getElementById("disconnect_ble").style.display = "inline";
        
        write("BLE connection established!\n\n");
        
    } catch (error) {
        write(`Error: ${error.message}\n`);
        console.error('BLE connection error:', error);
        
        // Clean up on error
        if (server && server.connected) {
            server.disconnect();
        }
        device = null;
        server = null;
        service = null;
        txCharacteristic = null;
        rxCharacteristic = null;
    }
}

async function disconnect_ble() {
    try {
        if (rxCharacteristic) {
            await rxCharacteristic.stopNotifications();
            rxCharacteristic.removeEventListener('characteristicvaluechanged', handleNotification);
        }
        if (server && server.connected) {
            server.disconnect();
        }
        
        device = null;
        server = null;
        service = null;
        txCharacteristic = null;
        rxCharacteristic = null;
        
        // Update UI
        document.getElementById("connect_ble").style.display = "inline";
        document.getElementById("disconnect_ble").style.display = "none";
        
        write("Disconnected from BLE device\n\n");
        
    } catch (error) {
        write(`Disconnect error: ${error.message}\n`);
        console.error('BLE disconnect error:', error);
    }
}

function handleNotification(event) {
    const value = event.target.value;
    const decoder = new TextDecoder();
    const data = decoder.decode(value);
    write(`Received: ${data}\n`);
}

let transmit_buffer = [];

function ble_write(c) {
    transmit_buffer.push(c);
}

async function flush() {
    if (!txCharacteristic || transmit_buffer.length === 0) {
        return;
    }
    
    try {
        // BLE characteristics typically have a 20-byte MTU limit
        const MTU_SIZE = 20;
        const data = new Uint8Array(transmit_buffer);
        
        // Send data in chunks if it exceeds MTU
        for (let i = 0; i < data.length; i += MTU_SIZE) {
            const chunk = data.slice(i, i + MTU_SIZE);
            await txCharacteristic.writeValue(chunk);
        }
        
        transmit_buffer = [];
    } catch (error) {
        write(`Write error: ${error.message}\n`);
        console.error('BLE write error:', error);
    }
}

function send_escaped_byte(b) {
    switch (b) {
        case END:
            ble_write(ESC);
            ble_write(ESC_END);
            break;

        case ESC:
            ble_write(ESC);
            ble_write(ESC_ESC);
            break;

        default:
            ble_write(b);
    }
}

async function send_report(report) {
    if (!server || !server.connected || !txCharacteristic) {
        return;
    }

    let data = new Uint8Array(4 + 8 + 4);
    data[0] = 1;
    data[1] = 2;
    data[2] = 8;
    data[3] = 0
    data.set(report, 4);
    const crc = crc32(new DataView(data.buffer), 12);
    data[12] = (crc >> 0) & 0xFF;
    data[13] = (crc >> 8) & 0xFF;
    data[14] = (crc >> 16) & 0xFF;
    data[15] = (crc >> 24) & 0xFF;

    ble_write(END);
    for (let i = 0; i < 16; i++) {
        send_escaped_byte(data[i]);
    }
    ble_write(END);
    await flush();
}

async function loop() {
    try {
        clear_output();
        if (server && server.connected) {
            write(`BLE CONNECTED (${device.name || 'Unknown'})\n\n`);
        } else {
            write("BLE NOT CONNECTED\n\n");
        }

        let b = false;
        let a = false;
        let y = false;
        let x = false;
        let l = false;
        let r = false;
        let zl = false;
        let zr = false;
        let minus = false;
        let plus = false;
        let ls = false;
        let rs = false;
        let home = false;
        let capture = false;
        let dpad_left = false;
        let dpad_right = false;
        let dpad_up = false;
        let dpad_down = false;
        let lx = 128;
        let ly = 128;
        let rx = 128;
        let ry = 128;
        
        // Process physical gamepads
        for (const gamepad of navigator.getGamepads()) {
            if (!gamepad) {
                continue;
            }
            write(gamepad.id);
            write("\n");
            if ((gamepad.mapping == 'standard') && !gamepad.id.includes('HID Receiver')) {
                for (const b of gamepad.buttons) {
                    write(b.value);
                    write(" ");
                }
                for (const b of gamepad.axes) {
                    write(b);
                    write(" ");
                }
                write("\n");
                b |= gamepad.buttons[0].value;
                a |= gamepad.buttons[1].value;
                y |= gamepad.buttons[2].value;
                x |= gamepad.buttons[3].value;
                l |= gamepad.buttons[4].value;
                r |= gamepad.buttons[5].value;
                zl |= gamepad.buttons[6].value > 0.25;
                zr |= gamepad.buttons[7].value > 0.25;
                minus |= gamepad.buttons[8].value;
                plus |= gamepad.buttons[9].value;
                ls |= gamepad.buttons[10].value;
                rs |= gamepad.buttons[11].value;
                home |= gamepad.buttons[16].value;
                dpad_up |= gamepad.buttons[12].value;
                dpad_down |= gamepad.buttons[13].value;
                dpad_left |= gamepad.buttons[14].value;
                dpad_right |= gamepad.buttons[15].value;
                lx += gamepad.axes[0] * 128;
                ly += gamepad.axes[1] * 128;
                rx += gamepad.axes[2] * 128;
                ry += gamepad.axes[3] * 128;
            } else {
                write("IGNORED\n");
            }
            write("\n");
        }
        
        // Include virtual button states
        a |= virtualButtonA;
        
        // Show virtual button status
        if (virtualButtonA) {
            write("VIRTUAL: Button A PRESSED\n");
        }

        let report = new Uint8Array(8);

        report[0] = (y << 0) | (b << 1) | (a << 2) | (x << 3) | (l << 4) | (r << 5) | (zl << 6) | (zr << 7);
        report[1] = (minus << 0) | (plus << 1) | (ls << 2) | (rs << 3) | (home << 4) | (capture << 5);
        report[2] = dpad_lut[(dpad_left << 0) | (dpad_right << 1) | (dpad_up << 2) | (dpad_down << 3)];
        report[3] = Math.max(0, Math.min(255, lx));
        report[4] = Math.max(0, Math.min(255, ly));
        report[5] = Math.max(0, Math.min(255, rx));
        report[6] = Math.max(0, Math.min(255, ry));

        write("OUTPUT\n");
        for (let i = 0; i < 8; i++) {
            write(report[i].toString(16).padStart(2, '0'));
            write(" ");
        }
        write("\n");

        if (!reports_equal(prev_report, report)) {
            await send_report(report);
            prev_report = report;
        }
    } catch (e) {
        console.log(e);
    }
}

function write(s) {
    output.innerText += s;
}

function clear_output() {
    output.innerHTML = '';
}

function reports_equal(a, b) {
    if (a.length != b.length) {
        return false;
    }
    for (let i = 0; i < a.length; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}