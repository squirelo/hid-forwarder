import struct
import serial
import binascii

BAUDRATE = 921600

END = 0o300  # indicates end of packet
ESC = 0o333  # indicates byte stuffing
ESC_END = 0o334  # ESC ESC_END means END data byte
ESC_ESC = 0o335  # ESC ESC_ESC means ESC data byte


class SerialTransmitter:
    def __init__(self, device):
        self.ser = serial.Serial(device, BAUDRATE)
        self.buffer = bytes()

    def send_raw_byte(self, b):
        self.buffer += bytes((b,))

    def flush(self):
        self.ser.write(self.buffer)
        self.buffer = bytes()

    def send_escaped_byte(self, b):
        if b == END:
            self.send_raw_byte(ESC)
            self.send_raw_byte(ESC_END)
        elif b == ESC:
            self.send_raw_byte(ESC)
            self.send_raw_byte(ESC_ESC)
        else:
            self.send_raw_byte(b)

    def send(self, data):
        crc = binascii.crc32(data)
        self.send_raw_byte(END)
        for b in data:
            self.send_escaped_byte(b)
        for i in range(4):
            self.send_escaped_byte((crc >> (i * 8)) & 0xFF)
        self.send_raw_byte(END)
        self.flush()
