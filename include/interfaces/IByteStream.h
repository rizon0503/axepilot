#pragma once
#include <cstddef>

// Abstraction over a raw byte-oriented network stream (Arduino's
// WiFiClient/Stream on the device) so the chunked-transfer decoder in
// core/ChunkedDecoder can be tested on the host without any ESP32/WiFi
// dependency.
class IByteStream {
public:
    virtual ~IByteStream() = default;
    virtual int available() = 0;                       // bytes ready to read now, 0 if none
    virtual int read() = 0;                             // read a single byte, -1 if none available
    virtual size_t readBytes(char* buf, size_t len) = 0; // block read, returns actual bytes read (may be < len)
    virtual bool connected() = 0;                        // false once the underlying connection has closed
};
