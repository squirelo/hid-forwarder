#!/usr/bin/env python3

import pyglet

import devices
import transmitter_helper

transmitter = transmitter_helper.get_transmitter()

controller_manager = pyglet.input.ControllerManager()
for controller in controller_manager.get_controllers():
    if not ("HID Receiver" in controller.device.name):
        controller.open()

prev_data = bytes()


@controller_manager.event
def on_connect(controller):
    print(f"Connected: {controller}")
    if not ("HID Receiver" in controller.device.name):
        controller.open()


@controller_manager.event
def on_disconnect(controller):
    print(f"Disconnected: {controller}")


def update(dt):
    gamepad = devices.SwitchGamepad()
    for controller in controller_manager.get_controllers():
        gamepad.lx += controller.leftx * 128
        gamepad.ly += controller.lefty * 128
        gamepad.rx += controller.rightx * 128
        gamepad.ry += controller.righty * 128
        gamepad.b = gamepad.b or controller.a
        gamepad.a = gamepad.a or controller.b
        gamepad.y = gamepad.y or controller.x
        gamepad.x = gamepad.x or controller.y
        gamepad.l = gamepad.l or controller.leftshoulder
        gamepad.r = gamepad.r or controller.rightshoulder
        gamepad.zl = gamepad.zl or (controller.lefttrigger > 0.25)
        gamepad.zr = gamepad.zr or (controller.righttrigger > 0.25)
        gamepad.minus = gamepad.minus or controller.back
        gamepad.plus = gamepad.plus or controller.start
        gamepad.ls = gamepad.ls or controller.leftstick
        gamepad.rs = gamepad.rs or controller.rightstick
        gamepad.home = gamepad.home or controller.guide
        # gamepad.capture
        gamepad.dpad_left = gamepad.dpad_left or (controller.dpadx == -1)
        gamepad.dpad_right = gamepad.dpad_right or (controller.dpadx == 1)
        gamepad.dpad_up = gamepad.dpad_up or (controller.dpady == 1)
        gamepad.dpad_down = gamepad.dpad_down or (controller.dpady == -1)
    gamepad.lx = int(max(0, min(255, gamepad.lx)))
    gamepad.ly = int(max(0, min(255, gamepad.ly)))
    gamepad.rx = int(max(0, min(255, gamepad.rx)))
    gamepad.ry = int(max(0, min(255, gamepad.ry)))
    global prev_data
    data = gamepad.get_data()
    if data != prev_data:
        transmitter.send(gamepad.get_data())
        prev_data = data


pyglet.clock.schedule_interval(update, 0.01)

pyglet.app.run()
