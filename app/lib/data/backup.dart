/// Sicherung aller Daten in eine Datei und zurueck.
///
/// {"format":"parlo-backup","v":1,"app":"0.1.0","created":"2026-11-25T11:30:00Z",
///  "rounds":[{"received":"...","device":"<Kennung>","round":{...},"original":{...}}],
///  "courses":[{"updated":"...","course":{...}}],
///  "settings":{...}}
///
/// "original" fehlt, wenn die Runde nicht korrigiert wurde; "device" und
/// "settings", wenn sie unbekannt sind. Runden, Plaetze und Einstellungen
/// stehen im selben Format wie auf dem Geraet.
library;

import 'dart:convert';

import '../app_info.dart';
import '../model/course.dart';
import '../model/device_config.dart';
import '../model/format.dart';
import '../model/round.dart';
import 'store.dart';

const String backupFormat = 'parlo-backup';
const int backupVersion = 1;

class ImportResult {
  const ImportResult({
    required this.roundsAdded,
    required this.roundsSkipped,
    required this.coursesAdded,
    required this.coursesSkipped,
    required this.settingsImported,
  });

  final int roundsAdded;

  /// Schon vorhanden und deshalb nicht angeruehrt.
  final int roundsSkipped;
  final int coursesAdded;
  final int coursesSkipped;
  final bool settingsImported;
}

Future<String> exportBackup(ParloStore store, {DateTime? now}) async {
  final rounds = await store.rounds();
  final courses = await store.courses();
  final config = await store.deviceConfig();
  // nach Kennung sortiert: dieselben Daten ergeben dieselbe Datei
  rounds.sort((a, b) => a.id.compareTo(b.id));
  courses.sort((a, b) => a.id.compareTo(b.id));
  return jsonEncode({
    'format': backupFormat,
    'v': backupVersion,
    'app': appVersion,
    'created': _time(now ?? DateTime.now()),
    'rounds': [
      for (final r in rounds)
        {
          'received': _time(r.receivedAt),
          if (r.deviceId != null) 'device': r.deviceId,
          'round': r.round.toMap(),
          if (r.isEdited) 'original': r.original.toMap(),
        },
    ],
    'courses': [
      for (final c in courses)
        {'updated': _time(c.updatedAt), 'course': c.course.toMap()},
    ],
    if (config != null) 'settings': config.toMap(),
  });
}

/// Liest eine Sicherung ein. Sie wird erst vollstaendig geprueft; bei einem
/// Fehler ([FormatError]) wird nichts veraendert.
///
/// Einlesen fuegt nur hinzu: Vorhandene Runden und Plaetze bleiben, wie sie
/// sind. Die Einstellungen werden nur uebernommen, wenn noch keine da sind.
Future<ImportResult> importBackup(ParloStore store, String content) async {
  final backup = _Backup.parse(content);
  return store.db.transaction(() async {
    var roundsAdded = 0;
    for (final r in backup.rounds) {
      if (await store.restoreRound(r)) roundsAdded++;
    }
    var coursesAdded = 0;
    for (final c in backup.courses) {
      if (await store.restoreCourse(c)) coursesAdded++;
    }
    var settingsImported = false;
    if (backup.config != null && await store.deviceConfig() == null) {
      await store.saveDeviceConfig(backup.config!);
      settingsImported = true;
    }
    return ImportResult(
      roundsAdded: roundsAdded,
      roundsSkipped: backup.rounds.length - roundsAdded,
      coursesAdded: coursesAdded,
      coursesSkipped: backup.courses.length - coursesAdded,
      settingsImported: settingsImported,
    );
  });
}

String _time(DateTime t) => t.toUtc().toIso8601String();

DateTime _readTime(Object? value, String what) {
  final t = value is String ? DateTime.tryParse(value) : null;
  if (t == null || !t.isUtc) fail('$what ungueltig');
  return t;
}

class _Backup {
  _Backup(this.rounds, this.courses, this.config);

  final List<StoredRound> rounds;
  final List<StoredCourse> courses;
  final DeviceConfig? config;

  static _Backup parse(String content) {
    final Object? doc;
    try {
      doc = jsonDecode(content);
    } on FormatException {
      fail('keine Sicherungsdatei');
    }
    if (doc is! Map<String, Object?> || doc['format'] != backupFormat) {
      fail('keine Sicherungsdatei');
    }
    final version = doc['v'];
    if (version is! int || version < 1) fail('keine Sicherungsdatei');
    if (version > backupVersion) {
      fail('Sicherung stammt aus einer neueren Version der App');
    }
    final roundItems = doc['rounds'];
    final courseItems = doc['courses'];
    if (roundItems is! List || courseItems is! List) {
      fail('Sicherung unvollstaendig');
    }

    final rounds = <StoredRound>[];
    final roundIds = <String>{};
    for (final item in roundItems) {
      if (item is! Map<String, Object?>) fail('Runde ungueltig');
      final round = GolfRound.fromMap(item['round']);
      final original = item['original'] == null
          ? round
          : GolfRound.fromMap(item['original']);
      final id = round.id;
      if (id == null || original.id != id) fail('Runde ohne Kennung');
      if (!roundIds.add(id)) fail('Runde doppelt');
      rounds.add(
        StoredRound(
          round: round,
          original: original,
          receivedAt: _readTime(item['received'], 'Zeitpunkt'),
          deviceId: readId(item['device'], 'Geraetekennung'),
        ),
      );
    }

    final courses = <StoredCourse>[];
    final courseIds = <String>{};
    for (final item in courseItems) {
      if (item is! Map<String, Object?>) fail('Platz ungueltig');
      final course = Course.fromMap(item['course']);
      final id = course.id;
      if (id == null) fail('Platz ohne Kennung');
      if (!courseIds.add(id)) fail('Platz doppelt');
      courses.add(
        StoredCourse(
          course: course,
          updatedAt: _readTime(item['updated'], 'Zeitpunkt'),
        ),
      );
    }

    final config = doc['settings'] == null
        ? null
        : DeviceConfig.fromMap(doc['settings']);
    return _Backup(rounds, courses, config);
  }
}
