## CYBERINO


I started the tool as a side project to test the 2.4ghz frequincies and what can i do with the tft displays to get exprience 

## Mainly implemented Features:

- Raw 2.4

- Wifi

- Bluetooth

- Nfc

## challenges 

- the first challenge i have faced was init the display cuz it uses st7796 driver (not a regular one like st7789 and ili9431)
- second problem was the deauthentication attack i faced some problems with packets sending 
- I have faced some problems with the wiring so i made a pcb its files are included in the repo and it is being printed now
- Spi Handling between the 4 spi devices (Nfc , 2x Nrf24 , TFT dISPLAY)


## What exactly Actually works:

### Wifi
(Deauth , Scanner , Beacon , Save Network, Captive portal)

### Ble 
(MOuse , Scanner , Device adv)

### 2.4ghz
(Analyzer ,Jammer , Jam on specific channel , Blue jammer)

### Nfc
(Read Uid and tag type)



## Bom

Esp32 Wroom (350 egp -> 7$)

2x Nrf Modules (400 egp -> 8$)

Rc522 Nfc Modules (150 egp -> 3$)

St7796 3.5 inches Spi Display (800 egp -> 16$)

Bread Bords, Solder , etc (200egp -> 4$)

Overall 1900 egp -> 38$

## What do I look forward to:

Add Much wireless protocols like Zigbee And thread and wifi 6 and the 5ghz
Bluetooth Classi and some games 
