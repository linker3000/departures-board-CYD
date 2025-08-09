# departures-board [![License Badge](https://img.shields.io/badge/BY--NC--SA%204.0%20License-grey?style=flat&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

**09-Aug-2025: I have just forked the source project and have not begun to work on modifying the code to run on the CYD. As long as this message is here assume the code is not yet working or done.**

----------

**Running update: 09-Aug-2025**

Just started working on readme to update or remove references to the original OLED dislay code version and other features etc. that won't transfer to the CYD. As this is is a work in progress do not buy anything or make any plans based on what you read here while this message is in place.

NO CODE REFACTORING HAS BEEN DONE YET

Todo:

  * Remove unneeded files - eg: stl printing files.
  * Update description text (readme) - started, not finished 
  * Update the code to run on the CYD!! ...
  * Check over web configurator
  * Confirm data fetches work
  * Get rudamentary info on TFT display. 
  * Decide on display format options.
  * Create new display renderings
  * Add other features!?
  * Touch screen control?
----------
This is a fork of gadec-uk's excellent work, adapted to run on a 2.8" Cheap Yellow Display (CYD), which is an ESP32-based module with 2.8" TFT display with touchscreen.

The original project is here: https://github.com/gadec-uk/departures-board

This is a mini Departures Board replicating those at many UK railway stations using data provided by National Rail's public API and London Underground Arrivals boards using data provided by TfL.

## Features
* All processing is done onboard by the ESP32 processor
* Smooth animation matching the real departures and arrivals boards
* Displays up to the next 9 departures with platform, calling stations and expected departure time
* Optionally only show services calling at a selected station
* Network Rail service messages
* Train information (operator, class, number of coaches etc.)
* Displays up to the next 9 arrivals with time to station (London Underground mode)
* TfL station and network service messages (London Underground mode)
* Fully-featured Web UI - choose any station on the UK network / London Tube & DLR network
* Automatic firmware updates (optional)
* Displays the weather at the selected location (optional)

## Quick Start

### What you'll need

1. A 2.8" Cheap Yellow Display (CYD). NB: So far I have not identified the controller chip used on the 3.5" versions and have not been able to drive them with ANY code.
2. A National Rail Darwin Lite API token (these are free of charge - request one [here](https://realtime.nationalrail.co.uk/OpenLDBWSRegistration)).
3. Optionally, an OpenWeather Map API token to display weather conditions at the selected station (these are also free, sign-up for a free developer account [here](https://home.openweathermap.org/users/sign_up)).
4. Optionally, a TfL Open Data API token to display the London Underground/DLR Arrivals Board. These are free, sign-up for a free developer account [here](https://api-portal.tfl.gov.uk/signup)

### Installing the firmware

(NOT YET UPDATED FOR CYD)

The project uses the Arduino framework and the ESP32 v3.2.0 core. If you want to build from source, you'll need [PlatformIO](https://platformio.org).

Alternatively, you can download pre-compiled firmware images from the [releases](https://github.com/gadec-uk/departures-board/releases). These can be installed over the USB serial connection using [esptool](https://github.com/espressif/esptool). If you have python installed, install with *pip install esptool*. For convenience, a pre-compiled executable version for Windows is included [here](https://github.com/gadec-uk/departures-board/tree/main/esptool).

If the board is not recognised you are probably using a version with the CP2104 USB-to-Serial chip. Drivers for the CP2104 are [here](https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers?tab=downloads)

Attach the ESP32 Mini board via it's USB port and use the following command to flash the firmware:

```
esptool.py --chip esp32 --baud 460800 write_flash -z \
  0x1000 bootloader.bin \
  0xe000 boot_app0.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

The tool should automatically find the correct serial port. If it fails to, you can manually specify the correct port by adding *--port COMx* (replace *COMx* with your actual port, e.g. COM3, /dev/ttyUSB0, etc.).

If using the pre-compiled esptool.exe version on Windows, save the esptool.exe and the four firmware (.bin) files to the same directory. Open a command prompt (Windows Key + R, type cmd and press enter) and change to the directory you saved the files into. Now type the following command on one line and press enter:
```
.\esptool --chip esp32 --baud 460800 write_flash -z 0x1000 bootloader.bin 0xe000 boot_app0.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

Subsequent updates can be carried out automatically over-the-air or you can manually update from the Web GUI.

### First time configuration

WiFiManager is used to setup the initial WiFi connection on first boot. The ESP32 will broadcast a temporary WiFi network named "Departures Board", connect to the network and follow the on-screen instuctions. You can also watch a video walkthrough of the entire process below.
[![Departures Board Setup Video](https://github.com/user-attachments/assets/176f0489-d846-42de-913f-eb838d9ab941)](https://youtu.be/bMyI56zwHyc)

Once the ESP32 has established an Internet connection, the Web GUI files will be downloaded and installed from the latest release on GitHub. The next step is to enter your National Rail API token (and optionally, your OpenWeather Map and Transport for London API tokens). Finally, select a station location. Start typing the location name and valid choices will be displayed as you type.

### Web GUI

At start-up, the ESP32's IP address is displayed. To change the station or to configure other miscellaneous settings, open the web page at that address. The settings available are:
- **Station** - start typing a few characters of a station name and select from the drop-down station picker displayed (National Rail mode).
- **Only show services calling at** - filter services based on *calling at* location (National Rail mode - if you want to see the next trains *to* a particular station).
- **Underground Station** - start typing a few characters of an Underground or DLR station name and select from the drop-down station picker displayed (London Underground mode).
- **Brightness** - adjusts the brightness of the OLED screen.
- **London Underground Arrivals Board Mode** - switch between National Rail Departures Board and London Underground Arrivals Board modes.
- **Show the date on screen** - displays the date in the upper-right corner (useful if you're also using this as a desk clock!)
- **Include Bus services** - optionally include bus replacement services (shown with a small bus icon in place of platform number).
- **Include current weather at station location** - this option requires a valid OpenWeather Map API key (see above).
- **Enable automatic firmware updates at startup** - automatically checks for AND installs the latest firmware/Web GUI from this repository.
- **Enable overnight sleep mode (screensaver)** - if you're running the board 24/7, you can help prevent screen burn-in by enabling this option overnight.

A drop-down menu (top-right) adds the following options:
- **Check for Updates** - manually checks for and installs any updates to the firmware/Web GUI.
- **Edit API Keys** - view/edit your National Rail / OpenWeather Map API keys.
- **Clear WiFi Settings** - deletes the stored WiFi credentials and restarts in WiFiManager mode (useful to change WiFi network).
- **Restart System** - restarts the ESP32.

#### Other Web GUI Endpoints

A few other urls have been implemented, primarily for debugging/developer use:
- **/factoryreset** - deletes all configuration information, api keys and WiFi credentials. The entire setup process will need to be repeated.
- **/update** - for manual firmware updates. Download the latest binary from the [releases](https://github.com/gadec-uk/departures-board/releases). Only the **firmware.bin** file should be uploaded via */update*. The other .bin files are not used for upgrades. This method is *not* recommended for normal use.
- **/info** - displays some basic information about the current running state.
- **/formatffs** - formats the filing system, erasing the configuration & Web GUI (but not the WiFi credentials).
- **/dir** - displays a (basic) directory listing of the file system with the ability to view/delete files.
- **/upload** - upload a file to the file system.

### License
This work is licensed under **Creative Commons Attribution-NonCommercial-ShareAlike 4.0**. To view a copy of this license, visit [https://creativecommons.org/licenses/by-nc-sa/4.0/](https://creativecommons.org/licenses/by-nc-sa/4.0/)
