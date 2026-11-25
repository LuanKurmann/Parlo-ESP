import 'format.dart';

/// Eine Position in Zehnmillionstel Grad, so wie sie in den Dateien steht.
/// Ganzzahlen machen Lesen und Schreiben verlustfrei (rund 1 cm genau).
class GeoPoint {
  const GeoPoint.e7(this.latE7, this.lonE7);

  GeoPoint.degrees(double lat, double lon)
    : latE7 = (lat * 1e7).round(),
      lonE7 = (lon * 1e7).round();

  final int latE7;
  final int lonE7;

  double get lat => latE7 / 1e7;
  double get lon => lonE7 / 1e7;

  /// Liest `[Breite, Laenge]`; null, wenn der Wert fehlt.
  static GeoPoint? read(Object? value, String what) {
    if (value == null) return null;
    if (value is! List || value.length != 2) fail('$what ungueltig');
    return GeoPoint.e7(
      readInt(value[0], -900000000, 900000000, what),
      readInt(value[1], -1800000000, 1800000000, what),
    );
  }

  List<int> toJson() => [latE7, lonE7];

  @override
  bool operator ==(Object other) =>
      other is GeoPoint && other.latE7 == latE7 && other.lonE7 == lonE7;

  @override
  int get hashCode => Object.hash(latE7, lonE7);

  @override
  String toString() => 'GeoPoint($lat, $lon)';
}
