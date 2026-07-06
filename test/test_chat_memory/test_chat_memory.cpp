#include <unity.h>
#include "core/ChatMemory.h"
#include <string>

void setUp(void) {}
void tearDown(void) {}

void test_memory_stores_exchanges_oldest_first() {
    ChatMemory memory;
    memory.addExchange("question 1", "answer 1");
    memory.addExchange("question 2", "answer 2");

    TEST_ASSERT_EQUAL(2, memory.size());
    TEST_ASSERT_EQUAL_STRING("question 1", memory.get(0).user.c_str());
    TEST_ASSERT_EQUAL_STRING("answer 2", memory.get(1).assistant.c_str());
}

void test_memory_wraps_at_capacity() {
    ChatMemory memory;
    for (int i = 0; i < 6; i++) {
        memory.addExchange("q" + std::to_string(i), "a" + std::to_string(i));
    }

    TEST_ASSERT_EQUAL(ChatMemory::CAPACITY, memory.size());
    TEST_ASSERT_EQUAL_STRING("q2", memory.get(0).user.c_str()); // oldest kept
    TEST_ASSERT_EQUAL_STRING("q5", memory.get(ChatMemory::CAPACITY - 1).user.c_str());
}

void test_memory_truncates_long_texts() {
    ChatMemory memory;
    std::string longText(500, 'x');
    memory.addExchange(longText, longText);

    TEST_ASSERT_EQUAL(ChatMemory::MAX_TEXT_LEN, memory.get(0).user.length());
    TEST_ASSERT_EQUAL(ChatMemory::MAX_TEXT_LEN, memory.get(0).assistant.length());
}

void test_memory_clear() {
    ChatMemory memory;
    memory.addExchange("q", "a");
    memory.clear();

    TEST_ASSERT_EQUAL(0, memory.size());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_memory_stores_exchanges_oldest_first);
    RUN_TEST(test_memory_wraps_at_capacity);
    RUN_TEST(test_memory_truncates_long_texts);
    RUN_TEST(test_memory_clear);
    return UNITY_END();
}
