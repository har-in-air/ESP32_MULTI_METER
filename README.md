# ESP32_INA226_METER

An ESP32 module together with an INA226 current sensor monitors load bus voltage
and load current of a Device Under Test (DUT). A web page running on WiFi web server provides configuration, capture and display.

<img src="docs/block.png">

Full-scale bus voltage is ~40V. 

There are two ranges for current measurement : 
* HIGH : Full-scale 1638mA, with a resolution of 50uA.
* LOW : Full-scale 78mA, with a resolution of 2.4uA.

There are 3 sample rate options :
* 1000Hz : 1mS sample period, no sample averaging, vbus ADC conversion time = 140uS, shunt ADC conversion time = 332uS.
* 500Hz : 2mS sample period, no sample averaging, vbus ADC conversion time 140uS, shunt ADC conversion time = 1100uS.
* 200Hz : 5mS sample period, averaging 4 samples, vbus ADC conversion time 140uS, shunt ADC conversion time = 332uS.

A maximum of 16000 samples can be captured with a single capture trigger. 
* Sample Rate = 1000Hz : Maximum 16 second capture.
* Sample Rate = 500Hz : Maximum 32 second capture.
* Sample Rate = 200Hz : Maximum 80 second capture.

Gated capture up to the maximum of 16000 samples is available. The external gate signal is input via an opto-isolator. 
This gate signal can come from the Device Under Test (DUT) or some other external automated trigger.

If an external WiFi Access Point has not been configured or is unavailable, the system boots up
as a stand-alone WiFi Access Point and web server with SSID `ESP32_INA226`, no password required.

Connect to this WiFi AP, then open the page `http://192.168.4.1` in a browser. 
If your OS has mDNS support, use the url `http://meter.local`.
MacOS has built-in support for mDNS. For Windows, install Bonjour. For Ubuntu, install Avahi.

<img src="docs/home_page.png">

On the home page you can configure an external WiFi Access Point SSID and password. 
Submit the WiFi credentials and restart the ESP32.
Now it will connect as a station to the configured Access Point and then start the web server. 
The web page is at `http://meter.local` as before. 

If you do not have mDNS support, leave the external WiFi AP un-configured, and access the meter as a 
stand-alone WiFi Access Point and web server with web page at fixed address : `http://192.168.4.1`.

Otherwise, you will have to check the serial monitor log to get the station IP address assigned to the meter
by your WiFi Access Point. 

<img src="docs/gated_capture_record.gif">

# Build Environment
* Ubuntu 20.04 LTS amdx64
* Visual Studio Code with PlatformIO plugin using Arduino framework targeting `esp32dev` board. 
* The file `platformio.ini` specifies the framework packages, ESP32 configuration, and external libraries.
* External libraries used :
	* EspAsyncWebServer
	* AsyncTCP
	* ArduinoJson

# Hardware 

[Circuit Schematic](docs/esp32_ina226_schematic.pdf)

* Any ESP32 Development board with on-board USB-UART interface for flashing/serial monitor.
* INA226 current sensor.
* A filter network + low-noise MIC5205 regulator provides the 3.3V power supply for the INA226.
* IRF7831 N-chan mosfet x 2 : used for switching shunt resistors for scale change.
* PC817 opto-coupler for gated current measurement.
* 0.05ohm 1% shunt resistor.
* 1.0 ohm 1% shunt resistor.
* SS56 schottky diode, acts as protection for the 1.0 ohm shunt resistor when the low current scale is selected.

# Usage

Sampling at 200Hz will result in less noise and allow you to capture longer intervals. However, it may not capture 
brief current pulses and accurate maximum/minimum values.

Here is an example :

The power supply is an 18650 Li-Ion battery. The DUT is an ESP32 module. 

On reset, the ESP32 executes the following sequence :
1. Connects to a WiFi Internet Access Point as a station.
2. Connects to a Network Time Protocol (NTP) server to get local time.
3. Enters deep sleep for 10 seconds before restarting. 

The ESP32 sets the gate pin high on entering setup(), and resets the gate pin low just before going to sleep. 
So the gated capture records the supply current & voltage while the ESP32 is active, i.e. not in deep-sleep.

In the first capture snapshot, a sample rate of 200Hz was selected.

<img src="docs/capture_gated_200Hz.png">

In the second capture snapshot, a sample rate of 1000Hz was selected. 
It is noisier, but captures the narrow current pulses due to WiFi transmission bursts. 
Maximum and minimum current values are more accurately captured.

<img src="docs/capture_gated_1000Hz.png">

# Credits
* [Range switching with FET switches](https://www.youtube.com/watch?v=xSEYPP5Xsi0)

