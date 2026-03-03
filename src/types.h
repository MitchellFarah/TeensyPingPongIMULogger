// types.h - Type definitions for TeensyPingPongIMULogger

#pragma once
#include <stdint.h>

// ============================================
// Record Structure (28 bytes packed)
// ============================================

#pragma pack(push, 1)
struct ImuRecord {
    uint32_t seq;        // Sequence number (monotonic counter)
    uint32_t ts_us;      // micros() timestamp (uint32, rolls over)
    int16_t ax, ay, az;  // Accelerometer (raw int16)
    int16_t gx, gy, gz;  // Gyroscope (raw int16)
    int16_t temp;        // Temperature (raw int16)
    int16_t mx, my, mz;  // Magnetometer (raw int16)
};
#pragma pack(pop)

// Verify record size is exactly 28 bytes
static_assert(sizeof(ImuRecord) == 28, "ImuRecord must be 28 bytes packed");

// ============================================
// Telemetry State Structure
// ============================================

struct Telemetry {
    // Performance metrics
    uint32_t maxWriteTimeUs = 0;      // Maximum SD write time observed
    uint32_t samplesRecorded = 0;     // Total samples written to SD

    // Timing for status printing
    uint32_t lastStatusPrint = 0;     // Last time status was printed
    uint32_t lastIsrCount = 0;        // ISR count at last status print
    uint32_t lastSamplesTaken = 0;    // Samples taken at last status print
    uint32_t lastTime = 0;            // Timestamp at last status print
    uint32_t lastWritePrint = 0;      // Last time write status was printed
};
