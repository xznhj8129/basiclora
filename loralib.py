import serial

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

    def get_rssi(self):
        self.serial.write(b'C+RSSI')
        while True:
            response = self.serial.readline().decode().strip()
            if response:
                return int(response)

    def get_snr(self):
        self.serial.write(b'C+SNR')
        while True:
            response = self.serial.readline().decode().strip()
            if response:
                return float(response)

    def close(self):
        self.serial.close()


# Usage example:
if __name__ == "__main__":
    lora = LoRaHandler('/dev/ttyACM0')

    # Send a message
    status = lora.send_message("Hello, LoRa!")
    print(f"Send status: {status}")

    # Get a message
    message = lora.get_message()
    if message:
        print(f"Received message: {message}")

    # Get RSSI
    rssi = lora.get_rssi()
    print(f"RSSI: {rssi}")

    # Get SNR
    snr = lora.get_snr()
    print(f"SNR: {snr}")

    lora.close()
