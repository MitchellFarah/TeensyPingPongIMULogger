// pingpong_buffer.cpp - Ping-pong buffer management implementation

#include "pingpong_buffer.h"

void PingPongBuffer::init() {
    activeBuf = 0;
    wrIndex = 0;
    ready[0] = false;
    ready[1] = false;
    bufferFullCount[0] = 0;
    bufferFullCount[1] = 0;
    swapCount = 0;
    dropCount = 0;
}

bool PingPongBuffer::tryPush(const void* data, uint32_t len) {
    // Check if record fits in current buffer
    if (wrIndex + len > CHUNK_BYTES) {
        // Buffer is full - try to swap

        // Mark current buffer as ready
        ready[activeBuf] = true;
        bufferFullCount[activeBuf]++;

        // Try to swap to other buffer
        uint8_t nextBuf = activeBuf ^ 1;

        // If other buffer is also ready, we have no space - drop
        if (ready[nextBuf]) {
            dropCount++;
            return false;
        }

        // Swap to empty buffer
        activeBuf = nextBuf;
        wrIndex = 0;
        swapCount++;
    }

    // Copy record to active buffer
    memcpy(&chunk[activeBuf][wrIndex], data, len);
    wrIndex += len;

    return true;
}

bool PingPongBuffer::isReady(uint8_t bufIndex) const {
    return (bufIndex < 2) ? ready[bufIndex] : false;
}

uint8_t* PingPongBuffer::getBuf(uint8_t bufIndex) {
    return (bufIndex < 2) ? chunk[bufIndex] : nullptr;
}

void PingPongBuffer::markFree(uint8_t bufIndex) {
    if (bufIndex < 2) {
        ready[bufIndex] = false;
    }
}

uint32_t PingPongBuffer::getActiveBytes() const {
    return wrIndex;
}

uint8_t PingPongBuffer::getActiveIndex() const {
    return activeBuf;
}

uint8_t* PingPongBuffer::getActiveBuf() {
    return chunk[activeBuf];
}

uint32_t PingPongBuffer::getBufferFullCount(uint8_t bufIndex) const {
    return (bufIndex < 2) ? bufferFullCount[bufIndex] : 0;
}

uint32_t PingPongBuffer::getSwapCount() const {
    return swapCount;
}

uint32_t PingPongBuffer::getDropCount() const {
    return dropCount;
}
