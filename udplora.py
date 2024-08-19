import socket
import serial
import argparse
import time

class LoRaUDPServer:
    def __init__(self, udp_port, dest_port, lora_handler):
        self.server_address = ('localhost', udp_port)
        self.dest_address = ('localhost', dest_port)
        self.lora_handler = lora_handler
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setblocking(False)  # Set UDP socket to non-blocking
        self.sock.settimeout(0.1)
        self.running = True
        self.client = None

        self.bind_socket()

    def bind_socket(self):
        max_retries = 5
        for attempt in range(max_retries):
            try:
                self.sock.bind(self.server_address)
                print(f"UDP server bound to {self.server_address}")
                return
            except OSError as e:
                print(f"Failed to bind UDP server on {self.server_address}: {e}")
                time.sleep(1)  # Wait a bit before retrying
        raise RuntimeError(f"Could not bind UDP server to {self.server_address} after {max_retries} attempts")

    def start(self):
        print(f"UDP server listening on {self.server_address}")
        while self.running:
            try:
                try:
                    data, address = self.sock.recvfrom(4096)
                    self.client = address
                    if self.dest_address[1] != self.client:
                        self.dest_address = address
                    print(f"Sending {data} from {address}")

                    # Handle the received UDP data by sending it via LoRa
                    status = self.lora_handler.send_message(data)
                    if status != 0:
                        print(f"Send status: {status}")
                except socket.timeout:
                    pass

                lora_message = self.lora_handler.get_message()
                if lora_message:
                    print(f"LoRa message received: {lora_message}")
                    if self.client:
                        # Forward the LoRa message to the destination address
                        self.sock.sendto(lora_message, self.dest_address)

            except BlockingIOError:
                continue

    def close(self):
        self.running = False
        self.sock.close()
        self.lora_handler.close()

class LoRaHandler:
    def __init__(self, port, baudrate=115200):
        self.serial = serial.Serial(port, baudrate, timeout=1)
        self.rx_queue = False

    def send_message(self, message):
        command = f"C+SEND {message}"
        self.serial.write(command.encode())
        while True:
            response = self.serial.readline().decode().strip()
            if response:
                return int(response)

    def get_message(self):
        self.serial.write(b'C+GET')
        while True:
            response = self.serial.readline().decode().strip()
            self.rx_queue = response[0]=='2'
            if response == '0':
                return None
            elif response.startswith('1') or response.startswith('2'):
                message = response[(response.find(" ")+1):]
                return message

    def close(self):
        self.serial.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='UDP LoRa bridge')
    parser.add_argument('--loraport', action='store', required=True, help='Lora serial port')
    parser.add_argument('--port', action='store', required=True, help='UDP port for this server')
    parser.add_argument('--destport', action='store', required=False, help='Destination UDP port (optional)')
    arguments = parser.parse_args()
    if not arguments.destport:
        destport = 0
    else:
        destport = arguments.destport

    lora_handler = LoRaHandler(arguments.loraport)
    server = LoRaUDPServer(int(arguments.port), int(destport), lora_handler)
    try:
        server.start()
    except KeyboardInterrupt:
        print("Server shutting down...")
        server.close()
    except RuntimeError as e:
        print(e)
        server.close()
