import argparse


def get_transmitter():
    parser = argparse.ArgumentParser()
    parser.add_argument("--address", help="HID Receiver IP address")
    parser.add_argument("--serial-port", help="HID Receiver serial port/device")
    config = parser.parse_args()
    if not config.address and not config.serial_port:
        raise Exception("Either --address or --serial-port must be specified.")
    if config.address and config.serial_port:
        raise Exception(
            "--address and --serial-port can't be specified at the same time."
        )
    if config.address:
        import network_transmitter

        return network_transmitter.NetworkTransmitter(config.address)
    if config.serial_port:
        import serial_transmitter

        return serial_transmitter.SerialTransmitter(config.serial_port)
