#pragma once
#include <string>
#include <cstddef>

// Bounded conversation memory for the AI chat: the last few exchanges are
// replayed into every request, so follow-ups like "now 25 more" keep their
// context. Texts are truncated to keep RAM usage predictable
// (CAPACITY * 2 * MAX_TEXT_LEN ≈ 1.3 KB worst case).
class ChatMemory {
public:
    static constexpr size_t CAPACITY = 4;
    static constexpr size_t MAX_TEXT_LEN = 160;

    struct Exchange {
        std::string user;
        std::string assistant;
    };

    void addExchange(const std::string& user, const std::string& assistant);
    size_t size() const;
    const Exchange& get(size_t index) const; // 0 = oldest
    void clear();

private:
    Exchange exchanges[CAPACITY];
    size_t count = 0;
    size_t head = 0; // next write position
};
