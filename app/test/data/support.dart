import 'dart:io';

import 'package:drift/drift.dart';
import 'package:drift/native.dart';
import 'package:parlo/data/database.dart';
import 'package:parlo/data/store.dart';
import 'package:parlo/model/course.dart';
import 'package:parlo/model/geo_point.dart';
import 'package:parlo/model/round.dart';

final DateTime testNow = DateTime.utc(2026, 11, 25, 12, 0, 0);

class TestStore {
  TestStore() {
    // Tests oeffnen absichtlich mehrere Datenbanken nebeneinander
    driftRuntimeOptions.dontWarnAboutMultipleDatabases = true;
    store = ParloStore(
      db,
      clock: () => now,
      makeId: () => (nextId++).toRadixString(16).padLeft(16, '0'),
    );
  }

  final ParloDatabase db = ParloDatabase(NativeDatabase.memory());
  late final ParloStore store;
  DateTime now = testNow;
  int nextId = 0xa000;

  Future<void> close() => db.close();
}

GolfRound sampleRound() => GolfRound.fromJson(
  File('../testdata/format/round.json').readAsStringSync(),
);

Course sampleCourse() =>
    Course.fromJson(File('../testdata/format/course.json').readAsStringSync());

/// Eine kleine Runde mit eigener Kennung und eigenem Datum.
GolfRound roundWith(String id, {String? date, String course = 'Platz'}) =>
    GolfRound(
      id: id,
      courseName: course,
      startedText: date,
      holes: [
        RoundHole(
          par: 4,
          done: true,
          shots: const [Shot(position: GeoPoint.e7(473769001, 85417002))],
        ),
      ],
    );

String idOf(int n) => n.toRadixString(16).padLeft(16, '0');

class FileDatabase {
  FileDatabase(this.dir);
  final Directory dir;

  ({ParloDatabase db, ParloStore store}) open() {
    final db = ParloDatabase(NativeDatabase(File('${dir.path}/parlo.sqlite')));
    return (db: db, store: ParloStore(db, clock: () => testNow));
  }

  void delete() => dir.deleteSync(recursive: true);
}

Future<FileDatabase> fileDatabase() async =>
    FileDatabase(await Directory.systemTemp.createTemp('parlo_test'));
