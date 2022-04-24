# Low-Level-Computing

For the course Low Level Computing I used a Attiny-85 microcontroller to read the temperature and airpressure using a BMP280 sensor and display these values on a 8x32 dot-matrix display without using libraries. I used the SPI protocols to interface with the different components and wrote this from scratch.

**Features**

- Uses an Attiny-85
- Uses SPI (built from scratch)
- Uses BMP280 sensor (not using libraries)
- Initialises sensor 
- Calibrates sensor
- Reads sensor
- Displays values (temperature in Celsius and Fahrenheid, airpressure in kPascal) on 8x32 Dot-Matrix display (no libraries used)
- Protected from incorrect connection to powersupply
- Protected against short power failures (simulated using PWM from Arduino Uno)

**Demovideo**

https://youtu.be/Y2DHPWWYPcU
