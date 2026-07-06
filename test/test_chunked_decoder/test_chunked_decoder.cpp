#include <unity.h>
#include "core/ChunkedDecoder.h"
#include "../mocks/MockByteStream.h"
#include "../mocks/MockSystemTime.h"

void setUp(void) {}
void tearDown(void) {}

static constexpr uint32_t TIMEOUT_MS = 5000;

void test_decodes_single_chunk() {
    MockByteStream stream;
    stream.data = "5\r\nhello\r\n0\r\n\r\n";
    MockSystemTime time;
    std::string out;

    bool ok = ChunkedDecoder::decode(stream, time, TIMEOUT_MS, out);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello", out.c_str());
}

void test_decodes_multiple_chunks() {
    MockByteStream stream;
    stream.data = "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n";
    MockSystemTime time;
    std::string out;

    bool ok = ChunkedDecoder::decode(stream, time, TIMEOUT_MS, out);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello world", out.c_str());
}

void test_decodes_chunk_split_byte_by_byte() {
    MockByteStream stream;
    stream.data = "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n";
    stream.maxAvailablePerCall = 1; // worst case: one byte per poll
    MockSystemTime time;
    std::string out;

    bool ok = ChunkedDecoder::decode(stream, time, TIMEOUT_MS, out);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello world", out.c_str());
}

void test_zero_length_chunk_terminates_immediately() {
    MockByteStream stream;
    stream.data = "0\r\n\r\n";
    MockSystemTime time;
    std::string out;

    bool ok = ChunkedDecoder::decode(stream, time, TIMEOUT_MS, out);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(0, out.length());
}

void test_dropped_connection_mid_chunk_fails() {
    MockByteStream stream;
    // Declares a 16-byte (0x10) chunk but the server vanishes after 5 bytes
    stream.data = "10\r\nhello";
    stream.disconnectAtPos = stream.data.size();
    MockSystemTime time;
    std::string out;

    bool ok = ChunkedDecoder::decode(stream, time, TIMEOUT_MS, out);

    TEST_ASSERT_FALSE(ok);
}

void test_garbage_hex_length_fails_without_hanging() {
    MockByteStream stream;
    stream.data = "ZZZ\r\nhello\r\n0\r\n\r\n";
    MockSystemTime time;
    std::string out;

    bool ok = ChunkedDecoder::decode(stream, time, TIMEOUT_MS, out);

    TEST_ASSERT_FALSE(ok);
}

void test_disconnected_before_any_data_fails() {
    MockByteStream stream;
    stream.data = "";
    stream.isConnected = false;
    MockSystemTime time;
    std::string out;

    bool ok = ChunkedDecoder::decode(stream, time, TIMEOUT_MS, out);

    TEST_ASSERT_FALSE(ok);
}

void test_times_out_waiting_for_data() {
    MockByteStream stream;
    stream.data = ""; // never arrives, but connection stays "open"
    stream.isConnected = true;
    MockSystemTime time;
    std::string out;

    bool ok = ChunkedDecoder::decode(stream, time, TIMEOUT_MS, out);

    TEST_ASSERT_FALSE(ok);
    // The decoder must have advanced the clock while waiting, not hung
    TEST_ASSERT_TRUE(time.currentTime >= TIMEOUT_MS);
}

void test_read_exact_reads_full_length() {
    MockByteStream stream;
    stream.data = "exactly11bytes";
    MockSystemTime time;
    std::string out;

    bool ok = ChunkedDecoder::readExact(stream, time, TIMEOUT_MS, 8, out);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("exactly1", out.c_str());
}

void test_read_exact_across_multiple_reads() {
    MockByteStream stream;
    stream.data = "abcdefghij";
    stream.maxAvailablePerCall = 3;
    MockSystemTime time;
    std::string out;

    bool ok = ChunkedDecoder::readExact(stream, time, TIMEOUT_MS, 10, out);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("abcdefghij", out.c_str());
}

void test_read_exact_dropped_connection_before_complete() {
    MockByteStream stream;
    stream.data = "short";
    stream.disconnectAtPos = stream.data.size();
    MockSystemTime time;
    std::string out;

    bool ok = ChunkedDecoder::readExact(stream, time, TIMEOUT_MS, 50, out);

    TEST_ASSERT_FALSE(ok);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_decodes_single_chunk);
    RUN_TEST(test_decodes_multiple_chunks);
    RUN_TEST(test_decodes_chunk_split_byte_by_byte);
    RUN_TEST(test_zero_length_chunk_terminates_immediately);
    RUN_TEST(test_dropped_connection_mid_chunk_fails);
    RUN_TEST(test_garbage_hex_length_fails_without_hanging);
    RUN_TEST(test_disconnected_before_any_data_fails);
    RUN_TEST(test_times_out_waiting_for_data);
    RUN_TEST(test_read_exact_reads_full_length);
    RUN_TEST(test_read_exact_across_multiple_reads);
    RUN_TEST(test_read_exact_dropped_connection_before_complete);
    return UNITY_END();
}
