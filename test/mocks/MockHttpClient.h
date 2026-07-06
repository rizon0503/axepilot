#pragma once
#include "interfaces/IHttpClient.h"
#include <string>
#include <vector>

class MockHttpClient : public IHttpClient {
public:
    std::string getResponse;
    std::string postResponse = "{\"ok\":true}";
    std::string patchResponse;

    std::string lastGetUrl;
    std::string lastPostUrl;
    std::string lastPostPayload;
    std::string lastPatchUrl;
    std::string lastPatchPayload;
    std::vector<std::string> postUrls; // every POST url, in order

    int getCount = 0;
    int postCount = 0;
    int patchCount = 0;

    bool postedTo(const std::string& urlPart) const {
        for (const auto& url : postUrls) {
            if (url.find(urlPart) != std::string::npos) return true;
        }
        return false;
    }

    std::string get(const std::string& url) override {
        getCount++;
        lastGetUrl = url;
        return getResponse;
    }

    std::string post(const std::string& url, const std::string& payload, const std::string& headers) override {
        postCount++;
        lastPostUrl = url;
        lastPostPayload = payload;
        postUrls.push_back(url);
        return postResponse;
    }

    std::string patch(const std::string& url, const std::string& payload, const std::string& headers) override {
        patchCount++;
        lastPatchUrl = url;
        lastPatchPayload = payload;
        return patchResponse;
    }
};
