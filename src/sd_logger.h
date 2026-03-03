// sd_logger.h - SD card operations

#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "SdFat.h"
#include "config.h"

class SdLogger {
public:
    // Initialize SD card
    bool begin();

    // Create new log file with sequential numbering
    // Returns file number (0-99) or 255 on failure
    uint8_t createLogFile(char* binFilename, char* jsonFilename);

    // Preallocate file space
    bool preallocate();

    // Write a full chunk from buffer to SD
    // Returns true on success, false on failure
    bool writeChunk(const uint8_t* data, uint32_t len);

    // Write partial data (for final flush)
    bool writePartial(const uint8_t* data, uint32_t len);

    // Truncate to actual size and close file
    void finalize(uint64_t actualBytes);

    // Get current file position
    uint64_t getPosition() const;

    // Get max write time observed
    uint32_t getMaxWriteTimeUs() const;

    // Check if file position is at/near preallocation limit
    bool isNearFull(uint32_t marginBytes = CHUNK_BYTES) const;

    // Write count
    uint32_t getWriteChunkCount() const;

    // Helper to find next available file number
    static uint8_t findNextFileNumber();

private:
    SdFs sd;
    FsFile file;
    uint32_t maxWriteTimeUs = 0;
    uint32_t writeChunks = 0;
    uint8_t currentFileNumber = 255;
    char currentBinFilename[16] = {0};
};
