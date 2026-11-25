import 'dart:convert';

import 'club.dart';
import 'course.dart';
import 'format.dart';
import 'geo_point.dart';

const int maxShotsPerHole = 20;

class Shot {
  const Shot({this.position, this.club});

  /// null, wenn beim Schlag kein GPS-Empfang da war.
  final GeoPoint? position;

  /// null, solange kein Schlaeger gewaehlt ist.
  final Club? club;

  bool get isPutt => club == Club.putter;

  Shot copyWith({GeoPoint? Function()? position, Club? Function()? club}) =>
      Shot(
        position: position != null ? position() : this.position,
        club: club != null ? club() : this.club,
      );

  factory Shot.fromJson(Object? value) {
    if (value is! Map<String, Object?>) fail('Schlag ist kein Objekt');
    final c = value['c'];
    Club? club;
    if (c != null) {
      club = c is String ? Club.fromShortName(c) : null;
      if (club == null) fail('unbekannter Schlaeger');
    }
    return Shot(position: GeoPoint.read(value['p'], 'Position'), club: club);
  }

  Map<String, Object?> toJson() => {
    if (position != null) 'p': position!.toJson(),
    if (club != null) 'c': club!.shortName,
  };

  @override
  bool operator ==(Object other) =>
      other is Shot && other.position == position && other.club == club;

  @override
  int get hashCode => Object.hash(position, club);
}

class RoundHole {
  RoundHole({
    this.par = 0,
    this.green,
    this.done = false,
    List<Shot> shots = const [],
  }) : shots = List.unmodifiable(shots) {
    if (shots.length > maxShotsPerHole) {
      throw ArgumentError.value(shots.length, 'shots', 'zu viele Schlaege');
    }
    if (done && shots.isEmpty) {
      throw ArgumentError('abgeschlossenes Loch ohne Schlag');
    }
  }

  /// Par des Lochs zum Zeitpunkt der Runde; 0 = unbekannt.
  final int par;

  /// Gruenmitte zum Zeitpunkt der Runde, falls bekannt.
  final GeoPoint? green;
  final bool done;
  final List<Shot> shots;

  int get strokes => shots.length;
  int get putts => shots.where((s) => s.isPutt).length;

  RoundHole copyWith({
    int? par,
    GeoPoint? Function()? green,
    bool? done,
    List<Shot>? shots,
  }) => RoundHole(
    par: par ?? this.par,
    green: green != null ? green() : this.green,
    done: done ?? this.done,
    shots: shots ?? this.shots,
  );

  factory RoundHole.fromJson(Object? value) {
    if (value is! Map<String, Object?>) fail('Loch ist kein Objekt');
    final done = value['done'];
    if (done is! bool) fail('done fehlt');
    final shots = value['shots'];
    if (shots is! List || shots.length > maxShotsPerHole) {
      fail('Schlaege ungueltig');
    }
    if (done && shots.isEmpty) fail('abgeschlossenes Loch ohne Schlag');
    return RoundHole(
      par: value['par'] == null ? 0 : readInt(value['par'], 3, 6, 'Par'),
      green: GeoPoint.read(value['green'], 'Gruen'),
      done: done,
      shots: [for (final s in shots) Shot.fromJson(s)],
    );
  }

  Map<String, Object?> toJson() => {
    if (par > 0) 'par': par,
    if (green != null) 'green': green!.toJson(),
    'done': done,
    'shots': [for (final s in shots) s.toJson()],
  };
}

final RegExp _datePattern = RegExp(
  r'^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})$',
);

/// Liest "2026-11-23T08:15" (UTC, vom GPS). Die Pruefung ist dieselbe wie auf
/// dem Geraet: Jahr 2000 bis 2099, Tag 1 bis 31 ohne Blick auf den Monat.
DateTime? readRoundDate(Object? value) {
  if (value == null) return null;
  final m = value is String ? _datePattern.firstMatch(value) : null;
  if (m == null) fail('Datum ungueltig');
  final p = [for (var i = 1; i <= 5; i++) int.parse(m.group(i)!)];
  if (p[0] < 2000 ||
      p[0] > 2099 ||
      p[1] < 1 ||
      p[1] > 12 ||
      p[2] < 1 ||
      p[2] > 31 ||
      p[3] > 23 ||
      p[4] > 59) {
    fail('Datum ungueltig');
  }
  return DateTime.utc(p[0], p[1], p[2], p[3], p[4]);
}

String _two(int v) => v.toString().padLeft(2, '0');

class GolfRound {
  GolfRound({
    this.id,
    this.courseId,
    this.courseName = '',
    this.currentHole = 0,
    this.startedText,
    required List<RoundHole> holes,
  }) : holes = List.unmodifiable(holes) {
    if (holes.isEmpty || holes.length > maxHoles) {
      throw ArgumentError.value(holes.length, 'holes', '1 bis $maxHoles');
    }
    if (currentHole < 0 || currentHole >= holes.length) {
      throw ArgumentError.value(currentHole, 'currentHole');
    }
  }

  final String? id;
  final String? courseId;
  final String courseName;

  /// Loch, auf dem die Runde stand; 0 = Loch 1.
  final int currentHole;

  /// Zeitpunkt des ersten Schlags, wortwoertlich wie vom Geraet geschrieben
  /// ("2026-11-23T08:15", UTC). So bleibt auch ein Datum wie der 31. Februar
  /// erhalten, das das Geraet nicht abweist.
  final String? startedText;
  final List<RoundHole> holes;

  /// Der Zeitpunkt als DateTime (UTC); null, wenn unbekannt.
  DateTime? get started => readRoundDate(startedText);

  static String formatDate(DateTime utc) =>
      '${utc.year.toString().padLeft(4, '0')}-${_two(utc.month)}-'
      '${_two(utc.day)}T${_two(utc.hour)}:${_two(utc.minute)}';

  GolfRound copyWith({
    String? Function()? id,
    String? Function()? courseId,
    String? courseName,
    int? currentHole,
    String? Function()? startedText,
    List<RoundHole>? holes,
  }) => GolfRound(
    id: id != null ? id() : this.id,
    courseId: courseId != null ? courseId() : this.courseId,
    courseName: courseName ?? this.courseName,
    currentHole: currentHole ?? this.currentHole,
    startedText: startedText != null ? startedText() : this.startedText,
    holes: holes ?? this.holes,
  );

  factory GolfRound.fromJson(String json) =>
      GolfRound.fromMap(parseDocument(json));

  factory GolfRound.fromMap(Object? value) {
    final doc = checkDocument(value);
    final holes = doc['holes'];
    if (holes is! List || holes.isEmpty || holes.length > maxHoles) {
      fail('Anzahl Loecher ungueltig');
    }
    readRoundDate(doc['date']);
    return GolfRound(
      id: readId(doc['id'], 'Kennung'),
      courseId: readId(doc['course_id'], 'Platzkennung'),
      courseName: readName(doc['course'], 'Platzname'),
      currentHole: readInt(doc['hole'], 0, holes.length - 1, 'Loch'),
      startedText: doc['date'] as String?,
      holes: [for (final h in holes) RoundHole.fromJson(h)],
    );
  }

  Map<String, Object?> toMap() => {
    'v': formatVersion,
    if (id != null) 'id': id,
    if (courseId != null) 'course_id': courseId,
    'course': courseName,
    'hole': currentHole,
    if (startedText != null) 'date': startedText,
    'holes': [for (final h in holes) h.toJson()],
  };

  String toJson() => jsonEncode(toMap());

  @override
  bool operator ==(Object other) =>
      other is GolfRound && other.toJson() == toJson();

  @override
  int get hashCode => toJson().hashCode;
}
