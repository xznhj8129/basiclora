# basiclora
simplest basic lora python library using TTGO T-Lora modules

## Serial Messages
C+SEND mymessage
Reply: 0 or 1 or 2, sent; 1, message queued, transmitting; 2, message queued, receiving; 3, queue full
C+GET
Reply: 0 (no messages), "1+theincomingmessage" (only message stored), "2+theincomingmessage" (more messages waiting)
C+RSSI: last packet RSSI
C+SNR: last packet SNR
C+FREQ 915: set LORA BAND to 915E3 (have to restart it?)
C+FREQ: get current LORA BAND
C+BW 250: LoRa.setSignalBandwidth(250E3)
C+CR 5: LoRa.setCodingRate4(5)
C+SF 7: LoRa.setSpreadingFactor(7)
C+PL 8: LoRa.setPreambleLength(8)
C+DBM 10: LoRa.setTxPower(10)
C+RST: restart esp32
C+SYNC syncWord: LoRa.setSyncWord(syncWord)
C+CRC on: LoRa.enableCrc() (or disable is "off")
C+GAIN 6: LoRa.setGain(n)
C+CLR: clear saved lora settings from EPROM and restore defaults

## To Do:
add ID-based addressing 
add repeating/meshing
add encryption?