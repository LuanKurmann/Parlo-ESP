import 'dart:convert';

import 'format.dart';
import 'geo_point.dart';

const int maxHoles = 18;
const int defaultGreenRadiusDm = 130;
const int minGreenRadiusDm = 10;
const int maxGreenRadiusDm = 600;

bool isValidCoursePar(int par) => par == 0 || (par >= 3 && par <= 6);

class CourseHole {
  const CourseHole({
    this.par = 0,
    this.green,
    this.greenRadiusDm = defaultGreenRadiusDm,
    this.tee,
  });

  /// 0 = unbekannt, sonst 3 bis 6.
  final int par;

  /// Mitte des Gruens; null, solange es weder importiert noch markiert ist.
  final GeoPoint? green;

  /// Halbe Tiefe des Gruens in Spielrichtung, in Dezimetern.
  final int greenRadiusDm;
  final GeoPoint? tee;

  double get greenRadiusM => greenRadiusDm / 10;

  CourseHole copyWith({
    int? par,
    GeoPoint? Function()? green,
    int? greenRadiusDm,
    GeoPoint? Function()? tee,
  }) => CourseHole(
    par: par ?? this.par,
    green: green != null ? green() : this.green,
    greenRadiusDm: greenRadiusDm ?? this.greenRadiusDm,
    tee: tee != null ? tee() : this.tee,
  );

  factory CourseHole.fromJson(Object? value) {
    if (value is! Map<String, Object?>) fail('Loch ist kein Objekt');
    final par = readInt(value['par'], 0, 6, 'Par');
    if (!isValidCoursePar(par)) fail('Par ungueltig');
    return CourseHole(
      par: par,
      green: GeoPoint.read(value['green'], 'Gruen'),
      tee: GeoPoint.read(value['tee'], 'Abschlag'),
      greenRadiusDm: value['r'] == null
          ? defaultGreenRadiusDm
          : readInt(value['r'], minGreenRadiusDm, maxGreenRadiusDm, 'Radius'),
    );
  }

  Map<String, Object?> toJson() => {
    'par': par,
    if (green != null) 'green': green!.toJson(),
    if (green != null) 'r': greenRadiusDm,
    if (tee != null) 'tee': tee!.toJson(),
  };

  @override
  bool operator ==(Object other) =>
      other is CourseHole &&
      other.par == par &&
      other.green == green &&
      // der Radius zaehlt nur mit Gruen, ohne wird er nicht gespeichert
      (green == null || other.greenRadiusDm == greenRadiusDm) &&
      other.tee == tee;

  @override
  int get hashCode => Object.hash(par, green, tee);
}

class Course {
  Course({
    this.id,
    this.revision = 0,
    this.name = '',
    required List<CourseHole> holes,
  }) : holes = List.unmodifiable(holes) {
    if (holes.isEmpty || holes.length > maxHoles) {
      throw ArgumentError.value(holes.length, 'holes', '1 bis $maxHoles');
    }
  }

  /// Dauerhafte Kennung; null, solange keine vergeben ist.
  final String? id;

  /// Zaehlt Aenderungen am Geraet (z. B. Gruen markiert).
  final int revision;
  final String name;
  final List<CourseHole> holes;

  int get holeCount => holes.length;

  Course copyWith({
    String? Function()? id,
    int? revision,
    String? name,
    List<CourseHole>? holes,
  }) => Course(
    id: id != null ? id() : this.id,
    revision: revision ?? this.revision,
    name: name ?? this.name,
    holes: holes ?? this.holes,
  );

  factory Course.fromJson(String json) => Course.fromMap(parseDocument(json));

  factory Course.fromMap(Object? value) {
    final doc = checkDocument(value);
    final holes = doc['holes'];
    if (holes is! List || holes.isEmpty || holes.length > maxHoles) {
      fail('Anzahl Loecher ungueltig');
    }
    final id = readId(doc['id'], 'Kennung');
    final revision = doc['rev'] == null
        ? 0
        : readInt(doc['rev'], 0, 2000000000, 'Aenderungszaehler');
    return Course(
      id: id,
      // ohne Kennung gibt es keinen Zaehler
      revision: id == null ? 0 : revision,
      name: readName(doc['name'], 'Name'),
      holes: [for (final h in holes) CourseHole.fromJson(h)],
    );
  }

  Map<String, Object?> toMap() => {
    'v': formatVersion,
    if (id != null) 'id': id,
    if (id != null) 'rev': revision,
    'name': name,
    'holes': [for (final h in holes) h.toJson()],
  };

  String toJson() => jsonEncode(toMap());

  @override
  bool operator ==(Object other) =>
      other is Course && other.toJson() == toJson();

  @override
  int get hashCode => toJson().hashCode;
}
