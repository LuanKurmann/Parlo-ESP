import 'package:flutter_test/flutter_test.dart';
import 'package:parlo/model/club.dart';
import 'package:parlo/model/course.dart';
import 'package:parlo/model/device_config.dart';
import 'package:parlo/model/round.dart';

import 'support.dart';

void main() {
  late TestStore t;
  setUp(() => t = TestStore());
  tearDown(() => t.close());

  group('Runden', () {
    test('leere Datenbank', () async {
      expect(await t.store.rounds(), isEmpty);
      expect(await t.store.roundIds(), isEmpty);
      expect(await t.store.round(idOf(1)), isNull);
    });

    test('speichern und unveraendert wieder lesen', () async {
      final round = sampleRound();
      expect(await t.store.addRoundFromDevice(round, deviceId: idOf(9)), true);

      final stored = (await t.store.round(round.id!))!;
      expect(stored.round.toJson(), round.toJson());
      expect(stored.original.toJson(), round.toJson());
      expect(stored.isEdited, false);
      expect(stored.receivedAt, testNow);
      expect(stored.deviceId, idOf(9));
      expect(await t.store.roundIds(), {round.id});
    });

    test('eine vorhandene Runde wird nie ueberschrieben', () async {
      final round = sampleRound();
      await t.store.addRoundFromDevice(round);
      final other = round.copyWith(courseName: 'Anderer Platz');
      t.now = testNow.add(const Duration(hours: 1));

      expect(await t.store.addRoundFromDevice(other), false);

      final stored = (await t.store.round(round.id!))!;
      expect(stored.round.courseName, round.courseName);
      expect(stored.receivedAt, testNow);
      expect(await t.store.rounds(), hasLength(1));
    });

    test('Runde ohne Kennung wird abgewiesen', () {
      final round = sampleRound().copyWith(id: () => null);
      expect(() => t.store.addRoundFromDevice(round), throwsArgumentError);
      expect(() => t.store.updateRound(round), throwsArgumentError);
    });

    test('Reihenfolge: neueste zuerst, ohne Datum am Ende', () async {
      await t.store.addRoundFromDevice(roundWith(idOf(1)));
      await t.store.addRoundFromDevice(
        roundWith(idOf(2), date: '2026-05-01T09:00'),
      );
      await t.store.addRoundFromDevice(
        roundWith(idOf(3), date: '2026-11-23T08:15'),
      );
      await t.store.addRoundFromDevice(
        roundWith(idOf(4), date: '2025-12-31T23:59'),
      );
      t.now = testNow.add(const Duration(minutes: 1));
      await t.store.addRoundFromDevice(roundWith(idOf(5)));

      expect((await t.store.rounds()).map((r) => r.id), [
        idOf(3),
        idOf(2),
        idOf(4),
        idOf(5), // ohne Datum: zuletzt empfangene zuerst
        idOf(1),
      ]);
    });

    test('Korrektur behaelt die Fassung vom Geraet', () async {
      final round = sampleRound();
      await t.store.addRoundFromDevice(round);
      final holes = [...round.holes];
      holes[0] = holes[0].copyWith(
        shots: [
          holes[0].shots[0].copyWith(club: () => Club.wood3),
          ...holes[0].shots.skip(1),
        ],
      );
      final edited = round.copyWith(holes: holes);

      expect(await t.store.updateRound(edited), true);

      var stored = (await t.store.round(round.id!))!;
      expect(stored.isEdited, true);
      expect(stored.round.holes[0].shots[0].club, Club.wood3);
      expect(stored.original.holes[0].shots[0].club, Club.driver);
      expect(stored.original.toJson(), round.toJson());

      // ein erneuter Abgleich aendert daran nichts
      expect(await t.store.addRoundFromDevice(round), false);
      stored = (await t.store.round(round.id!))!;
      expect(stored.round.holes[0].shots[0].club, Club.wood3);

      expect(await t.store.resetRound(round.id!), true);
      stored = (await t.store.round(round.id!))!;
      expect(stored.isEdited, false);
      expect(stored.round.toJson(), round.toJson());
    });

    test('aendern und loeschen einer unbekannten Runde', () async {
      expect(await t.store.updateRound(roundWith(idOf(7))), false);
      expect(await t.store.resetRound(idOf(7)), false);
      expect(await t.store.deleteRound(idOf(7)), false);
      expect(await t.store.rounds(), isEmpty);
    });

    test('loeschen', () async {
      await t.store.addRoundFromDevice(roundWith(idOf(1)));
      await t.store.addRoundFromDevice(roundWith(idOf(2)));
      expect(await t.store.deleteRound(idOf(1)), true);
      expect(await t.store.roundIds(), {idOf(2)});
    });

    test('Beobachten meldet neue Runden', () async {
      final seen = <int>[];
      final sub = t.store.watchRounds().listen((r) => seen.add(r.length));
      await pumpEventQueue();
      await t.store.addRoundFromDevice(roundWith(idOf(1)));
      await pumpEventQueue();
      await t.store.addRoundFromDevice(roundWith(idOf(2)));
      await pumpEventQueue();
      await sub.cancel();
      expect(seen, [0, 1, 2]);
    });

    test('die vollste moegliche Runde passt', () async {
      final round = GolfRound(
        id: idOf(1),
        courseName: 'ü' * 15,
        holes: [
          for (var h = 0; h < maxHoles; h++)
            RoundHole(
              par: 5,
              done: true,
              shots: [
                for (var s = 0; s < maxShotsPerHole; s++)
                  Shot(position: sampleRound().holes[0].shots[0].position),
              ],
            ),
        ],
      );
      await t.store.addRoundFromDevice(round);
      expect((await t.store.round(idOf(1)))!.round.toJson(), round.toJson());
    });
  });

  group('Plaetze', () {
    test('speichern, lesen, ersetzen, loeschen', () async {
      final course = sampleCourse();
      expect((await t.store.saveCourse(course)).id, course.id);
      expect(
        (await t.store.course(course.id!))!.course.toJson(),
        course.toJson(),
      );
      expect((await t.store.course(course.id!))!.updatedAt, testNow);

      t.now = testNow.add(const Duration(days: 1));
      final renamed = course.copyWith(name: 'Neuer Name', revision: 8);
      await t.store.saveCourse(renamed);
      final all = await t.store.courses();
      expect(all, hasLength(1));
      expect(all.single.course.name, 'Neuer Name');
      expect(all.single.course.revision, 8);
      expect(all.single.updatedAt, t.now);

      expect(await t.store.deleteCourse(course.id!), true);
      expect(await t.store.deleteCourse(course.id!), false);
      expect(await t.store.courses(), isEmpty);
      expect(await t.store.course(course.id!), isNull);
    });

    test('ein Platz ohne Kennung bekommt eine', () async {
      final course = Course(name: 'Neu', holes: const [CourseHole(par: 4)]);
      final first = await t.store.saveCourse(course);
      final second = await t.store.saveCourse(course);
      expect(first.id, idOf(0xa000));
      expect(second.id, idOf(0xa001));
      expect(first.revision, 0);
      expect(await t.store.courses(), hasLength(2));
    });

    test('sortiert nach Name ohne Gross- und Kleinschreibung', () async {
      for (final name in ['bern', 'Zürich', 'Aarau', 'chur']) {
        await t.store.saveCourse(
          Course(name: name, holes: const [CourseHole()]),
        );
      }
      expect((await t.store.courses()).map((c) => c.course.name), [
        'Aarau',
        'bern',
        'chur',
        'Zürich',
      ]);
    });

    test('Beobachten meldet Aenderungen', () async {
      final seen = <List<String>>[];
      final sub = t.store.watchCourses().listen(
        (c) => seen.add([for (final x in c) x.course.name]),
      );
      await pumpEventQueue();
      final saved = await t.store.saveCourse(
        Course(name: 'A', holes: const [CourseHole()]),
      );
      await pumpEventQueue();
      await t.store.saveCourse(saved.copyWith(name: 'B'));
      await pumpEventQueue();
      await sub.cancel();
      expect(seen, [
        <String>[],
        ['A'],
        ['B'],
      ]);
    });
  });

  group('Einstellungen', () {
    test('anfangs keine, danach die gespeicherten', () async {
      expect(await t.store.deviceConfig(), isNull);
      final config = DeviceConfig(
        bag: {Club.driver, Club.iron7},
        carryM: {Club.iron7: 150},
        brightness: 90,
        swapButtons: true,
      );
      await t.store.saveDeviceConfig(config);
      expect(await t.store.deviceConfig(), config);
      await t.store.saveDeviceConfig(config.copyWith(dimAfterS: 60));
      expect((await t.store.deviceConfig())!.dimAfterS, 60);
    });
  });

  test('Daten ueberstehen das Schliessen und Oeffnen der Datei', () async {
    // eigener Test mit Datei statt Arbeitsspeicher
    final file = await fileDatabase();
    var s = file.open();
    await s.store.addRoundFromDevice(sampleRound());
    await s.store.saveCourse(sampleCourse());
    await s.store.saveDeviceConfig(DeviceConfig(brightness: 77));
    await s.db.close();

    s = file.open();
    expect(
      (await s.store.rounds()).single.round.toJson(),
      sampleRound().toJson(),
    );
    expect((await s.store.courses()).single.course, sampleCourse());
    expect((await s.store.deviceConfig())!.brightness, 77);
    await s.db.close();
    file.delete();
  });
}
