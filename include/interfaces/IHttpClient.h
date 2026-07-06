#pragma once
#include <string>

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual std::string get(const std::string& url) = 0;
    virtual std::string post(const std::string& url, const std::string& payload, const std::string& headers = "") = 0;
    virtual std::string patch(const std::string& url, const std::string& payload, const std::string& headers = "") = 0;
};
