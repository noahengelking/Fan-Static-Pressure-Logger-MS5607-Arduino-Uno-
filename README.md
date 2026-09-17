# Fan-Static-Pressure-Logger-MS5607-Arduino-Uno-
An absolute barometric pressure sensor (MS5607) is sampled over I²C by an Arduino Uno. Raw readings are temperature-compensated using the sensor's factory calibration, averaged, and referenced to a 5-second baseline so that only the *change* in pressure (the fan's effect) is reported. Data is streamed as CSV over the USB serial port.
