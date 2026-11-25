import 'package:drift/drift.dart';

import '../model/course.dart';
import '../model/device_config.dart';
import '../model/format.dart';
import '../model/round.dart';
import 'database.dart';

/// Eine gespeicherte Runde mit dem, was nur das Handy weiss.
class StoredRound {
  StoredRound({
    required this.round,
    required this.original,
    required this.receivedAt,
    this.deviceId,
  });

  /// Aktueller Stand, nach Korrekturen in der App.
  final GolfRound round;

  /// So, wie die Runde vom Geraet kam.
  final GolfRound original;
  final DateTime receivedAt;
  final String? deviceId;

  String get id => round.id!;
  bool get isEdited => round != original;
}

class StoredCourse {
  StoredCourse({required this.course, required this.updatedAt});

  final Course course;
  final DateTime updatedAt;

  String get id => course.id!;
}

/// Zugriff auf die Daten der App. Alles, was die Oberflaeche und der Abgleich
/// lesen und schreiben, geht hier durch.
class ParloStore {
  ParloStore(this.db, {DateTime Function()? clock, String Function()? makeId})
    : _clock = clock ?? DateTime.now,
      _makeId = makeId ?? newId;

  final ParloDatabase db;
  final DateTime Function() _clock;
  final String Function() _makeId;

  DateTime get _now => _clock().toUtc();

  static const _configKey = 'device_config';

  // ---------- Runden ----------

  StoredRound _round(RoundRow row) => StoredRound(
    round: GolfRound.fromJson(row.json),
    original: GolfRound.fromJson(row.deviceJson),
    receivedAt: row.receivedAt.toUtc(),
    deviceId: row.deviceId,
  );

  SimpleSelectStatement<$RoundsTable, RoundRow> get _roundsNewestFirst =>
      db.select(db.rounds)..orderBy([
        // Runden ohne Datum ans Ende, sonst die neueste zuerst
        (r) => OrderingTerm(
          expression: r.started,
          mode: OrderingMode.desc,
          nulls: NullsOrder.last,
        ),
        (r) => OrderingTerm(expression: r.receivedAt, mode: OrderingMode.desc),
        (r) => OrderingTerm(expression: r.id),
      ]);

  Future<List<StoredRound>> rounds() async =>
      (await _roundsNewestFirst.get()).map(_round).toList();

  Stream<List<StoredRound>> watchRounds() =>
      _roundsNewestFirst.watch().map((rows) => rows.map(_round).toList());

  Future<StoredRound?> round(String id) async {
    final row = await (db.select(
      db.rounds,
    )..where((r) => r.id.equals(id))).getSingleOrNull();
    return row == null ? null : _round(row);
  }

  Future<Set<String>> roundIds() async {
    final query = db.selectOnly(db.rounds)..addColumns([db.rounds.id]);
    return (await query.get()).map((r) => r.read(db.rounds.id)!).toSet();
  }

  /// Speichert eine Runde vom Geraet. Eine schon vorhandene Runde wird nie
  /// ueberschrieben; dann kommt false zurueck.
  Future<bool> addRoundFromDevice(
    GolfRound round, {
    String? deviceId,
    DateTime? receivedAt,
  }) => _insertRound(round, round, deviceId, receivedAt ?? _now);

  Future<bool> _insertRound(
    GolfRound round,
    GolfRound original,
    String? deviceId,
    DateTime receivedAt,
  ) async {
    final id = round.id;
    if (id == null) throw ArgumentError('Runde ohne Kennung');
    if (original.id != id) throw ArgumentError('Kennungen verschieden');
    final row = await db
        .into(db.rounds)
        .insertReturningOrNull(
          RoundsCompanion.insert(
            id: id,
            courseId: Value(round.courseId),
            courseName: round.courseName,
            started: Value(round.startedText),
            json: round.toJson(),
            deviceJson: original.toJson(),
            deviceId: Value(deviceId),
            receivedAt: receivedAt.toUtc(),
          ),
          mode: InsertMode.insertOrIgnore,
        );
    // null, wenn es die Zeile schon gab und nichts eingefuegt wurde
    return row != null;
  }

  /// Speichert eine Korrektur. Die Fassung vom Geraet bleibt erhalten.
  /// false, wenn es die Runde nicht gibt.
  Future<bool> updateRound(GolfRound round) async {
    final id = round.id;
    if (id == null) throw ArgumentError('Runde ohne Kennung');
    final count = await (db.update(db.rounds)..where((r) => r.id.equals(id)))
        .write(
          RoundsCompanion(
            courseId: Value(round.courseId),
            courseName: Value(round.courseName),
            started: Value(round.startedText),
            json: Value(round.toJson()),
          ),
        );
    return count == 1;
  }

  /// Verwirft alle Korrekturen einer Runde.
  Future<bool> resetRound(String id) async {
    final stored = await round(id);
    if (stored == null) return false;
    return updateRound(stored.original);
  }

  Future<bool> deleteRound(String id) async =>
      await (db.delete(db.rounds)..where((r) => r.id.equals(id))).go() == 1;

  // ---------- Plaetze ----------

  StoredCourse _course(CourseRow row) => StoredCourse(
    course: Course.fromJson(row.json),
    updatedAt: row.updatedAt.toUtc(),
  );

  SimpleSelectStatement<$CoursesTable, CourseRow> get _coursesByName =>
      db.select(db.courses)..orderBy([
        (c) => OrderingTerm(expression: c.name.collate(Collate.noCase)),
        (c) => OrderingTerm(expression: c.id),
      ]);

  Future<List<StoredCourse>> courses() async =>
      (await _coursesByName.get()).map(_course).toList();

  Stream<List<StoredCourse>> watchCourses() =>
      _coursesByName.watch().map((rows) => rows.map(_course).toList());

  Future<StoredCourse?> course(String id) async {
    final row = await (db.select(
      db.courses,
    )..where((c) => c.id.equals(id))).getSingleOrNull();
    return row == null ? null : _course(row);
  }

  /// Speichert einen Platz in der Bibliothek und ersetzt einen vorhandenen
  /// mit derselben Kennung. Ein Platz ohne Kennung bekommt eine neue.
  /// Zurueck kommt der Platz, wie er gespeichert ist.
  Future<Course> saveCourse(Course course, {DateTime? updatedAt}) async {
    final saved = course.id == null ? course.copyWith(id: _makeId) : course;
    await db
        .into(db.courses)
        .insertOnConflictUpdate(
          CoursesCompanion.insert(
            id: saved.id!,
            name: saved.name,
            revision: saved.revision,
            json: saved.toJson(),
            updatedAt: (updatedAt ?? _now).toUtc(),
          ),
        );
    return saved;
  }

  Future<bool> deleteCourse(String id) async =>
      await (db.delete(db.courses)..where((c) => c.id.equals(id))).go() == 1;

  // ---------- Einstellungen fuer das Geraet ----------

  /// Die Einstellungen; null, solange keine gespeichert sind.
  Future<DeviceConfig?> deviceConfig() async {
    final value = await _value(_configKey);
    return value == null ? null : DeviceConfig.fromJson(value);
  }

  Future<void> saveDeviceConfig(DeviceConfig config) =>
      _setValue(_configKey, config.toJson());

  Future<String?> _value(String key) async => (await (db.select(
    db.keyValues,
  )..where((k) => k.key.equals(key))).getSingleOrNull())?.value;

  Future<void> _setValue(String key, String value) => db
      .into(db.keyValues)
      .insertOnConflictUpdate(
        KeyValuesCompanion.insert(key: key, value: value),
      );

  // ---------- fuer die Sicherung ----------

  /// Fuegt eine Runde samt Geraetefassung ein, falls es sie noch nicht gibt.
  Future<bool> restoreRound(StoredRound stored) => _insertRound(
    stored.round,
    stored.original,
    stored.deviceId,
    stored.receivedAt,
  );

  /// Fuegt einen Platz ein, falls es ihn noch nicht gibt.
  Future<bool> restoreCourse(StoredCourse stored) async {
    final row = await db
        .into(db.courses)
        .insertReturningOrNull(
          CoursesCompanion.insert(
            id: stored.id,
            name: stored.course.name,
            revision: stored.course.revision,
            json: stored.course.toJson(),
            updatedAt: stored.updatedAt.toUtc(),
          ),
          mode: InsertMode.insertOrIgnore,
        );
    return row != null;
  }
}
