#include "core/store.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

#include "core/safe_file.h"

namespace golf {

namespace {

const char kCoursePath[] = "course.json";
const char kRoundPath[] = "round.json";
const char kSettingsPath[] = "settings.json";
const char kDevicePath[] = "device.json";
const char kArchiveDir[] = "rounds";
constexpr int kMaxArchiveNumber = 9999;

std::string archivePath(int number) {
    char name[32];
    std::snprintf(name, sizeof(name), "%s/%04d.json", kArchiveDir, number);
    return name;
}

// "0042.json" oder "0042.json.new" -> 42, sonst 0.
int archiveNumber(const std::string& file_name) {
    if (file_name.size() < 9) return 0;
    int number = 0;
    for (int i = 0; i < 4; i++) {
        const char c = file_name[static_cast<size_t>(i)];
        if (c < '0' || c > '9') return 0;
        number = number * 10 + (c - '0');
    }
    const std::string rest = file_name.substr(4);
    if (rest != ".json" && rest != ".json.new") return 0;
    return number;
}

} // namespace

bool Store::saveCourseJson(const std::string& json) { return saveSafely(fs_, kCoursePath, json); }
bool Store::saveCurrentRoundJson(const std::string& json) { return saveSafely(fs_, kRoundPath, json); }

bool Store::saveCourse(const Course& course) { return saveCourseJson(courseToJson(course)); }

bool Store::loadCourse(Course& course) {
    std::string json;
    return loadSafely(fs_, kCoursePath, json) && courseFromJson(json, course);
}

bool Store::saveCurrentRound(const Round& round, int hole, const DateTime& started, const char* course_name,
                             const Course* course, const char* round_id) {
    return saveCurrentRoundJson(roundToJson(round, hole, started, course_name, course, round_id));
}

bool Store::loadCurrentRound(RoundState& state) {
    std::string json;
    return loadSafely(fs_, kRoundPath, json) && roundFromJson(json, state);
}

std::vector<int> Store::archivedRounds() {
    std::vector<int> numbers;
    for (const std::string& name : fs_.list(kArchiveDir)) {
        const int n = archiveNumber(name);
        if (n > 0 && std::find(numbers.begin(), numbers.end(), n) == numbers.end()) numbers.push_back(n);
    }
    std::sort(numbers.begin(), numbers.end());
    return numbers;
}

int Store::archiveRound(const Round& round, const DateTime& started, const char* course_name, const Course* course,
                        const char* round_id) {
    const std::vector<int> existing = archivedRounds();
    const int number = existing.empty() ? 1 : existing.back() + 1;
    if (number > kMaxArchiveNumber) return 0;
    if (!saveSafely(fs_, archivePath(number), roundToJson(round, 0, started, course_name, course, round_id))) return 0;
    return number;
}

bool Store::loadArchivedRoundJson(int number, std::string& json) {
    if (number < 1 || number > kMaxArchiveNumber) return false;
    return loadSafely(fs_, archivePath(number), json);
}

bool Store::loadArchivedRound(int number, RoundState& state) {
    std::string json;
    if (!loadArchivedRoundJson(number, json) || !roundFromJson(json, state)) return false;
    // Aeltere Dateien haben keine Kennung; sie bekommen eine, die vom Inhalt
    // abhaengt und deshalb bei jedem Laden dieselbe ist.
    if (state.id[0] == '\0') idFromContent(state.id, json);
    return true;
}

int Store::findArchivedRound(const char* id) {
    if (!isValidId(id)) return 0;
    std::unique_ptr<RoundState> state(new RoundState());
    for (int number : archivedRounds()) {
        if (loadArchivedRound(number, *state) && std::strcmp(state->id, id) == 0) return number;
    }
    return 0;
}

bool Store::saveDeviceId(const char* id) {
    return isValidId(id) && saveSafely(fs_, kDevicePath, std::string("{\"v\":1,\"id\":\"") + id + "\"}");
}

bool Store::loadDeviceId(Id out) {
    std::string json;
    if (!loadSafely(fs_, kDevicePath, json)) return false;
    // Fester Aufbau: {"v":1,"id":"<16 Zeichen>"}
    const std::string prefix = "{\"v\":1,\"id\":\"";
    if (json.size() != prefix.size() + kIdLength + 2 || json.compare(0, prefix.size(), prefix) != 0) return false;
    const std::string id = json.substr(prefix.size(), kIdLength);
    if (!isValidId(id.c_str()) || json.compare(prefix.size() + kIdLength, 2, "\"}") != 0) return false;
    std::memcpy(out, id.c_str(), kIdLength + 1);
    return true;
}

bool Store::deleteArchivedRound(int number) {
    if (number < 1 || number > kMaxArchiveNumber) return false;
    const std::string path = archivePath(number);
    bool found = false;
    for (const std::string& candidate : {path + ".new", path}) {
        if (!fs_.exists(candidate)) continue;
        if (!fs_.remove(candidate)) return false;
        found = true;
    }
    return found;
}

bool Store::saveSettings(const Bag& bag, const DeviceSettings& device) {
    return saveSafely(fs_, kSettingsPath, settingsToJson(bag, device));
}

bool Store::loadSettings(Bag& bag, DeviceSettings& device) {
    std::string json;
    return loadSafely(fs_, kSettingsPath, json) && settingsFromJson(json, bag, device);
}

} // namespace golf
