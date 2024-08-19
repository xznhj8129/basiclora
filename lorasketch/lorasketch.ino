#include <LoRa.h>
#include <SPI.h>
#include <EEPROM.h>

#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DIO0 26
#define BAND_DEFAULT 915E6
#define SF_DEFAULT 7
#define BW_DEFAULT 250E3
#define CR_DEFAULT 5
#define PL_DEFAULT 8
#define TX_POWER_DEFAULT 10
#define SYNC_WORD_DEFAULT 0x12
#define CRC_DEFAULT false
#define GAIN_DEFAULT 0
#define EEPROM_SIZE 64
#define MAX_QUEUE_SIZE 10

const int maxlen = 255;
struct Message {
  byte data[maxlen];
  int length;
};

Message txQueue[MAX_QUEUE_SIZE];
Message rxQueue[MAX_QUEUE_SIZE];
volatile int txQueueHead = 0;
volatile int txQueueTail = 0;
volatile int rxQueueHead = 0;
volatile int rxQueueTail = 0;
volatile bool isReceiving = false;
volatile bool isTransmitting = false;
volatile bool messageReceived = false;
volatile int receivedLength = 0;
volatile bool cadDone = false;
volatile bool cadDetected = false;
byte receivedMessage[maxlen];

struct LoRaSettings {
  long band;
  int sf;
  long bw;
  int cr;
  int pl;
  int txPower;
  int syncWord;
  bool crc;
  int gain;
};

LoRaSettings settings;


void restartESP() {
  ESP.restart();
}

void loadSettings() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, settings);
  EEPROM.end();
}

void saveSettings() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0, settings);
  EEPROM.commit();
  EEPROM.end();
}

void setDefaultSettings() {
  settings.band = BAND_DEFAULT;
  settings.sf = SF_DEFAULT;
  settings.bw = BW_DEFAULT;
  settings.cr = CR_DEFAULT;
  settings.pl = PL_DEFAULT;
  settings.txPower = TX_POWER_DEFAULT;
  settings.syncWord = SYNC_WORD_DEFAULT;
  settings.crc = CRC_DEFAULT;
  settings.gain = GAIN_DEFAULT;
}

void applySettings() {
  LoRa.setFrequency(settings.band);
  LoRa.setSpreadingFactor(settings.sf);
  LoRa.setSignalBandwidth(settings.bw);
  LoRa.setCodingRate4(settings.cr);
  LoRa.setPreambleLength(settings.pl);
  LoRa.setTxPower(settings.txPower);
  LoRa.setSyncWord(settings.syncWord);
  if (settings.crc) {
    LoRa.enableCrc();
  } else {
    LoRa.disableCrc();
  }
  LoRa.setGain(settings.gain);
}

void handleReceiveCommand() {
  if (rxQueueHead == rxQueueTail) {
    Serial.println("0"); // No messages received
  } else if ((rxQueueHead + 1) % MAX_QUEUE_SIZE == rxQueueTail) {
    Serial.print("1+"); // Only message in queue
  } else {
    Serial.print("2+"); // More messages waiting
  }

  if (rxQueueHead != rxQueueTail) {
    Message rxMessage = rxQueue[rxQueueHead];
    Serial.write(rxMessage.data, rxMessage.length);
    rxQueueHead = (rxQueueHead + 1) % MAX_QUEUE_SIZE;
  }
  Serial.println("");
}

void handleReceivedMessage() {
  Message rxMessage;
  rxMessage.length = receivedLength;
  memcpy(rxMessage.data, receivedMessage, receivedLength);

  if ((rxQueueTail + 1) % MAX_QUEUE_SIZE != rxQueueHead) {
    memcpy(rxQueue[rxQueueTail].data, rxMessage.data, rxMessage.length);
    rxQueue[rxQueueTail].length = rxMessage.length;
    rxQueueTail = (rxQueueTail + 1) % MAX_QUEUE_SIZE;
  }

  messageReceived = false;
  isReceiving = false;
  LoRa.receive(); // Put radio back in receive mode
}

int handleSendCommand(const char* message, int len) {
  int status = -1;
  if (isTransmitting || isReceiving) {
    if ((txQueueTail + 1) % MAX_QUEUE_SIZE != txQueueHead) {
      memcpy(txQueue[txQueueTail].data, message, len);
      txQueue[txQueueTail].length = len;
      txQueueTail = (txQueueTail + 1) % MAX_QUEUE_SIZE;

      if (isTransmitting) {
        status = 1; // Message queued: currently transmitting
      } else {
        status = 2; // Message queued: currently receiving
      }
    } else {
      status = 3; // Queue full: message not queued
    }
  } else {
    status = checkChannelAndSend((byte*)message, len);
  }
  return status;
}

int checkChannelAndSend(byte* message, int length) {
  cadDone = false;
  cadDetected = false;
  LoRa.idle();
  LoRa.channelActivityDetection();
  int status = -1;
  
  while (!cadDone) {
    // Allow other operations while waiting
    yield();
  }
  
  if (!cadDetected) {
    sendMessage(message, length);
    status = 0; // Message sent immediately
  } else {
    // Channel is busy, queue the message
    if ((txQueueTail + 1) % MAX_QUEUE_SIZE != txQueueHead) {
      memcpy(txQueue[txQueueTail].data, message, length);
      txQueue[txQueueTail].length = length;
      txQueueTail = (txQueueTail + 1) % MAX_QUEUE_SIZE;
      status = 2; // Message queued: channel busy
    } else {
      status = 3; // Queue full: message not queued
    }
  }
  return status;
}

void sendMessage(byte* message, int length) {
  isTransmitting = true;
  LoRa.beginPacket();
  LoRa.write(message, length);
  LoRa.endPacket();
  LoRa.receive(); 
  isTransmitting = false;
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;
  isReceiving = true;
  receivedLength = packetSize;
  int len = 0;
  while (LoRa.available()) {
    receivedMessage[len++] = LoRa.read();
  }
  messageReceived = true;
}

void onCadDone(bool signalDetected) {
  cadDone = true;
  cadDetected = signalDetected;
}



void setup() {
  Serial.begin(115200);
  while (!Serial);
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);
  
  loadSettings();
  if (settings.band <= 0) {
    setDefaultSettings();
    saveSettings();
  }
  applySettings();

  if (!LoRa.begin(settings.band)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  
  LoRa.onReceive(onReceive);
  LoRa.onCadDone(onCadDone);
  LoRa.receive();
  Serial.println("LoRa initialized.");
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    int ret = -1;
    if (command.startsWith("C+SEND ")) { 
      String message = command.substring(7);
      ret = handleSendCommand(message.c_str(), message.length());
      Serial.println(ret);
    } else if (command.startsWith("C+GET")) { 
      handleReceiveCommand();
    } else if (command.startsWith("C+RSSI")) { 
      int rssi = LoRa.packetRssi();
      Serial.println(rssi);
    } else if (command.startsWith("C+SNR")) { 
      float snr = LoRa.packetSnr();
      Serial.println(snr);
    } else if (command.startsWith("C+FREQ ")) { 
      long frequency = command.substring(7).toInt() * 1000;
      settings.band = frequency;
      saveSettings();
      restartESP();
    } else if (command.startsWith("C+FREQ")) {
      Serial.println(settings.band);
    } else if (command.startsWith("C+BW ")) { 
      long bandwidth = command.substring(5).toInt() * 1000;
      settings.bw = bandwidth;
      LoRa.setSignalBandwidth(bandwidth);
      saveSettings();
      restartESP();
    } else if (command.startsWith("C+BW")) {
      Serial.println(settings.bw);
    } else if (command.startsWith("C+CR ")) { 
      int codingRate = command.substring(5).toInt();
      settings.cr = codingRate;
      LoRa.setCodingRate4(codingRate);
      saveSettings();
      restartESP();
    } else if (command.startsWith("C+CR")) {
      Serial.println(settings.cr);
    } else if (command.startsWith("C+SF ")) { 
      int spreadingFactor = command.substring(5).toInt();
      settings.sf = spreadingFactor;
      LoRa.setSpreadingFactor(spreadingFactor);
      saveSettings();
      restartESP();
    } else if (command.startsWith("C+SF")) {
      Serial.println(settings.sf);
    } else if (command.startsWith("C+PL ")) { 
      int preambleLength = command.substring(5).toInt();
      settings.pl = preambleLength;
      LoRa.setPreambleLength(preambleLength);
      saveSettings();
      restartESP();
    } else if (command.startsWith("C+PL")) {
      Serial.println(settings.pl);
    }else if (command.startsWith("C+DBM ")) { 
      int txPower = command.substring(6).toInt();
      settings.txPower = txPower;
      LoRa.setTxPower(txPower);
      saveSettings();
      restartESP();
    } else if (command.startsWith("C+DBM")) {
      Serial.println(settings.txPower);
    } else if (command.startsWith("C+SYNC ")) { 
      int syncWord = command.substring(7).toInt();
      settings.syncWord = syncWord;
      LoRa.setSyncWord(syncWord);
      saveSettings();
      restartESP();
    } else if (command.startsWith("C+SYNC")) {
      Serial.println(settings.syncWord);
    } else if (command.startsWith("C+CRC ")) {
      String crcSetting = command.substring(6);
      if (crcSetting == "on") {
        settings.crc = true;
        LoRa.enableCrc();
      } else {
        settings.crc = false;
        LoRa.disableCrc();
      }
      saveSettings();
      restartESP();
    } else if (command.startsWith("C+CRC")) {
      Serial.println(settings.crc);
    } else if (command.startsWith("C+GAIN ")) {
      int gain = command.substring(7).toInt();
      settings.gain = gain;
      LoRa.setGain(gain);
      saveSettings();
      restartESP();
    }  else if (command.startsWith("C+GAIN")) {
      Serial.println(settings.gain);
    } else if (command.startsWith("C+CLR")) {
      setDefaultSettings();
      saveSettings();
      restartESP();
    } else if (command.startsWith("C+RST")) {
      restartESP();
    }
  }

  // Handle queued messages
  if (!isTransmitting && !isReceiving && txQueueHead != txQueueTail) {
    checkChannelAndSend(txQueue[txQueueHead].data, txQueue[txQueueHead].length);
    txQueueHead = (txQueueHead + 1) % MAX_QUEUE_SIZE;
  }

  // Process received message
  if (messageReceived) {
    handleReceivedMessage();
  }
}
