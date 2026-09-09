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
Mohammad Hossein Ahmadi 
