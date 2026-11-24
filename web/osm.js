// Platzdaten aus OpenStreetMap: Abfragen fuer die Overpass-API und Umwandlung
// der Antwort in das Platzformat des Geraets.
//
// In OpenStreetMap ist eine Spielbahn eine Linie vom Abschlag zum Gruen mit
// golf=hole, der Lochnummer in "ref" und dem Par in "par". Gruens sind
// Flaechen mit golf=green.
(function (root) {
  'use strict';

  var Geo = typeof require !== 'undefined' ? require('./geo.js') : root.GolfGeo;

  var OVERPASS_URL = 'https://overpass-api.de/api/interpreter';
  var MAX_NAME_BYTES = 31;       // so viel passt im Geraet in den Platznamen
  var GREEN_SEARCH_M = 60;       // so weit darf ein Gruen vom Ende der Bahn entfernt sein
  var MIN_RADIUS_M = 5, MAX_RADIUS_M = 40;

  // Text so maskieren, dass er in einer Overpass-Abfrage als Suchwort gilt.
  function escapeForQuery(text) {
    return text.replace(/[\\"]/g, '').replace(/[.*+?^${}()|[\]]/g, '\\\\$&').trim();
  }

  function courseSearchQuery(name) {
    return '[out:json][timeout:25];\n' +
      'nwr["leisure"="golf_course"]["name"~"' + escapeForQuery(name) + '",i];\n' +
      'out tags center 20;';
  }

  function holesQuery(lat, lon, radius) {
    var around = '(around:' + Math.round(radius || 2500) + ',' + lat.toFixed(6) + ',' + lon.toFixed(6) + ')';
    return '[out:json][timeout:25];\n(\n' +
      '  way["golf"="hole"]' + around + ';\n' +
      '  way["golf"="green"]' + around + ';\n' +
      ');\nout tags geom;';
  }

  // Trefferliste einer Platzsuche: [{name, place, lat, lon}]
  function parseCourseSearch(response) {
    var result = [];
    ((response && response.elements) || []).forEach(function (element) {
      var tags = element.tags || {};
      var lat = element.center ? element.center.lat : element.lat;
      var lon = element.center ? element.center.lon : element.lon;
      if (!tags.name || typeof lat !== 'number' || typeof lon !== 'number') return;
      result.push({
        name: tags.name,
        place: tags['addr:city'] || tags['addr:town'] || tags['addr:village'] || tags['is_in'] || '',
        lat: lat,
        lon: lon
      });
    });
    return result;
  }

  // Platzname fuer das Geraet: keine Steuerzeichen, hoechstens 31 Bytes in UTF-8.
  function cleanName(name) {
    var text = String(name || '').replace(/[\u0000-\u001f]/g, ' ').trim();
    var encoder = new TextEncoder();
    while (encoder.encode(text).length > MAX_NAME_BYTES) text = text.slice(0, -1);
    return text.trim();
  }

  function centroid(points) {
    var n = points.length;
    // Geschlossene Flaechen wiederholen den ersten Punkt am Ende.
    if (n > 1 && points[0].lat === points[n - 1].lat && points[0].lon === points[n - 1].lon) n -= 1;
    var lat = 0, lon = 0;
    for (var i = 0; i < n; i++) { lat += points[i].lat; lon += points[i].lon; }
    return { lat: lat / n, lon: lon / n };
  }

  // Halbe Ausdehnung der Flaeche in Spielrichtung (von `from` nach `to`), in Metern.
  function radiusAlong(points, from, to) {
    var dir = Geo.toLocal(to.lat, to.lon, from.lat, from.lon);
    var length = Math.sqrt(dir.east * dir.east + dir.north * dir.north);
    if (length < 1) return null;
    var min = Infinity, max = -Infinity;
    points.forEach(function (p) {
      var local = Geo.toLocal(p.lat, p.lon, from.lat, from.lon);
      var along = (local.east * dir.east + local.north * dir.north) / length;
      if (along < min) min = along;
      if (along > max) max = along;
    });
    return (max - min) / 2;
  }

  function nearestGreen(greens, point) {
    var best = null, bestDistance = GREEN_SEARCH_M;
    greens.forEach(function (green) {
      var d = Geo.distance(point.lat, point.lon, green.center.lat, green.center.lon);
      if (d <= bestDistance) { best = green; bestDistance = d; }
    });
    return best;
  }

  // Baut aus den Overpass-Elementen einen Platz.
  //   elements  Antwort von holesQuery (response.elements)
  //   name      Platzname
  //   center    {lat, lon} des Platzes; entscheidet, wenn es eine Lochnummer mehrfach gibt
  // Liefert {course, warnings, holesFound, greensMatched}. `course` hat das
  // Format des Geraets; `warnings` sind Hinweise fuer den Benutzer.
  function courseFromOsm(elements, name, center) {
    var warnings = [];
    var greens = [];
    var candidates = {}; // Lochnummer -> Bahnen
    var withoutNumber = 0;

    (elements || []).forEach(function (element) {
      var tags = element.tags || {};
      var points = (element.geometry || []).filter(function (p) {
        return p && typeof p.lat === 'number' && typeof p.lon === 'number';
      });
      if (tags.golf === 'green' && points.length >= 3) {
        greens.push({ points: points, center: centroid(points) });
      } else if (tags.golf === 'hole' && points.length >= 2) {
        var number = /^\d{1,2}$/.test(String(tags.ref || '').trim()) ? parseInt(tags.ref, 10) : 0;
        if (number < 1 || number > 18) { withoutNumber++; return; }
        var par = /^\d$/.test(String(tags.par || '').trim()) ? parseInt(tags.par, 10) : 0;
        if (par < 3 || par > 6) par = 0;
        (candidates[number] = candidates[number] || []).push({ points: points, par: par });
      }
    });

    var numbers = Object.keys(candidates).map(Number);
    if (numbers.length === 0) {
      return { course: null, warnings: ['In OpenStreetMap sind für diesen Platz keine Spielbahnen erfasst.'], holesFound: 0, greensMatched: 0 };
    }
    var holeCount = Math.max.apply(null, numbers) > 9 ? 18 : 9;

    var duplicates = [];
    var holes = [];
    var missing = [];
    var withoutPar = [];
    var greensMatched = 0;
    for (var n = 1; n <= holeCount; n++) {
      var list = candidates[n];
      if (!list) { holes.push({ par: 0 }); missing.push(n); continue; }
      if (list.length > 1) {
        duplicates.push(n);
        if (center) {
          list.sort(function (a, b) {
            var ma = centroid(a.points), mb = centroid(b.points);
            return Geo.distance(center.lat, center.lon, ma.lat, ma.lon) - Geo.distance(center.lat, center.lon, mb.lat, mb.lon);
          });
        }
      }
      var way = list[0];
      var points = way.points;
      var first = points[0], last = points[points.length - 1];

      // Normal endet die Bahn am Gruen. Liegt nur am Anfang eines, ist sie verkehrt herum erfasst.
      var green = nearestGreen(greens, last);
      if (!green && nearestGreen(greens, first)) {
        points = points.slice().reverse();
        first = points[0];
        last = points[points.length - 1];
        green = nearestGreen(greens, last);
      }

      var hole = { par: way.par };
      if (way.par === 0) withoutPar.push(n);
      if (green) {
        greensMatched++;
        hole.green = [Geo.toE7(green.center.lat), Geo.toE7(green.center.lon)];
        var radius = radiusAlong(green.points, points[points.length - 2], last);
        if (radius !== null) {
          radius = Math.min(MAX_RADIUS_M, Math.max(MIN_RADIUS_M, radius));
          hole.r = Math.round(radius * 10);
        }
      } else {
        hole.green = [Geo.toE7(last.lat), Geo.toE7(last.lon)];
      }
      hole.tee = [Geo.toE7(first.lat), Geo.toE7(first.lon)];
      holes.push(hole);
    }

    if (missing.length) warnings.push('Nicht erfasst: Loch ' + missing.join(', ') + '. Diese Grüns kannst du auf dem Platz selbst markieren.');
    if (withoutPar.length) warnings.push('Ohne Par: Loch ' + withoutPar.join(', ') + '.');
    if (greensMatched < numbers.length) {
      var without = numbers.length - greensMatched;
      warnings.push('Bei ' + without + (without === 1 ? ' Loch' : ' Löchern') + ' fehlt die Grünfläche; dort gilt das Ende der Bahn als Grünmitte.');
    }
    if (duplicates.length) warnings.push('Loch ' + duplicates.join(', ') + ' gibt es mehrfach (mehrere Plätze nebeneinander?). Verwendet wird jeweils die Bahn, die näher an der Platzmitte liegt.');
    if (withoutNumber) warnings.push(withoutNumber + (withoutNumber === 1 ? ' Bahn ohne gültige Lochnummer wurde' : ' Bahnen ohne gültige Lochnummer wurden') + ' übersprungen.');

    return {
      course: { v: 1, name: cleanName(name), holes: holes },
      warnings: warnings,
      holesFound: numbers.length,
      greensMatched: greensMatched
    };
  }

  var api = {
    OVERPASS_URL: OVERPASS_URL,
    courseSearchQuery: courseSearchQuery,
    holesQuery: holesQuery,
    parseCourseSearch: parseCourseSearch,
    courseFromOsm: courseFromOsm,
    cleanName: cleanName
  };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  root.GolfOsm = api;
})(typeof window !== 'undefined' ? window : globalThis);
