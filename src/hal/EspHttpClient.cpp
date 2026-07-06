#include "hal/EspHttpClient.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <memory>

namespace {

// LLM APIs (DeepSeek) can take a long time to answer; local Bitaxe and
// Telegram getUpdates?timeout=0 respond quickly, so a shorter timeout keeps
// the main loop responsive when the network misbehaves.
constexpr uint16_t TIMEOUT_FAST_MS = 10000;
constexpr uint16_t TIMEOUT_LLM_MS = 45000;

std::unique_ptr<WiFiClient> makeClient(const std::string& url) {
    if (url.rfind("https://", 0) == 0) {
        auto secureClient = std::unique_ptr<WiFiClientSecure>(new WiFiClientSecure());
        secureClient->setInsecure();
        return secureClient;
    }
    return std::unique_ptr<WiFiClient>(new WiFiClient());
}

void beginRequest(HTTPClient& http, WiFiClient& client, const std::string& url, uint16_t timeoutMs) {
    http.begin(client, url.c_str());
    http.setReuse(false);
    http.setTimeout(timeoutMs);
}

std::string errorJson(int httpCode, const char* msg) {
    return "{\"error_http\": " + std::to_string(httpCode) + ", \"error_msg\": \"" + msg + "\"}";
}

std::string readSimpleResponse(HTTPClient& http, int httpCode) {
    if (httpCode <= 0) {
        return errorJson(httpCode, "HttpFailed");
    }
    String res = http.getString();
    if (res.length() == 0) {
        return errorJson(httpCode, "EmptyBody");
    }
    return res.c_str();
}

} // namespace

EspHttpClient::EspHttpClient() {
    telegramClient.setInsecure();
}

bool EspHttpClient::isTelegramUrl(const std::string& url) {
    return url.rfind("https://api.telegram.org", 0) == 0;
}

std::string EspHttpClient::get(const std::string& url) {
    if (isTelegramUrl(url)) {
        telegramHttp.begin(telegramClient, url.c_str());
        telegramHttp.setReuse(true); // keep the TLS session alive between polls
        telegramHttp.setTimeout(TIMEOUT_FAST_MS);
        int httpCode = telegramHttp.GET();
        std::string payload = readSimpleResponse(telegramHttp, httpCode);
        telegramHttp.end(); // with reuse enabled the socket stays connected
        return payload;
    }

    auto client = makeClient(url);
    HTTPClient http;
    beginRequest(http, *client, url, TIMEOUT_FAST_MS);

    int httpCode = http.GET();
    std::string payload = readSimpleResponse(http, httpCode);
    http.end();
    return payload;
}

std::string EspHttpClient::post(const std::string& url, const std::string& payload, const std::string& headers) {
    if (isTelegramUrl(url)) {
        telegramHttp.begin(telegramClient, url.c_str());
        telegramHttp.setReuse(true);
        telegramHttp.setTimeout(TIMEOUT_FAST_MS);
        telegramHttp.addHeader("Content-Type", "application/json");
        int httpCode = telegramHttp.POST(payload.c_str());
        std::string response = readSimpleResponse(telegramHttp, httpCode);
        telegramHttp.end();
        return response;
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
    std::string response = "";

    if (httpCode == 200) {
        // Read the body manually: HTTPClient::getString() truncates or drops
        // chunked transfer encoded bodies that LLM APIs commonly send.
        String res = "";
        WiFiClient* stream = http.getStreamPtr();
        stream->setTimeout(TIMEOUT_LLM_MS);

        bool isChunked = (http.getSize() == -1);

        if (isChunked) {
            while (http.connected()) {
                String hexLenStr = stream->readStringUntil('\n');
                hexLenStr.trim();
                if (hexLenStr.length() == 0) continue;

                long chunkLen = strtol(hexLenStr.c_str(), NULL, 16);
                if (chunkLen == 0) break;

                size_t readSoFar = 0;
                unsigned long start = millis();
                while (readSoFar < chunkLen && http.connected()) {
                    if (stream->available()) {
                        res += (char)stream->read();
                        readSoFar++;
                        start = millis();
                    } else {
                        delay(5);
                        if (millis() - start > TIMEOUT_LLM_MS) break;
                    }
                }
                stream->readStringUntil('\n'); // trailing \r\n
            }
        } else {
            int len = http.getSize();
            unsigned long start = millis();
            while (http.connected() && len > 0) {
                if (stream->available()) {
                    res += (char)stream->read();
                    len--;
                    start = millis();
                } else {
                    delay(5);
                    if (millis() - start > TIMEOUT_LLM_MS) break;
                }
            }
        }

        if (res.length() == 0) {
            response = errorJson(200, "EmptyBody_ManualRead");
        } else {
            response = res.c_str();
        }
    } else {
        response = readSimpleResponse(http, httpCode);
    }
    http.end();
    return response;
}

std::string EspHttpClient::patch(const std::string& url, const std::string& payload, const std::string& headers) {
    auto client = makeClient(url);
    HTTPClient http;
    beginRequest(http, *client, url, TIMEOUT_FAST_MS);
    http.addHeader("Content-Type", "application/json");

    if (!headers.empty()) {
        http.addHeader("Authorization", headers.c_str());
    }

    int httpCode = http.PATCH(payload.c_str());
    std::string response = readSimpleResponse(http, httpCode);
    http.end();
    return response;
}
