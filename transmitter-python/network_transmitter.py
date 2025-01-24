import socket
import struct

PORT = 42734


class NetworkTransmitter:
    def __init__(self, address):
        self.address = address
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def send(self, data):
        self.sock.sendto(data, (self.address, PORT))
