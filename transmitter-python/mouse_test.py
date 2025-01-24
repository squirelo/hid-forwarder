#!/usr/bin/env python3

import devices
import math
import time

import transmitter_helper

transmitter = transmitter_helper.get_transmitter()
mouse = devices.Mouse()

t = 0
while True:
    mouse.x = int(4 * math.sin(math.pi * t / 100))
    mouse.y = int(4 * math.cos(math.pi * t / 100))
    t += 1
    transmitter.send(mouse.get_data())
    time.sleep(0.01)
