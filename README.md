# CYBERINO:

## 2.4GHZ DEDICATED CYBERDECK + NFC


<img width="500" height="350" alt="746779693_1996340757677217_3893139298974123519_n" src="https://github.com/user-attachments/assets/8f9d270c-ad08-4170-bce7-e9efb6b99fc3" />




BOM:

esp32wroom
st7796 3.5 tft
2x NRF24l01 
mfrc522
4 buttons

### Done features:


Wifi:
Scanner
deauth attack
captive portal
Beacon attack



ble:
ble scanner
ble mouse



2.4ghz:
analyzer
full spectrum jamming
jam on a specs channel


Nfc:
Read uid
Nfc type analyzer


### wiring:

tft st7796

MOSI  23
MISO  19
SCK   18
CS    5
DC    2
RST   4



BUTTONS

UP     12
DOWN   22
SELECT 13
BACK   14


RC522

MOSI 23
MISO 19
SCK  18
SS   21
RST  15

NRF 1

SHARED SPI BUS
CE  27
CSN 26


NRF 2 

SHARED SPI BUS
CE  25
CSN 33




