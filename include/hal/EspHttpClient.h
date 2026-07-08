#pragma once
#include "interfaces/IHttpClient.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

class EspHttpClient : public IHttpClient {
public:
    EspHttpClient();
    HttpResult get(const std::string& url) override;
    HttpResult post(const std::string& url, const std::string& payload, const std::string& headers = "") override;
    HttpResult patch(const std::string& url, const std::string& payload, const std::string& headers = "") override;

    // Root CA (PEM) for HTTPS hosts without a built-in pin (i.e. a custom
    // AI_BASE_URL — Telegram and DeepSeek are always pinned via RootCerts).
    // Without it such hosts fall back to no certificate validation (#76);
    // the PEM must outlive this object (main.cpp passes a string literal
    // from secrets.h).
    void setCustomCaCert(const char* pem);

private:
    const char* customCaCert = nullptr;

    // Telegram is polled every 2 s; a persistent keep-alive session avoids
    // paying a full TLS handshake (~1 s + heap churn) on every poll.
    // Only ever touched from the network task — no locking needed.
    WiFiClientSecure telegramClient;
    HTTPClient telegramHttp;

    static bool isTelegramUrl(const std::string& url);
};
