# **EHU32**

EHU32 brings bluetooth audio to your Opel/Vauxhall vehicle, integrating with the onboard display and radio. 

Compatible with vehicles equipped with CID/GID/BID/TID display units, additionally giving you the option to view live diagnostic data. Supports headunits such as CD30/CD30MP3/CD40USB/CDC40Opera/CD70Navi/DVD90Navi, as long as you've got an **Aux input**.

**EHU32 is non-invasive** and does not require modification to the existing hardware in your car - it can be connected to the OBD-II diagnostic port and radio unit's Aux input. 

**Simple schematic, small bill of materials and inexpensive, widely available components** are what makes EHU32 a great addition to your Astra H/Corsa D/Vectra C/Zafira B/Meriva A.

## Features
- **Bluetooth (A2DP) audio**, data is output to an external I2S DAC, such as PCM5102
- **control connected audio source** (play/pause, previous/next track) using buttons on the **steering wheel**
- shows you what is currently playing on the center console display in **Aux mode**
  * prints **Artist**, **Track title** and **Album**, just like regular CD playback would
  * this is the default mode, otherwise accessible by long pressing "1"
- **automatically reconnects** to your phone when the radio is started
- alternatively, EHU32 can also display live data, such as vehicle speed, RPMs, coolant temperature and battery voltage
  * accessible by long pressing "2" on the radio panel
  * for single line displays, "3" prints just the coolant temperature
  * note that this mode will overwrite anything on the screen, even in FM radio mode or during CD playback
  * disable printing to the screen by holding "9" (or hold it for the total of 5 seconds to reset the entire board, that also clears the settings)
- allows toggling AC with just a **single long-press of the AC selector knob** if it's held for at least half a second
  
EHU32 can be updated over-the-air, holding "8" enables the wifi hotspot (password ehu32updater). To leave the OTA mode, press "8" for 5 seconds. 

## How it looks
Demo videos:

[![Click here to watch EHU32 demo on YouTube](https://img.youtube.com/vi/CZvhz3yvV1g/0.jpg)](https://www.youtube.com/watch?v=CZvhz3yvV1g) [![Click here to watch EHU32 demo on YouTube](https://img.youtube.com/vi/cj5L4aGAB5w/0.jpg)](https://www.youtube.com/watch?v=cj5L4aGAB5w)

Here's another, extended demo showing EHU32 in action: [https://www.youtube.com/watch?v=8fi7kX9ci_o](https://www.youtube.com/watch?v=8fi7kX9ci_o)

![VID_20250507_180309 mp4_snapshot_04 09 045](https://github.com/user-attachments/assets/ea93fcec-3e86-4963-869a-c7194ca0c965)

![IMG_20240217_172706](https://github.com/PNKP237/EHU32/assets/153071841/46e31e0d-70b7-423b-9a04-b4522eb96506)

![VID_20240224_174250 mp4_snapshot_00 11 305](https://github.com/PNKP237/EHU32/assets/153071841/030defa7-99e6-42d9-bbc5-f6a6a656e597)

Video showing measurement data displayed in real time (warning, contains music!) https://www.youtube.com/watch?v=uxLYr1c_TJA 

## How it works and general usage tips
While this project aims to make the experience as seamless as possible, there are some shortcomings that have to be addressed:
- First start (or hard resetting) will take up to 30-40 seconds, because EHU32 will attempt to test your vehicle's display capability and other modules your vehicle is equipped with in order to ensure high level of compatibility. **Please turn on your headunit (engine/ignition is not necessary) and wait patiently** until the startup message is shown!
- Bluetooth is ONLY enabled once EHU32 detects the radio talking to the display over CAN bus
- The audio source volume has to be set to maximum in order to avoid unnecessary noise (unless the audio is clearly distorted and is clipping). Adjust the volume as usual, using the radio's volume control knob or steering wheel buttons.
- EHU32 scans the messages and is looking for "Aux", but once you switch off of Aux mode, there might be a delay before the screen is updated to FM radio mode
- If you have a CD30/CD40 headunit, press "SOUND" twice if you want to adjust bass/treble/balance etc. This is a necessary evil because EHU32 can only block messages ahead of time, way before it knows what are they saying. Nevertheless, it still detects that Aux is no longer displayed hence the sound menu will show up on the second button press
- In case your android device experiences issues with music playback (skipping, crackling and such), go into bluetooth settings, choose EHU32 from the list and disable **"Keep volume consistent"**

If you came here looking for inspiration I'd recommend checking out the [wiki page](https://github.com/PNKP237/EHU32/wiki). I have documented some basics that might come in handy when developing your own addons for these vehicles.

## Building it yourself
Required hardware:
- ESP32 module (preferably an official Espressif-made module) with an antenna (IPX) connector;
- any IPX antenna - you can use one recovered from an old, broken down notebook or buy one from any website;
- PCM5102A DAC module (with configurable jumpers on the bottom, **make sure to configure them accordingly**);
- any CAN transceiver module - MCP2551 (VCC 5V), TDA104x/TDA1050 (VCC 5V), SN65HVD23x (VCC 3.3V).

Refer to [EHU32_wiring.pdf](https://github.com/PNKP237/EHU32/blob/main/EHU32_wiring.pdf) for a guide on how to wire up the modules together.

Please note to configure the PCM5102A module correctly with the jumpers on the bottom.

The MS-CAN bus can be accessed through the diagnostic port (pins 3 and 11 respectively for CAN-H and CAN-L), the headunit, the display, the climate control panel or the factory bluetooth hands-free module.

You can install these modules inside the factory headunit:
- for CD30MP3 (Delphi-Grundig) refer to [this post](https://github.com/PNKP237/EHU32/issues/3#issuecomment-2121866276);
- for CD70Navi refer to [this wiki article](https://github.com/PNKP237/EHU32/wiki/Hardware-modification-%E2%80%90-EHU32-installation-in-CD70-Navi).

Some ESP32 boards have been found to cause problems with audio playback over I2S (mainly exhibited with iPhones and Huawei phones), while it's difficult to suggest an ESP32 board that's confirmed to work, the ones with antenna connectors are often fine. **Look for boards with Espressif etched on the RF shield!**

Note that this should be soldered directly in the radio unit as the OBD-II port only provides unswitched 12V. Powering it from a 5V car charger also works.
Do not connect headphones to the DAC module, its output is supposed to only be connected to amplifier input - in case of this project either the AUX socket of radio's internal AUX input.

## Compilation notes
Please use version **2.0.17 of ESP32 arduino core**. More recent versions don't seem stable enough, at least in my limited testing. 
Tested with ESP32-A2DP v1.8.7 and arduino-audio-tools v1.1.1.

TWAI driver written by ESP as part of their ESP-IDF framework isn't perfect. To ensure everything works properly you'll need to modify "**sdkconfig**" which is located in %USERPROFILE%\AppData\Local\Arduino15\packages\esp32\hardware\esp32\version\tools\sdk\esp32\

Under "TWAI configuration" section enable **CONFIG_TWAI_ISR_IN_IRAM** and modify **CONFIG_TWAI_ERRATA_FIX_TX_INTR_LOST** so the errata fix is not applied. The entire **TWAI configuration** section should look like this:
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
