// telemetry.h - Debug and telemetry printing

#pragma once
#include <Arduino.h>
#include "config.h"
#include "types.h"

// Forward declarations
class ImuSource;
class PingPongBuffer;

class TelemetryPrinter {
public:
    // Print startup banner
    static void printStartupBanner() {
        SERIAL_PORT.println(F("=== Teensy IMU Logger ==="));
        SERIAL_PORT.println(F("Refactored Architecture - v2.0"));
    }

    // Print periodic status during logging
    static void printStatus(Telemetry& telem,
                           const ImuSource& imu,
                           const PingPongBuffer& buffer,
                           uint32_t now) {
        uint32_t dt = now - telem.lastTime;
        if (dt == 0) dt = 1; // Avoid divide by zero

        // Calculate rates
        uint32_t isrCount = imu.getIsrCount();
        uint32_t samplesTaken = imu.getSamplesTaken();
        float isrRate = (float)(isrCount - telem.lastIsrCount) * 1000.0 / dt;
        float sampleRate = (float)(samplesTaken - telem.lastSamplesTaken) * 1000.0 / dt;

        telem.lastIsrCount = isrCount;
        telem.lastSamplesTaken = samplesTaken;
        telem.lastTime = now;

        SERIAL_PORT.print(F("ISR: "));
        SERIAL_PORT.print(isrRate, 1);
        SERIAL_PORT.print(F(" Hz, Samples: "));
        SERIAL_PORT.print(sampleRate, 1);
        SERIAL_PORT.print(F(" Hz, Acquired: "));
        SERIAL_PORT.print(samplesTaken);
        SERIAL_PORT.print(F(", Drops: "));
        SERIAL_PORT.print(buffer.getDropCount());
        SERIAL_PORT.print(F(", ISR/Sample: "));
        SERIAL_PORT.print(samplesTaken > 0 ? (float)isrCount / samplesTaken : 0, 2);
        SERIAL_PORT.println();
    }

    // Print write status after chunk write
    static void printWriteStatus(uint32_t samplesRecorded,
                                 uint32_t maxWriteTimeUs,
                                 const PingPongBuffer& buffer) {
        static uint32_t lastWritePrint = 0;
        uint32_t now = millis();

        if (now - lastWritePrint > 5000) {
            lastWritePrint = now;
            SERIAL_PORT.print(F("Wrote chunk, samples: "));
            SERIAL_PORT.print(samplesRecorded);
            SERIAL_PORT.print(F(", max write time: "));
            SERIAL_PORT.print(maxWriteTimeUs);
            SERIAL_PORT.print(F(" us, buffers filled: ["));
            SERIAL_PORT.print(buffer.getBufferFullCount(0));
            SERIAL_PORT.print(F(", "));
            SERIAL_PORT.print(buffer.getBufferFullCount(1));
            SERIAL_PORT.print(F("], swaps: "));
            SERIAL_PORT.print(buffer.getSwapCount());
            SERIAL_PORT.print(F(", drops: "));
            SERIAL_PORT.println(buffer.getDropCount());
        }
    }

    // Print final report at completion
    static void printFinalReport(const Telemetry& telem,
                                 const ImuSource& imu,
                                 const PingPongBuffer& buffer,
                                 uint32_t samplesRecorded) {
        uint32_t isrCount = imu.getIsrCount();
        uint32_t samplesTaken = imu.getSamplesTaken();
        uint32_t dropCount = buffer.getDropCount();

        SERIAL_PORT.println(F("\n=== Logging Complete ==="));
        SERIAL_PORT.print(F("Total ISR calls: "));
        SERIAL_PORT.println(isrCount);
        SERIAL_PORT.print(F("Total samples acquired: "));
        SERIAL_PORT.println(samplesTaken);
        SERIAL_PORT.print(F("Total samples recorded to SD: "));
        SERIAL_PORT.println(samplesRecorded);
        SERIAL_PORT.print(F("Total samples dropped: "));
        SERIAL_PORT.println(dropCount);
        SERIAL_PORT.print(F("ISR/Sample ratio: "));
        SERIAL_PORT.println(samplesTaken > 0 ? (float)isrCount / samplesTaken : 0, 3);
        SERIAL_PORT.print(F("Drop rate: "));
        if (samplesRecorded + dropCount > 0) {
            SERIAL_PORT.print(100.0 * dropCount / (samplesRecorded + dropCount), 2);
            SERIAL_PORT.println(F("%"));
        } else {
            SERIAL_PORT.println(F("N/A"));
        }
        SERIAL_PORT.print(F("Max SD write time: "));
        SERIAL_PORT.print(telem.maxWriteTimeUs);
        SERIAL_PORT.println(F(" us"));
        SERIAL_PORT.print(F("Expected file size: "));
        SERIAL_PORT.print((samplesRecorded * sizeof(ImuRecord)) / 1024);
        SERIAL_PORT.println(F(" KiB"));
        SERIAL_PORT.print(F("Buffer 0 filled: "));
        SERIAL_PORT.println(buffer.getBufferFullCount(0));
        SERIAL_PORT.print(F("Buffer 1 filled: "));
        SERIAL_PORT.println(buffer.getBufferFullCount(1));
        SERIAL_PORT.print(F("Buffer swaps: "));
        SERIAL_PORT.println(buffer.getSwapCount());
    }
};
