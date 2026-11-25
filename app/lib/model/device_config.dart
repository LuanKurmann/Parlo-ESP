import 'dart:convert';

import 'club.dart';
import 'format.dart';

const int minBrightness = 20;
const int maxBrightness = 255;
const int minDimAfterS = 5;
const int maxDimAfterS = 600;

/// Alles, was die App am Geraet einstellt: Bag, Schlagweiten, Anzeige, Knoepfe.
class DeviceConfig {
  DeviceConfig({
    Map<Club, int>? carryM,
    Set<Club>? bag,
    this.brightness = 200,
    this.dimAfterS = 20,
    this.swapButtons = false,
  }) : carryM = Map.unmodifiable({
         for (final c in Club.values)
           if (c != Club.putter) c: carryM?[c] ?? c.defaultCarryM,
       }),
       bag = Set.unmodifiable({...(bag ?? defaultBag), Club.putter}) {
    final problem = validate();
    if (problem != null) throw ArgumentError(problem);
  }

  /// Schlagweite jedes Schlaegers des Katalogs ausser dem Putter, in Metern.
  final Map<Club, int> carryM;

  /// Die Schlaeger im Bag; der Putter ist immer dabei.
  final Set<Club> bag;
  final int brightness;
  final int dimAfterS;
  final bool swapButtons;

  /// Die Schlaeger im Bag in der Reihenfolge des Katalogs.
  List<Club> get bagClubs => [
    for (final c in Club.values)
      if (bag.contains(c)) c,
  ];

  String? validate() {
    if (bag.length < 2 || bag.length > maxBagClubs) {
      return 'Bag braucht 2 bis $maxBagClubs Schlaeger';
    }
    if (carryM.values.any((m) => m < minCarryM || m > maxCarryM)) {
      return 'Schlagweite ausserhalb $minCarryM bis $maxCarryM m';
    }
    if (brightness < minBrightness || brightness > maxBrightness) {
      return 'Helligkeit ungueltig';
    }
    if (dimAfterS < minDimAfterS || dimAfterS > maxDimAfterS) {
      return 'Dimmzeit ungueltig';
    }
    return null;
  }

  DeviceConfig copyWith({
    Map<Club, int>? carryM,
    Set<Club>? bag,
    int? brightness,
    int? dimAfterS,
    bool? swapButtons,
  }) => DeviceConfig(
    carryM: carryM ?? this.carryM,
    bag: bag ?? this.bag,
    brightness: brightness ?? this.brightness,
    dimAfterS: dimAfterS ?? this.dimAfterS,
    swapButtons: swapButtons ?? this.swapButtons,
  );

  /// Uebernimmt die Angaben aus dem JSON. Was fehlt, bleibt wie in [base]
  /// (ohne [base]: Werkseinstellung). Mindestens eine bekannte Angabe muss
  /// vorhanden sein.
  factory DeviceConfig.fromJson(String json, {DeviceConfig? base}) =>
      DeviceConfig.fromMap(parseDocument(json), base: base);

  factory DeviceConfig.fromMap(Object? value, {DeviceConfig? base}) {
    final doc = checkDocument(value);
    final start = base ?? DeviceConfig();
    var touched = false;

    final carry = Map<Club, int>.of(start.carryM);
    if (doc['carry'] != null) {
      final entries = doc['carry'];
      if (entries is! Map<String, Object?>) fail('carry ist kein Objekt');
      for (final e in entries.entries) {
        final club = Club.fromShortName(e.key);
        if (club == null || club == Club.putter) fail('unbekannter Schlaeger');
        carry[club] = readInt(e.value, minCarryM, maxCarryM, 'Schlagweite');
      }
      touched = true;
    }

    var bag = start.bag;
    if (doc['clubs'] != null) {
      final list = doc['clubs'];
      if (list is! List) fail('clubs ist keine Liste');
      final chosen = <Club>{};
      for (final item in list) {
        final club = item is String ? Club.fromShortName(item) : null;
        if (club == null) fail('unbekannter Schlaeger');
        if (!chosen.add(club)) fail('Schlaeger doppelt');
      }
      chosen.add(Club.putter);
      if (chosen.length < 2 || chosen.length > maxBagClubs) {
        fail('Groesse des Bags ungueltig');
      }
      bag = chosen;
      touched = true;
    }

    var brightness = start.brightness;
    if (doc['brightness'] != null) {
      brightness = readInt(
        doc['brightness'],
        minBrightness,
        maxBrightness,
        'Helligkeit',
      );
      touched = true;
    }
    var dimAfterS = start.dimAfterS;
    if (doc['dim_s'] != null) {
      dimAfterS = readInt(doc['dim_s'], minDimAfterS, maxDimAfterS, 'Dimmzeit');
      touched = true;
    }
    var swapButtons = start.swapButtons;
    if (doc['swap_buttons'] != null) {
      final swap = doc['swap_buttons'];
      if (swap is! bool) fail('swap_buttons ungueltig');
      swapButtons = swap;
      touched = true;
    }
    if (!touched) fail('keine bekannte Angabe');
    return DeviceConfig(
      carryM: carry,
      bag: bag,
      brightness: brightness,
      dimAfterS: dimAfterS,
      swapButtons: swapButtons,
    );
  }

  Map<String, Object?> toMap() => {
    'v': formatVersion,
    'carry': {
      for (final c in Club.values)
        if (c != Club.putter) c.shortName: carryM[c],
    },
    'clubs': [for (final c in bagClubs) c.shortName],
    'brightness': brightness,
    'dim_s': dimAfterS,
    'swap_buttons': swapButtons,
  };

  String toJson() => jsonEncode(toMap());

  @override
  bool operator ==(Object other) =>
      other is DeviceConfig && other.toJson() == toJson();

  @override
  int get hashCode => toJson().hashCode;
}
