# ESP32_INA226_METER

An ESP32 module together with an INA226 current sensor is used to monitor bus voltage
and load current.

The bus voltage can be up to 40V. There are two ranges for current measurement : full-scale ~1.6A with a resolution of 50uA, and 78mA with a resolution of 2.4uA.

The meter is accessed online via a web page. If an AP is not configured, the system boots up
as a stand-alone Access Point and web server with SSID "ESP32INA226Meter" and password "123456789
.

Once you connect to this AP, access the page http://192.168.4.1 and here you can configure an
AP SSID and password.

Restart, and the system will reboot as a station connected to the configured SSID.



# Build Environment
* Ubuntu 20.04 LTS amdx64
* Visual Studio Code with PlatformIO plugin using Arduino framework targeting `esp32dev` board. The file `platformio.ini` specifies the framework packages and libraries used by the project.
* Custom `partition.csv` file with two 1.9MB code partitions supporting OTA firmware update
* ~160kByte LittleFS partition for hosting HTML web server pages

# Hardware 

* ESP32 Development module. You can use any module that has an onboard USB-UART chip.
* INA226 current sensor
* IRF7831 n-chan mosfet x 2 for switching shunt resistors for scale change.
* PC817 opto-coupler for gated current measurement

# Credits
* [Range switching with FET switches](https://www.youtube.com/watch?v=xSEYPP5Xsi0)

