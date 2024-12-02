# MP3 Player STM32-F746G-Discovery

This repository contains the firmware for a bare-bones MP3 player implemented on the STM32 platform. The project leverages the STM32F7 microcontroller and supports MP3 playback from an SD card. This implementation provides playback functionality and graphical display features via the integrated LCD screen.

## Features

- **MP3 Playback**: Decode and play MP3 files from an SD card using the Helix MP3 decoder.
- **Graphical Display**: Visualize track information and playback status on an LCD screen.
- **Clock Display**: Real-time clock functionality with updates on the LCD.
- **Audio Analysis**: Spectrum visualization and other audio-related displays.
- **File Management**: FATFS integration for file system handling on SD cards.
- **Push-Button State Control**: Adjust display modes via an onboard push-button.

## Hardware Requirements

1. **STM32F7 Series Microcontroller** (e.g., STM32F746G-Discovery Board)
3. **SD Card**: SD card for MP3 file storage used by the microcontroller as playback.
5. **Audio Output**: Headphones or speaker connected to the STM32's audio interface.

## Software Requirements

- **STM32CubeMX**
- **STM32CubeIDE**

## Known Limitations

1. **USB Audio Playback Not Supported**: The current firmware version (`v1.17`) does not support USB interfacing due to known firmware issues.

## Getting Started

### Setup

1. Clone the repository to your local machine and navigate to it:
   
   ```bash
   git clone https://github.com/tum-air/MP3-Player-STM32-F746G-DISCO.git
   cd MP3-Player-STM32-F746G-DISCO
   ```
3. Open the project in **STM32CubeIDE**.
4. Verify and reconfigure the peripherals in **STM32CubeMX** if wished. 
5. Build the project using **STM32CubeIDE**.
6. Flash the firmware onto your STM32 board using the built-in programmer or an external ST-LINK debugger.

### Playback Instructions

1. Copy MP3 files onto an SD card formatted with FAT/FAT32.
2. Insert the SD card into the STM32 board's SD card slot.
3. Power on the board. Playback should start automatically.
4. Use the onboard button to switch between display modes:
   - Real-time clock
   - Spectrum analyzer
   - Amplitude visualization
5. Connect audio output (e.g., headphones or speaker) to the board's audio interface for playback.

---

## Directory Structure

```plaintext
.
├── Core         # Main code
│   ├── Src      # Main source files
│   ├── Inc      # Header files
│   └── ...
├── Drivers      # HAL / BSP libraries
├── FATFS        # FATFS library for SD card access
├── Middlewares  # Third-party libraries
└── Utilities    # Font and log utilities
```

## Inquiries

For inquiries, contact Alois Knoll at knoll@in.tum.de

