// config.h - Central configuration for TeensyPingPongIMULogger
// All user-configurable parameters extracted from main.cpp

#pragma once
#include "ICM_20948.h"  // For IMU enum types (gpm2, dps250, etc.)

// ============================================
// Board + Logging Configuration
// ============================================

#define SERIAL_PORT          Serial
#define LED_PIN              LED_BUILTIN

// Buffer configuration
#define CHUNK_BYTES          (128 * 1024)     // 128 KiB per buffer
#define PREALLOC_BYTES       (1ULL << 30)     // 1 GiB preallocation

// File naming
#define FILE_PREFIX          "imuLog_"
#define MAX_FILE_SLOTS       100              // 00-99

// ============================================
// IMU Hardware Configuration
// ============================================

#define IMU_DRDY_PIN         33               // DRDY interrupt pin (connect to ICM-20948 INT)
#define IMU_CS_PIN           36               // SPI chip select pin
#define IMU_SPI_PORT         SPI              // SPI port for IMU communication
#define IMU_SPI_CLOCK_HZ     1000000          // 1 MHz SPI clock
#define IMU_DRDY_MODE        FALLING          // RISING/FALLING/CHANGE (use FALLING for SparkFun breakout)

// ============================================
// IMU Behavior Configuration (User-Facing)
// ============================================

// Accel Full Scale Range: gpm2, gpm4, gpm8, gpm16
#define IMU_ACCEL_FSR        gpm2             // +/- 2g

// Gyro Full Scale Range: dps250, dps500, dps1000, dps2000
#define IMU_GYRO_FSR         dps250           // +/- 250 degrees/sec

// Accel DLPF settings:
//   acc_d246bw_n265bw   - 246.0 Hz 3dB BW,  265.0 Hz Nyquist BW
//   acc_d111bw4_n136bw  - 111.4 Hz 3dB BW, 136.0 Hz Nyquist BW
//   acc_d50bw4_n68bw8   - 50.4 Hz 3dB BW,  68.8 Hz Nyquist BW
//   acc_d23bw9_n34bw4   - 23.9 Hz 3dB BW,  34.4 Hz Nyquist BW
//   acc_d11bw5_n17bw    - 11.5 Hz 3dB BW,  17.0 Hz Nyquist BW
//   acc_d5bw7_n8bw3     - 5.7 Hz 3dB BW,   8.3 Hz Nyquist BW
//   acc_d473bw_n499bw   - 473.0 Hz 3dB BW, 499.0 Hz Nyquist BW
#define IMU_ACCEL_DLPF       acc_d473bw_n499bw

// Gyro DLPF settings:
//   gyr_d196bw6_n229bw8 - 196.6 Hz 3dB BW, 229.8 Hz Nyquist BW
//   gyr_d151bw8_n187bw6 - 151.8 Hz 3dB BW, 187.6 Hz Nyquist BW
//   gyr_d119bw5_n154bw3 - 119.5 Hz 3dB BW, 154.3 Hz Nyquist BW
//   gyr_d51bw2_n73bw3   - 51.2 Hz 3dB BW,  73.3 Hz Nyquist BW
//   gyr_d23bw9_n35bw9   - 23.9 Hz 3dB BW,  35.9 Hz Nyquist BW
//   gyr_d11bw6_n17bw8   - 11.6 Hz 3dB BW,  17.8 Hz Nyquist BW
//   gyr_d5bw7_n8bw9     - 5.7 Hz 3dB BW,   8.9 Hz Nyquist BW
//   gyr_d361bw4_n376bw5 - 361.4 Hz 3dB BW, 376.5 Hz Nyquist BW (bypass mode)
#define IMU_GYRO_DLPF        gyr_d5bw7_n8bw9

// Sample Rate Divider (SMPLRT_DIV): 0-255
// ODR = 1125 / (SMPLRT_DIV + 1)
// Examples: 0 = 1125 Hz, 10 = ~102.27 Hz, 112 = ~10 Hz
#define IMU_SMPLRT_DIV       10               // 1125/11 = ~102.27 Hz

// ============================================
// Interrupt Configuration
// ============================================

#define IMU_INT_ACTIVE_LOW   true             // true = active low (SparkFun breakout with pullup)
#define IMU_INT_OPEN_DRAIN   false            // false = push-pull, true = open-drain
#define IMU_INT_LATCH        true             // true = latched until cleared (more reliable)

// ============================================
// Derived Configuration Helpers
// ============================================

// Calculate actual ODR from SMPLRT_DIV
#define IMU_ODR_HZ           (1125.0 / (IMU_SMPLRT_DIV + 1))
