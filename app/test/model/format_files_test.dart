// Prueft das Datenmodell gegen die gemeinsamen Testdaten in testdata/format.
// Dieselben Dateien liest die Firmware (test/test_format_files).
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:parlo/model/course.dart';
import 'package:parlo/model/device_config.dart';
import 'package:parlo/model/format.dart';
import 'package:parlo/model/round.dart';

String readFile(String name) =>
    File('../testdata/format/$name').readAsStringSync();

String rewrite(String kind, String json) => switch (kind) {
  'course' => Course.fromJson(json).toJson(),
  'round' => GolfRound.fromJson(json).toJson(),
  'settings' => DeviceConfig.fromJson(json).toJson(),
  _ => throw ArgumentError(kind),
};

void main() {
  for (final (kind, file) in [
    ('course', 'course.json'),
    ('round', 'round.json'),
    ('settings', 'settings.json'),
    ('settings', 'settings_default.json'),
  ]) {
    test('$file wird unveraendert wieder geschrieben', () {
      final json = readFile(file);
      expect(rewrite(kind, json), json);
    });
  }

  test('Werkseinstellung entspricht der Datei', () {
    expect(DeviceConfig().toJson(), readFile('settings_default.json'));
  });

  test('rejects.json: alles wird abgelehnt', () {
    final items = jsonDecode(readFile('rejects.json')) as List;
    expect(items.length, greaterThan(60));
    for (final item in items) {
      final kind = item['kind'] as String;
      expect(
        () => rewrite(kind, item['json'] as String),
        throwsA(isA<FormatError>()),
        reason: '$kind: ${item['why']}',
      );
    }
  });

  test('accepts.json: angenommen und gleich geschrieben', () {
    final items = jsonDecode(readFile('accepts.json')) as List;
    expect(items.length, greaterThan(8));
    for (final item in items) {
      final kind = item['kind'] as String;
      expect(
        rewrite(kind, item['json'] as String),
        item['canonical'],
        reason: '$kind: ${item['why']}',
      );
    }
  });
}
