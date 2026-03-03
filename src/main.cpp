// Teensy IMU Logger - Refactored Modular Architecture
// Based on original implementation, reorganized for maintainability

#ifndef DISABLE_FS_H_WARNING
#define DISABLE_FS_H_WARNING
#endif

#include "SdFat.h"
#include "ICM_20948.h"
#include "SPI.h"
#include "FreeStack.h"

#include "config.h"
#include "types.h"
#include "pingpong_buffer.h"
#include "imu_source.h"
#include "sd_logger.h"
#include "json_writer.h"
#include "telemetry.h"

// Global module instances
static ImuSource imu;
static PingPongBuffer ppBuffer;
static SdLogger logger;
static Telemetry telemetry;

// State machine states
enum class State {
    INIT,
    WAIT_START,
    LOGGING,
    STOPPING,
    FINALIZED
};
static State state = State::INIT;

// Filenames
static char binFilename[16];
static char jsonFilename[16];
static uint8_t currentFileNumber = 0;

// Forward declarations
void serviceWriter();
void stopAndFinalize();

// ============================================
// Setup
// ============================================

void setup() {
    SERIAL_PORT.begin(115200);
    while (!SERIAL_PORT) yield();

    TelemetryPrinter::printStartupBanner();

    // Configure DRDY pin
    pinMode(IMU_DRDY_PIN, INPUT);

    // Initialize IMU
    if (!imu.begin()) {
        SERIAL_PORT.println(F("IMU init failed"));
        while (1) yield();
    }

    // Wait for start command
    SERIAL_PORT.println(F("Press any key to start logging..."));
    while (!SERIAL_PORT.available()) yield();
    while (SERIAL_PORT.read() >= 0) {}

    // Initialize SD and create log file
    if (!logger.begin()) {
        SERIAL_PORT.println(F("SD init failed"));
        while (1) yield();
    }

    currentFileNumber = logger.createLogFile(binFilename, jsonFilename);
    if (currentFileNumber == 255) {
        SERIAL_PORT.println(F("No available file slots"));
        while (1) yield();
    }

    if (!logger.preallocate()) {
        SERIAL_PORT.println(F("Preallocate failed"));
        while (1) yield();
    }

    // Clear any pending interrupts and start
    imu.clearInterrupts();
    imu.attachInterrupt();

    ppBuffer.init();
    telemetry.lastTime = millis();

    SERIAL_PORT.println(F("=== Logging Started ==="));
    SERIAL_PORT.println(F("Type any character and press Enter to stop"));
    state = State::LOGGING;
}

// ============================================
// Main Loop
// ============================================

void loop() {
    if (state == State::LOGGING) {
        // Process pending IMU samples
        ImuRecord rec;
        if (imu.tryRead(rec)) {
            if (!ppBuffer.tryPush(&rec, sizeof(rec))) {
                // Drop - both buffers full (drop count tracked inside PingPongBuffer)
            }
        }

        // Service SD writer
        serviceWriter();

        // Check for stop command
        if (SERIAL_PORT.available()) {
            SERIAL_PORT.println(F("\nStop command received"));
            while (SERIAL_PORT.read() >= 0) {}
            state = State::STOPPING;
            return;
        }

        // Check for file full
        if (logger.isNearFull()) {
            SERIAL_PORT.println(F("\nFile full - stopping"));
            state = State::STOPPING;
            return;
        }

        // Periodic status print
        uint32_t now = millis();
        if (now - telemetry.lastStatusPrint > 2000) {
            TelemetryPrinter::printStatus(telemetry, imu, ppBuffer, now);
            telemetry.lastStatusPrint = now;
        }
    }
    else if (state == State::STOPPING) {
        stopAndFinalize();
        state = State::FINALIZED;
    }
    else if (state == State::FINALIZED) {
        SERIAL_PORT.println(F("\nLogger stopped. Press Reset to restart."));
        while (1) yield();
    }
}

// ============================================
// Service Writer: Write ready buffers to SD
// ============================================

void serviceWriter() {
    for (uint8_t i = 0; i < 2; i++) {
        if (!ppBuffer.isReady(i)) continue;

        // Check if we're near the file limit before writing
        if (logger.isNearFull()) {
            SERIAL_PORT.println(F("File full during write"));
            state = State::STOPPING;
            return;
        }

        // Write full chunk
        if (logger.writeChunk(ppBuffer.getBuf(i), CHUNK_BYTES)) {
            ppBuffer.markFree(i);
            telemetry.samplesRecorded += CHUNK_BYTES / sizeof(ImuRecord);
        } else {
            SERIAL_PORT.println(F("Write failed"));
            state = State::STOPPING;
            return;
        }

        TelemetryPrinter::printWriteStatus(telemetry.samplesRecorded,
                                           logger.getMaxWriteTimeUs(),
                                           ppBuffer);
    }
}

// ============================================
// Stop and Finalize
// ============================================

void stopAndFinalize() {
    SERIAL_PORT.println(F("\n=== Stopping Logging ==="));

    // Disable interrupts
    imu.detachInterrupt();
    delay(10);

    // Drain pending sample (was flagged but not read)
    ImuRecord rec;
    if (imu.hasPendingSample()) {
        imu.tryRead(rec);
        ppBuffer.tryPush(&rec, sizeof(rec));
    }

    // Drain full buffers
    SERIAL_PORT.println(F("Draining buffers..."));
    while (ppBuffer.isReady(0) || ppBuffer.isReady(1)) {
        serviceWriter();
    }

    // Write final partial chunk
    uint32_t finalBytes = ppBuffer.getActiveBytes();
    if (finalBytes > 0) {
        SERIAL_PORT.print(F("Writing final "));
        SERIAL_PORT.print(finalBytes);
        SERIAL_PORT.println(F(" bytes..."));
        logger.writePartial(ppBuffer.getActiveBuf(), finalBytes);
        telemetry.samplesRecorded += finalBytes / sizeof(ImuRecord);
    }

    // Get actual file position for metadata
    uint64_t actualSize = logger.getPosition();

    // Finalize file (truncate and close)
    logger.finalize(actualSize);

    // Print final telemetry
    TelemetryPrinter::printFinalReport(telemetry, imu, ppBuffer, telemetry.samplesRecorded);

    // Write JSON
    SERIAL_PORT.println(F("\nWriting JSON header..."));
    JsonWriter::write(jsonFilename, binFilename, jsonFilename, currentFileNumber,
                      imu, ppBuffer, telemetry, static_cast<uint32_t>(actualSize));
}
