#include <memory>
#include <string>

#include <ArduinoJson.h>
#include <unity.h>

#include "../memory_fs.h"
#include "../scenario.h"
#include "core/api.h"
#include "core/power.h"
#include "core/safe_file.h"
#include "core/serialize.h"

using namespace golf;
using testsupport::MemoryFs;

void setUp() {}
void tearDown() {}

#define ASSERT_STATE(expected, power) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>((power).state()))

static ApiResponse call(Session& session, const char* method, const std::string& body = "") {
    ApiRequest request;
    request.method = method;
    request.path = "/api/settings";
    request.body = body;
    return handleApi(session, request);
}

static JsonDocument parse(const std::string& text) {
    JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc, text) == DeserializationError::Ok);
    return doc;
}

// ---------- Datei und Format ----------

static void test_defaults() {
    const DeviceSettings d;
    TEST_ASSERT_EQUAL_INT(200, d.brightness);
    TEST_ASSERT_EQUAL_INT(20, d.dim_after_s);
    TEST_ASSERT_FALSE(d.swap_buttons);
}

static void test_settings_round_trip() {
    Bag bag;
    bag.carry_m[0] = 187;
    bag.in_bag[static_cast<int>(Club::LobWedge)] = true;
    DeviceSettings device;
    device.brightness = 120;
    device.dim_after_s = 45;
    device.swap_buttons = true;

    Bag loaded_bag;
    DeviceSettings loaded;
    const std::string json = settingsToJson(bag, device);
    TEST_ASSERT_TRUE(settingsFromJson(json, loaded_bag, loaded));
    TEST_ASSERT_EQUAL_INT(187, loaded_bag.carry_m[0]);
    TEST_ASSERT_TRUE(loaded_bag.in_bag[static_cast<int>(Club::LobWedge)]);
    TEST_ASSERT_EQUAL_INT(120, loaded.brightness);
    TEST_ASSERT_EQUAL_INT(45, loaded.dim_after_s);
    TEST_ASSERT_TRUE(loaded.swap_buttons);
    TEST_ASSERT_TRUE(json == settingsToJson(loaded_bag, loaded)); // stabil
}

static void test_partial_updates_keep_the_rest() {
    Bag bag;
    DeviceSettings device;
    TEST_ASSERT_TRUE(settingsFromJson(R"({"v":1,"brightness":90})", bag, device));
    TEST_ASSERT_EQUAL_INT(90, device.brightness);
    TEST_ASSERT_EQUAL_INT(20, device.dim_after_s);
    TEST_ASSERT_TRUE(settingsFromJson(R"({"v":1,"swap_buttons":true})", bag, device));
    TEST_ASSERT_EQUAL_INT(90, device.brightness);
    TEST_ASSERT_TRUE(device.swap_buttons);
    TEST_ASSERT_TRUE(settingsFromJson(R"({"v":1,"carry":{"Dr":199}})", bag, device));
    TEST_ASSERT_EQUAL_INT(199, bag.carry_m[0]);
    TEST_ASSERT_TRUE(device.swap_buttons); // unveraendert
}

static void test_limits() {
    Bag bag;
    DeviceSettings device;
    TEST_ASSERT_TRUE(settingsFromJson(R"({"v":1,"brightness":20,"dim_s":5})", bag, device));
    TEST_ASSERT_TRUE(settingsFromJson(R"({"v":1,"brightness":255,"dim_s":600})", bag, device));
    const char* bad[] = {
        R"({"v":1,"brightness":19})", R"({"v":1,"brightness":256})", R"({"v":1,"brightness":0})",
        R"({"v":1,"brightness":"hell"})", R"({"v":1,"brightness":120.5})",
        R"({"v":1,"dim_s":4})", R"({"v":1,"dim_s":601})", R"({"v":1,"dim_s":-20})", R"({"v":1,"dim_s":true})",
        R"({"v":1,"swap_buttons":1})", R"({"v":1,"swap_buttons":"ja"})",
        R"({"v":1})", R"({"v":2,"brightness":100})", "{}", "", "[]",
        R"({"v":1,"unbekannt":5})", // nichts Bekanntes dabei
    };
    for (const char* json : bad) {
        Bag b;
        DeviceSettings d;
        d.brightness = 77;
        TEST_ASSERT_FALSE_MESSAGE(settingsFromJson(json, b, d), json);
        TEST_ASSERT_EQUAL_INT(77, d.brightness);
    }
}

static void test_one_invalid_value_rejects_the_whole_request() {
    Bag bag;
    DeviceSettings device;
    TEST_ASSERT_FALSE(settingsFromJson(R"({"v":1,"brightness":100,"dim_s":9999,"carry":{"Dr":150}})", bag, device));
    TEST_ASSERT_EQUAL_INT(200, device.brightness);
    TEST_ASSERT_EQUAL_INT(210, bag.carry_m[0]);
    TEST_ASSERT_FALSE(settingsFromJson(R"({"v":1,"brightness":100,"clubs":["ZZ"]})", bag, device));
    TEST_ASSERT_EQUAL_INT(200, device.brightness);
}

// ---------- Schnittstelle und Speicher ----------

static void test_api_reports_and_changes_device_settings() {
    MemoryFs fs;
    std::unique_ptr<Session> session(new Session(fs));
    JsonDocument doc = parse(call(*session, "GET").body);
    TEST_ASSERT_EQUAL_INT(200, doc["brightness"].as<int>());
    TEST_ASSERT_EQUAL_INT(20, doc["dim_s"].as<int>());
    TEST_ASSERT_FALSE(doc["swap_buttons"].as<bool>());
    TEST_ASSERT_EQUAL_INT(11, static_cast<int>(doc["clubs"].size())); // das Bag steht weiterhin drin

    TEST_ASSERT_EQUAL_INT(200, call(*session, "PUT", R"({"v":1,"brightness":90,"dim_s":60,"swap_buttons":true})").status);
    TEST_ASSERT_EQUAL_INT(90, session->deviceSettings().brightness);
    TEST_ASSERT_EQUAL_INT(11, bagSize(session->app().bag())); // Bag unberuehrt

    session.reset(new Session(fs)); // Neustart
    TEST_ASSERT_EQUAL_INT(90, session->deviceSettings().brightness);
    TEST_ASSERT_EQUAL_INT(60, session->deviceSettings().dim_after_s);
    TEST_ASSERT_TRUE(session->deviceSettings().swap_buttons);
}

static void test_changing_the_bag_keeps_device_settings_and_vice_versa() {
    MemoryFs fs;
    std::unique_ptr<Session> session(new Session(fs));
    call(*session, "PUT", R"({"v":1,"brightness":90})");
    call(*session, "PUT", R"({"v":1,"clubs":["Dr","E7"]})");
    call(*session, "PUT", R"({"v":1,"dim_s":30})");
    session.reset(new Session(fs));
    TEST_ASSERT_EQUAL_INT(90, session->deviceSettings().brightness);
    TEST_ASSERT_EQUAL_INT(30, session->deviceSettings().dim_after_s);
    TEST_ASSERT_EQUAL_INT(3, bagSize(session->app().bag()));
}

static void test_invalid_or_unsaved_settings_change_nothing() {
    MemoryFs fs;
    std::unique_ptr<Session> session(new Session(fs));
    TEST_ASSERT_EQUAL_INT(400, call(*session, "PUT", R"({"v":1,"brightness":5})").status);
    TEST_ASSERT_EQUAL_INT(200, session->deviceSettings().brightness);
    fs.read_only = true;
    TEST_ASSERT_EQUAL_INT(500, call(*session, "PUT", R"({"v":1,"brightness":90})").status);
    TEST_ASSERT_EQUAL_INT(200, session->deviceSettings().brightness); // nicht halb uebernommen
}

static void test_settings_files_of_earlier_versions_are_read() {
    // Nur Schlagweiten (erste Fassung) bzw. Bag ohne Display-Angaben (zweite).
    const char* old_files[] = {
        R"({"v":1,"carry":{"Dr":180,"H3":165,"Hy":150,"E5":135,"E6":125,"E7":115,"E8":105,"E9":95,"PW":85,"SW":60}})",
        R"({"v":1,"carry":{"Dr":180},"clubs":["Dr","E7","Pt"]})",
    };
    for (const char* content : old_files) {
        MemoryFs fs;
        fs.files["settings.json"] = wrapPayload(content);
        Session session(fs);
        TEST_ASSERT_EQUAL_INT(180, session.app().bag().carry_m[0]);
        TEST_ASSERT_EQUAL_INT(200, session.deviceSettings().brightness); // Standard
        TEST_ASSERT_FALSE(session.deviceSettings().swap_buttons);
    }
}

// ---------- Wirkung ----------

static void test_mapped_button() {
    DeviceSettings normal, swapped;
    swapped.swap_buttons = true;
    TEST_ASSERT_EQUAL_INT(1, mappedButton(1, normal));
    TEST_ASSERT_EQUAL_INT(2, mappedButton(2, normal));
    TEST_ASSERT_EQUAL_INT(2, mappedButton(1, swapped));
    TEST_ASSERT_EQUAL_INT(1, mappedButton(2, swapped));
    TEST_ASSERT_EQUAL_INT(0, mappedButton(0, swapped)); // unbekannte Knoepfe bleiben, was sie sind
    TEST_ASSERT_EQUAL_INT(3, mappedButton(3, swapped));
}

static void test_screen_power_uses_brightness_and_dim_time() {
    DeviceSettings settings;
    settings.brightness = 120;
    settings.dim_after_s = 60;
    ScreenPower power;
    power.configure(settings);
    TEST_ASSERT_EQUAL_UINT8(120, power.brightness());
    uint32_t now = 0;
    for (; now < 59000; now += 100) power.tick(now, false);
    ASSERT_STATE(ScreenState::Active, power); // die 20 Sekunden gelten nicht mehr
    for (; now <= 60000; now += 100) power.tick(now, false);
    ASSERT_STATE(ScreenState::Dim, power);
    TEST_ASSERT_EQUAL_UINT8(40, power.brightness());
}

static void test_dim_is_never_brighter_than_half() {
    DeviceSettings settings;
    settings.brightness = 30;
    ScreenPower power;
    power.configure(settings);
    uint32_t now = 0;
    for (; now <= 21000; now += 100) power.tick(now, false);
    ASSERT_STATE(ScreenState::Dim, power);
    TEST_ASSERT_EQUAL_UINT8(15, power.brightness()); // halb so hell, nicht die ueblichen 40
}

static void test_screen_power_clamps_nonsense() {
    DeviceSettings settings;
    settings.brightness = 100000;
    settings.dim_after_s = -5;
    ScreenPower power;
    power.configure(settings);
    TEST_ASSERT_EQUAL_UINT8(255, power.brightness());
    uint32_t now = 0;
    for (; now <= 5000; now += 100) power.tick(now, false);
    ASSERT_STATE(ScreenState::Dim, power); // kuerzeste erlaubte Zeit
    settings.brightness = 0;
    power.configure(settings);
    power.onInput(now);
    TEST_ASSERT_EQUAL_UINT8(20, power.brightness()); // nie unsichtbar
}

static void test_configuring_does_not_wake_or_reset_the_timer() {
    DeviceSettings settings;
    ScreenPower power;
    uint32_t now = 0;
    for (; now <= 25000; now += 100) {
        power.configure(settings); // so ruft es die Hauptschleife in jedem Durchlauf auf
        power.tick(now, false);
    }
    ASSERT_STATE(ScreenState::Dim, power);
}

static void test_swapped_buttons_in_a_real_sequence() {
    // Mit getauschten Knoepfen zaehlt der zweite Knopf den Schlag und der erste schliesst das Loch ab.
    MemoryFs fs;
    Store(fs).saveCourse(demoCourse());
    {
        Session setup(fs);
        TEST_ASSERT_EQUAL_INT(200, call(setup, "PUT", R"({"v":1,"swap_buttons":true})").status);
    }
    testsupport::Scenario s(fs);
    s.placeAt(s.app().course().holes[0].tee);
    s.wait(2.0);
    s.press2(); // physischer Knopf 2 = Schlag
    TEST_ASSERT_TRUE(s.app().overlay() == Overlay::ClubPicker);
    s.press2();
    TEST_ASSERT_EQUAL_INT(1, s.app().round().shotCount(0));
    s.hold2(); // lang: Schlag loeschen
    TEST_ASSERT_EQUAL_INT(0, s.app().round().shotCount(0));
    s.press2();
    s.press2();
    s.press1(); // physischer Knopf 1 = Loch abschliessen
    TEST_ASSERT_TRUE(s.app().overlay() == Overlay::HoleDone);
    s.press1();
    TEST_ASSERT_EQUAL_INT(1, s.app().currentHole());
    s.hold1(); // lang: Menue
    TEST_ASSERT_TRUE(s.app().overlay() == Overlay::Menu);
}

static void test_dim_time_from_settings_applies_on_the_device_loop() {
    MemoryFs fs;
    {
        Session setup(fs);
        call(setup, "PUT", R"({"v":1,"dim_s":120,"brightness":150})");
    }
    testsupport::Scenario s(fs);
    s.wait(60.0);
    ASSERT_STATE(ScreenState::Active, s.power());
    TEST_ASSERT_EQUAL_UINT8(150, s.power().brightness());
    s.wait(61.0);
    ASSERT_STATE(ScreenState::Dim, s.power());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_defaults);
    RUN_TEST(test_settings_round_trip);
    RUN_TEST(test_partial_updates_keep_the_rest);
    RUN_TEST(test_limits);
    RUN_TEST(test_one_invalid_value_rejects_the_whole_request);
    RUN_TEST(test_api_reports_and_changes_device_settings);
    RUN_TEST(test_changing_the_bag_keeps_device_settings_and_vice_versa);
    RUN_TEST(test_invalid_or_unsaved_settings_change_nothing);
    RUN_TEST(test_settings_files_of_earlier_versions_are_read);
    RUN_TEST(test_mapped_button);
    RUN_TEST(test_screen_power_uses_brightness_and_dim_time);
    RUN_TEST(test_dim_is_never_brighter_than_half);
    RUN_TEST(test_screen_power_clamps_nonsense);
    RUN_TEST(test_configuring_does_not_wake_or_reset_the_timer);
    RUN_TEST(test_swapped_buttons_in_a_real_sequence);
    RUN_TEST(test_dim_time_from_settings_applies_on_the_device_loop);
    return UNITY_END();
}
