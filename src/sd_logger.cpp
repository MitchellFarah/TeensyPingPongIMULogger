// sd_logger.cpp - SD card operations implementation

#include "sd_logger.h"
#include "FreeStack.h"

bool SdLogger::begin() {
    SERIAL_PORT.print(F("Initializing SD card..."));
    if (!sd.begin(SdioConfig(FIFO_SDIO))) {
        SERIAL_PORT.println(F(" FAILED"));
        return false;
    }
    SERIAL_PORT.println(F(" OK"));
    return true;
}

uint8_t SdLogger::createLogFile(char* binFilename, char* jsonFilename) {
    SERIAL_PORT.print(F("Finding next available file number..."));
    currentFileNumber = findNextFileNumber();

    if (currentFileNumber == 255) {
        SERIAL_PORT.println(F(" FAILED - All 100 slots used"));
        return 255;
    }

    SERIAL_PORT.print(F(" using imuLog_"));
    if (currentFileNumber < 10) SERIAL_PORT.print(F("0"));
    SERIAL_PORT.print(currentFileNumber);
    SERIAL_PORT.println(F(".bin"));

    // Generate filenames
    sprintf(currentBinFilename, "imuLog_%02d.bin", currentFileNumber);
    sprintf(binFilename, "imuLog_%02d.bin", currentFileNumber);
    sprintf(jsonFilename, "imuLog_%02d.json", currentFileNumber);

    SERIAL_PORT.print(F("FreeStack: "));
    SERIAL_PORT.println(FreeStack());

    // Open/create log file (truncate if exists)
    SERIAL_PORT.print(F("Opening log file: "));
    SERIAL_PORT.println(currentBinFilename);
    if (!file.open(currentBinFilename, O_CREAT | O_TRUNC | O_RDWR)) {
        SERIAL_PORT.println(F("File open FAILED"));
        return 255;
    }

    return currentFileNumber;
}

bool SdLogger::preallocate() {
    SERIAL_PORT.print(F("Preallocating "));
    SERIAL_PORT.print(PREALLOC_BYTES / (1024ULL * 1024ULL * 1024ULL));
    SERIAL_PORT.println(F(" GiB..."));

    if (!file.preAllocate(PREALLOC_BYTES)) {
        SERIAL_PORT.println(F("Preallocate FAILED"));
        return false;
    }
    SERIAL_PORT.println(F("Preallocation OK"));
    return true;
}

bool SdLogger::writeChunk(const uint8_t* data, uint32_t len) {
    uint32_t startTime = micros();
    size_t written = file.write(data, len);
    uint32_t writeTime = micros() - startTime;

    if (written != len) {
        return false;
    }

    // Update metrics
    if (writeTime > maxWriteTimeUs) {
        maxWriteTimeUs = writeTime;
    }
    writeChunks++;

    return true;
}

bool SdLogger::writePartial(const uint8_t* data, uint32_t len) {
    size_t written = file.write(data, len);
    return (written == len);
}

void SdLogger::finalize(uint64_t actualBytes) {
    SERIAL_PORT.println(F("Truncating file..."));
    if (!file.truncate()) {
        SERIAL_PORT.println(F("Truncate FAILED"));
    }

    SERIAL_PORT.println(F("Closing file..."));
    file.close();
}

uint64_t SdLogger::getPosition() const {
    return file.curPosition();
}

uint32_t SdLogger::getMaxWriteTimeUs() const {
    return maxWriteTimeUs;
}

bool SdLogger::isNearFull(uint32_t marginBytes) const {
    uint64_t currentPos = file.curPosition();
    return (currentPos + marginBytes > PREALLOC_BYTES);
}

uint32_t SdLogger::getWriteChunkCount() const {
    return writeChunks;
}

uint8_t SdLogger::findNextFileNumber() {
    bool usedNumbers[100] = {false};

    FsFile dir;
    if (!dir.open("/", O_RDONLY)) {
        return 255;
    }

    FsFile entry;
    while (entry.openNext(&dir, O_RDONLY)) {
        char filename[32];
        if (entry.getName(filename, sizeof(filename))) {
            int num;
            if (sscanf(filename, "imuLog_%02d.bin", &num) == 1) {
                if (num >= 0 && num <= 99) {
                    usedNumbers[num] = true;
                }
            }
        }
        entry.close();
    }
    dir.close();

    for (int i = 0; i < 100; i++) {
        if (!usedNumbers[i]) {
            return (uint8_t)i;
        }
    }

    return 255;
}
