import 'package:drift/drift.dart';

part 'database.g.dart';

/// Runden, wie sie vom Geraet kamen, und ihr aktueller Stand in der App.
@DataClassName('RoundRow')
class Rounds extends Table {
  TextColumn get id => text()();
  TextColumn get courseId => text().nullable()();
  TextColumn get courseName => text()();

  /// "2026-11-23T08:15" (UTC); als Text sortierbar. null = unbekannt.
  TextColumn get started => text().nullable()();

  /// Aktueller Stand, nach Korrekturen in der App.
  TextColumn get json => text()();

  /// Unveraendert so, wie die Runde vom Geraet kam.
  TextColumn get deviceJson => text()();
  TextColumn get deviceId => text().nullable()();
  DateTimeColumn get receivedAt => dateTime()();

  @override
  Set<Column> get primaryKey => {id};
}

/// Die Platz-Bibliothek.
@DataClassName('CourseRow')
class Courses extends Table {
  TextColumn get id => text()();
  TextColumn get name => text()();
  IntColumn get revision => integer()();
  TextColumn get json => text()();
  DateTimeColumn get updatedAt => dateTime()();

  @override
  Set<Column> get primaryKey => {id};
}

/// Einzelne Werte: Einstellungen fuer das Geraet, bekanntes Geraet usw.
@DataClassName('KeyValueRow')
class KeyValues extends Table {
  TextColumn get key => text()();
  TextColumn get value => text()();

  @override
  Set<Column> get primaryKey => {key};
}

@DriftDatabase(tables: [Rounds, Courses, KeyValues])
class ParloDatabase extends _$ParloDatabase {
  ParloDatabase(super.executor);

  @override
  int get schemaVersion => 1;

  @override
  MigrationStrategy get migration => MigrationStrategy(
    onCreate: (m) => m.createAll(),
    onUpgrade: (m, from, to) async {
      // Noch keine aelteren Fassungen. Jede kuenftige bekommt hier ihren
      // Schritt und einen Test in test/data/migration_test.dart.
    },
  );
}
