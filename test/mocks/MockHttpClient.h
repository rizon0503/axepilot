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
    std::vector<std::string> postPayloads; // every POST payload, same order as postUrls

    int getCount = 0;
    int postCount = 0;
    int patchCount = 0;

    bool postedTo(const std::string& urlPart) const {
        for (const auto& url : postUrls) {
            if (url.find(urlPart) != std::string::npos) return true;
        }
        return false;
    }

    // The most recent POST payload sent to a URL containing urlPart — useful
    // when a single call under test issues more than one POST (e.g. an AI
    // request followed by relaying the reply via Telegram) and lastPostPayload
    // alone would only reflect whichever one happened last.
    std::string lastPostPayloadTo(const std::string& urlPart) const {
        for (size_t i = postUrls.size(); i-- > 0;) {
            if (postUrls[i].find(urlPart) != std::string::npos) return postPayloads[i];
        }
        return "";
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
        postPayloads.push_back(payload);
        return postResponse;
    }

    std::string patch(const std::string& url, const std::string& payload, const std::string& headers) override {
        patchCount++;
        lastPatchUrl = url;
        lastPatchPayload = payload;
        return patchResponse;
    }
};
