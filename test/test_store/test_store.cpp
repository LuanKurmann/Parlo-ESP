#include <cstring>

#include <unity.h>

#include "../memory_fs.h"
#include "../test_support.h"
#include "core/safe_file.h"
#include "core/store.h"

using namespace golf;
using testsupport::kTee;
using testsupport::MemoryFs;
using testsupport::offset;

void setUp() {}
void tearDown() {}

static Course sampleCourse() {
    Course c;
    std::strcpy(c.name, "Testplatz");
    c.holes[0].par = 4;
    c.holes[0].has_green = true;
    c.holes[0].green_center = offset(kTee, 350.0);
    return c;
}

static Round roundWithShots(int shots_on_hole_one) {
    Round r;
    for (int i = 0; i < shots_on_hole_one; i++) {
        r.addShot(0, offset(kTee, i * 100.0));
        r.setClub(0, i, Club::Iron7);
    }
    return r;
}

static void test_nothing_stored_yet() {
    MemoryFs fs;
    Store store(fs);
    Course c;
    RoundState s;
    TEST_ASSERT_FALSE(store.loadCourse(c));
    TEST_ASSERT_FALSE(store.loadCurrentRound(s));
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(store.archivedRounds().size()));
    TEST_ASSERT_FALSE(store.loadArchivedRound(1, s));
}

static void test_course_is_stored_and_loaded() {
    MemoryFs fs;
    Store store(fs);
    TEST_ASSERT_TRUE(store.saveCourse(sampleCourse()));
    Course loaded;
    TEST_ASSERT_TRUE(Store(fs).loadCourse(loaded));
    TEST_ASSERT_EQUAL_STRING("Testplatz", loaded.name);
    TEST_ASSERT_TRUE(loaded.holes[0].has_green);
    TEST_ASSERT_EQUAL_INT(350, greenDistances(loaded.holes[0], kTee).center);
}

static void test_current_round_is_stored_and_loaded() {
    MemoryFs fs;
    Store store(fs);
    TEST_ASSERT_TRUE(store.saveCurrentRound(roundWithShots(3), 0, DateTime(), "Testplatz"));
    RoundState loaded;
    TEST_ASSERT_TRUE(Store(fs).loadCurrentRound(loaded));
    TEST_ASSERT_EQUAL_INT(3, loaded.round.shotCount(0));
    TEST_ASSERT_EQUAL_STRING("Testplatz", loaded.course_name);
}

static void test_corrupt_files_are_not_loaded() {
    MemoryFs fs;
    Store store(fs);
    store.saveCourse(sampleCourse());
    store.saveCurrentRound(roundWithShots(2), 0, DateTime(), "");
    fs.files["course.json"][30] ^= 0x01;
    fs.files["round.json"].resize(fs.files["round.json"].size() / 2);
    Course c;
    RoundState s;
    TEST_ASSERT_FALSE(store.loadCourse(c));
    TEST_ASSERT_FALSE(store.loadCurrentRound(s));
}

static void test_valid_checksum_but_wrong_content_is_not_loaded() {
    // Datei ist unbeschaedigt, enthaelt aber keinen gueltigen Platz.
    MemoryFs fs;
    Store store(fs);
    fs.files["course.json"] = wrapPayload(R"({"v":1,"holes":[{"par":99}]})");
    fs.files["round.json"] = wrapPayload("irgendwas");
    Course c;
    RoundState s;
    TEST_ASSERT_FALSE(store.loadCourse(c));
    TEST_ASSERT_FALSE(store.loadCurrentRound(s));
}

static void test_archive_numbers_count_up() {
    MemoryFs fs;
    Store store(fs);
    TEST_ASSERT_EQUAL_INT(1, store.archiveRound(roundWithShots(1), DateTime(), "A"));
    TEST_ASSERT_EQUAL_INT(2, store.archiveRound(roundWithShots(2), DateTime(), "B"));
    TEST_ASSERT_EQUAL_INT(3, store.archiveRound(roundWithShots(3), DateTime(), "C"));
    const std::vector<int> list = store.archivedRounds();
    TEST_ASSERT_EQUAL_INT(3, static_cast<int>(list.size()));
    TEST_ASSERT_EQUAL_INT(1, list[0]);
    TEST_ASSERT_EQUAL_INT(3, list[2]);
    RoundState s;
    TEST_ASSERT_TRUE(store.loadArchivedRound(2, s));
    TEST_ASSERT_EQUAL_INT(2, s.round.shotCount(0));
    TEST_ASSERT_EQUAL_STRING("B", s.course_name);
    TEST_ASSERT_TRUE(fs.files.count("rounds/0002.json") == 1);
}

static void test_archive_never_reuses_a_number_after_deletion() {
    MemoryFs fs;
    Store store(fs);
    store.archiveRound(roundWithShots(1), DateTime(), "");
    store.archiveRound(roundWithShots(2), DateTime(), "");
    fs.files.erase("rounds/0001.json");
    TEST_ASSERT_EQUAL_INT(3, store.archiveRound(roundWithShots(3), DateTime(), ""));
    RoundState s;
    TEST_ASSERT_FALSE(store.loadArchivedRound(1, s));
    TEST_ASSERT_TRUE(store.loadArchivedRound(2, s));
    TEST_ASSERT_EQUAL_INT(2, s.round.shotCount(0));
}

static void test_archive_ignores_foreign_files() {
    MemoryFs fs;
    Store store(fs);
    fs.files["rounds/notiz.txt"] = "x";
    fs.files["rounds/12.json"] = "x";
    fs.files["rounds/abcd.json"] = "x";
    fs.files["rounds/00010.json"] = "x";
    fs.files["rounds/sub/0050.json"] = "x";
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(store.archivedRounds().size()));
    TEST_ASSERT_EQUAL_INT(1, store.archiveRound(roundWithShots(1), DateTime(), ""));
}

static void test_half_written_archive_file_blocks_its_number() {
    // Stromausfall beim Archivieren: Die Nummer wird nicht noch einmal vergeben.
    MemoryFs fs;
    Store store(fs);
    fs.power_cut_at = 1;
    TEST_ASSERT_EQUAL_INT(0, store.archiveRound(roundWithShots(1), DateTime(), ""));
    fs.reboot();
    TEST_ASSERT_EQUAL_INT(2, store.archiveRound(roundWithShots(2), DateTime(), ""));
    RoundState s;
    TEST_ASSERT_FALSE(store.loadArchivedRound(1, s));
    TEST_ASSERT_TRUE(store.loadArchivedRound(2, s));
}

static void test_archive_stops_at_9999() {
    MemoryFs fs;
    Store store(fs);
    fs.files["rounds/9999.json"] = "x";
    TEST_ASSERT_EQUAL_INT(0, store.archiveRound(roundWithShots(1), DateTime(), ""));
}

static void test_load_archived_round_with_invalid_number() {
    MemoryFs fs;
    Store store(fs);
    store.archiveRound(roundWithShots(1), DateTime(), "");
    RoundState s;
    TEST_ASSERT_FALSE(store.loadArchivedRound(0, s));
    TEST_ASSERT_FALSE(store.loadArchivedRound(-1, s));
    TEST_ASSERT_FALSE(store.loadArchivedRound(10000, s));
    TEST_ASSERT_FALSE(store.loadArchivedRound(2, s));
}

static void test_saving_fails_cleanly_on_read_only_storage() {
    MemoryFs fs;
    Store store(fs);
    fs.read_only = true;
    TEST_ASSERT_FALSE(store.saveCourse(sampleCourse()));
    TEST_ASSERT_FALSE(store.saveCurrentRound(roundWithShots(1), 0, DateTime(), ""));
    TEST_ASSERT_EQUAL_INT(0, store.archiveRound(roundWithShots(1), DateTime(), ""));
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(fs.files.size()));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_nothing_stored_yet);
    RUN_TEST(test_course_is_stored_and_loaded);
    RUN_TEST(test_current_round_is_stored_and_loaded);
    RUN_TEST(test_corrupt_files_are_not_loaded);
    RUN_TEST(test_valid_checksum_but_wrong_content_is_not_loaded);
    RUN_TEST(test_archive_numbers_count_up);
    RUN_TEST(test_archive_never_reuses_a_number_after_deletion);
    RUN_TEST(test_archive_ignores_foreign_files);
    RUN_TEST(test_half_written_archive_file_blocks_its_number);
    RUN_TEST(test_archive_stops_at_9999);
    RUN_TEST(test_load_archived_round_with_invalid_number);
    RUN_TEST(test_saving_fails_cleanly_on_read_only_storage);
    return UNITY_END();
}
