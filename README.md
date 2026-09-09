# Smart Greenhouse Monitoring and Control System

An embedded systems project based on **STM32F103C8T6 (Blue Pill)** for monitoring environmental conditions and controlling basic greenhouse functions.

## Features
- Temperature measurement using **DS18B20**
- Light intensity sensing using **LDR**
- Soil moisture input monitoring
- Real-time display on **SSD1306 OLED**
- **UART** communication with PC
- Peripheral configuration using **STM32CubeMX**
- Control outputs for greenhouse actuators

## Hardware
- STM32F103C8T6 (Blue Pill)
- DS18B20 temperature sensor
- LDR sensor
- Soil moisture input / simulated analog input
- SSD1306 OLED display
- Push button
- ST-LINK V2
- USB to TTL converter

## Peripherals Used
- GPIO
- ADC
- I2C
- UART
- PWM

## Project Structure
```bash
Core/
Drivers/
smart-greenhouse.ioc
docs/images/
README.md

## How It Works
The system reads environmental parameters such as temperature, light level, and soil moisture, then displays the measured values on an OLED screen. It also supports UART-based communication with a PC for monitoring and control. Output signals can be used to drive greenhouse actuators such as lighting, ventilation, or other control elements.

## Demo
- Circuit setup image
- OLED output image
- CubeMX pin configuration
- YouTube demo link (if available)

## Images
Add your project images in `docs/images/` and reference them here.

## Author
[Your Name]
B.Sc. Electrical Engineering
K. N. Toosi University of Technology


---

# نسخه بهتر و حرفه‌ای‌تر README
اگر بخواهی رزومه‌ای‌تر و polishedتر باشد:

```md
# Smart Greenhouse Monitoring and Control System

This project presents a simple embedded **greenhouse monitoring and control prototype** implemented on the **STM32F103C8T6 (Blue Pill)** platform. The system monitors environmental parameters and provides real-time display and communication capabilities for basic automation tasks.

## Key Features
- Temperature sensing with **DS18B20**
- Ambient light measurement via **LDR**
- Soil moisture monitoring through analog input
- Real-time visualization on **SSD1306 OLED**
- **UART** interface for PC monitoring/control
- Configured using **STM32CubeMX**
- Output control for greenhouse actuators

## Hardware Platform
- STM32F103C8T6 (Blue Pill)
- DS18B20
- LDR
- Soil moisture input
- SSD1306 OLED
- Pushbutton
- ST-LINK V2
- USB-TTL module

## MCU Peripherals
- **GPIO**
- **ADC**
- **I2C**
- **UART**
- **PWM**

## Project Media
You can add:
- `docs/images/setup.jpg`
- `docs/images/oled-display.jpg`
- `docs/images/cubemx-pinout.png`

## Development Tools
- STM32CubeMX
- STM32CubeIDE / Keil (depending on what you used)
- Embedded C

## Notes
This project was developed as a **Microprocessor course project** in the B.Sc. Electrical Engineering program.

## Author
[Your Name]
