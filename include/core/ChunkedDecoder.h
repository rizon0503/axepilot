#pragma once
#include <string>
#include <cstdint>
#include "interfaces/IByteStream.h"
#include "interfaces/ISystemTime.h"

// Pure, hardware-independent HTTP body readers extracted from
// EspHttpClient::post(). Standard Arduino HTTPClient::getString() truncates
// or drops chunked-transfer-encoded bodies, which every LLM API in this
// project sends — hence the manual parsing, now testable on the host.
namespace ChunkedDecoder {

// Decodes an HTTP chunked-transfer-encoded body from `stream` into `out`.
// Returns true once the terminating zero-length chunk is read cleanly;
// false on a dropped connection, a stall past `timeoutMs`, or a malformed
// (non-hex) chunk length.
bool decode(IByteStream& stream, ISystemTime& sysTime, uint32_t timeoutMs, std::string& out);

// Reads exactly `length` bytes from `stream` into `out` (the Content-Length
// path, when the server didn't chunk the response). Returns true only if
// the full length was read before a drop/timeout.
bool readExact(IByteStream& stream, ISystemTime& sysTime, uint32_t timeoutMs, size_t length, std::string& out);

} // namespace ChunkedDecoder
