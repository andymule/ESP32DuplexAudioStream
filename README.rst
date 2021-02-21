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
