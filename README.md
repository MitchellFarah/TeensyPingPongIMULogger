# TeensyPingPongIMULogger

A high-performance data logger for Teensy 4.1 that captures IMU (accelerometer, gyroscope, magnetometer) data via interrupts with ping-pong buffering and SD card streaming. Designed for reliability, extensibility, and zero dropped samples under normal operation.

---

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Hardware Requirements](#hardware-requirements)
4. [Quick Start](#quick-start)
5. [Architecture](#architecture)
6. [Data Format](#data-format)
7. [MATLAB Processing Tools](#matlab-processing-tools)
8. [Configuration](#configuration)
9. [Project Structure](#project-structure)
10. [Performance Characteristics](#performance-characteristics)
11. [Adding New Sensors](#adding-new-sensors)
12. [Troubleshooting](#troubleshooting)
13. [License](#license)

---

## Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    TEENSY 4.1 DATA LOGGER ARCHITECTURE                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────┐  │
│  │   IMU       │    │   Ping-Pong │    │   SD Card   │    │  JSON   │  │
│  │ (ICM-20948) │───▶│   Buffers   │───▶│   Writer    │───▶│  Header │  │
│  │ Acc/Gyr/Mag │    │  2×128 KiB  │    │  1 GiB/file │    │  Output │  │
│  │  ~100 Hz    │    │             │    │             │    │         │  │
│  └──────┬──────┘    └─────────────┘    └─────────────┘    └─────────┘  │
│         │                                                               │
│    DRDY Interrupt (minimal ISR)                                         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

The TeensyPingPongIMULogger is a robust data acquisition system designed for high-reliability IMU logging. It uses the SparkFun ICM-20948 9-axis IMU (accelerometer, gyroscope, and magnetometer) connected via SPI to a Teensy 4.1 microcontroller. Data is captured using interrupt-driven sampling, buffered using a ping-pong scheme, and streamed to SD card with minimal CPU overhead.

### Key Specifications

| Feature | Specification |
|---------|--------------|
| **MCU** | Teensy 4.1 @ 600 MHz |
| **IMU** | SparkFun ICM-20948 (SPI) |
| **Sensors** | Accelerometer, Gyroscope, Magnetometer, Temperature |
| **Sample Rate** | Configurable: 1125Hz ÷ (1-256) = ~4-1125 Hz |
| **Buffer Strategy** | Ping-pong (2×128 KiB) |
| **Record Size** | 28 bytes (packed binary) |
| **Max Files** | 100 sequential slots (imuLog_00-99) |
| **Preallocation** | 1 GiB per file (truncated on close) |

---

## Features

- **9-Axis Sensing**: Accelerometer (3-axis), Gyroscope (3-axis), and Magnetometer (3-axis) plus temperature
- **Interrupt-Driven**: Minimal ISR stores timestamp and sets flag; all heavy lifting in main loop
- **Ping-Pong Buffering**: Double-buffered DMA-safe operation prevents data loss during SD writes
- **Zero-Copy Architecture**: Records pushed directly to buffers, no intermediate copies
- **JSON Metadata**: Human-readable headers with configuration, calibration, and performance metrics
- **Sequential File Naming**: Automatic file numbering (imuLog_00 through imuLog_99)
- **Performance Monitoring**: Tracks drop rate, max write time, buffer utilization
- **Extensible**: Abstract `DataSource` interface for adding new sensors

---

## Hardware Requirements

### Bill of Materials

| Component | Recommended Part | Purpose |
|-----------|-----------------|---------|
| Microcontroller | Teensy 4.1 | Main processing and SD interface |
| IMU | SparkFun ICM-20948 (Qwiic or breakout) | 9-axis motion sensing |
| SD Card | Class 10 or UHS-I, 8GB+ | Data storage |
| USB Cable | Micro-USB | Power and programming |
| Jumper Wires | 22 AWG dupont | Connections (if not using Qwiic) |

### Pin Connections

```
    Teensy 4.1                    SparkFun ICM-20948
    ┌─────────┐                   ┌───────────────┐
    │         │                   │               │
    │  3.3V   │──────────────────▶│  VCC          │
    │  GND    │──────────────────▶│  GND          │
    │         │                   │               │
    │  MOSI   │──────────────────▶│  SDI (MOSI)   │  Pin 11
    │  MISO   │◀──────────────────│  SDO (MISO)   │  Pin 12
    │  SCK    │──────────────────▶│  SCL (SCK)    │  Pin 13
    │         │                   │               │
    │  CS 36  │──────────────────▶│  CS           │  Configurable
    │  INT 33 │◀──────────────────│  INT          │  Configurable
    │         │                   │               │
    └─────────┘                   └───────────────┘

    Note: Insert microSD card into Teensy 4.1 slot
```

### Default Pin Configuration

| Signal | Teensy 4.1 Pin | Direction | Description |
|--------|---------------|-----------|-------------|
| CS | 36 | Output | SPI Chip Select |
| MOSI | 11 | Output | SPI Data Out |
| MISO | 12 | Input | SPI Data In |
| SCK | 13 | Output | SPI Clock |
| DRDY/INT | 33 | Input | Data Ready Interrupt |

---

## Quick Start

### 1. Install Dependencies

Install [PlatformIO](https://platformio.org/) (recommended) or use Arduino IDE with Teensyduino.

**Required Libraries (auto-installed via PlatformIO):**
- SparkFun 9DoF IMU Breakout - ICM 20948 - Arduino Library
- SdFat - Adafruit Fork

### 2. Build and Upload

```bash
# Clone or download the project
cd TeensyPingPongIMULogger

# Build the firmware
pio run

# Upload to Teensy
pio run --target upload
```

### 3. Run the Logger

1. **Open Serial Monitor** (115200 baud):
   ```bash
   pio device monitor
   ```

2. **Press any key** to start logging

3. **Press any key again** to stop logging

4. **Files created** on SD card:
   ```
   imuLog_00.bin   ← Binary sensor data (28 bytes/record)
   imuLog_00.json  ← Metadata and configuration
   ```

### Example Serial Output

```
=== Teensy IMU Logger ===
Press any key to start logging...

Initializing SD card... OK
Finding next available file number... using imuLog_00.bin
Opening log file: imuLog_00.bin
Preallocating 1 GiB...
Preallocation OK
Initializing ICM-20948 IMU... OK
Configuring IMU...
Accel sample rate: 102.27 Hz
Gyro sample rate: 102.27 Hz
Initializing magnetometer... OK
 complete!
Attaching interrupt... ready

=== LOGGING ACTIVE ===
Press any key to stop...

Status: 26425 samples, 26425 taken, ISR/taken=1.00, max_write=8102us
Stop command received

=== Stopping Logging ===
Draining buffers...
Writing final 57134 bytes...
Truncating file...
Closing file...
Writing JSON header...
JSON: Successfully wrote 1800 bytes to imuLog_00.json

=== Logging Complete ===
Total ISR calls: 26425
Total samples recorded: 26425
Total samples dropped: 0
Drop rate: 0.00%
```

---

## Architecture

### Data Flow

```
TIMING DIAGRAM: One Sample Journey
═══════════════════════════════════════════════════════════════════════════

Time ──▶

IMU DRDY ─┐        ┌─┐        ┌─┐        ┌─┐        ┌─┐
  (102Hz) │        │ │        │ │        │ │        │ │
         ─┴────────┴─┴────────┴─┴────────┴─┴────────┴─┴───
           │        │          │          │
           │        │          │          │
ISR        ├─[1µs]──┤          │          │
          ─┴┐      ┌┴──────────┤          │
           │SET    │           │          │
           │FLAG   │           │          │
           ▼       │           │          │
           ┌───────┴───────────┤          │
           │  dataReadyFlag=true
           │  isrTimestamp=captured
           └──────────────────────────────┤
                                        │
Main Loop  ─────────────────────────────┼──────────────────▶
                                        │◄─── Check flag
                                        │
                                        ├──[20µs]──┤
                                        │  SPI Read  │
                                        │  Build Record
                                        │  Push to Buffer
                                        └────────────┘

Buffer Write ──────────────────────────────────────┬────────────▶
                                                   │
                              ┌────────────────────┤
                              │  When buffer full  │
                              │  (128 KiB)         │
                              ▼                    │
                         mark ready[buf]=true      │
                              │                    │
SD Write ─────────────────────┼────────────────────┼────┬──────▶
                              │                    │    │
                              │                    │◄───┤ Check ready[]
                              │                    │    │
                              │                    ├──[2-8ms]──┤
                              │                    │   Write    │
                              │                    │   128 KiB  │
                              │                    │   to SD    │
                              │                    └───────────┘

═══════════════════════════════════════════════════════════════════════════
```

### Memory Layout

```
TEENSY 4.1 RAM USAGE
════════════════════

┌─────────────────────────────────────────┐ 0x2000_0000
│                                         │
│  IDATA (ISR-safe memory)                │
│  • Stack                                │
│  • Global variables                     │
│                                         │
├─────────────────────────────────────────┤
│                                         │
│   DMAMEM                                │
│  • Ping-Pong Buffer 0: 131,072 bytes   │
│    ┌─────────────────────────────┐     │
│    │ Records: 28 bytes each      │     │
│    │ Capacity: 4,676 records     │     │
│    │ ~46ms @ 102Hz               │     │
│    └─────────────────────────────┘     │
│                                         │
│  • Ping-Pong Buffer 1: 131,072 bytes   │
│    (identical structure)               │
│                                         │
│  • Available for more buffers...       │
│                                         │
├─────────────────────────────────────────┤
│                                         │
│  PROGMEM (Flash)                        │
│  • Program code                         │
│  • String literals (F() macro)          │
│                                         │
└─────────────────────────────────────────┘

Total RAM: 512 KB (TCM: 512KB ITCM + DTCM, OCRAM: 512KB)
Buffer Usage: 256 KB (50% of OCRAM)
```

---

## Data Format

### Binary Record Structure (28 bytes)

```
IMU RECORD STRUCTURE (28 bytes, packed, little-endian)
═══════════════════════════════════════════════════════════════

Byte Offset    Field           Type        Description
───────────    ─────────       ────        ───────────────────
0-3            seq             uint32_t    Monotonic sequence counter
4-7            ts_us           uint32_t    micros() timestamp (µs)
8-9            ax              int16_t     Accelerometer X (raw)
10-11          ay              int16_t     Accelerometer Y (raw)
12-13          az              int16_t     Accelerometer Z (raw)
14-15          gx              int16_t     Gyroscope X (raw)
16-17          gy              int16_t     Gyroscope Y (raw)
18-19          gz              int16_t     Gyroscope Z (raw)
20-21          temp            int16_t     Temperature (raw)
22-23          mx              int16_t     Magnetometer X (raw)
24-25          my              int16_t     Magnetometer Y (raw)
26-27          mz              int16_t     Magnetometer Z (raw)

Layout:
┌──────────┬──────────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│  seq     │  ts_us   │ ax  │ ay  │ az  │ gx  │ gy  │ gz  │temp │ mx  │ my  │ mz  │
│ 4 bytes  │ 4 bytes  │2    │2    │2    │2    │2    │2    │2    │2    │2    │2    │
└──────────┴──────────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘

Total: 4 + 4 + (10 × 2) = 28 bytes

At 102 Hz: 2,856 bytes/sec = ~2.8 KB/s
At 1 GiB preallocation: ~4.4 days logging time
```

### Unit Conversions

| Sensor | Raw Type | Conversion | Physical Unit |
|--------|----------|------------|---------------|
| Accelerometer | int16_t | ±2g: / 16384, ±4g: / 8192, ±8g: / 4096, ±16g: / 2048 | g |
| Gyroscope | int16_t | ±250dps: / 131.07, ±500dps: / 65.535, ±1000dps: / 32.768, ±2000dps: / 16.384 | °/s |
| Temperature | int16_t | (raw - 21) / 333.87 + 21 | °C |
| Magnetometer | int16_t | × 0.15 | µT |

### JSON Metadata Format

```json
{
  "header": {
    "version": "1.1",
    "created": "1234567",
    "logger": "Teensy IMU Logger"
  },
  "hardware": {
    "mcu": "Teensy 4.1",
    "imu_drdy_pin": 33,
    "imu_cs_pin": 36,
    "spi_clock_hz": 4000000
  },
  "imu_config": {
    "accel": {
      "odr_hz": 102.27,
      "sample_rate_divider": 10,
      "full_scale_range": "+/-2g",
      "dlpf": "acc_d473bw_n499bw"
    },
    "gyro": {
      "odr_hz": 102.27,
      "sample_rate_divider": 10,
      "full_scale_range": "+/-250dps",
      "dlpf": "gyr_d361bw4_n376bw5"
    },
    "magnetometer": {
      "enabled": true,
      "odr_hz": 100.0,
      "full_scale_range": "±4900 µT",
      "lsb_per_ut": 6.6667
    }
  },
  "buffer_config": {
    "chunk_bytes": 131072,
    "buffer_count": 2,
    "record_size_bytes": 28,
    "preallocate_bytes": 1073741824
  },
  "performance": {
    "isr_count": 26425,
    "samples_recorded": 26425,
    "samples_dropped": 0,
    "drop_rate_percent": 0.00,
    "max_write_time_us": 8102,
    "buffer_0_filled": 2,
    "buffer_1_filled": 2,
    "buffer_swaps": 4
  },
  "file_metadata": {
    "bin_filename": "imuLog_00.bin",
    "json_filename": "imuLog_00.json",
    "actual_size_bytes": 740700,
    "sample_count": 26425,
    "duration_seconds": 258.33
  }
}
```

---

## MATLAB Processing Tools

The `MATLAB/` directory contains a suite of functions for importing, analyzing, and visualizing IMU data.

### Available Functions

| Function | Purpose | Location |
|----------|---------|----------|
| `decodeImuLog` | Main decoder - loads .bin/.json pairs into MATLAB structs | `MATLAB/Functions/` |
| `jsonToStruct` | Helper to load JSON metadata into MATLAB struct | `MATLAB/Functions/` |
| `imuTimingAnalysis` | Analyzes sample timing, jitter, and drop detection | `MATLAB/Functions/` |
| `readSeqTs_chunkAware` | Low-level reader for sequence/timestamp only | `MATLAB/Functions/` |

### Quick Start - MATLAB

```matlab
% Decode a logging session
log = decodeImuLog('imuLog_00');

% Access raw integer data
ax_raw = log.raw.ax;      % int16 array
mx_raw = log.raw.mx;      % magnetometer X (int16)

% Access physical units
accel_g = log.phys.accel_g;    % N×3 matrix [ax, ay, az] in g
gyro_dps = log.phys.gyro_dps;  % N×3 matrix [gx, gy, gz] in °/s
mag_uT = log.phys.mag_uT;      % N×3 matrix [mx, my, mz] in µT
temp_C = log.phys.temp_C;      % Temperature in °C

% Access timestamps
t_s = log.time.t_s;       % Time in seconds (re-zeroed)
t_us = log.time.t_us;     % Raw microsecond timestamps

% Use timetable (if available)
log.timetable             % Timetable with all signals
```

### decodeImuLog

Primary function for loading IMU log files into MATLAB.

```matlab
% Basic usage
log = decodeImuLog('imuLog_00');

% With initial trim (discard first N samples)
log = decodeImuLog('imuLog_00', 100);  % Skip first 100 samples

% Full path also works
log = decodeImuLog('C:\Data\imuLog_05');
```

**Output Structure:**
```matlab
log =
  struct with fields:
        meta: [1×1 struct]          % JSON metadata
       files: [1×1 struct]          % File paths
         raw: [1×1 struct]          % Raw integer data
        phys: [1×1 struct]          % Physical units
        time: [1×1 struct]          % Time vectors
   timetable: [N×10 timetable]      % Combined timetable
```

**Raw Fields:**
- `seq` - Sequence number (uint32)
- `ts_us` - Microsecond timestamp (uint32)
- `ax, ay, az` - Accelerometer raw (int16)
- `gx, gy, gz` - Gyroscope raw (int16)
- `temp` - Temperature raw (int16)
- `mx, my, mz` - Magnetometer raw (int16)

**Physical Fields:**
- `accel_g` - Acceleration in g (N×3 double)
- `gyro_dps` - Angular velocity in °/s (N×3 double)
- `mag_uT` - Magnetic field in µT (N×3 double)
- `temp_C` - Temperature in °C (N×1 double)

### imuTimingAnalysis

Analyzes timing characteristics of the logged data.

```matlab
% Analyze timing
stats = imuTimingAnalysis(log);

% Output:
%   stats.mean_dt      - Mean sample period
%   stats.std_dt       - Standard deviation of period
%   stats.mean_fs      - Mean sampling frequency
%   stats.jitter_rms   - RMS timing jitter
%   stats.jitter_peak  - Peak timing jitter
%   stats.num_drops    - Number of detected dropped samples
%   stats.drop_locations - Indices where drops occurred
```

**Example Output:**
```
===== IMU Timing Analysis =====
Mean sample period : 0.009780 s
Mean sampling rate : 102.250 Hz
Std period         : 1.234567e-06 s
RMS jitter         : 8.765432e-07 s
Peak jitter        : 4.321000e-05 s
Min dt             : 0.009700 s
Max dt             : 0.009890 s
Dropped samples    : 0
```

### Example Analysis Script

```matlab
% Load data
log = decodeImuLog('imuLog_00');

% Timing analysis
stats = imuTimingAnalysis(log);

% Plot accelerometer data
figure('Name', 'Accelerometer Data');
plot(log.time.t_s, log.phys.accel_g);
xlabel('Time (s)');
ylabel('Acceleration (g)');
legend('X', 'Y', 'Z');
grid on;

% Plot magnetometer data (Earth's field ~25-65 µT)
figure('Name', 'Magnetometer Data');
plot(log.time.t_s, log.phys.mag_uT);
xlabel('Time (s)');
ylabel('Magnetic Field (µT)');
legend('X', 'Y', 'Z');
grid on;

% Calculate orientation from magnetometer
heading = atan2d(log.phys.mag_uT(:,2), log.phys.mag_uT(:,1));
figure('Name', 'Heading');
plot(log.time.t_s, heading);
xlabel('Time (s)');
ylabel('Heading (degrees)');
grid on;

% 3D trajectory (simple integration)
vel = cumtrapz(log.time.t_s, log.phys.accel_g * 9.81);
pos = cumtrapz(log.time.t_s, vel);
figure('Name', '3D Position (integrated)');
plot3(pos(:,1), pos(:,2), pos(:,3));
xlabel('X (m)'); ylabel('Y (m)'); zlabel('Z (m)');
grid on; axis equal;
```

---

## Configuration

All user-configurable parameters are in `src/config.h`:

```cpp
// ============================================
// Hardware Pins
// ============================================
#define IMU_DRDY_PIN      33      // Data Ready interrupt pin
#define IMU_CS_PIN        36      // SPI Chip Select
#define IMU_SPI_PORT      SPI1    // SPI port to use

// ============================================
// IMU Settings
// ============================================
#define IMU_ACCEL_FSR     gpm2    // ±2g, ±4g, ±8g, ±16g
#define IMU_GYRO_FSR      dps250  // ±250, ±500, ±1000, ±2000 dps
#define IMU_ACCEL_DLPF    acc_d246bw_n265bw
#define IMU_GYRO_DLPF     gyr_d196bw6_n229bw8
#define IMU_SMPLRT_DIV    10      // ODR = 1125/(DIV+1) = ~102 Hz

// ============================================
// Buffer & File Settings
// ============================================
#define CHUNK_BYTES       (128 * 1024)   // 128 KiB per buffer
#define PREALLOC_BYTES    (1ULL << 30)   // 1 GiB preallocation
#define MAX_FILE_NUMBER   99             // imuLog_00 to imuLog_99
```

### Sample Rate Calculator

| SMPLRT_DIV | ODR (Hz) | Buffer Duration @ 128 KiB |
|-----------|----------|---------------------------|
| 0         | 1125.0   | ~4.2 ms                   |
| 10        | 102.27   | ~46 ms                    |
| 112       | 10.0     | ~467 ms                   |

---

## Project Structure

```
TeensyPingPongIMULogger/
├── src/                          # Firmware source code
│   ├── main.cpp                  # Application state machine, main loop
│   ├── config.h                  # User configuration (pins, rates)
│   ├── types.h                   # ImuRecord and Telemetry structs
│   ├── sensor_interface.h        # Abstract DataSource interface
│   │
│   ├── pingpong_buffer.h/cpp     # Ping-pong buffer management
│   ├── imu_source.h/cpp          # ICM-20948 hardware driver
│   ├── sd_logger.h/cpp           # SD card operations
│   ├── json_writer.h/cpp         # JSON metadata generation
│   └── telemetry.h               # Debug/telemetry output
│
├── MATLAB/                       # MATLAB processing tools
│   ├── Functions/                # Main function library
│   │   ├── decodeImuLog.m        # Primary decoder
│   │   ├── jsonToStruct.m        # JSON loader
│   │   ├── imuTimingAnalysis.m   # Timing analysis
│   │   ├── readSeqTs_chunkAware.m# Low-level reader
│   │   └── IMU_LOGGER_FORMAT.md  # Format documentation
│   └── Depreciated/              # Older versions (reference)
│
├── platformio.ini                # PlatformIO configuration
└── README.md                     # This file
```

---

## Performance Characteristics

```
TIMING BUDGET ANALYSIS @ 102 Hz ODR
═══════════════════════════════════════════════════════════════

Operation                    Typical      Worst Case    Budget
───────────────────────────────────────────────────────────────
ISR (DRDY)                   1 µs         2 µs          9 µs
SPI Read (28 bytes)          25 µs        35 µs         50 µs
Record Construction          2 µs         5 µs          10 µs
Buffer Push                  1 µs         3 µs          10 µs
───────────────────────────────────────────────────────────────
Per-Sample Processing        29 µs        45 µs         79 µs

Sample Period @ 102 Hz       9,804 µs
───────────────────────────────────────────────────────────────
HEADROOM                     9,775 µs     9,759 µs      99.5%

SD Card Write (128 KiB)      2-8 ms       20 ms         N/A
Buffer Duration @ 102 Hz     46 ms        46 ms         N/A
───────────────────────────────────────────────────────────────
BUFFER SAFETY MARGIN         38 ms        26 ms         5.3x

Conclusion: System can tolerate 5x longer SD writes before drops
═══════════════════════════════════════════════════════════════
```

---

## Adding New Sensors

The architecture supports multiple data sources through the `DataSource` interface.

### The DataSource Interface

```cpp
class DataSource {
public:
    virtual ~DataSource() = default;
    virtual bool begin() = 0;
    virtual void attachInterrupt() {}
    virtual void detachInterrupt() {}
    virtual bool hasPendingSample() const = 0;
    virtual bool tryRead(void* record) = 0;
    virtual uint32_t getSampleCount() const = 0;
};
```

### Multi-Sensor Architecture Options

```
MULTI-SENSOR DATA MERGING OPTIONS
═══════════════════════════════════════════════════════════════

Option A: Separate Log Files (Recommended)
──────────────────────────────────────────
┌─────────────┐    ┌─────────────┐
│ IMU Source  │───▶│ imu_00.bin  │
│ (0x01)      │    │ imu_00.json │
└─────────────┘    └─────────────┘
                          │
┌─────────────┐    ┌─────────────┐
│ BARO Source │───▶│ baro_00.bin │
│ (0x02)      │    │ baro_00.json│
└─────────────┘    └─────────────┘
                          │
                    Sync via timestamp


Option B: Tagged Envelope Format (Future)
─────────────────────────────────────────
┌─────────────────────────────────────────┐
│ [SourceID(1)] [Timestamp(4)] [Payload(N)]
│                                         │
│ IMU:     0x01 │ ts │ 28 bytes          │
│ Baro:    0x02 │ ts │ 16 bytes          │
│ GPS:     0x03 │ ts │ 64 bytes          │
└─────────────────────────────────────────┘


Option C: Fixed-Slot Frame (High Rate)
──────────────────────────────────────
┌─────────────────────────────────────────────────┐
│ [Timestamp(4)] [IMU(28)] [Baro(16)] [GPS flags] │
│                                                 │
│ GPS only valid when flag set (lower rate)       │
└─────────────────────────────────────────────────┘
═══════════════════════════════════════════════════════════════
```

---

## Troubleshooting

| Symptom | Cause | Solution |
|---------|-------|----------|
| High drop count | SD card slow | Use faster card (Class 10/UHS-I) |
| ISR/Sample ratio > 1.2 | Bounce/interference | Check wiring, add shielding |
| File truncated early | Buffer overflow | Increase CHUNK_BYTES |
| IMU init fails | Wiring/CS pin | Check SPI connections |
| Magnetometer init fails | I2C passthrough | Check ICM-20948 library version |
| Corrupted binary file | Power loss during write | Always press stop before power off |
| Missing JSON file | Power loss before stop | JSON created only on clean shutdown |
| All slots used | 100 files exist | Archive or delete old logs |

### Debug Serial Output

Enable verbose output by checking the serial monitor at 115200 baud during operation. The logger prints:

1. **Initialization status** - SD card, IMU, magnetometer
2. **Configuration** - Sample rates, buffer sizes
3. **Runtime stats** - Sample count, drop rate, max write time
4. **Shutdown status** - Final counts, JSON write confirmation

---

## License

MIT License - Feel free to use, modify, and distribute.

---

**Last Updated**: 2026-03-02
**Firmware Version**: 1.1+ (with magnetometer support)
**MATLAB Tools Version**: 1.0+
