#pragma once
#include "interfaces/IHttpClient.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

class EspHttpClient : public IHttpClient {
public:
    EspHttpClient();
    std::string get(const std::string& url) override;
    std::string post(const std::string& url, const std::string& payload, const std::string& headers = "") override;
    std::string patch(const std::string& url, const std::string& payload, const std::string& headers = "") override;

private:
    // Telegram is polled every 2 s; a persistent keep-alive session avoids
    // paying a full TLS handshake (~1 s + heap churn) on every poll.
    // Only ever touched from the network task — no locking needed.
    WiFiClientSecure telegramClient;
    HTTPClient telegramHttp;

    static bool isTelegramUrl(const std::string& url);
};
