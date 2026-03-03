// pingpong_buffer.h - Ping-pong buffer management

#pragma once
#include <stdint.h>
#include <string.h>
#include "config.h"

class PingPongBuffer {
public:
    // Initialize the buffer manager
    void init();

    // Try to push a record to the active buffer
    // Returns true if pushed successfully, false if dropped (both buffers full)
    bool tryPush(const void* data, uint32_t len);

    // Check if buffer i has data ready for writing
    bool isReady(uint8_t bufIndex) const;

    // Get pointer to buffer i (for SD writing)
    uint8_t* getBuf(uint8_t bufIndex);

    // Mark buffer i as written/free (call after SD write completes)
    void markFree(uint8_t bufIndex);

    // Get current write position in active buffer (for final partial write)
    uint32_t getActiveBytes() const;

    // Get active buffer index (for final partial write)
    uint8_t getActiveIndex() const;

    // Get pointer to active buffer
    uint8_t* getActiveBuf();

    // Debug counters
    uint32_t getBufferFullCount(uint8_t bufIndex) const;
    uint32_t getSwapCount() const;
    uint32_t getDropCount() const;

private:
    // Aligned to 32 bytes for DMA/cache-friendly access
    alignas(32) uint8_t chunk[2][CHUNK_BYTES];
    volatile uint8_t activeBuf = 0;      // Currently filling buffer (0 or 1)
    volatile uint32_t wrIndex = 0;       // Write position in active buffer
    volatile bool ready[2] = {false, false};  // Buffer ready for SD write
    volatile uint32_t bufferFullCount[2] = {0, 0};  // How many times each buffer filled
    volatile uint32_t swapCount = 0;     // Buffer swap count
    volatile uint32_t dropCount = 0;     // Records dropped (both buffers full)
};
