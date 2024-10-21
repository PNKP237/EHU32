# **EHU32**

EHU32 brings bluetooth audio to your Opel/Vauxhall vehicle, integrating with the onboard display and radio. 

Compatible with vehicles equipped with CID/GID/BID/TID display units, additionally giving you the option to view live diagnostic data. Supports headunits such as CD30/CD30MP3/CD40USB/CD70Navi/DVD90Navi, as long as you've got an Aux input.

**EHU32 is non-invasive** and does not require modification to the existing hardware in your car - it can be connected to the OBD-II diagnostic port and radio unit's Aux input. 

**Simple schematic, small bill of materials and inexpensive, widely available components** are what makes EHU32 a great addition to your Astra H/Corsa D/Vectra C/Zafira B/Meriva A.

## Features
- **Bluetooth (A2DP) audio**, data is output to an external I2S DAC, such as PCM5102
- **control connected audio source** (play/pause, previous/next track) using buttons on the **steering wheel**
- shows you what is currently playing on the center console display
  * prints **Artist**, **Track title** and **Album**, just like a CD playback would
  * this is the default mode, otherwise accessible by long pressing "1"
- alternatively, EHU32 can also display live data, such as vehicle speed, RPMs, coolant temperature and battery voltage*
  * accessible by long pressing "2" on the radio panel
  * for single line displays, "3" prints just the coolant temperature
  * disable printing to the screen by holding "9"
  * *battery voltage is available if your vehicle is equipped with Electronic Climate Control*
- allows toggling AC with just a **single press of the AC selector knob**
  
EHU32 can be updated over-the-air, holding "8" enables the wifi hotspot (password ehu32updater). Note that this disables CAN and bluetooth A2DP until the board is reset.

## How it looks
Demo video:

[![Click here to watch EHU32 demo on YouTube](https://img.youtube.com/vi/cj5L4aGAB5w/0.jpg)](https://www.youtube.com/watch?v=cj5L4aGAB5w)

Here's another, extended demo showing EHU32 in action: [https://www.youtube.com/watch?v=8fi7kX9ci_o](https://www.youtube.com/watch?v=8fi7kX9ci_o)

![IMG_20240217_172706](https://github.com/PNKP237/EHU32/assets/153071841/46e31e0d-70b7-423b-9a04-b4522eb96506)

![VID_20240224_174250 mp4_snapshot_00 11 305](https://github.com/PNKP237/EHU32/assets/153071841/030defa7-99e6-42d9-bbc5-f6a6a656e597)

Video showing measurement data displayed in real time (warning, contains music!) https://www.youtube.com/watch?v=uxLYr1c_TJA 

## How it works and general usage tips
While this project aims to make the experience as seamless as possible, there are some shortcomings that have to be addressed:
- Bluetooth is ONLY enabled once EHU32 detects the radio talking to the display over CAN bus
- the audio source volume has to be set to maximum in order to avoid unnecessary noise. Adjust the volume as usual, using the radio's volume control knob or steering wheel buttons.
- EHU32 scans the messages and is looking for "Aux", but once you switch off of Aux mode, there might be a delay before the screen is updated to FM radio mode

If you came here looking for inspiration I'd recommend checking out the [wiki page](https://github.com/PNKP237/EHU32/wiki). I have documented some basics that might come in handy when developing your own addons for these vehicles.

## Building it yourself
Required hardware: ESP32 board with antenna connector and an antenna (of the classic flavor, A2DP doesn't work on ESP32-C3), PCM5102A DAC module, any CAN transceiver module (in my case MCP2551).
Required connections:
- 5V to: ESP32 VIN pin, MCP2551 VCC pin, PCM5102 VIN pin;
- CAN bus: D4 to CAN_RX, D5 to CAN_TX, CANL and CANH wired up to the vehicle's MS-CAN (accessible by either OBD-II diagnostic port - CAN-H and CAN-L on pins 3 and 11 respectively, radio, display, electronic climate control);
- I2S DAC: GND to SCK, D26 to BCK, D22 to DIN, D25 to LCK, D23 to XSMT;
- Configure jumpers on the back of the I2S DAC module: short 1-L, 2-L, 4-L, 3 NOT SHORTED.

This repo contains a PDF schematic outlining which connections are required to make this work and [this post](https://github.com/PNKP237/EHU32/issues/3#issuecomment-2121866276) shows how to install the modules within a CD30MP3 radio unit.

Note that this should be soldered directly in the radio unit as the OBD-II port only provides unswitched 12V. Powering it from a 5V car charger also works.
Do not connect headphones to the DAC module, its output is supposed to only be connected to amplifier input - in case of this project either the AUX socket of radio's internal AUX input.

If you're successful in putting it together and satisfied with operation of EHU32 then I would be extremely grateful if you can share a photo or a short video showing the module in action!
Any kind of feedback is valuable to me - be it issues, general usage or recommendations for additional functionality, I'll be happy to hear them.

The EHU32 standalone PCB project is finally complete, but it'll take some time before I publish all the fabrication files, since I need to distribute the devices to volunteers for testing and gather feedback.

## Compilation notes
Please use version **2.0.17 of ESP32 arduino core**. More recent versions don't seem stable enough, at least in my limited testing. 
Tested with ESP32-A2DP v1.8.4 and arduino-audio-tools v1.0.0.

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
