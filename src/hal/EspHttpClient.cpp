#include "hal/EspHttpClient.h"
#include "hal/EspSystemTime.h"
#include "hal/RootCerts.h"
#include "core/ChunkedDecoder.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp32-hal-log.h>
#include <memory>

namespace {

// Adapts an Arduino WiFiClient/HTTPClient pair to the hardware-independent
// IByteStream interface so ChunkedDecoder (core/, natively tested) can read
// the response body without knowing about WiFiClient at all.
class EspByteStream : public IByteStream {
public:
    EspByteStream(WiFiClient* client, HTTPClient* http) : client(client), http(http) {}
    int available() override { return client->available(); }
    int read() override { return client->read(); }
    size_t readBytes(char* buf, size_t len) override { return client->readBytes(buf, len); }
    bool connected() override { return http->connected(); }

private:
    WiFiClient* client;
    HTTPClient* http;
};

// LLM APIs (DeepSeek) can take a long time to answer; local Bitaxe and
// Telegram getUpdates?timeout=0 respond quickly, so a shorter timeout keeps
// the main loop responsive when the network misbehaves.
constexpr uint16_t TIMEOUT_FAST_MS = 10000;
constexpr uint16_t TIMEOUT_LLM_MS = 45000;

std::unique_ptr<WiFiClient> makeClient(const std::string& url) {
    if (url.rfind("https://", 0) == 0) {
        auto secureClient = std::unique_ptr<WiFiClientSecure>(new WiFiClientSecure());
        if (url.rfind("https://api.deepseek.com", 0) == 0) {
            secureClient->setCACert(RootCerts::AMAZON_ROOT_CA_1);
        } else {
            // A user-configured AI_BASE_URL (OpenRouter, self-hosted Ollama,
            // etc.) can point anywhere — we can't pin a CA we don't know.
            secureClient->setInsecure();
        }
        return secureClient;
    }
    return std::unique_ptr<WiFiClient>(new WiFiClient());
}

void beginRequest(HTTPClient& http, WiFiClient& client, const std::string& url, uint16_t timeoutMs) {
    http.begin(client, url.c_str());
    http.setReuse(false);
    http.setTimeout(timeoutMs);
}

HttpResult errorResult(int httpCode, const char* msg) {
    log_w("HTTP error: code=%d msg=%s", httpCode, msg);
    return {false, httpCode, "", msg};
}

HttpResult readSimpleResponse(HTTPClient& http, int httpCode) {
    if (httpCode <= 0) {
        return errorResult(httpCode, "HttpFailed");
    }
    String res = http.getString();
    if (res.length() == 0) {
        return errorResult(httpCode, "EmptyBody");
    }
    return {true, httpCode, res.c_str(), ""};
}

} // namespace

EspHttpClient::EspHttpClient() {
    telegramClient.setCACert(RootCerts::GODADDY_ROOT_CA_G2);
}

bool EspHttpClient::isTelegramUrl(const std::string& url) {
    return url.rfind("https://api.telegram.org", 0) == 0;
}

HttpResult EspHttpClient::get(const std::string& url) {
    if (isTelegramUrl(url)) {
        telegramHttp.begin(telegramClient, url.c_str());
        telegramHttp.setReuse(true); // keep the TLS session alive between polls
        telegramHttp.setTimeout(TIMEOUT_FAST_MS);
        int httpCode = telegramHttp.GET();
        HttpResult result = readSimpleResponse(telegramHttp, httpCode);
        telegramHttp.end(); // with reuse enabled the socket stays connected
        return result;
    }

    auto client = makeClient(url);
    HTTPClient http;
    beginRequest(http, *client, url, TIMEOUT_FAST_MS);

    int httpCode = http.GET();
    HttpResult result = readSimpleResponse(http, httpCode);
    http.end();
    return result;
}

HttpResult EspHttpClient::post(const std::string& url, const std::string& payload, const std::string& headers) {
    if (isTelegramUrl(url)) {
        telegramHttp.begin(telegramClient, url.c_str());
        telegramHttp.setReuse(true);
        telegramHttp.setTimeout(TIMEOUT_FAST_MS);
        telegramHttp.addHeader("Content-Type", "application/json");
        int httpCode = telegramHttp.POST(payload.c_str());
        HttpResult result = readSimpleResponse(telegramHttp, httpCode);
        telegramHttp.end();
        return result;
    }

    auto client = makeClient(url);
    HTTPClient http;
    beginRequest(http, *client, url, TIMEOUT_LLM_MS);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept-Encoding", "identity"); // Prevent gzip

    if (!headers.empty()) {
        http.addHeader("Authorization", headers.c_str());
    }

    int httpCode = http.POST(payload.c_str());
    HttpResult result;

    if (httpCode == 200) {
        // Read the body manually: HTTPClient::getString() truncates or drops
        // chunked transfer encoded bodies that LLM APIs commonly send.
        WiFiClient* stream = http.getStreamPtr();
        stream->setTimeout(TIMEOUT_LLM_MS);

        EspByteStream byteStream(stream, &http);
        EspSystemTime sysTime;
        std::string body;
        bool isChunked = (http.getSize() == -1);
        bool decoded = isChunked
            ? ChunkedDecoder::decode(byteStream, sysTime, TIMEOUT_LLM_MS, body)
            : ChunkedDecoder::readExact(byteStream, sysTime, TIMEOUT_LLM_MS, (size_t)http.getSize(), body);

        if (!decoded) {
            result = errorResult(200, "ManualReadFailed");
        } else if (body.empty()) {
            result = errorResult(200, "EmptyBody_ManualRead");
        } else {
            result = {true, 200, body, ""};
        }
    } else {
        result = readSimpleResponse(http, httpCode);
    }
    http.end();
    return result;
}

HttpResult EspHttpClient::patch(const std::string& url, const std::string& payload, const std::string& headers) {
    auto client = makeClient(url);
    HTTPClient http;
    beginRequest(http, *client, url, TIMEOUT_FAST_MS);
    http.addHeader("Content-Type", "application/json");

    if (!headers.empty()) {
        http.addHeader("Authorization", headers.c_str());
    }

    int httpCode = http.PATCH(payload.c_str());
    HttpResult result = readSimpleResponse(http, httpCode);
    http.end();
    return result;
}
