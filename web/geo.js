// Distanzen und Koordinaten. Gleiche Formel wie im Geraet (lib/core/src/core/geo.cpp).
(function (root) {
  'use strict';

  var SEMI_MAJOR = 6378137.0;
  var ECC_SQ = 6.69437999014e-3;
  var DEG = Math.PI / 180;

  // Distanz in Metern zwischen zwei Punkten in Dezimalgrad.
  function distance(lat1, lon1, lat2, lon2) {
    var mid = ((lat1 + lat2) / 2) * DEG;
    var dLat = (lat2 - lat1) * DEG;
    var dLonDeg = lon2 - lon1;
    while (dLonDeg > 180) dLonDeg -= 360;
    while (dLonDeg < -180) dLonDeg += 360;
    var sin = Math.sin(mid);
    var w = 1 - ECC_SQ * sin * sin;
    var north = dLat * (SEMI_MAJOR * (1 - ECC_SQ)) / (w * Math.sqrt(w));
    var east = dLonDeg * DEG * (SEMI_MAJOR / Math.sqrt(w)) * Math.cos(mid);
    return Math.sqrt(north * north + east * east);
  }

  // Die Dateien des Geraets speichern Koordinaten als Ganzzahlen in Zehnmillionstel Grad.
  function toE7(degrees) { return Math.round(degrees * 1e7); }
  function fromE7(value) { return value / 1e7; }

  // Distanz zwischen zwei [Breite, Laenge]-Paaren im Geraeteformat.
  function distanceE7(a, b) {
    return distance(fromE7(a[0]), fromE7(a[1]), fromE7(b[0]), fromE7(b[1]));
  }

  // Punkt in lokale Meter (Osten, Norden) relativ zu einem Bezugspunkt.
  function toLocal(lat, lon, refLat, refLon) {
    var north = distance(refLat, refLon, lat, refLon) * (lat >= refLat ? 1 : -1);
    var east = distance(refLat, refLon, refLat, lon) * (lon >= refLon ? 1 : -1);
    return { east: east, north: north };
  }

  var api = { distance: distance, toE7: toE7, fromE7: fromE7, distanceE7: distanceE7, toLocal: toLocal };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  root.GolfGeo = api;
})(typeof window !== 'undefined' ? window : globalThis);
