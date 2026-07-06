#pragma once
#include <string>

// Structured HTTP outcome. Consumers check `ok` directly instead of the old
// fragile contract of parsing the body as JSON and looking for a
// {"error_http": ..., "error_msg": ...} sentinel object embedded in it —
// a real API response could in principle contain those same keys, and
// several consumers didn't check for it at all, silently treating an error
// body as valid data (see #19).
// Explicit constructors (rather than relying on aggregate init with default
// member initializers) so HttpResult{...} works the same regardless of
// which C++ standard a given build target compiles with — the esp32-cyd
// env doesn't opt into C++14/17 aggregate rules the way the native test
// env's -std=gnu++17 does.
struct HttpResult {
    bool ok;
    int statusCode;
    std::string body;         // response body when ok; empty on failure
    std::string errorMessage; // human-readable reason when !ok

    HttpResult() : ok(false), statusCode(0) {}
    HttpResult(bool ok, int statusCode, std::string body, std::string errorMessage)
        : ok(ok), statusCode(statusCode), body(std::move(body)), errorMessage(std::move(errorMessage)) {}
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual HttpResult get(const std::string& url) = 0;
    virtual HttpResult post(const std::string& url, const std::string& payload, const std::string& headers = "") = 0;
    virtual HttpResult patch(const std::string& url, const std::string& payload, const std::string& headers = "") = 0;
};
