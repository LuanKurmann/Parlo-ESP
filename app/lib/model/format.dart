/// Gemeinsame Bausteine fuer das Lesen der Geraete-Formate.
///
/// Die Regeln entsprechen lib/core/src/core/serialize.cpp der Firmware und
/// werden gegen dieselben Testdaten geprueft (testdata/format).
library;

import 'dart:convert';
import 'dart:math';

const int formatVersion = 1;

/// Eine Eingabe entspricht nicht dem Format.
class FormatError implements Exception {
  const FormatError(this.message);
  final String message;

  @override
  String toString() => 'FormatError: $message';
}

Never fail(String message) => throw FormatError(message);

/// Liest das aeussere Objekt und prueft die Versionsnummer.
Map<String, Object?> parseDocument(String json) {
  final Object? value;
  try {
    value = jsonDecode(json);
  } on FormatException {
    fail('kein JSON');
  }
  return checkDocument(value);
}

Map<String, Object?> checkDocument(Object? value) {
  if (value is! Map<String, Object?>) fail('kein Objekt');
  if (value['v'] != formatVersion || value['v'] is! int) {
    fail('unbekannte Version');
  }
  return value;
}

int readInt(Object? value, int min, int max, String what) {
  if (value is! int || value < min || value > max) fail('$what ungueltig');
  return value;
}

/// Namen: hoechstens 31 Bytes in UTF-8, keine Steuerzeichen. null = leer.
String readName(Object? value, String what) {
  if (value == null) return '';
  if (value is! String) fail('$what ist kein Text');
  if (utf8.encode(value).length > maxNameBytes) fail('$what zu lang');
  if (value.codeUnits.any((c) => c < 0x20)) fail('$what mit Steuerzeichen');
  return value;
}

const int maxNameBytes = 31;

final RegExp _idPattern = RegExp(r'^[0-9a-f]{16}$');

bool isValidId(String id) => _idPattern.hasMatch(id);

/// Liest eine Kennung, falls vorhanden; eine vorhandene muss gueltig sein.
String? readId(Object? value, String what) {
  if (value == null) return null;
  if (value is! String || !isValidId(value)) fail('$what ungueltig');
  return value;
}

/// Neue zufaellige Kennung: 16 Hex-Zeichen wie auf dem Geraet.
String newId([Random? random]) {
  final r = random ?? Random.secure();
  final out = StringBuffer();
  for (var i = 0; i < 8; i++) {
    out.write(r.nextInt(256).toRadixString(16).padLeft(2, '0'));
  }
  return out.toString();
}
