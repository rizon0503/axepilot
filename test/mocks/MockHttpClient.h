#pragma once
#include "interfaces/IHttpClient.h"
#include <string>
#include <vector>

class MockHttpClient : public IHttpClient {
public:
    std::string getResponse;
    std::string postResponse = "{\"ok\":true}";
    std::string patchResponse;

    // When set, the corresponding call returns HttpResult{ok=false} instead
    // of the canned response above — simulates a real transport failure
    // (timeout, connection refused, etc.), as opposed to a successful
    // request that merely returned an empty/garbage body.
    bool getShouldFail = false;
    bool postShouldFail = false;
    bool patchShouldFail = false;

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

    // Mirrors EspHttpClient's real behavior: an empty body is always treated
    // as a failure (readSimpleResponse() never returns ok=true with an
    // empty body), not a successful-but-empty response.
    HttpResult get(const std::string& url) override {
        getCount++;
        lastGetUrl = url;
        if (getShouldFail) return {false, 0, "", "MockFailure"};
        if (getResponse.empty()) return {false, 0, "", "EmptyBody"};
        return {true, 200, getResponse, ""};
    }

    HttpResult post(const std::string& url, const std::string& payload, const std::string& headers) override {
        postCount++;
        lastPostUrl = url;
        lastPostPayload = payload;
        postUrls.push_back(url);
        postPayloads.push_back(payload);
        if (postShouldFail) return {false, 0, "", "MockFailure"};
        if (postResponse.empty()) return {false, 0, "", "EmptyBody"};
        return {true, 200, postResponse, ""};
    }

    HttpResult patch(const std::string& url, const std::string& payload, const std::string& headers) override {
        patchCount++;
        lastPatchUrl = url;
        lastPatchPayload = payload;
        if (patchShouldFail) return {false, 0, "", "MockFailure"};
        if (patchResponse.empty()) return {false, 0, "", "EmptyBody"};
        return {true, 200, patchResponse, ""};
    }
};
