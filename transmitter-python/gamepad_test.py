#!/usr/bin/env python3

import devices
import math
import time

import transmitter_helper

transmitter = transmitter_helper.get_transmitter()
gamepad = devices.SwitchGamepad()


t = 0

while True:
    gamepad.lx = int(128 + 127 * math.sin(math.pi * t / 100))
    gamepad.ly = int(128 + 127 * math.cos(math.pi * t / 100))
    gamepad.rx = int(128 + -127 * math.sin(math.pi * t / 100))
    gamepad.ry = int(128 + 127 * math.cos(math.pi * t / 100))
    button = (t % 900) // 50
    gamepad.b = button == 0
    gamepad.a = button == 1
    gamepad.y = button == 2
    gamepad.x = button == 3
    gamepad.l = button == 4
    gamepad.r = button == 5
    gamepad.zl = button == 6
    gamepad.zr = button == 7
    gamepad.minus = button == 8
    gamepad.plus = button == 9
    gamepad.ls = button == 10
    gamepad.rs = button == 11
    gamepad.home = button == 12
    gamepad.capture = button == 13
    gamepad.dpad_left = button == 14
    gamepad.dpad_right = button == 15
    gamepad.dpad_up = button == 16
    gamepad.dpad_down = button == 17
    t += 1
    transmitter.send(gamepad.get_data())
    time.sleep(0.01)
