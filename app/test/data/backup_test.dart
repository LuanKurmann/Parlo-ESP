import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:parlo/data/backup.dart';
import 'package:parlo/model/club.dart';
import 'package:parlo/model/course.dart';
import 'package:parlo/model/device_config.dart';
import 'package:parlo/model/format.dart';

import 'support.dart';

/// Fuellt einen Speicher mit zwei Runden (eine korrigiert), zwei Plaetzen
/// und Einstellungen.
Future<void> fill(TestStore t) async {
  await t.store.addRoundFromDevice(sampleRound(), deviceId: idOf(9));
  await t.store.updateRound(sampleRound().copyWith(courseName: 'Korrigiert'));
  t.now = testNow.add(const Duration(hours: 2));
  await t.store.addRoundFromDevice(
    roundWith(idOf(2), date: '2026-05-01T09:00'),
  );
  await t.store.saveCourse(sampleCourse());
  await t.store.saveCourse(
    Course(name: 'Ohne Kennung', holes: const [CourseHole(par: 3)]),
  );
  await t.store.saveDeviceConfig(
    DeviceConfig(bag: {Club.driver, Club.lobWedge}, dimAfterS: 45),
  );
}

Map<String, Object?> decode(String s) => jsonDecode(s) as Map<String, Object?>;

void main() {
  late TestStore a;
  late TestStore b;
  setUp(() {
    a = TestStore();
    b = TestStore();
  });
  tearDown(() async {
    await a.close();
    await b.close();
  });

  final at = DateTime.utc(2026, 11, 26, 7, 30);

  test('leere Sicherung', () async {
    final backup = await exportBackup(a.store, now: at);
    expect(decode(backup), {
      'format': 'parlo-backup',
      'v': 1,
      'app': '0.1.0',
      'created': '2026-11-26T07:30:00.000Z',
      'rounds': <Object?>[],
      'courses': <Object?>[],
    });
    final result = await importBackup(b.store, backup);
    expect(result.roundsAdded, 0);
    expect(result.coursesAdded, 0);
    expect(result.settingsImported, false);
  });

  test(
    'sichern und in eine leere App einlesen ergibt dieselben Daten',
    () async {
      await fill(a);
      final backup = await exportBackup(a.store, now: at);

      final result = await importBackup(b.store, backup);

      expect(result.roundsAdded, 2);
      expect(result.roundsSkipped, 0);
      expect(result.coursesAdded, 2);
      expect(result.coursesSkipped, 0);
      expect(result.settingsImported, true);
      // dieselben Daten ergeben dieselbe Datei
      expect(await exportBackup(b.store, now: at), backup);

      final round = (await b.store.round(sampleRound().id!))!;
      expect(round.round.courseName, 'Korrigiert');
      expect(round.original.toJson(), sampleRound().toJson());
      expect(round.isEdited, true);
      expect(round.deviceId, idOf(9));
      expect(round.receivedAt, testNow);
      expect((await b.store.round(idOf(2)))!.isEdited, false);
      expect((await b.store.deviceConfig())!.dimAfterS, 45);
    },
  );

  test(
    'die Sicherung enthaelt Runden und Plaetze im Format des Geraets',
    () async {
      await fill(a);
      final doc = decode(await exportBackup(a.store, now: at));
      final rounds = doc['rounds'] as List;
      final edited = rounds.firstWhere((r) => r['original'] != null);
      expect(jsonEncode(edited['original']), sampleRound().toJson());
      final plain = rounds.firstWhere((r) => r['original'] == null);
      expect(plain['round']['id'], idOf(2));
      expect(plain.containsKey('device'), false);
      final courses = doc['courses'] as List;
      expect(
        courses.map((c) => jsonEncode(c['course'])),
        contains(sampleCourse().toJson()),
      );
    },
  );

  test('zweimal einlesen aendert nichts', () async {
    await fill(a);
    final backup = await exportBackup(a.store, now: at);
    await importBackup(b.store, backup);

    final again = await importBackup(b.store, backup);

    expect(again.roundsAdded, 0);
    expect(again.roundsSkipped, 2);
    expect(again.coursesAdded, 0);
    expect(again.coursesSkipped, 2);
    expect(again.settingsImported, false);
    expect(await exportBackup(b.store, now: at), backup);
  });

  test('einlesen ueberschreibt nichts Vorhandenes', () async {
    await fill(a);
    final backup = await exportBackup(a.store, now: at);
    // b hat dieselbe Runde und denselben Platz in anderem Stand, eigene
    // Einstellungen und eine eigene Runde
    await b.store.addRoundFromDevice(sampleRound());
    await b.store.saveCourse(sampleCourse().copyWith(name: 'Mein Name'));
    await b.store.saveDeviceConfig(DeviceConfig(brightness: 50));
    await b.store.addRoundFromDevice(roundWith(idOf(0x77)));

    final result = await importBackup(b.store, backup);

    expect(result.roundsAdded, 1);
    expect(result.roundsSkipped, 1);
    expect(result.coursesAdded, 1);
    expect(result.coursesSkipped, 1);
    expect(result.settingsImported, false);
    expect(
      (await b.store.round(sampleRound().id!))!.round.courseName,
      sampleRound().courseName,
    );
    expect(
      (await b.store.course(sampleCourse().id!))!.course.name,
      'Mein Name',
    );
    expect((await b.store.deviceConfig())!.brightness, 50);
    expect(await b.store.roundIds(), {sampleRound().id, idOf(2), idOf(0x77)});
  });

  group('fehlerhafte Sicherung veraendert nichts:', () {
    late Map<String, Object?> good;
    setUp(() async {
      await fill(a);
      good = decode(await exportBackup(a.store, now: at));
      await b.store.addRoundFromDevice(roundWith(idOf(0x77)));
    });

    Future<void> expectRejected(Object? doc) async {
      final content = doc is String ? doc : jsonEncode(doc);
      await expectLater(
        importBackup(b.store, content),
        throwsA(isA<FormatError>()),
      );
      expect(await b.store.roundIds(), {idOf(0x77)});
      expect(await b.store.courses(), isEmpty);
      expect(await b.store.deviceConfig(), isNull);
    }

    List<Object?> list(String key) => good[key] as List<Object?>;
    Map<String, Object?> item(String key, int i) =>
        list(key)[i] as Map<String, Object?>;

    test('kein JSON', () => expectRejected('PK\u0003\u0004'));
    test('leer', () => expectRejected(''));
    test('Liste', () => expectRejected(<Object?>[]));
    test('abgeschnitten', () {
      final text = jsonEncode(good);
      return expectRejected(text.substring(0, text.length ~/ 2));
    });
    test('andere Datei', () => expectRejected({...good, 'format': 'x'}));
    test('eine Runde vom Geraet statt einer Sicherung', () {
      return expectRejected(sampleRound().toJson());
    });
    test('neuere Version', () => expectRejected({...good, 'v': 2}));
    test('Version 0', () => expectRejected({...good, 'v': 0}));
    test('Runden fehlen', () => expectRejected({...good}..remove('rounds')));
    test('Plaetze fehlen', () => expectRejected({...good}..remove('courses')));
    test('letzte Runde kaputt', () {
      (item('rounds', 1)['round'] as Map)['hole'] = 99;
      return expectRejected(good);
    });
    test('Runde ohne Kennung', () {
      (item('rounds', 0)['round'] as Map).remove('id');
      return expectRejected(good);
    });
    test('Runde doppelt', () {
      list('rounds').add(list('rounds')[0]);
      return expectRejected(good);
    });
    test('Geraetefassung gehoert zu einer anderen Runde', () {
      final edited = list('rounds')
          .indexWhere((r) => (r as Map)['original'] != null);
      (item('rounds', edited)['original'] as Map)['id'] = idOf(0x55);
      return expectRejected(good);
    });
    test('Zeitpunkt fehlt', () {
      item('rounds', 0).remove('received');
      return expectRejected(good);
    });
    test('Zeitpunkt ohne Zeitzone', () {
      item('rounds', 0)['received'] = '2026-11-25T12:00:00';
      return expectRejected(good);
    });
    test('Geraetekennung ungueltig', () {
      item('rounds', 0)['device'] = 'abc';
      return expectRejected(good);
    });
    test('letzter Platz kaputt', () {
      (item('courses', 1)['course'] as Map)['holes'] = <Object?>[];
      return expectRejected(good);
    });
    test('Platz ohne Kennung', () {
      (item('courses', 0)['course'] as Map).remove('id');
      return expectRejected(good);
    });
    test('Platz doppelt', () {
      list('courses').add(list('courses')[1]);
      return expectRejected(good);
    });
    test('Einstellungen kaputt', () {
      (good['settings'] as Map)['brightness'] = 1;
      return expectRejected(good);
    });
  });

  test('unbekannte zusaetzliche Angaben stoeren nicht', () async {
    await fill(a);
    final doc = decode(await exportBackup(a.store, now: at));
    doc['spaeter'] = {'x': 1};
    ((doc['rounds'] as List)[0] as Map)['notiz'] = 'hallo';
    final result = await importBackup(b.store, jsonEncode(doc));
    expect(result.roundsAdded, 2);
  });
}
