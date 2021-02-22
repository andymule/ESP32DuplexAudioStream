ESP32 full duplex audio w UDP broadcasting

uses both cores, so won't work on single core variants of ESP32 

Pins used (look up your pinouts for proper GPIO):

DAC_CHANNEL_2

DAC_CHANNEL_1

ADC1_CHANNEL_0

Currently, shit noise is generated on DAC_CHANNEL_2. I connect this to my ADC1_CHANNEL_0 for simulating a mic input. This is broadcast to 192.168.1.255 subgroup on port 4444. I include python file that listens to this data and broadcasts it straight back to 192.168.1.255 port 4445. The ESP32 takes this data, fills an audio buffer, and plays it on DAC_CHANNEL_1

==BUILD==

Install PlatformIO into VSCode. Open this folder, build and upload from there very easily.

-OR-

Copy/Paste main.cpp into a new Arduino sketch, you should be able to just upload and run (assuming you've already installed ESP32 libraries)


-OR- 

How to build PlatformIO based project
=====================================

1. `Install PlatformIO Core <http://docs.platformio.org/page/core.html>`_
2. Download `development platform with examples <https://github.com/platformio/platform-espressif32/archive/develop.zip>`_
3. Extract ZIP archive
4. Run these commands:

.. code-block:: bash

    # Change directory to this directory

    # Build project
    > platformio run

    # Upload firmware
    > platformio run --target upload

    # Build specific environment
    > platformio run -e esp32dev

    # Upload firmware for the specific environment
    > platformio run -e esp32dev --target upload

    # Clean build files
    > platformio run --target clean
