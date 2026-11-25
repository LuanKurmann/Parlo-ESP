#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <ArduinoJson.h>
#include <unity.h>

#include "../memory_fs.h"
#include "core/crc32.h"
#include "core/demo_course.h"
#include "core/link.h"

using namespace golf;
using testsupport::MemoryFs;

void setUp() {}
void tearDown() {}

#define ASSERT_RESULT(expected, actual) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual))

static std::vector<std::string> fragments(const std::string& message, size_t max_size) {
    LinkFragmenter f;
    f.start(message);
    std::vector<std::string> out;
    std::string piece;
    while (f.next(max_size, piece)) out.push_back(piece);
    return out;
}

static LinkReassembler::Result feed(LinkReassembler& r, const std::string& fragment) {
    return r.add(reinterpret_cast<const uint8_t*>(fragment.data()), fragment.size());
}

// Text mit allen Bytewerten, damit nichts an "druckbaren Zeichen" haengt.
static std::string sample(size_t length) {
    std::string s;
    s.reserve(length);
    uint32_t state = 5;
    for (size_t i = 0; i < length; i++) {
        state = state * 1664525u + 1013904223u;
        s.push_back(static_cast<char>(state >> 24));
    }
    return s;
}

// ---------- Zerlegen ----------

static void test_first_fragment_layout() {
    const std::vector<std::string> f = fragments("hallo", 100);
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(f.size()));
    const uint8_t expected[] = {1, 0, 0, 5, 0, 0, 0, 0xD1, 0x31, 0x72, 0xB9, 'h', 'a', 'l', 'l', 'o'};
    TEST_ASSERT_EQUAL_INT(sizeof(expected), f[0].size());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, f[0].data(), sizeof(expected)); // CRC-32 von "hallo" ist B97231D1
}

static void test_fragments_respect_the_size_limit_and_number_up() {
    const std::string message = sample(1000);
    for (size_t max = 20; max <= 247; max += 7) {
        const std::vector<std::string> f = fragments(message, max);
        size_t payload = 0;
        for (size_t i = 0; i < f.size(); i++) {
            TEST_ASSERT_TRUE(f[i].size() <= max);
            TEST_ASSERT_EQUAL_UINT8(i == 0 ? 1 : 2, f[i][0]);
            TEST_ASSERT_EQUAL_INT(static_cast<int>(i), static_cast<uint8_t>(f[i][1]) | (static_cast<uint8_t>(f[i][2]) << 8));
            payload += f[i].size() - (i == 0 ? kLinkFirstHeader : kLinkNextHeader);
            if (i + 1 < f.size()) TEST_ASSERT_EQUAL_UINT32(max, f[i].size()); // alle bis auf das letzte sind voll
        }
        TEST_ASSERT_EQUAL_UINT32(1000, payload);
    }
}

static void test_too_small_limit_is_raised_to_the_minimum() {
    const std::vector<std::string> f = fragments(sample(100), 5);
    for (const std::string& piece : f) TEST_ASSERT_TRUE(piece.size() <= kLinkMinFragment);
    TEST_ASSERT_EQUAL_UINT32(kLinkMinFragment, f[0].size());
}

static void test_empty_message_is_one_fragment() {
    const std::vector<std::string> f = fragments("", 50);
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(f.size()));
    TEST_ASSERT_EQUAL_UINT32(kLinkFirstHeader, f[0].size());
}

static void test_fragmenter_is_idle_before_start_and_after_the_end() {
    LinkFragmenter f;
    std::string piece;
    TEST_ASSERT_FALSE(f.pending());
    TEST_ASSERT_FALSE(f.next(100, piece));
    f.start("abc");
    TEST_ASSERT_TRUE(f.pending());
    TEST_ASSERT_TRUE(f.next(100, piece));
    TEST_ASSERT_FALSE(f.pending());
    TEST_ASSERT_FALSE(f.next(100, piece));
    f.start(sample(500));
    f.next(50, piece);
    f.reset(); // mittendrin abbrechen
    TEST_ASSERT_FALSE(f.next(50, piece));
}

// ---------- Zusammensetzen ----------

static void test_round_trip_for_many_lengths_and_fragment_sizes() {
    const size_t lengths[] = {0, 1, 8, 9, 10, 16, 17, 18, 100, 255, 256, 1000, 5000};
    const size_t sizes[] = {20, 21, 23, 27, 100, 185, 247, 512};
    for (size_t length : lengths) {
        const std::string message = sample(length);
        for (size_t size : sizes) {
            LinkReassembler r;
            const std::vector<std::string> f = fragments(message, size);
            for (size_t i = 0; i < f.size(); i++) {
                const LinkReassembler::Result result = feed(r, f[i]);
                ASSERT_RESULT(i + 1 == f.size() ? LinkReassembler::Result::Complete : LinkReassembler::Result::Incomplete, result);
            }
            TEST_ASSERT_TRUE(r.message() == message);
            TEST_ASSERT_EQUAL_UINT32(0, r.errors());
        }
    }
}

static void test_largest_allowed_message() {
    const std::string message = sample(kLinkMaxMessage);
    LinkReassembler r;
    const std::vector<std::string> f = fragments(message, 247);
    for (size_t i = 0; i + 1 < f.size(); i++) feed(r, f[i]);
    ASSERT_RESULT(LinkReassembler::Result::Complete, feed(r, f.back()));
    TEST_ASSERT_TRUE(r.message() == message);
}

static void test_announced_length_over_the_limit_is_refused_at_once() {
    std::string first = fragments("x", 50)[0];
    const uint32_t too_big = kLinkMaxMessage + 1;
    std::memcpy(&first[3], &too_big, 4);
    LinkReassembler r;
    ASSERT_RESULT(LinkReassembler::Result::Error, feed(r, first));
    const uint32_t huge = 0xFFFFFFFFu; // darf keinen Speicher reservieren
    std::memcpy(&first[3], &huge, 4);
    ASSERT_RESULT(LinkReassembler::Result::Error, feed(r, first));
}

static void test_lost_duplicated_and_swapped_fragments_are_detected() {
    const std::string message = sample(300);
    const std::vector<std::string> f = fragments(message, 60);
    TEST_ASSERT_TRUE(f.size() >= 5);

    LinkReassembler lost;
    feed(lost, f[0]);
    feed(lost, f[1]);
    ASSERT_RESULT(LinkReassembler::Result::Error, feed(lost, f[3])); // Stueck 2 fehlt
    ASSERT_RESULT(LinkReassembler::Result::Error, feed(lost, f[4])); // der Rest wird verworfen

    LinkReassembler duplicated;
    feed(duplicated, f[0]);
    feed(duplicated, f[1]);
    ASSERT_RESULT(LinkReassembler::Result::Error, feed(duplicated, f[1]));

    LinkReassembler swapped;
    feed(swapped, f[0]);
    ASSERT_RESULT(LinkReassembler::Result::Error, feed(swapped, f[2]));

    LinkReassembler headless;
    ASSERT_RESULT(LinkReassembler::Result::Error, feed(headless, f[1])); // weiteres Stueck ohne Anfang
}

static void test_after_an_error_the_next_message_works() {
    const std::vector<std::string> bad = fragments(sample(200), 50);
    const std::string good = "naechste Nachricht";
    LinkReassembler r;
    feed(r, bad[0]);
    feed(r, bad[2]); // Fehler
    ASSERT_RESULT(LinkReassembler::Result::Complete, feed(r, fragments(good, 50)[0]));
    TEST_ASSERT_TRUE(r.message() == good);
    TEST_ASSERT_EQUAL_UINT32(1, r.errors());
}

static void test_new_first_fragment_restarts_in_the_middle_of_a_message() {
    const std::vector<std::string> abandoned = fragments(sample(200), 50);
    const std::string good = sample(120);
    LinkReassembler r;
    feed(r, abandoned[0]);
    feed(r, abandoned[1]);
    const std::vector<std::string> f = fragments(good, 50);
    for (size_t i = 0; i + 1 < f.size(); i++) ASSERT_RESULT(LinkReassembler::Result::Incomplete, feed(r, f[i]));
    ASSERT_RESULT(LinkReassembler::Result::Complete, feed(r, f.back()));
    TEST_ASSERT_TRUE(r.message() == good);
    TEST_ASSERT_EQUAL_UINT32(0, r.errors()); // ein Neuanfang ist kein Fehler
}

static void test_corrupted_payload_fails_the_checksum() {
    const std::string message = sample(150);
    std::vector<std::string> f = fragments(message, 60);
    f[1][10] ^= 0x01;
    LinkReassembler r;
    feed(r, f[0]);
    feed(r, f[1]);
    ASSERT_RESULT(LinkReassembler::Result::Error, feed(r, f[2]));
    TEST_ASSERT_TRUE(r.message().empty()); // nichts Halbes bleibt liegen
}

static void test_malformed_fragments() {
    LinkReassembler r;
    const uint8_t too_short[] = {1, 0};
    const uint8_t first_too_short[] = {1, 0, 0, 5, 0, 0, 0, 1, 2, 3};
    const uint8_t unknown_type[] = {9, 0, 0, 1, 2, 3};
    const uint8_t first_with_sequence[] = {1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'x'};
    ASSERT_RESULT(LinkReassembler::Result::Error, r.add(too_short, sizeof(too_short)));
    ASSERT_RESULT(LinkReassembler::Result::Error, r.add(first_too_short, sizeof(first_too_short)));
    ASSERT_RESULT(LinkReassembler::Result::Error, r.add(unknown_type, sizeof(unknown_type)));
    ASSERT_RESULT(LinkReassembler::Result::Error, r.add(first_with_sequence, sizeof(first_with_sequence)));
    ASSERT_RESULT(LinkReassembler::Result::Error, r.add(nullptr, 10));
    ASSERT_RESULT(LinkReassembler::Result::Error, r.add(too_short, 0));
}

static void test_more_data_than_announced_and_empty_follow_ups_are_errors() {
    std::vector<std::string> f = fragments(sample(100), 60);
    LinkReassembler overrun;
    feed(overrun, f[0]);
    ASSERT_RESULT(LinkReassembler::Result::Error, feed(overrun, f[1] + "zuviel"));

    LinkReassembler stalled;
    feed(stalled, f[0]);
    const uint8_t empty_next[] = {2, 1, 0};
    ASSERT_RESULT(LinkReassembler::Result::Error, stalled.add(empty_next, sizeof(empty_next)));
}

static void test_random_fragments_never_crash_or_deliver_garbage() {
    LinkReassembler r;
    uint32_t state = 4242;
    int completed = 0;
    for (int i = 0; i < 200000; i++) {
        uint8_t buffer[64];
        state = state * 1664525u + 1013904223u;
        const size_t length = (state >> 8) % sizeof(buffer);
        for (size_t k = 0; k < length; k++) {
            state = state * 1664525u + 1013904223u;
            buffer[k] = static_cast<uint8_t>(state >> 16);
        }
        if (length > 0 && (state & 3) != 0) buffer[0] = static_cast<uint8_t>(1 + (state >> 4) % 2); // oft gueltige Art
        if (length > 6 && (state & 12) != 0) { buffer[4] = 0; buffer[5] = 0; buffer[6] = 0; }      // oft kleine Laenge
        if (r.add(buffer, length) == LinkReassembler::Result::Complete) {
            completed++;
            // Was durchkommt, hat die Pruefsumme bestanden.
            TEST_ASSERT_TRUE(r.message().size() <= 255);
        }
    }
    TEST_ASSERT_TRUE(completed < 50); // praktisch nur leere Nachrichten mit zufaellig passender Pruefsumme
}

// ---------- Anfragen und Antworten als Text ----------

static void test_request_encoding() {
    ApiRequest request;
    request.method = "PUT";
    request.path = "/api/course";
    request.body = "{\"v\":1}\nzweite Zeile";
    const std::string text = encodeLinkRequest(42, request);
    TEST_ASSERT_EQUAL_STRING("42\nPUT /api/course\n{\"v\":1}\nzweite Zeile", text.c_str());
    uint32_t id = 0;
    ApiRequest decoded;
    TEST_ASSERT_TRUE(decodeLinkRequest(text, id, decoded));
    TEST_ASSERT_EQUAL_UINT32(42, id);
    TEST_ASSERT_EQUAL_STRING("PUT", decoded.method.c_str());
    TEST_ASSERT_EQUAL_STRING("/api/course", decoded.path.c_str());
    TEST_ASSERT_TRUE(decoded.body == request.body); // Zeilenumbrueche im Inhalt bleiben
}

static void test_request_without_body() {
    uint32_t id = 9;
    ApiRequest request;
    TEST_ASSERT_TRUE(decodeLinkRequest("0\nGET /api/status\n", id, request));
    TEST_ASSERT_EQUAL_UINT32(0, id);
    TEST_ASSERT_EQUAL_STRING("", request.body.c_str());
    TEST_ASSERT_TRUE(decodeLinkRequest("999999999\nDELETE /api/rounds/id/00aa11bb22cc33dd\n", id, request));
    TEST_ASSERT_EQUAL_UINT32(999999999u, id);
}

static void test_malformed_requests_are_rejected() {
    const char* bad[] = {
        "", "7", "7\n", "7\nGET /api/status", // zweite Zeile nicht abgeschlossen
        "\nGET /api/status\n", "x\nGET /api/status\n", "-1\nGET /api/status\n", "1234567890\nGET /api/status\n",
        "7\nGET\n", "7\n /api/status\n", "7\nget /api/status\n", "7\nGETTTTTTT /api/status\n",
        "7\nGET api/status\n", "7\nGET /api/sta tus\n", "7\nGET /api/status\t\n", "7\nGET \n",
        "7\r\nGET /api/status\r\n",
    };
    for (const char* text : bad) {
        uint32_t id = 0;
        ApiRequest request;
        TEST_ASSERT_FALSE_MESSAGE(decodeLinkRequest(text, id, request), text);
    }
    uint32_t id = 0;
    ApiRequest request;
    TEST_ASSERT_FALSE(decodeLinkRequest("7\nGET /" + std::string(200, 'a') + "\n", id, request)); // Pfad zu lang
}

static void test_response_encoding() {
    ApiResponse response;
    response.status = 404;
    response.body = "{\"error\":\"x\"}";
    const std::string text = encodeLinkResponse(7, response);
    TEST_ASSERT_EQUAL_STRING("7\n404\n{\"error\":\"x\"}", text.c_str());
    uint32_t id = 0;
    int status = 0;
    std::string body;
    TEST_ASSERT_TRUE(decodeLinkResponse(text, id, status, body));
    TEST_ASSERT_EQUAL_UINT32(7, id);
    TEST_ASSERT_EQUAL_INT(404, status);
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"x\"}", body.c_str());
    TEST_ASSERT_FALSE(decodeLinkResponse("7\n20\n", id, status, body));
    TEST_ASSERT_FALSE(decodeLinkResponse("7\n2000\n", id, status, body));
    TEST_ASSERT_FALSE(decodeLinkResponse("7\nok\n", id, status, body));
    TEST_ASSERT_FALSE(decodeLinkResponse("7\n200", id, status, body));
}

// ---------- Gegenstelle: ganze Anfragen ueber kleine Pakete ----------

struct Rig {
    MemoryFs fs;
    std::unique_ptr<Session> session;
    std::unique_ptr<LinkServer> server;
    uint32_t next_id = 1;
    size_t packet = 20; // kleinstes Bluetooth-Paket

    Rig() {
        Store(fs).saveCourse(demoCourse());
        session.reset(new Session(fs));
        server.reset(new LinkServer(*session));
    }

    void send(const std::string& fragment) {
        server->receive(reinterpret_cast<const uint8_t*>(fragment.data()), fragment.size());
    }

    // Holt die anstehende Antwort ab. false, wenn keine vollstaendige kommt.
    bool collect(uint32_t& id, int& status, std::string& body) {
        LinkReassembler r;
        std::string fragment;
        while (server->nextOutgoing(packet, fragment)) {
            if (feed(r, fragment) == LinkReassembler::Result::Complete) return decodeLinkResponse(r.message(), id, status, body);
        }
        return false;
    }

    // Ganze Anfrage hin, ganze Antwort zurueck.
    int call(const char* method, const char* path, const std::string& body, std::string& answer) {
        ApiRequest request;
        request.method = method;
        request.path = path;
        request.body = body;
        const uint32_t sent_id = next_id++;
        for (const std::string& f : fragments(encodeLinkRequest(sent_id, request), packet)) send(f);
        uint32_t id = 0;
        int status = 0;
        TEST_ASSERT_TRUE_MESSAGE(collect(id, status, answer), path);
        TEST_ASSERT_EQUAL_UINT32(sent_id, id);
        return status;
    }

    std::string direct(const char* method, const char* path, const std::string& body = "") {
        ApiRequest request;
        request.method = method;
        request.path = path;
        request.body = body;
        return handleApi(*session, request).body;
    }
};

static void test_status_request_over_the_smallest_packets() {
    Rig rig;
    std::string answer;
    TEST_ASSERT_EQUAL_INT(200, rig.call("GET", "/api/status", "", answer));
    TEST_ASSERT_TRUE(answer == rig.direct("GET", "/api/status")); // dieselbe Antwort wie ohne Zerlegen
    TEST_ASSERT_EQUAL_UINT32(1, rig.server->handledRequests());
}

static void test_every_packet_size_gives_the_same_answers() {
    for (size_t packet : {20u, 23u, 64u, 185u, 247u, 512u}) {
        Rig rig;
        rig.packet = packet;
        std::string answer;
        TEST_ASSERT_EQUAL_INT(200, rig.call("GET", "/api/course", "", answer));
        TEST_ASSERT_TRUE(answer == rig.direct("GET", "/api/course"));
        TEST_ASSERT_TRUE(answer.size() > 1000); // eine Antwort ueber viele Pakete
    }
}

static void test_large_upload_changes_the_device() {
    Rig rig;
    Course course = demoCourse();
    std::strcpy(course.name, "Per Funk");
    std::strcpy(course.id, "00aa11bb22cc33dd");
    std::string answer;
    TEST_ASSERT_EQUAL_INT(200, rig.call("PUT", "/api/course", courseToJson(course), answer));
    TEST_ASSERT_EQUAL_STRING("Per Funk", rig.session->app().course().name);
    TEST_ASSERT_EQUAL_INT(342, holeLengthMeters(rig.session->app().course().holes[0]));
}

static void test_errors_of_the_api_travel_back_with_their_status() {
    Rig rig;
    std::string answer;
    TEST_ASSERT_EQUAL_INT(404, rig.call("GET", "/api/nichts", "", answer));
    TEST_ASSERT_TRUE(answer.find("error") != std::string::npos);
    TEST_ASSERT_EQUAL_INT(400, rig.call("PUT", "/api/course", "kein json", answer));
    TEST_ASSERT_EQUAL_INT(405, rig.call("POST", "/api/status", "", answer));
    TEST_ASSERT_EQUAL_INT(200, rig.call("GET", "/api/status", "", answer)); // danach geht es normal weiter
}

static void test_many_requests_in_a_row() {
    Rig rig;
    rig.packet = 100;
    std::string answer;
    for (int i = 0; i < 200; i++) {
        TEST_ASSERT_EQUAL_INT(200, rig.call("GET", i % 2 ? "/api/status" : "/api/settings", "", answer));
    }
    TEST_ASSERT_EQUAL_UINT32(200, rig.server->handledRequests());
}

static void test_malformed_request_is_answered_with_400_and_id_0() {
    Rig rig;
    for (const std::string& f : fragments("das ist keine Anfrage", 20)) rig.send(f);
    uint32_t id = 99;
    int status = 0;
    std::string body;
    TEST_ASSERT_TRUE(rig.collect(id, status, body));
    TEST_ASSERT_EQUAL_UINT32(0, id);
    TEST_ASSERT_EQUAL_INT(400, status);
    TEST_ASSERT_EQUAL_UINT32(0, rig.server->handledRequests());
}

static void test_transmission_error_is_reported_once_and_server_recovers() {
    Rig rig;
    ApiRequest request;
    request.method = "PUT";
    request.path = "/api/course";
    request.body = courseToJson(demoCourse());
    const std::vector<std::string> f = fragments(encodeLinkRequest(5, request), 50);
    rig.send(f[0]);
    rig.send(f[2]); // Stueck 1 ging verloren
    for (size_t i = 3; i < f.size(); i++) rig.send(f[i]);

    uint32_t id = 99;
    int status = 0;
    std::string body;
    TEST_ASSERT_TRUE(rig.collect(id, status, body));
    TEST_ASSERT_EQUAL_UINT32(0, id);
    TEST_ASSERT_EQUAL_INT(400, status);
    TEST_ASSERT_FALSE(rig.collect(id, status, body)); // nur eine Fehlermeldung, nicht eine pro Stueck
    TEST_ASSERT_EQUAL_STRING("Beispielplatz", rig.session->app().course().name); // nichts halb uebernommen

    std::string answer;
    TEST_ASSERT_EQUAL_INT(200, rig.call("GET", "/api/status", "", answer));
}

static void test_new_request_replaces_an_unfetched_answer() {
    Rig rig;
    ApiRequest request;
    request.method = "GET";
    request.path = "/api/course";
    for (const std::string& f : fragments(encodeLinkRequest(1, request), 50)) rig.send(f);
    std::string fragment;
    TEST_ASSERT_TRUE(rig.server->nextOutgoing(50, fragment)); // nur der Anfang der Antwort wird abgeholt

    std::string answer;
    rig.next_id = 2;
    TEST_ASSERT_EQUAL_INT(200, rig.call("GET", "/api/status", "", answer)); // prueft auch die Nummer 2
    TEST_ASSERT_TRUE(answer.find("device_id") != std::string::npos);
}

static void test_disconnect_drops_half_finished_work() {
    Rig rig;
    ApiRequest request;
    request.method = "PUT";
    request.path = "/api/course";
    request.body = courseToJson(demoCourse());
    const std::vector<std::string> f = fragments(encodeLinkRequest(5, request), 50);
    rig.send(f[0]);
    rig.send(f[1]);
    rig.server->disconnect(); // Funkverbindung weg
    TEST_ASSERT_FALSE(rig.server->hasOutgoing());

    // Nach dem Wiederverbinden gehoert ein Folgestueck der alten Anfrage zu nichts mehr.
    rig.send(f[2]);
    uint32_t id = 99;
    int status = 0;
    std::string body;
    TEST_ASSERT_TRUE(rig.collect(id, status, body));
    TEST_ASSERT_EQUAL_INT(400, status);
    std::string answer;
    TEST_ASSERT_EQUAL_INT(200, rig.call("GET", "/api/status", "", answer));
}

static void test_request_marks_the_device_as_changed_only_when_handled() {
    Rig rig;
    ApiRequest request;
    request.method = "GET";
    request.path = "/api/status";
    const std::vector<std::string> f = fragments(encodeLinkRequest(1, request), 20);
    for (size_t i = 0; i + 1 < f.size(); i++) {
        TEST_ASSERT_FALSE(rig.server->receive(reinterpret_cast<const uint8_t*>(f[i].data()), f[i].size()));
    }
    TEST_ASSERT_TRUE(rig.server->receive(reinterpret_cast<const uint8_t*>(f.back().data()), f.back().size()));
}

static void test_random_bytes_never_break_the_server() {
    Rig rig;
    uint32_t state = 77;
    std::string fragment;
    for (int i = 0; i < 50000; i++) {
        uint8_t buffer[48];
        state = state * 1664525u + 1013904223u;
        const size_t length = (state >> 8) % sizeof(buffer);
        for (size_t k = 0; k < length; k++) {
            state = state * 1664525u + 1013904223u;
            buffer[k] = static_cast<uint8_t>(state >> 16);
        }
        if (length > 0) buffer[0] = static_cast<uint8_t>(1 + (state >> 4) % 2);
        rig.server->receive(buffer, length);
        while (rig.server->nextOutgoing(64, fragment)) {} // Antworten abholen
    }
    std::string answer;
    TEST_ASSERT_EQUAL_INT(200, rig.call("GET", "/api/status", "", answer));
    TEST_ASSERT_EQUAL_STRING("Beispielplatz", rig.session->app().course().name);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_first_fragment_layout);
    RUN_TEST(test_fragments_respect_the_size_limit_and_number_up);
    RUN_TEST(test_too_small_limit_is_raised_to_the_minimum);
    RUN_TEST(test_empty_message_is_one_fragment);
    RUN_TEST(test_fragmenter_is_idle_before_start_and_after_the_end);
    RUN_TEST(test_round_trip_for_many_lengths_and_fragment_sizes);
    RUN_TEST(test_largest_allowed_message);
    RUN_TEST(test_announced_length_over_the_limit_is_refused_at_once);
    RUN_TEST(test_lost_duplicated_and_swapped_fragments_are_detected);
    RUN_TEST(test_after_an_error_the_next_message_works);
    RUN_TEST(test_new_first_fragment_restarts_in_the_middle_of_a_message);
    RUN_TEST(test_corrupted_payload_fails_the_checksum);
    RUN_TEST(test_malformed_fragments);
    RUN_TEST(test_more_data_than_announced_and_empty_follow_ups_are_errors);
    RUN_TEST(test_random_fragments_never_crash_or_deliver_garbage);
    RUN_TEST(test_request_encoding);
    RUN_TEST(test_request_without_body);
    RUN_TEST(test_malformed_requests_are_rejected);
    RUN_TEST(test_response_encoding);
    RUN_TEST(test_status_request_over_the_smallest_packets);
    RUN_TEST(test_every_packet_size_gives_the_same_answers);
    RUN_TEST(test_large_upload_changes_the_device);
    RUN_TEST(test_errors_of_the_api_travel_back_with_their_status);
    RUN_TEST(test_many_requests_in_a_row);
    RUN_TEST(test_malformed_request_is_answered_with_400_and_id_0);
    RUN_TEST(test_transmission_error_is_reported_once_and_server_recovers);
    RUN_TEST(test_new_request_replaces_an_unfetched_answer);
    RUN_TEST(test_disconnect_drops_half_finished_work);
    RUN_TEST(test_request_marks_the_device_as_changed_only_when_handled);
    RUN_TEST(test_random_bytes_never_break_the_server);
    return UNITY_END();
}
