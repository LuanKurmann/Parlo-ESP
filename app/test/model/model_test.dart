import 'dart:convert';
import 'dart:io';
import 'dart:math';

import 'package:flutter_test/flutter_test.dart';
import 'package:parlo/model/club.dart';
import 'package:parlo/model/course.dart';
import 'package:parlo/model/device_config.dart';
import 'package:parlo/model/format.dart';
import 'package:parlo/model/geo_point.dart';
import 'package:parlo/model/round.dart';

String readFile(String name) =>
    File('../testdata/format/$name').readAsStringSync();

void main() {
  group('Kennungen', () {
    test('neue Kennungen sind gueltig und verschieden', () {
      final ids = {for (var i = 0; i < 200; i++) newId()};
      expect(ids, hasLength(200));
      expect(ids.every(isValidId), true);
    });

    test('gleicher Zufall ergibt gleiche Kennung', () {
      expect(newId(Random(1)), newId(Random(1)));
      expect(newId(Random(1)), isNot(newId(Random(2))));
    });

    test('Gueltigkeit', () {
      expect(isValidId('0123456789abcdef'), true);
      expect(isValidId('0123456789abcde'), false);
      expect(isValidId('0123456789abcdef0'), false);
      expect(isValidId('0123456789ABCDEF'), false);
      expect(isValidId('0123456789abcdef\n'), false);
      expect(isValidId(''), false);
    });
  });

  group('Position', () {
    test('Grad und Zehnmillionstel', () {
      final p = GeoPoint.degrees(47.3800123, -8.5456789);
      expect(p.latE7, 473800123);
      expect(p.lonE7, -85456789);
      expect(p.lat, closeTo(47.3800123, 1e-9));
      expect(p, const GeoPoint.e7(473800123, -85456789));
      expect(p.hashCode, const GeoPoint.e7(473800123, -85456789).hashCode);
      expect(p == const GeoPoint.e7(473800123, -85456788), false);
    });

    test('Raender des Erlaubten', () {
      expect(GeoPoint.read([900000000, -1800000000], 'x'), isNotNull);
      expect(
        () => GeoPoint.read([-900000001, 0], 'x'),
        throwsA(isA<FormatError>()),
      );
      expect(GeoPoint.read(null, 'x'), isNull);
    });
  });

  group('Schlaeger', () {
    test('Katalog wie in der Firmware', () {
      final header = File('../lib/core/src/core/clubs.cpp').readAsStringSync();
      final names = RegExp(r'kShortNames\[kClubCount\] = \{([^}]*)\}')
          .firstMatch(header)!
          .group(1)!;
      final fromFirmware = RegExp(r'"(\w+)"')
          .allMatches(names)
          .map((m) => m.group(1))
          .toList();
      expect([for (final c in Club.values) c.shortName], fromFirmware);
      expect(Club.values, hasLength(17));
    });

    test('Kurznamen', () {
      for (final c in Club.values) {
        expect(Club.fromShortName(c.shortName), c);
      }
      expect(Club.fromShortName('E2'), isNull);
      expect(Club.fromShortName('dr'), isNull);
    });
  });

  group('Platz', () {
    test('Grenzen', () {
      expect(() => Course(holes: const []), throwsArgumentError);
      expect(
        () => Course(holes: List.filled(19, const CourseHole())),
        throwsArgumentError,
      );
      expect(Course(holes: List.filled(18, const CourseHole())).holeCount, 18);
    });

    test('Loecher lassen sich nicht nachtraeglich aendern', () {
      final course = Course(holes: const [CourseHole(par: 4)]);
      expect(
        () => course.holes.add(const CourseHole()),
        throwsUnsupportedError,
      );
    });

    test('copyWith kann Felder leeren', () {
      final course = Course.fromJson(readFile('course.json'));
      expect(course.copyWith(id: () => null).id, isNull);
      expect(course.copyWith(name: 'A').id, course.id);
      final hole = course.holes[0];
      expect(hole.copyWith(green: () => null).green, isNull);
      expect(hole.copyWith(par: 5).green, hole.green);
      expect(hole.greenRadiusM, 17.5);
    });

    test('Gleichheit', () {
      final a = Course.fromJson(readFile('course.json'));
      final b = Course.fromJson(readFile('course.json'));
      expect(a, b);
      expect(a.hashCode, b.hashCode);
      expect(a == a.copyWith(revision: 8), false);
      expect(
        const CourseHole(par: 3, greenRadiusDm: 200),
        const CourseHole(par: 3),
        reason: 'ohne Gruen zaehlt der Radius nicht',
      );
    });
  });

  group('Runde', () {
    final round = GolfRound.fromJson(readFile('round.json'));

    test('gelesene Werte', () {
      expect(round.id, '0123456789abcdef');
      expect(round.courseId, '00aa11bb22cc33dd');
      expect(round.currentHole, 2);
      expect(round.started, DateTime.utc(2026, 11, 23, 8, 15));
      expect(round.holes, hasLength(4));
      expect(round.holes[0].strokes, 4);
      expect(round.holes[0].putts, 2);
      expect(round.holes[0].shots[2].position, isNull);
      expect(round.holes[1].shots[1].club, isNull);
      expect(round.holes[1].par, 3);
      expect(round.holes[2].par, 0);
      expect(round.holes[3].shots, isEmpty);
    });

    test('Grenzen', () {
      expect(() => GolfRound(holes: const []), throwsArgumentError);
      expect(
        () => GolfRound(currentHole: 1, holes: [RoundHole()]),
        throwsArgumentError,
      );
      expect(() => RoundHole(done: true), throwsArgumentError);
      expect(
        () => RoundHole(shots: List.filled(21, const Shot())),
        throwsArgumentError,
      );
    });

    test('Datum schreiben', () {
      expect(
        GolfRound.formatDate(DateTime.utc(2026, 1, 5, 7, 3, 59)),
        '2026-01-05T07:03',
      );
    });

    test('ein Datum, das der Kalender nicht kennt, bleibt erhalten', () {
      // das Geraet prueft den Tag nur auf 1 bis 31
      final json = readFile('round.json')
          .replaceFirst('2026-11-23', '2026-02-31');
      expect(GolfRound.fromJson(json).toJson(), json);
    });
  });

  group('Einstellungen', () {
    test('Werkseinstellung', () {
      final c = DeviceConfig();
      expect(c.bagClubs, hasLength(11));
      expect(c.carryM[Club.driver], 210);
      expect(c.carryM.containsKey(Club.putter), false);
      expect(c.brightness, 200);
      expect(c.dimAfterS, 20);
      expect(c.swapButtons, false);
    });

    test('der Putter ist immer im Bag', () {
      expect(DeviceConfig(bag: {Club.driver}).bag, {Club.driver, Club.putter});
    });

    test('ungueltige Werte', () {
      expect(() => DeviceConfig(bag: {}), throwsArgumentError);
      expect(
        () => DeviceConfig(bag: Club.values.take(15).toSet()),
        throwsArgumentError,
      );
      expect(() => DeviceConfig(carryM: {Club.driver: 0}), throwsArgumentError);
      expect(
        () => DeviceConfig(carryM: {Club.driver: 401}),
        throwsArgumentError,
      );
      expect(() => DeviceConfig(brightness: 19), throwsArgumentError);
      expect(() => DeviceConfig(dimAfterS: 601), throwsArgumentError);
      expect(
        DeviceConfig(bag: Club.values.take(13).toSet()).bag,
        hasLength(14),
      );
    });

    test('Angaben werden auf einen vorhandenen Stand angewendet', () {
      final base = DeviceConfig(brightness: 90, bag: {Club.driver});
      final next = DeviceConfig.fromJson('{"v":1,"dim_s":30}', base: base);
      expect(next.brightness, 90);
      expect(next.dimAfterS, 30);
      expect(next.bag, base.bag);
    });
  });

  test(
    'beschaedigte Eingaben: abgelehnt oder gueltig, nie ein anderer Fehler',
    () {
      final random = Random(20261125);
      const replacements = [
        '0',
        '-1',
        '99',
        'null',
        '"x"',
        '[]',
        '{}',
        'true',
        '1.5',
      ];
      var accepted = 0;
      var rejected = 0;
      for (final (kind, file) in [
        ('course', 'course.json'),
        ('round', 'round.json'),
        ('settings', 'settings.json'),
      ]) {
        final original = readFile(file);
        final tokens = RegExp(r'"(?:[^"\\]|\\.)*"|-?\d+|true|false|[\[\]{}:,]')
            .allMatches(original)
            .toList();
        for (var i = 0; i < 4000; i++) {
          final token = tokens[random.nextInt(tokens.length)];
          final String damaged;
          switch (random.nextInt(3)) {
            case 0: // Wert ersetzen
              damaged = original.replaceRange(
                token.start,
                token.end,
                replacements[random.nextInt(replacements.length)],
              );
            case 1: // Zeichen entfernen
              damaged = original.replaceRange(token.start, token.end, '');
            default: // abschneiden
              damaged = original.substring(0, token.start);
          }
          try {
            final String first;
            final String second;
            switch (kind) {
              case 'course':
                first = Course.fromJson(damaged).toJson();
                second = Course.fromJson(first).toJson();
              case 'round':
                first = GolfRound.fromJson(damaged).toJson();
                second = GolfRound.fromJson(first).toJson();
              default:
                first = DeviceConfig.fromJson(damaged).toJson();
                second = DeviceConfig.fromJson(first).toJson();
            }
            expect(second, first, reason: damaged);
            jsonDecode(first);
            accepted++;
          } on FormatError {
            rejected++;
          }
        }
      }
      // beide Wege muessen wirklich vorkommen
      expect(accepted, greaterThan(50));
      expect(rejected, greaterThan(3000));
    },
  );
}
