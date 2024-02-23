This Arduino sketch enables ESP32 to communicate over MS-CAN bus in Opel vehicles such as Astra H, Zafira B, Vectra C (and many more!) in addition to acting as a bluetooth audio receiver.
![IMG_20240222_223427r](https://github.com/PNKP237/EHU32/assets/153071841/0387c5a4-0133-4d70-9a6b-a7824ea84370)

![IMG_20240219_215018r](https://github.com/PNKP237/EHU32/assets/153071841/0d320950-1f8f-4e58-8fe0-17751e60074a)

Features:
- A2DP audio sink, data is output to an external I2S DAC, such as PCM5102
- reads steering wheel button presses, which allows for control of connected audio source (play/pause, previous/next track)
- receives bluetooth metadata from connected audio source and prints it to vehicle's center console display - tested on CID, GID and BID, 1-line and 3-lines are supported - only "Aux" messages are overwritten
- simulates button presses in Climate Control menu to allow for one-press enabling/disabling of AC compressor
- long pressing "2" button on the radio switches prints diagnostic data provided by Electronic Climate Control (ECC) module - Engine coolant temp, Speed + RPMs, battery voltage. Long pressing "1" goes back to audio metadata mode
- Over-the-air updates, holding "8" enables the wifi hotspot (password ehu32updater). Note that this disables CAN and bluetooth A2DP until restart.
- more to come, hopefully

Required hardware: ESP32 board (of the classic flavor, A2DP doesn't work on ESP32-C3), PCM5102A DAC module, any CAN transceiver module (in my case MCP2551).
Required connections:
- 5V to: ESP32 VIN pin, MCP2551 VCC pin, PCM5102 VIN pin;
- CAN bus: D4 to CAN_RX, D5 to CAN_TX, CANL and CANH wired up to the vehicle's MS-CAN (accessible by either OBD-II diagnostic port, radio, display, electronic climate control);
- I2S DAC: GND to SCK, D26 to BCK, D22 to DIN, D25 to LCK, D23 to XSMT;
- Configure jumpers on the back of the I2S DAC module: short 1-L, 2-L, 4-L, 3 NOT SHORTED.

This repo contains a PDF outlining which connections are required to make this work.

Discrete PCB with everything on board is in the making, once software quirks are resolved and I find some free time to finish the layout.

Note that this should be soldered directly in the radio unit as the OBD-II port only provides unswitched 12V. Powering it from a 5V car charger also works.
Do not connect headphones to the DAC module, its output is supposed to only be connected to amplifier input - in case of this project either the AUX socket of radio's internal AUX input.

Depends on Arduino ESP32-A2DP library by pschatzmann: https://github.com/pschatzmann/ESP32-A2DP

Reverse engineering of the vehicles various messages was done by JJToB: https://github.com/JJToB/Car-CAN-Message-DB

This project comes with absolutely no warranty of any kind, I'm not responsible for your car going up in flames.
