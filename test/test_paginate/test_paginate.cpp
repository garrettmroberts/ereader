#include "paginate.h"

#include <unity.h>

#include <cstring>
#include <string>

#include "../../src/paginate.cpp"

namespace {

std::string slice(const char* text, TextLine line) {
    return std::string(text + line.begin, text + line.end);
}

std::string linesOf(const char* text, int columns, int rows) {
    const PageLayout page = layoutPage(text, strlen(text), 0, columns, rows);
    std::string joined;
    for (const TextLine& line : page.lines) {
        if (!joined.empty()) {
            joined.push_back('|');
        }
        joined += slice(text, line);
    }
    return joined;
}

}  // namespace

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

extern "C" void test_empty_and_rejected_input(void) {
    TEST_ASSERT_EQUAL_INT(0, pageOffsets(nullptr, 0, 10, 4).size());
    TEST_ASSERT_EQUAL_INT(0, pageOffsets("", 0, 10, 4).size());

    const PageLayout missing = layoutPage(nullptr, 4, 3, 10, 4);
    TEST_ASSERT_TRUE(missing.lines.empty());
    TEST_ASSERT_EQUAL_INT(3, missing.next);

    const char* text = "hello";
    const PageLayout past = layoutPage(text, strlen(text), 99, 10, 4);
    TEST_ASSERT_TRUE(past.lines.empty());
    TEST_ASSERT_EQUAL_INT(99, past.next);

    const auto broken = pageOffsets(text, strlen(text), 0, 4);
    TEST_ASSERT_EQUAL_INT(1, broken.size());
    TEST_ASSERT_EQUAL_INT(0, broken[0]);
    TEST_ASSERT_EQUAL_INT(0, layoutPage(text, strlen(text), 0, 0, 4).next);
    TEST_ASSERT_EQUAL_INT(0, layoutPage(text, strlen(text), 0, 10, 0).next);
}

extern "C" void test_wraps_on_word_boundaries(void) {
    const char* text = "hello world";
    TEST_ASSERT_EQUAL_STRING("hello|world", linesOf(text, 5, 2).c_str());
    TEST_ASSERT_EQUAL_STRING("hello", linesOf(text, 5, 1).c_str());

    const auto pages = pageOffsets(text, strlen(text), 5, 1);
    TEST_ASSERT_EQUAL_INT(2, pages.size());
    TEST_ASSERT_EQUAL_INT(0, pages[0]);
    TEST_ASSERT_EQUAL_INT(6, pages[1]);

    const PageLayout second = layoutPage(text, strlen(text), pages[1], 5, 1);
    TEST_ASSERT_EQUAL_STRING("world", slice(text, second.lines[0]).c_str());
    TEST_ASSERT_EQUAL_INT(strlen(text), second.next);
}

extern "C" void test_keeps_spaces_that_fit_and_drops_edge_spaces(void) {
    TEST_ASSERT_EQUAL_STRING("a  b", linesOf("a  b", 10, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("hi", linesOf("  hi", 10, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("hi", linesOf("hi  ", 10, 1).c_str());

    const auto blank = pageOffsets("   ", 3, 10, 2);
    TEST_ASSERT_EQUAL_INT(1, blank.size());
    TEST_ASSERT_EQUAL_INT(3, layoutPage("   ", 3, 0, 10, 2).next);
}

extern "C" void test_newlines_and_carriage_returns(void) {
    TEST_ASSERT_EQUAL_STRING("ab||cd", linesOf("ab\n\ncd", 10, 4).c_str());

    const char* crlf = "a\r\nb";
    TEST_ASSERT_EQUAL_STRING("a|b", linesOf(crlf, 10, 4).c_str());

    const PageLayout embedded = layoutPage("a\rb", 3, 0, 10, 1);
    TEST_ASSERT_EQUAL_INT(1, embedded.lines.size());
    TEST_ASSERT_EQUAL_STRING("a\rb", slice("a\rb", embedded.lines[0]).c_str());

    const char* text = "ab\n\ncd";
    const PageLayout first = layoutPage(text, strlen(text), 0, 10, 1);
    TEST_ASSERT_EQUAL_STRING("ab", slice(text, first.lines[0]).c_str());
    TEST_ASSERT_EQUAL_INT(3, first.next);
    TEST_ASSERT_EQUAL_STRING("", slice(text, layoutPage(text, strlen(text), first.next, 10, 1).lines[0]).c_str());
}

extern "C" void test_splits_words_longer_than_the_line(void) {
    const char* text = "abcdefghij";
    const auto pages = pageOffsets(text, strlen(text), 4, 1);
    TEST_ASSERT_EQUAL_INT(3, pages.size());
    TEST_ASSERT_EQUAL_INT(0, pages[0]);
    TEST_ASSERT_EQUAL_INT(4, pages[1]);
    TEST_ASSERT_EQUAL_INT(8, pages[2]);
    TEST_ASSERT_EQUAL_STRING("abcd", linesOf(text, 4, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("ij", slice(text, layoutPage(text, strlen(text), 8, 4, 1).lines[0]).c_str());

    TEST_ASSERT_EQUAL_STRING("ab|cdef|ghij", linesOf("ab cdefghij", 4, 4).c_str());
}

extern "C" void test_page_offsets_cover_the_text(void) {
    const char* text = "one two three\n\nfour five six seven";
    const int columns = 8;
    const int rows = 2;
    const auto pages = pageOffsets(text, strlen(text), columns, rows);
    TEST_ASSERT_TRUE(pages.size() >= 2);
    TEST_ASSERT_EQUAL_INT(0, pages[0]);

    size_t cursor = 0;
    for (size_t i = 0; i < pages.size(); i++) {
        TEST_ASSERT_EQUAL_INT(cursor, pages[i]);
        const size_t next = layoutPage(text, strlen(text), cursor, columns, rows).next;
        TEST_ASSERT_TRUE(next > cursor);
        cursor = next;
    }
    TEST_ASSERT_EQUAL_INT(strlen(text), cursor);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_and_rejected_input);
    RUN_TEST(test_wraps_on_word_boundaries);
    RUN_TEST(test_keeps_spaces_that_fit_and_drops_edge_spaces);
    RUN_TEST(test_newlines_and_carriage_returns);
    RUN_TEST(test_splits_words_longer_than_the_line);
    RUN_TEST(test_page_offsets_cover_the_text);
    return UNITY_END();
}
