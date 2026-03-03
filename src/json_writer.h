// json_writer.h - JSON metadata file generation

#pragma once
#include <Arduino.h>
#include <stdint.h>

// Forward declarations
struct Telemetry;
class PingPongBuffer;
class ImuSource;

class JsonWriter {
public:
    // Write JSON header file with full configuration and performance data
    static bool write(const char* filename,
                      const char* binFilename,
                      const char* jsonFilename,
                      uint8_t fileNumber,
                      const ImuSource& imu,
                      const PingPongBuffer& buffer,
                      const Telemetry& telemetry,
                      uint32_t actualFileSize);
};
