#include "core/ChatMemory.h"

void ChatMemory::addExchange(const std::string& user, const std::string& assistant) {
    exchanges[head].user = user.substr(0, MAX_TEXT_LEN);
    exchanges[head].assistant = assistant.substr(0, MAX_TEXT_LEN);
    head = (head + 1) % CAPACITY;
    if (count < CAPACITY) {
        count++;
    }
}

size_t ChatMemory::size() const {
    return count;
}

const ChatMemory::Exchange& ChatMemory::get(size_t index) const {
    return exchanges[(head + CAPACITY - count + index) % CAPACITY];
}

void ChatMemory::clear() {
    count = 0;
    head = 0;
    for (size_t i = 0; i < CAPACITY; i++) {
        exchanges[i].user.clear();
        exchanges[i].assistant.clear();
    }
}
