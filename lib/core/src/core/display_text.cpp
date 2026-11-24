#include "core/display_text.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace golf {

namespace {
constexpr int kMaxShownDistance = 999;

ScoreTone toneFor(int diff) { return diff < 0 ? ScoreTone::Under : (diff > 0 ? ScoreTone::Over : ScoreTone::Par); }

// "+4", "E" oder "-2".
void formatToPar(char* out, size_t size, int diff) {
    if (diff == 0) {
        std::snprintf(out, size, "E");
    } else {
        std::snprintf(out, size, "%+d", diff);
    }
}

long shownMeters(double meters) {
    const long rounded = std::lround(meters);
    return rounded > kMaxShownDistance ? kMaxShownDistance : rounded;
}

void formatDistance(char* out, size_t size, bool available, int meters) {
    if (!available) {
        std::snprintf(out, size, "---");
    } else {
        std::snprintf(out, size, "%d", meters > kMaxShownDistance ? kMaxShownDistance : meters);
    }
}
} // namespace

DistanceScreenText distanceScreenText(const App& app) {
    DistanceScreenText t;
    std::memset(&t, 0, sizeof(t));

    const int hole_index = app.currentHole();
    const Hole& hole = app.course().holes[hole_index];
    std::snprintf(t.title, sizeof(t.title), "LOCH %d", hole_index + 1);

    const int length = holeLengthMeters(hole);
    if (hole.par > 0 && length > 0) {
        std::snprintf(t.subtitle, sizeof(t.subtitle), "Par %d · %d m", hole.par, length);
    } else if (hole.par > 0) {
        std::snprintf(t.subtitle, sizeof(t.subtitle), "Par %d", hole.par);
    } else if (length > 0) {
        std::snprintf(t.subtitle, sizeof(t.subtitle), "%d m", length);
    }

    t.gps_ok = app.hasPosition();
    const GreenDistances d = app.distances();
    formatDistance(t.back, sizeof(t.back), d.available, d.back);
    formatDistance(t.center, sizeof(t.center), d.available, d.center);
    formatDistance(t.front, sizeof(t.front), d.available, d.front);
    if (!t.gps_ok) {
        std::snprintf(t.hint, sizeof(t.hint), "Kein GPS-Empfang");
    } else if (!hole.has_green) {
        std::snprintf(t.hint, sizeof(t.hint), "Grün nicht markiert");
    }

    const int shots = app.round().shotCount(hole_index);
    if (shots == 0) {
        std::snprintf(t.foot, sizeof(t.foot), "Noch kein Schlag");
    } else {
        const char* word = shots == 1 ? "Schlag" : "Schläge";
        const ShotDistance last = app.lastShotDistance();
        if (last.available) {
            std::snprintf(t.foot, sizeof(t.foot), "%d %s · letzter %ld m", shots, word, shownMeters(last.meters));
        } else {
            std::snprintf(t.foot, sizeof(t.foot), "%d %s", shots, word);
        }
    }
    return t;
}

ShotsScreenText shotsScreenText(const App& app) {
    ShotsScreenText t;
    std::memset(&t, 0, sizeof(t));
    const int hole = app.currentHole();
    std::snprintf(t.title, sizeof(t.title), "LOCH %d", hole + 1);
    t.count = app.round().shotCount(hole);

    for (int i = 0; i < t.count; i++) {
        const Shot* shot = app.round().shot(hole, i);
        ShotRowText& row = t.rows[i];
        std::snprintf(row.number, sizeof(row.number), "%d", i + 1);
        std::snprintf(row.club, sizeof(row.club), "%s", shot->club == Club::None ? "–" : clubName(shot->club));

        if (shot->club == Club::Putter) {
            std::snprintf(row.distance, sizeof(row.distance), "Putt");
            continue;
        }
        // Der letzte Schlag des laufenden Lochs misst live bis zum Spieler.
        const ShotDistance d = (i == t.count - 1) ? app.lastShotDistance()
                                                  : shotDistance(app.round(), app.course(), hole, i, nullptr);
        if (d.available) std::snprintf(row.distance, sizeof(row.distance), "%ld m", shownMeters(d.meters));
    }
    return t;
}

ScorecardText scorecardText(const App& app) {
    ScorecardText t;
    std::memset(&t, 0, sizeof(t));
    const Round& round = app.round();
    t.hole_count = round.holeCount();

    for (int h = 0; h < t.hole_count; h++) {
        ScoreCellText& cell = t.cells[h];
        std::snprintf(cell.hole, sizeof(cell.hole), "%d", h + 1);
        cell.current = h == app.currentHole();
        const int strokes = round.shotCount(h);
        if (strokes == 0) {
            std::snprintf(cell.score, sizeof(cell.score), "–");
            cell.tone = ScoreTone::None;
            continue;
        }
        std::snprintf(cell.score, sizeof(cell.score), "%d", strokes);
        const int par = app.course().holes[h].par;
        if (!round.isFinished(h)) {
            cell.tone = ScoreTone::None; // laeuft noch
        } else {
            cell.tone = par > 0 ? toneFor(strokes - par) : ScoreTone::Par;
        }
    }

    const RoundTotals totals = roundTotals(round, app.course());
    if (totals.holes_played == 0) {
        std::snprintf(t.subtitle, sizeof(t.subtitle), "Noch kein Loch gespielt");
    } else {
        std::snprintf(t.subtitle, sizeof(t.subtitle), "%d %s gespielt", totals.holes_played,
                      totals.holes_played == 1 ? "Loch" : "Löcher");
    }
    std::snprintf(t.strokes, sizeof(t.strokes), "%d %s", totals.strokes, totals.strokes == 1 ? "Schlag" : "Schläge");
    std::snprintf(t.putts, sizeof(t.putts), "%d %s", totals.putts, totals.putts == 1 ? "Putt" : "Putts");
    if (totals.holes_with_par > 0) {
        formatToPar(t.to_par, sizeof(t.to_par), totals.to_par);
        t.to_par_tone = toneFor(totals.to_par);
    } else {
        std::snprintf(t.to_par, sizeof(t.to_par), "–");
        t.to_par_tone = ScoreTone::None;
    }
    return t;
}

ClubPickerText clubPickerText(const App& app) {
    ClubPickerText t;
    std::memset(&t, 0, sizeof(t));
    const GreenDistances d = app.distances();
    if (d.available) {
        std::snprintf(t.subtitle, sizeof(t.subtitle), "Noch %d m bis Grünmitte",
                      d.center > kMaxShownDistance ? kMaxShownDistance : d.center);
    }
    std::snprintf(t.hint, sizeof(t.hint), "%s vorgeschlagen", clubName(app.suggestedClub()));
    t.suggested = static_cast<int>(app.suggestedClub());
    return t;
}

const char* menuItemText(MenuItem item) {
    switch (item) {
        case MenuItem::MarkGreen: return "Grün hier markieren";
        case MenuItem::PreviousHole: return "Vorheriges Loch";
        case MenuItem::EndRound: return "Runde beenden";
        case MenuItem::Phone: return "Mit Handy verbinden";
        case MenuItem::Close: return "Schliessen";
    }
    return "";
}

namespace {
void scoreNameText(char* out, size_t size, int strokes, int par) {
    switch (scoreName(strokes, par)) {
        case ScoreName::Unknown: out[0] = '\0'; break;
        case ScoreName::HoleInOne: std::snprintf(out, size, "Hole-in-one"); break;
        case ScoreName::Albatross: std::snprintf(out, size, "Albatros"); break;
        case ScoreName::Eagle: std::snprintf(out, size, "Eagle"); break;
        case ScoreName::Birdie: std::snprintf(out, size, "Birdie"); break;
        case ScoreName::Par: std::snprintf(out, size, "Par"); break;
        case ScoreName::Bogey: std::snprintf(out, size, "Bogey"); break;
        case ScoreName::DoubleBogey: std::snprintf(out, size, "Doppelbogey"); break;
        case ScoreName::Worse: std::snprintf(out, size, "%+d", strokes - par); break;
    }
}
} // namespace

HoleDoneText holeDoneText(const App& app) {
    HoleDoneText t;
    std::memset(&t, 0, sizeof(t));
    const int hole = app.currentHole();
    const int par = app.course().holes[hole].par;
    const int strokes = app.round().shotCount(hole);
    const int putts = app.round().putts(hole);

    if (par > 0) {
        std::snprintf(t.caption, sizeof(t.caption), "LOCH %d · PAR %d", hole + 1, par);
    } else {
        std::snprintf(t.caption, sizeof(t.caption), "LOCH %d", hole + 1);
    }
    std::snprintf(t.strokes, sizeof(t.strokes), "%d", strokes);
    scoreNameText(t.name, sizeof(t.name), strokes, par);
    t.tone = par > 0 ? toneFor(strokes - par) : ScoreTone::Par;
    std::snprintf(t.putts, sizeof(t.putts), "%d %s", putts, putts == 1 ? "Putt" : "Putts");
    if (hole >= app.round().holeCount() - 1) {
        std::snprintf(t.hint, sizeof(t.hint), "Knopf drücken für die Rundenübersicht");
    } else {
        std::snprintf(t.hint, sizeof(t.hint), "Knopf drücken für Loch %d", hole + 2);
    }
    return t;
}

RoundSummaryText roundSummaryText(const App& app) {
    RoundSummaryText t;
    std::memset(&t, 0, sizeof(t));
    const RoundTotals totals = roundTotals(app.round(), app.course());
    std::snprintf(t.strokes, sizeof(t.strokes), "%d", totals.strokes);

    const char* holes_dative = totals.holes_played == 1 ? "Loch" : "Löchern";
    t.tone = ScoreTone::Par;
    if (totals.holes_played == 0) {
        std::snprintf(t.line, sizeof(t.line), "Noch kein Loch gespielt");
    } else if (totals.holes_with_par > 0) {
        char to_par[8];
        formatToPar(to_par, sizeof(to_par), totals.to_par);
        std::snprintf(t.line, sizeof(t.line), "%s nach %d %s", to_par, totals.holes_played, holes_dative);
        t.tone = toneFor(totals.to_par);
    } else {
        std::snprintf(t.line, sizeof(t.line), "%d %s", totals.holes_played, totals.holes_played == 1 ? "Loch" : "Löcher");
    }

    const char* putt_word = totals.putts == 1 ? "Putt" : "Putts";
    if (totals.longest_shot_m >= 1.0) {
        std::snprintf(t.detail, sizeof(t.detail), "%d %s · längster Schlag %ld m", totals.putts, putt_word,
                      shownMeters(totals.longest_shot_m));
    } else {
        std::snprintf(t.detail, sizeof(t.detail), "%d %s", totals.putts, putt_word);
    }
    std::snprintf(t.hint, sizeof(t.hint), "Knopf 1 lang: neue Runde · Knopf 2: zurück");
    return t;
}

PhoneText phoneText(const App& app) {
    PhoneText t;
    std::memset(&t, 0, sizeof(t));
    const NetStatus& net = app.netStatus();
    switch (net.mode) {
        case NetMode::Off:
            std::snprintf(t.line1, sizeof(t.line1), "WLAN wird gestartet ...");
            break;
        case NetMode::Connecting:
            std::snprintf(t.line1, sizeof(t.line1), "Verbinde mit WLAN");
            std::snprintf(t.value1, sizeof(t.value1), "%.32s", net.ssid);
            break;
        case NetMode::Hotspot:
            std::snprintf(t.line1, sizeof(t.line1), "Am Handy dieses WLAN wählen:");
            std::snprintf(t.value1, sizeof(t.value1), "%.32s", net.ssid);
            if (net.hotspot_password[0] != '\0') {
                std::snprintf(t.detail1, sizeof(t.detail1), "Passwort %.15s", net.hotspot_password);
            }
            std::snprintf(t.line2, sizeof(t.line2), "Dann im Browser öffnen:");
            std::snprintf(t.value2, sizeof(t.value2), "%.39s", net.address);
            break;
        case NetMode::Connected:
            std::snprintf(t.line1, sizeof(t.line1), "Verbunden mit WLAN");
            std::snprintf(t.value1, sizeof(t.value1), "%.32s", net.ssid);
            std::snprintf(t.line2, sizeof(t.line2), "Am Handy im Browser öffnen:");
            std::snprintf(t.value2, sizeof(t.value2), "%.39s", net.address);
            break;
    }
    std::snprintf(t.hint, sizeof(t.hint), "Knopf drücken zum Beenden");
    return t;
}

BatteryText batteryText(const App& app) {
    BatteryText t;
    std::memset(&t, 0, sizeof(t));
    const BatteryInfo& b = app.battery();
    if (!b.known) return t;
    std::snprintf(t.text, sizeof(t.text), b.charging ? "lädt %d %%" : "%d %%", b.percent);
    t.low = !b.charging && b.percent <= 20;
    return t;
}

const char* messageText(Message message) {
    switch (message) {
        case Message::None: return "";
        case Message::ShotRemoved: return "Letzter Schlag gelöscht";
        case Message::NoShotToRemove: return "Kein Schlag zum Löschen";
        case Message::NoShotYet: return "Noch kein Schlag erfasst";
        case Message::HoleFull: return "Maximale Schlagzahl erreicht";
        case Message::GreenMarked: return "Grünmitte gespeichert";
        case Message::NoGpsForGreen: return "Kein GPS: Grün nicht markiert";
        case Message::AlreadyFirstHole: return "Du bist auf Loch 1";
        case Message::NewRoundStarted: return "Neue Runde gestartet";
        case Message::BatteryLow: return "Akku fast leer";
    }
    return "";
}

} // namespace golf
