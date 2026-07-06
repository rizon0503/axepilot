#pragma once
#include "interfaces/IByteStream.h"
#include <string>
#include <algorithm>
#include <cstring>

// Feeds bytes from a preset buffer. maxAvailablePerCall caps how many bytes
// "arrive" per available() call, so tests can simulate reads split across
// multiple socket polls (the worst case for a chunked-transfer parser).
class MockByteStream : public IByteStream {
public:
    std::string data;
    size_t pos = 0;
    size_t maxAvailablePerCall = std::string::npos;
    bool isConnected = true; // flips to false once disconnectAtPos is reached

    // If set, connected() reports false as soon as `pos` reaches this offset —
    // simulates the server dropping the connection mid-response.
    size_t disconnectAtPos = std::string::npos;

    int available() override {
        if (pos >= disconnectAtPos) {
            isConnected = false;
        }
        size_t remaining = data.size() - pos;
        if (remaining == 0) return 0;
        return (int)std::min(remaining, maxAvailablePerCall);
    }

    int read() override {
        if (available() <= 0) return -1;
        return (unsigned char)data[pos++];
    }

    size_t readBytes(char* buf, size_t len) override {
        size_t avail = (size_t)available();
        size_t n = std::min(len, avail);
        if (n == 0) return 0;
        std::memcpy(buf, data.data() + pos, n);
        pos += n;
        return n;
    }

    bool connected() override {
        if (pos >= disconnectAtPos) {
            isConnected = false;
        }
        return isConnected;
    }
};
