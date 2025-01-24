import struct

PROTOCOL_VERSION = 1

DPAD_LUT = [15, 6, 2, 15, 0, 7, 1, 0, 4, 5, 3, 4, 15, 6, 2, 15]


class Mouse:
    def __init__(self):
        self.OUR_DESCRIPTOR_NUMBER = 0
        self.REPORT_ID = 1
        self.LENGTH = 9
        self.x = 0
        self.y = 0
        self.left_button = False
        self.right_button = False
        self.middle_button = False
        self.vscroll = 0
        self.hscroll = 0

    def get_data(self):
        buttons = (
            (self.left_button << 0)
            | (self.right_button << 1)
            | (self.middle_button << 2)
        )
        data = struct.pack(
            "<BBBBBhhhh",
            PROTOCOL_VERSION,
            self.OUR_DESCRIPTOR_NUMBER,
            self.LENGTH,
            self.REPORT_ID,
            buttons,
            self.x,
            self.y,
            self.vscroll,
            self.hscroll,
        )
        return data


class SwitchGamepad:
    def __init__(self):
        self.OUR_DESCRIPTOR_NUMBER = 2
        self.REPORT_ID = 0
        self.LENGTH = 8
        self.b = False
        self.a = False
        self.y = False
        self.x = False
        self.l = False
        self.r = False
        self.zl = False
        self.zr = False
        self.minus = False
        self.plus = False
        self.ls = False
        self.rs = False
        self.home = False
        self.capture = False
        self.dpad_left = False
        self.dpad_right = False
        self.dpad_up = False
        self.dpad_down = False
        self.lx = 128
        self.ly = 128
        self.rx = 128
        self.ry = 128

    def get_data(self):
        buttons1 = (
            (self.y << 0)
            | (self.b << 1)
            | (self.a << 2)
            | (self.x << 3)
            | (self.l << 4)
            | (self.r << 5)
            | (self.zl << 6)
            | (self.zr << 7)
        )
        buttons2 = (
            (self.minus << 0)
            | (self.plus << 1)
            | (self.ls << 2)
            | (self.rs << 3)
            | (self.home << 4)
            | (self.capture << 5)
        )
        dpad = DPAD_LUT[
            (self.dpad_left << 0)
            | (self.dpad_right << 1)
            | (self.dpad_up << 2)
            | (self.dpad_down << 3)
        ]
        data = struct.pack(
            "<BBBB BBBBBBBB",
            PROTOCOL_VERSION,
            self.OUR_DESCRIPTOR_NUMBER,
            self.LENGTH,
            self.REPORT_ID,
            buttons1,
            buttons2,
            dpad,
            self.lx,
            self.ly,
            self.rx,
            self.ry,
            0,
        )
        return data
