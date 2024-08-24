# UNO MQTT DSC Keybus interface #

## This project is now archived. I moved home and no longer require this thing.

Arduino sketch that interface DSC Alarm System keybus and through MQTT client.
Based off [dscKeybusInterface](https://github.com/taligentx/dscKeybusInterface)'s HomeAssistant-MQTT example for Arduino.

This code is to be used with an ATMEGA328p Uno board and a ENC28J60 Ethernet PHY device.
For actual keybus interface with Arduino, please refer to documentation of [dscKeybusInterface](https://github.com/taligentx/dscKeybusInterface) library.



### Specifics ###

Due to memory constraints of the ATMEGA328p, UIPEthernet libs and dscKeybusInterface libraries have slightly modified compilation properties.
For UIPEthernet:

* UDP is disabled
* ARP table hold max 4 entries
* Reduced max open ports to 3
* Reduced max TCP connections to 3
* Reduced TCP packets buffer size to 4
	


For dscKeybusInterface. Changes were made to accomodate my setup. I have a PC1616 with 7 active Zones, a single keypad, all on the same partition.
Changes in the lib were made to reflect this configuration.



Note: ENC28J60 was used because it was the only part I had on hand.



### Build setup ###

To be compiled using PlatformIO.

### Hardware ###

ENC28J60 and UIPEthernet library requires usage of the ATMEAG328p SPI bus.
DscKeybusInterface will require at least 1 digital input with Interrupt capabilites (Digital 2 or 3) and 1 or 2 regular digital input pins. 
Again, please refer to documentation of [dscKeybusInterface](https://github.com/taligentx/dscKeybusInterface) library.
