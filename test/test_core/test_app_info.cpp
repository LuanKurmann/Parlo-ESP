#include <cstring>

#include <unity.h>

#include "core/app_info.h"

void setUp() {}
void tearDown() {}

static void test_version_has_three_parts() {
    const char* v = golf::version();
    TEST_ASSERT_NOT_NULL(v);
    int dots = 0;
    for (const char* p = v; *p; p++) {
        if (*p == '.') dots++;
    }
    TEST_ASSERT_EQUAL_INT(2, dots);
    TEST_ASSERT_TRUE(std::strlen(v) >= 5);
}

static void test_display_is_466_pixels() {
    TEST_ASSERT_EQUAL_INT(466, golf::kDisplaySize);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_version_has_three_parts);
    RUN_TEST(test_display_is_466_pixels);
    return UNITY_END();
}
