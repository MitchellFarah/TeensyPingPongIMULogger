// sensor_interface.h - Abstract interface for data sources

#pragma once
#include <stdint.h>

// Interface that all data sources must implement
// This enables adding new sensors in the future while maintaining
// compatibility with the existing logging infrastructure
class DataSource {
public:
    virtual ~DataSource() = default;

    // Initialize the sensor hardware
    virtual bool begin() = 0;

    // Attach interrupt (if sensor uses interrupts)
    virtual void attachInterrupt() {}
    virtual void detachInterrupt() {}

    // Check if sample is pending
    virtual bool hasPendingSample() const = 0;

    // Try to read a sample into the provided buffer
    // Returns: number of bytes written to buffer, 0 if no sample available
    // The buffer must be large enough for the sensor's record type
    virtual uint32_t tryRead(void* buffer, uint32_t maxLen) = 0;

    // Get the fixed record size for this sensor
    virtual uint32_t getRecordSize() const = 0;

    // Get source ID (for tagged envelope format, future enhancement)
    virtual uint8_t getSourceId() const = 0;

    // Get debug counters
    virtual uint32_t getSampleCount() const = 0;
};
