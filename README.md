# RPI_HABImageTransmitter

## Based on https://github.com/PhilippeSimier/SX1278-LoRa-RaspberryPi

This source uses wiringPi. Installation on raspbian:  
```
apt install git
git clone https://github.com/WiringPi/WiringPi.git
cd WiringPi
./build
```

Vérifier la version
```
gpio -v
```
1. Wire raspberry and lora chip by the table below

|Raspberry Pi | SX1278 |
|----|----------|
| GPIO 0 | RESET| 
| GPIO 22 |DIO 0     |
| SPI CE0 (GPIO8, pin 24)| NSS | 
| MOSI (GPIO10, pin 19)| MOSI | 
| MISO (GPIO9, pin 21)| MISO | 
| CLK (GPIO11, pin 23)| SCK | 

2. Clone the repo

3. Enter cloned repo dir

4. make all

5. Try file ./tx

It uses continuous mode on module. Radio module will continiuously receive packets and each time execute user callback.

# HAXPROv4_RaspiRX
