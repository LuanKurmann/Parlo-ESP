#include <unistd.h>
#include <cstdio>
#include <string>

#include <unity.h>

#include "../memory_fs.h"
#include "core/crc32.h"
#include "core/posix_fs.h"
#include "core/safe_file.h"

using namespace golf;
using testsupport::MemoryFs;

void setUp() {}
void tearDown() {}

static const std::string kOld = "{\"stand\":\"alt\",\"schlaege\":3}";
static const std::string kNew = "{\"stand\":\"neu\",\"schlaege\":4,\"mehr\":\"daten daten daten\"}";

static void test_crc32_known_values() {
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, crc32("123456789", 9));
    TEST_ASSERT_EQUAL_HEX32(0x00000000u, crc32("", 0));
    TEST_ASSERT_EQUAL_HEX32(0xE8B7BE43u, crc32("a", 1));
}

static void test_wrap_and_unwrap() {
    const std::string wrapped = wrapPayload("hallo");
    TEST_ASSERT_EQUAL_STRING("GOLF1 B97231D1 5\nhallo", wrapped.c_str());
    std::string out;
    TEST_ASSERT_TRUE(unwrapPayload(wrapped, out));
    TEST_ASSERT_EQUAL_STRING("hallo", out.c_str());
}

static void test_empty_payload_round_trips() {
    std::string out = "x";
    TEST_ASSERT_TRUE(unwrapPayload(wrapPayload(""), out));
    TEST_ASSERT_EQUAL_STRING("", out.c_str());
}

static void test_payload_with_newlines_and_binary_round_trips() {
    std::string payload = "zeile1\nzeile2\n";
    payload.push_back('\0');
    payload += "\xff\x01 ende";
    std::string out;
    TEST_ASSERT_TRUE(unwrapPayload(wrapPayload(payload), out));
    TEST_ASSERT_TRUE(out == payload);
}

static void test_unwrap_rejects_malformed_headers() {
    const char* bad[] = {
        "",
        "GOLF1",
        "GOLF1 ",
        "GOLF2 B97231D1 5\nhallo",   // andere Kennung
        "golf1 B97231D1 5\nhallo",
        "GOLF1 b97231d1 5\nhallo",   // Kleinbuchstaben
        "GOLF1 B97231D 5\nhallo",    // CRC zu kurz
        "GOLF1 B97231D1  5\nhallo",  // doppeltes Leerzeichen
        "GOLF1 B97231D1 05\nhallo",  // fuehrende Null
        "GOLF1 B97231D1 5 \nhallo",
        "GOLF1 B97231D1 \nhallo",    // Laenge fehlt
        "GOLF1 B97231D1 -5\nhallo",
        "GOLF1 B97231D1 5hallo",     // Zeilenende fehlt
        "GOLF1 B97231D1 4\nhallo",   // Laenge zu klein
        "GOLF1 B97231D1 6\nhallo",   // Laenge zu gross
        "GOLF1 B97231D1 5\nhallo!",  // Anhang
        "GOLF1 B97231D1 5\nhalla",   // Inhalt veraendert
        "GOLF1 B97231D2 5\nhallo",   // CRC veraendert
        "GOLF1 B97231D1 99999999999999999999\nhallo",
    };
    for (const char* content : bad) {
        std::string out = "unveraendert";
        TEST_ASSERT_FALSE_MESSAGE(unwrapPayload(content, out), content);
        TEST_ASSERT_EQUAL_STRING("unveraendert", out.c_str());
    }
}

static void test_every_single_byte_change_is_detected() {
    const std::string wrapped = wrapPayload(kNew);
    for (size_t i = 0; i < wrapped.size(); i++) {
        for (int bit = 0; bit < 8; bit++) {
            std::string damaged = wrapped;
            damaged[i] = static_cast<char>(damaged[i] ^ (1 << bit));
            std::string out;
            TEST_ASSERT_FALSE(unwrapPayload(damaged, out));
        }
    }
}

static void test_every_truncation_is_detected() {
    const std::string wrapped = wrapPayload(kNew);
    for (size_t length = 0; length < wrapped.size(); length++) {
        std::string out;
        TEST_ASSERT_FALSE(unwrapPayload(wrapped.substr(0, length), out));
    }
}

static void test_oversized_payload_is_refused() {
    MemoryFs fs;
    TEST_ASSERT_FALSE(saveSafely(fs, "big", std::string(kMaxPayloadBytes + 1, 'x')));
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(fs.files.size()));
    TEST_ASSERT_TRUE(saveSafely(fs, "big", std::string(kMaxPayloadBytes, 'x')));
    std::string out;
    TEST_ASSERT_TRUE(loadSafely(fs, "big", out));
    TEST_ASSERT_EQUAL_UINT32(kMaxPayloadBytes, out.size());
}

static void test_save_then_load() {
    MemoryFs fs;
    TEST_ASSERT_TRUE(saveSafely(fs, "round.json", kOld));
    std::string out;
    TEST_ASSERT_TRUE(loadSafely(fs, "round.json", out));
    TEST_ASSERT_TRUE(out == kOld);
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(fs.files.size())); // keine Reste
}

static void test_overwrite_leaves_only_the_new_file() {
    MemoryFs fs;
    saveSafely(fs, "round.json", kOld);
    TEST_ASSERT_TRUE(saveSafely(fs, "round.json", kNew));
    std::string out;
    loadSafely(fs, "round.json", out);
    TEST_ASSERT_TRUE(out == kNew);
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(fs.files.size()));
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(fs.files.count("round.json")));
}

static void test_load_missing_file_fails() {
    MemoryFs fs;
    std::string out = "unveraendert";
    TEST_ASSERT_FALSE(loadSafely(fs, "nichts", out));
    TEST_ASSERT_EQUAL_STRING("unveraendert", out.c_str());
}

static void test_corrupt_file_without_newer_version_is_not_loaded() {
    MemoryFs fs;
    fs.files["round.json"] = "GOLF1 kaputt";
    std::string out;
    TEST_ASSERT_FALSE(loadSafely(fs, "round.json", out));
}

static void test_complete_new_file_wins_over_main_file() {
    MemoryFs fs;
    fs.files["round.json"] = wrapPayload(kOld);
    fs.files["round.json.new"] = wrapPayload(kNew);
    std::string out;
    TEST_ASSERT_TRUE(loadSafely(fs, "round.json", out));
    TEST_ASSERT_TRUE(out == kNew);
    // Das Laden bringt das unterbrochene Speichern zu Ende.
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(fs.files.size()));
    TEST_ASSERT_TRUE(fs.files["round.json"] == wrapPayload(kNew));
}

static void test_loading_works_even_if_cleanup_is_impossible() {
    MemoryFs fs;
    fs.files["round.json"] = wrapPayload(kOld);
    fs.files["round.json.new"] = wrapPayload(kNew);
    fs.read_only = true;
    std::string out;
    TEST_ASSERT_TRUE(loadSafely(fs, "round.json", out));
    TEST_ASSERT_TRUE(out == kNew);
}

static void test_two_power_cuts_in_a_row_never_go_back_to_an_older_state() {
    // 1. Ausfall direkt nach dem vollstaendigen Schreiben von B: nach dem
    //    Neustart gilt B. 2. Ausfall mitten im Schreiben von C: Es muss bei B
    //    bleiben und darf nicht auf A zurueckfallen.
    MemoryFs fs;
    saveSafely(fs, "round.json", "A");
    fs.mutations = 0;
    fs.power_cut_at = 1;
    fs.partial_write = 1.0;
    TEST_ASSERT_FALSE(saveSafely(fs, "round.json", "B"));
    fs.reboot();
    std::string out;
    TEST_ASSERT_TRUE(loadSafely(fs, "round.json", out));
    TEST_ASSERT_EQUAL_STRING("B", out.c_str());

    fs.mutations = 0;
    fs.power_cut_at = 1;
    fs.partial_write = 0.5;
    TEST_ASSERT_FALSE(saveSafely(fs, "round.json", "C"));
    fs.reboot();
    TEST_ASSERT_TRUE(loadSafely(fs, "round.json", out));
    TEST_ASSERT_EQUAL_STRING("B", out.c_str());
}

static void test_half_written_new_file_is_ignored() {
    MemoryFs fs;
    fs.files["round.json"] = wrapPayload(kOld);
    fs.files["round.json.new"] = wrapPayload(kNew).substr(0, 20);
    std::string out;
    TEST_ASSERT_TRUE(loadSafely(fs, "round.json", out));
    TEST_ASSERT_TRUE(out == kOld);
}

static void test_save_cleans_up_leftovers_from_earlier_crash() {
    MemoryFs fs;
    fs.files["round.json"] = wrapPayload(kOld);
    fs.files["round.json.new"] = "halb";
    TEST_ASSERT_TRUE(saveSafely(fs, "round.json", kNew));
    std::string out;
    loadSafely(fs, "round.json", out);
    TEST_ASSERT_TRUE(out == kNew);
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(fs.files.size()));
}

static void test_save_reports_failure_when_storage_is_read_only() {
    MemoryFs fs;
    saveSafely(fs, "round.json", kOld);
    fs.read_only = true;
    TEST_ASSERT_FALSE(saveSafely(fs, "round.json", kNew));
    fs.read_only = false;
    std::string out;
    TEST_ASSERT_TRUE(loadSafely(fs, "round.json", out));
    TEST_ASSERT_TRUE(out == kOld);
}

// Kern des Ganzen: Stromausfall bei jeder einzelnen Operation, in jeder Variante.
static void powerCutEverywhere(bool with_old_file) {
    const double partials[] = {0.0, 0.3, 0.5, 0.99, 1.0};
    for (double partial : partials) {
        for (int completes = 0; completes < 2; completes++) {
            for (int cut = 1; cut <= 5; cut++) {
                MemoryFs fs;
                if (with_old_file) saveSafely(fs, "round.json", kOld);
                fs.mutations = 0;
                fs.power_cut_at = cut;
                fs.partial_write = partial;
                fs.cut_op_completes = completes != 0;

                const bool saved = saveSafely(fs, "round.json", kNew);
                const bool was_cut = fs.dead;
                fs.reboot();

                char msg[96];
                std::snprintf(msg, sizeof(msg), "alt=%d Ausfall bei Op %d, Rest %.2f, ausgefuehrt=%d", with_old_file, cut,
                              partial, completes);

                std::string out;
                const bool loaded = loadSafely(fs, "round.json", out);
                if (saved) TEST_ASSERT_TRUE_MESSAGE(loaded && out == kNew, msg); // Erfolg gemeldet -> neu ist da
                if (with_old_file) {
                    TEST_ASSERT_TRUE_MESSAGE(loaded, msg); // nie ohne Daten
                    TEST_ASSERT_TRUE_MESSAGE(out == kOld || out == kNew, msg);
                } else if (loaded) {
                    TEST_ASSERT_TRUE_MESSAGE(out == kNew, msg);
                }
                if (!was_cut) TEST_ASSERT_TRUE_MESSAGE(saved, msg);

                // Nach dem Neustart muss normales Speichern wieder gehen.
                const std::string third = "{\"stand\":\"nach neustart\"}";
                TEST_ASSERT_TRUE_MESSAGE(saveSafely(fs, "round.json", third), msg);
                TEST_ASSERT_TRUE_MESSAGE(loadSafely(fs, "round.json", out) && out == third, msg);
                TEST_ASSERT_EQUAL_INT_MESSAGE(1, static_cast<int>(fs.files.size()), msg);
            }
        }
    }
}

static void test_power_cut_at_every_step_with_existing_file() { powerCutEverywhere(true); }
static void test_power_cut_at_every_step_on_first_save() { powerCutEverywhere(false); }

static void test_repeated_power_cuts_never_lose_the_last_good_state() {
    // 3000 Speicherversuche, etwa jeder zweite faellt an wechselnder Stelle aus,
    // oft mehrmals hintereinander.
    MemoryFs fs;
    std::string last_confirmed;
    std::string attempted;
    uint32_t state = 7;
    for (int i = 0; i < 3000; i++) {
        state = state * 1664525u + 1013904223u;
        const std::string next = "{\"n\":" + std::to_string(i) + ",\"fuell\":\"" + std::string((state >> 8) % 200, 'x') + "\"}";
        fs.mutations = 0;
        fs.power_cut_at = ((state >> 28) & 1) ? static_cast<int>((state >> 20) % 3) + 1 : 0;
        fs.partial_write = ((state >> 12) % 4 == 0) ? 1.0 : static_cast<double>((state >> 12) % 100) / 100.0;
        fs.cut_op_completes = ((state >> 4) & 1) != 0;
        attempted = next;
        if (saveSafely(fs, "round.json", next)) last_confirmed = next;
        fs.reboot();

        std::string out;
        const bool loaded = loadSafely(fs, "round.json", out);
        if (!last_confirmed.empty()) {
            TEST_ASSERT_TRUE(loaded);
            TEST_ASSERT_TRUE(out == last_confirmed || out == attempted);
            last_confirmed = out; // was nach dem Neustart da ist, gilt
        }
    }
}

// ---------- Echtes Dateisystem (Ordner auf dem PC) ----------

static std::string tempRoot() {
    static int counter = 0;
    return "/tmp/golf_fs_test_" + std::to_string(static_cast<long>(getpid())) + "_" + std::to_string(counter++);
}

static void test_posix_fs_basic_operations() {
    PosixFileSystem fs(tempRoot());
    std::string out;
    TEST_ASSERT_FALSE(fs.exists("a.txt"));
    TEST_ASSERT_FALSE(fs.read("a.txt", out));
    TEST_ASSERT_TRUE(fs.write("a.txt", "eins"));
    TEST_ASSERT_TRUE(fs.exists("a.txt"));
    TEST_ASSERT_TRUE(fs.read("a.txt", out));
    TEST_ASSERT_EQUAL_STRING("eins", out.c_str());
    TEST_ASSERT_TRUE(fs.write("a.txt", "zwei")); // ersetzt, haengt nicht an
    fs.read("a.txt", out);
    TEST_ASSERT_EQUAL_STRING("zwei", out.c_str());

    TEST_ASSERT_TRUE(fs.write("b.txt", "b"));
    TEST_ASSERT_FALSE(fs.rename("a.txt", "b.txt")); // Ziel existiert
    TEST_ASSERT_TRUE(fs.rename("a.txt", "c.txt"));
    TEST_ASSERT_FALSE(fs.exists("a.txt"));
    TEST_ASSERT_FALSE(fs.rename("fehlt", "d.txt"));
    TEST_ASSERT_TRUE(fs.remove("c.txt"));
    TEST_ASSERT_FALSE(fs.remove("c.txt"));
    fs.remove("b.txt");
}

static void test_posix_fs_creates_folders_and_lists_files() {
    PosixFileSystem fs(tempRoot());
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(fs.list("rounds").size()));
    TEST_ASSERT_TRUE(fs.write("rounds/0001.json", "a"));
    TEST_ASSERT_TRUE(fs.write("rounds/0002.json", "b"));
    TEST_ASSERT_TRUE(fs.write("course.json", "c"));
    TEST_ASSERT_EQUAL_INT(2, static_cast<int>(fs.list("rounds").size()));
    TEST_ASSERT_FALSE(fs.exists("rounds")); // ein Ordner ist keine Datei
}

static void test_posix_fs_keeps_binary_content() {
    PosixFileSystem fs(tempRoot());
    std::string data;
    for (int i = 0; i < 256; i++) data.push_back(static_cast<char>(i));
    TEST_ASSERT_TRUE(fs.write("bin", data));
    std::string out;
    TEST_ASSERT_TRUE(fs.read("bin", out));
    TEST_ASSERT_TRUE(out == data);
}

static void test_safe_file_on_posix_fs() {
    PosixFileSystem fs(tempRoot());
    TEST_ASSERT_TRUE(saveSafely(fs, "round.json", kOld));
    TEST_ASSERT_TRUE(saveSafely(fs, "round.json", kNew));
    std::string out;
    TEST_ASSERT_TRUE(loadSafely(fs, "round.json", out));
    TEST_ASSERT_TRUE(out == kNew);
    TEST_ASSERT_FALSE(fs.exists("round.json.new"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_crc32_known_values);
    RUN_TEST(test_wrap_and_unwrap);
    RUN_TEST(test_empty_payload_round_trips);
    RUN_TEST(test_payload_with_newlines_and_binary_round_trips);
    RUN_TEST(test_unwrap_rejects_malformed_headers);
    RUN_TEST(test_every_single_byte_change_is_detected);
    RUN_TEST(test_every_truncation_is_detected);
    RUN_TEST(test_oversized_payload_is_refused);
    RUN_TEST(test_save_then_load);
    RUN_TEST(test_overwrite_leaves_only_the_new_file);
    RUN_TEST(test_load_missing_file_fails);
    RUN_TEST(test_corrupt_file_without_newer_version_is_not_loaded);
    RUN_TEST(test_complete_new_file_wins_over_main_file);
    RUN_TEST(test_loading_works_even_if_cleanup_is_impossible);
    RUN_TEST(test_two_power_cuts_in_a_row_never_go_back_to_an_older_state);
    RUN_TEST(test_half_written_new_file_is_ignored);
    RUN_TEST(test_save_cleans_up_leftovers_from_earlier_crash);
    RUN_TEST(test_save_reports_failure_when_storage_is_read_only);
    RUN_TEST(test_power_cut_at_every_step_with_existing_file);
    RUN_TEST(test_power_cut_at_every_step_on_first_save);
    RUN_TEST(test_repeated_power_cuts_never_lose_the_last_good_state);
    RUN_TEST(test_posix_fs_basic_operations);
    RUN_TEST(test_posix_fs_creates_folders_and_lists_files);
    RUN_TEST(test_posix_fs_keeps_binary_content);
    RUN_TEST(test_safe_file_on_posix_fs);
    return UNITY_END();
}
