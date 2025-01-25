import crc32 from './crc.js';

const dpad_lut = [15, 6, 2, 15, 0, 7, 1, 0, 4, 5, 3, 4, 15, 6, 2, 15];

const END = 0o300;     /* indicates end of packet */
const ESC = 0o333;     /* indicates byte stuffing */
const ESC_END = 0o334; /* ESC ESC_END means END data byte */
const ESC_ESC = 0o335; /* ESC ESC_ESC means ESC data byte */

document.addEventListener("DOMContentLoaded", function () {
    document.getElementById("select_device").addEventListener("click", select_device);
    output = document.getElementById("output");
    // setInterval(loop, 8);
    loop();
});

if (!("serial" in navigator)) {
    // TODO
}

let port = null;
let prev_report = new Uint8Array([0, 0, 15, 0, 0, 0, 0, 0, 0]);
let output;

async function select_device() {
    if (port && port.connected) {
        port.close();
    }
    port = await navigator.serial.requestPort();
    await port.open({ baudRate: 921600 });
}

let transmit_buffer = [];

function ser_write(c) {
    transmit_buffer.push(c);
}

async function flush() {
    const writer = port.writable?.getWriter();
    if (writer) {
        await writer.write(new Uint8Array(transmit_buffer));
        writer.releaseLock();
    }
    transmit_buffer = [];
}

function send_escaped_byte(b) {
    switch (b) {
        case END:
            ser_write(ESC);
            ser_write(ESC_END);
            break;

        case ESC:
            ser_write(ESC);
            ser_write(ESC_ESC);
            break;

        default:
            ser_write(b);
    }
}

async function send_report(report) {
    if (!port || !port.connected) {
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

    ser_write(END);
    for (let i = 0; i < 16; i++) {
        send_escaped_byte(data[i]);
    }
    ser_write(END);
    await flush();
}

async function loop() {
    try {
        clear_output();
        if (port && port.connected) {
            write("CONNECTED\n\n");
        } else {
            write("NOT CONNECTED\n\n");
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

    requestAnimationFrame(loop);
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