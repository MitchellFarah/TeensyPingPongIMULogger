// imu_source.h - IMU hardware abstraction

#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "ICM_20948.h"
#include "types.h"

// Forward declaration - declared in .cpp
class ImuSource;
extern ImuSource* g_imuSource;

class ImuSource {
public:
    ImuSource();

    // Initialize IMU hardware and configure registers
    bool begin();

    // Attach/detach DRDY interrupt
    void attachInterrupt();
    void detachInterrupt();

    // Check if a sample is pending (flag set by ISR)
    bool hasPendingSample() const;

    // Try to read a sample into the provided record
    // Returns true if a sample was read (flag was set), false otherwise
    // Clears the interrupt flag atomically before reading
    bool tryRead(ImuRecord& out);

    // Clear any pending interrupts in IMU hardware
    void clearInterrupts();

    // Direct read without checking flag (for draining at stop)
    bool readDirect(ImuRecord& out);

    // Debug counters
    uint32_t getIsrCount() const;
    uint32_t getSamplesTaken() const;
    uint32_t getSeqCounter() const;

private:
    ICM_20948_SPI myICM;

    // Volatile state shared with ISR
    volatile bool dataReadyFlag = false;
    volatile uint32_t isrTimestamp = 0;
    volatile uint32_t isrCount = 0;
    volatile uint32_t samplesTaken = 0;
    volatile uint32_t seqCounter = 0;

    // ISR handler
    void handleIsr();
    friend void drdy_isr_wrapper();
};
