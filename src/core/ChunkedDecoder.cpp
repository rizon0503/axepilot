#include "core/ChunkedDecoder.h"
#include <cstdlib>

namespace {

constexpr size_t READ_BUF_SIZE = 512;

// Blocks (yielding via sysTime.delay) until at least one byte is available.
// Returns false if the connection drops or timeoutMs elapses with nothing
// to read — callers must check the return value before touching the stream.
bool waitForData(IByteStream& stream, ISystemTime& sysTime, uint32_t timeoutMs, uint32_t& start) {
    while (stream.available() <= 0) {
        if (!stream.connected()) return false;
        if (sysTime.millis() - start > timeoutMs) return false;
        sysTime.delay(5);
    }
    start = sysTime.millis();
    return true;
}

// Reads up to `remaining` bytes into `out` in READ_BUF_SIZE blocks (instead
// of one byte at a time) until `remaining` reaches zero or the connection
// drops/times out.
bool readN(IByteStream& stream, ISystemTime& sysTime, uint32_t timeoutMs, size_t remaining, std::string& out) {
    char buf[READ_BUF_SIZE];
    uint32_t start = sysTime.millis();
    while (remaining > 0) {
        if (!waitForData(stream, sysTime, timeoutMs, start)) return false;

        size_t avail = (size_t)stream.available();
        size_t want = remaining < avail ? remaining : avail;
        if (want > READ_BUF_SIZE) want = READ_BUF_SIZE;

        size_t n = stream.readBytes(buf, want);
        if (n == 0) return false;
        out.append(buf, n);
        remaining -= n;
    }
    return true;
}

// Reads a single line up to (and excluding) '\n', discarding '\r'.
bool readLine(IByteStream& stream, ISystemTime& sysTime, uint32_t timeoutMs, std::string& line) {
    uint32_t start = sysTime.millis();
    line.clear();
    for (;;) {
        if (!waitForData(stream, sysTime, timeoutMs, start)) return false;
        int c = stream.read();
        if (c < 0) continue;
        if (c == '\n') return true;
        if (c != '\r') line += (char)c;
    }
}

} // namespace

bool ChunkedDecoder::decode(IByteStream& stream, ISystemTime& sysTime, uint32_t timeoutMs, std::string& out) {
    for (;;) {
        std::string hexLine;
        if (!readLine(stream, sysTime, timeoutMs, hexLine)) return false;
        if (hexLine.empty()) continue; // blank keep-alive line between chunks

        char* endPtr = nullptr;
        long chunkLen = strtol(hexLine.c_str(), &endPtr, 16);
        if (endPtr == hexLine.c_str() || chunkLen < 0) {
            return false; // malformed (non-hex) chunk length
        }
        if (chunkLen == 0) {
            return true; // terminating chunk — clean end of body
        }

        if (!readN(stream, sysTime, timeoutMs, (size_t)chunkLen, out)) return false;

        std::string trailingCrLf;
        if (!readLine(stream, sysTime, timeoutMs, trailingCrLf)) return false;
    }
}

bool ChunkedDecoder::readExact(IByteStream& stream, ISystemTime& sysTime, uint32_t timeoutMs, size_t length, std::string& out) {
    return readN(stream, sysTime, timeoutMs, length, out);
}
