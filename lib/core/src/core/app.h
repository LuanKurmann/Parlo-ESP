// Ablaufsteuerung der App: Welche Ansicht ist offen, was tun die Knoepfe gerade.
// Kennt weder Display noch Hardware; die Oberflaeche liest den Zustand nur aus.
#pragma once

#include <cstdint>

#include "core/button.h"
#include "core/clubs.h"
#include "core/course.h"
#include "core/network.h"
#include "core/round.h"

namespace golf {

enum class View : uint8_t { Distance, Shots, Scorecard };
constexpr int kViewCount = 3;

// Was ueber der Ansicht liegt und die Knoepfe uebernimmt.
enum class Overlay : uint8_t { None, ClubPicker, Menu, HoleDone, RoundSummary, Phone };

enum class MenuItem : uint8_t { MarkGreen, PreviousHole, EndRound, Phone, Close };
constexpr int kMenuItemCount = 5;

// Kurze Rueckmeldungen an den Spieler (werden als Einblendung gezeigt).
enum class Message : uint8_t {
    None,
    ShotRemoved,
    NoShotToRemove,
    NoShotYet,        // Loch abschliessen ohne Schlag
    HoleFull,         // maximale Schlagzahl erreicht
    GreenMarked,
    NoGpsForGreen,    // Gruen markieren ohne GPS-Empfang
    AlreadyFirstHole,
    NewRoundStarted,
};

class App {
public:
    explicit App(const Course& course = Course(), const Bag& bag = Bag());

    // ---------- Eingaben ----------

    // Aktuelle Position vom GPS. clearPosition(), wenn der Empfang weg ist.
    void setPosition(const LatLon& position);
    void clearPosition();

    // Die zwei Knoepfe, jeweils kurz und lang gedrueckt.
    //
    //                 Knopf 1 kurz          Knopf 1 lang     Knopf 2 kurz         Knopf 2 lang
    //   Spiel         Schlag zaehlen        Schlag loeschen  Loch abschliessen    Menue oeffnen
    //   Schlaegerwahl Vorschlag bestaetigen Schlag loeschen  naechster Schlaeger  -
    //   Menue         Eintrag ausfuehren    -                naechster Eintrag    Menue schliessen
    //   Loch fertig   naechstes Loch        -                naechstes Loch       -
    //   Rundenende    -                     neue Runde       zurueck zur Runde    -
    //   Handy         schliessen            -                schliessen           -
    void button1Short();
    void button1Long();
    void button2Short();
    void button2Long();

    // Touch.
    void swipe(int direction);       // +1 = naechste Ansicht, -1 = vorherige; nur ohne Overlay
    void selectClub(Club club);      // in der Schlaegerwahl
    void selectMenuItem(MenuItem item); // im Menue
    void tap();                      // bei "Loch fertig": weiter; bei "Handy": schliessen

    // ---------- Zustand ----------

    View view() const { return view_; }
    Overlay overlay() const { return overlay_; }
    int currentHole() const { return hole_; } // 0 = Loch 1
    Club suggestedClub() const { return suggested_; }
    MenuItem menuSelection() const { return menu_; }
    bool hasPosition() const { return has_position_; }

    // Solange die Ansicht "Mit Handy verbinden" offen ist, soll das WLAN laufen.
    // Wer das WLAN betreibt, meldet dessen Zustand mit setNetStatus().
    bool phoneWanted() const { return overlay_ == Overlay::Phone; }
    void setNetStatus(const NetStatus& status) { net_status_ = status; }
    const NetStatus& netStatus() const { return net_status_; }

    // Liefert die anstehende Rueckmeldung und loescht sie.
    Message takeMessage();

    // Distanzen zum Gruen des aktuellen Lochs (nicht verfuegbar ohne GPS oder Gruen).
    GreenDistances distances() const;
    // Weite des letzten Schlags des aktuellen Lochs, live bis zur eigenen Position.
    ShotDistance lastShotDistance() const;

    const Round& round() const { return round_; }

    // ---------- Fuer das Speichern ----------

    // Setzt eine gespeicherte Runde fort (nach einem Neustart). Passt die Runde
    // nicht zum Platz, bleibt die App unveraendert und es kommt false zurueck.
    bool restoreRound(const Round& round, int hole);

    // Wechselt den Platz und beginnt darauf eine leere Runde. Die bisherige
    // Runde ist danach weg; wer sie behalten will, sichert sie vorher.
    void setCourse(const Course& course);

    // Neue Schlagweiten fuer den Schlaegervorschlag.
    void setBag(const Bag& bag) { bag_ = bag; }

    // Zaehlt hoch, sobald eine neue Runde begonnen wird. Die Runde davor
    // bleibt in previousRound() erhalten, bis die naechste beginnt.
    // Zaehlt jede Eingabe (Knopf oder Touch). Aendert er sich nicht, hat sich
    // auch nichts zu Speicherndes geaendert.
    uint32_t inputCount() const { return input_count_; }

    uint32_t roundNumber() const { return round_number_; }
    const Round& previousRound() const { return previous_round_; }
    const Course& course() const { return course_; }
    const Bag& bag() const { return bag_; }

private:
    void addShot();
    void confirmClub(Club club);
    void undo();
    void finishHole();
    void nextHole();
    void runMenuItem(MenuItem item);
    void newRound();

    Course course_;
    Bag bag_;
    Round round_;
    Round previous_round_;
    uint32_t round_number_ = 0;
    uint32_t input_count_ = 0;
    int hole_ = 0;
    View view_ = View::Distance;
    Overlay overlay_ = Overlay::None;
    Club suggested_ = Club::Driver;
    MenuItem menu_ = MenuItem::MarkGreen;
    Message message_ = Message::None;
    bool has_position_ = false;
    LatLon position_{0.0, 0.0};
    NetStatus net_status_;
};

// Leitet ein Knopf-Ereignis an die App weiter. button ist 1 oder 2.
void dispatchButton(App& app, int button, ButtonEvent event);

} // namespace golf
