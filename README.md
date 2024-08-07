# **EHU32**

EHU32 brings bluetooth audio to your Opel/Vauxhall vehicle, integrating with the onboard display and radio. 

Compatible with vehicles equipped with CID/GID/BID/TID display units, additionally giving you the option to view live diagnostic data. 

**EHU32 is non-invasive** and does not require modification to the existing hardware in your car - it can be connected to the OBD-II diagnostic port and radio unit's Aux input. 

**Simple schematic, small bill of materials and inexpensive, widely available components** are what makes EHU32 a great addition to your vehicle.

## Features
- **Bluetooth (A2DP) audio sink**, data is output to an external I2S DAC, such as PCM5102
- reads steering wheel button presses, which allows for **control of connected audio source** (play/pause, previous/next track), basically you can control your phone's music player
- receives bluetooth metadata from connected audio source and prints it to vehicle's center console display - tested on CID, GID and BID, 1-line and 3-lines are supported - only "Aux" messages are overwritten
- simulates button presses in Climate Control menu to allow for **one-press enabling/disabling of AC compressor**
- long pressing "2" button on the radio panel prints **diagnostic data** provided by Electronic Climate Control (ECC module - only Astra H/Zafira B/Corsa D/Vectra C facelift) - Engine coolant temp, Speed + RPMs, battery voltage. Long pressing "1" goes back to audio metadata mode
- in case the vehicle is not equipped with electronic climate control, EHU32 will print data provided by the **display module** (reduced data set - only coolant temperature, RPMs and speed)
- long pressing "3" button prints just the coolant temperature in a single line - useful for 1-line displays such as GID, BID and TID
- Over-the-air updates, holding "8" enables the wifi hotspot (password ehu32updater). Note that this disables CAN and bluetooth A2DP until restart.
- it is also possible to disable printing to the screen altogether by holding "9". This does not affect other functionality.
- more to come, hopefully

## How it looks
Demo video:

[![Click here to watch EHU32 demo on YouTube](https://img.youtube.com/vi/cj5L4aGAB5w/0.jpg)](https://www.youtube.com/watch?v=cj5L4aGAB5w)

Here's another, extended demo showing EHU32 in action: [https://www.youtube.com/watch?v=8fi7kX9ci_o](https://www.youtube.com/watch?v=8fi7kX9ci_o)

![IMG_20240217_172706](https://github.com/PNKP237/EHU32/assets/153071841/46e31e0d-70b7-423b-9a04-b4522eb96506)

![VID_20240224_174250 mp4_snapshot_00 11 305](https://github.com/PNKP237/EHU32/assets/153071841/030defa7-99e6-42d9-bbc5-f6a6a656e597)

Video showing measurement data displayed in real time (warning, contains music!) https://www.youtube.com/watch?v=uxLYr1c_TJA 

## How it works and general usage tips
While this project aims to make the experience as seamless as possible, there are some shortcomings that have to be addressed:
- the audio source volume has to be set to maximum in order to avoid unnecessary noise. Adjust the volume as usual, using the radio's volume control knob or steering wheel buttons.
- sometimes "Aux" will still show up, ~~about once per 20 minutes of driving for like 5 seconds~~. Turns out this happens more often when going in or out of various FM stations - the radio unit transmits RDS data that's not printed but is used to, for example, illuminate status indicators, such as the __[TA]__ symbol.

If you came here looking for inspiration I'd recommend checking out the [wiki page](https://github.com/PNKP237/EHU32/wiki). I have documented some basics that might come in handy when developing your own addons for these vehicles.

## Building it yourself
Required hardware: ESP32 board with antenna connector and an antenna (of the classic flavor, A2DP doesn't work on ESP32-C3), PCM5102A DAC module, any CAN transceiver module (in my case MCP2551).
Required connections:
- 5V to: ESP32 VIN pin, MCP2551 VCC pin, PCM5102 VIN pin;
- CAN bus: D4 to CAN_RX, D5 to CAN_TX, CANL and CANH wired up to the vehicle's MS-CAN (accessible by either OBD-II diagnostic port - CAN-H and CAN-L on pins 3 and 11 respectively, radio, display, electronic climate control);
- I2S DAC: GND to SCK, D26 to BCK, D22 to DIN, D25 to LCK, D23 to XSMT;
- Configure jumpers on the back of the I2S DAC module: short 1-L, 2-L, 4-L, 3 NOT SHORTED.

This repo contains a PDF schematic outlining which connections are required to make this work and [this post](https://github.com/PNKP237/EHU32/issues/3#issuecomment-2121866276) shows how to install the modules within a CD30MP3 radio unit.

Discrete PCB with everything on board is in the making, once software quirks are resolved and I find some free time to finish the layout.

Note that this should be soldered directly in the radio unit as the OBD-II port only provides unswitched 12V. Powering it from a 5V car charger also works.
Do not connect headphones to the DAC module, its output is supposed to only be connected to amplifier input - in case of this project either the AUX socket of radio's internal AUX input.

If you're successful in putting it together and satisfied with operation of EHU32 then I would be extremely grateful if you can share a photo or a short video showing the module in action!
Any kind of feedback is valuable to me - be it issues, general usage or recommendations for additional functionality, I'll be happy to hear them.

## Compilation notes
Please use version **2.0.17 of ESP32 arduino core**. More recent versions don't seem stable enough, at least in my limited testing. 
Tested with ESP32-A2DP v1.3.8 and arduino-audio-tools v0.9.8.

TWAI driver written by ESP as part of their ESP-IDF framework isn't perfect. To ensure everything works properly you'll need to modify "sdkconfig" which is located in %USERPROFILE%\AppData\Local\Arduino15\packages\esp32\hardware\esp32\version\tools\sdk\esp32\

Under "TWAI configuration" section enable **CONFIG_TWAI_ISR_IN_IRAM** and modify **CONFIG_TWAI_ERRATA_FIX_TX_INTR_LOST** so the errata fix is not applied. The whole section should look like this:
```
#
# TWAI configuration
#
CONFIG_TWAI_ISR_IN_IRAM=y
CONFIG_TWAI_ERRATA_FIX_BUS_OFF_REC=y
CONFIG_TWAI_ERRATA_FIX_TX_INTR_LOST=n
CONFIG_TWAI_ERRATA_FIX_RX_FRAME_INVALID=y
CONFIG_TWAI_ERRATA_FIX_RX_FIFO_CORRUPT=y
# CONFIG_TWAI_ERRATA_FIX_LISTEN_ONLY_DOM is not set
# end of TWAI configuration
```
In Arduino IDE set the following: Events on core 0, Arduino on core 1, partition scheme - Minimal SPIFFS.

### Credits
Depends on Arduino ESP32-A2DP and arduino-audio-tools libraries by pschatzmann: [https://github.com/pschatzmann/ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP) [https://github.com/pschatzmann/arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools)

Reverse engineering of the vehicles various messages was done by JJToB: [https://github.com/JJToB/Car-CAN-Message-DB](https://github.com/JJToB/Car-CAN-Message-DB)

This project comes with absolutely no warranty of any kind, I'm not responsible for your car going up in flames.
