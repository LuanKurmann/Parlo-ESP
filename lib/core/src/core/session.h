// Verbindet die App mit dem Speicher: laedt beim Start Platz und laufende
// Runde und sichert danach jede Aenderung.
#pragma once

#include <cstdint>
#include <string>

#include "core/app.h"
#include "core/store.h"

namespace golf {

// Hinweis: Eine Session ist rund 30 kB gross. Auf dem Geraet statisch oder
// auf dem Heap anlegen, nicht auf dem Stack einer Task.
class Session {
public:
    // Laedt den gespeicherten Platz und setzt eine laufende Runde fort. Fehlt
    // etwas oder ist es beschaedigt, beginnt die App an dieser Stelle leer.
    explicit Session(FileSystem& fs);

    App& app() { return app_; }
    const App& app() const { return app_; }
    Store& store() { return store_; }

    // Aktuelle Zeit vom GPS; wird als Startzeit der Runde vermerkt.
    void setDateTime(const DateTime& now) {
        now_ = now;
        if (now.valid && !started_.valid) pending_ = true;
    }
    const DateTime& roundStarted() const { return started_; }

    // Regelmaessig aufrufen (darf in jedem Durchlauf der Hauptschleife sein;
    // ohne neue Eingabe kostet es nichts). Speichert, was sich geaendert hat, und legt
    // eine beendete Runde ins Archiv. false, wenn etwas nicht gespeichert
    // werden konnte; es wird beim naechsten Aufruf erneut versucht.
    bool sync();

    bool lastSyncOk() const { return last_sync_ok_; }

    // Ersetzt den Platz (Import vom Handy). Eine laufende Runde mit Schlaegen
    // wird vorher ins Archiv gelegt; gelingt das nicht, bleibt alles beim Alten
    // und es kommt false zurueck. `archived` meldet, ob eine Runde abgelegt wurde.
    bool replaceCourse(const Course& course, bool* archived = nullptr);

    // Setzt und speichert die Schlagweiten.
    bool setBag(const Bag& bag);

private:
    static Course loadCourseOrEmpty(Store& store);
    std::string currentRoundJson() const;

    Store store_;
    App app_;
    DateTime now_;
    DateTime started_;
    DateTime previous_started_;
    uint32_t seen_round_number_ = 0;
    bool archive_pending_ = false;
    bool round_saved_ = false;
    bool course_saved_ = false;
    uint32_t round_crc_ = 0;
    uint32_t course_crc_ = 0;
    bool last_sync_ok_ = true;
    bool pending_ = true;            // beim ersten Aufruf immer pruefen
    uint32_t seen_input_count_ = 0;
};

} // namespace golf
