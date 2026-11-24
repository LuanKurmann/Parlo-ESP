// Tests der Rechenlogik der Web-Oberflaeche:  node --test web/test/*.test.js
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const Geo = require('../geo.js');
const Osm = require('../osm.js');
const Stats = require('../stats.js');

// Punkt `north` Meter noerdlich und `east` Meter oestlich eines Ausgangspunkts (bei Zuerich).
const BASE = { lat: 47.3769, lon: 8.5417 };
function at(north, east = 0, base = BASE) {
  return { lat: base.lat + north / 111180.0, lon: base.lon + east / 75400.0 };
}
function e7(p) { return [Geo.toE7(p.lat), Geo.toE7(p.lon)]; }
function near(actual, expected, tolerance, message) {
  assert.ok(Math.abs(actual - expected) <= tolerance, `${message || ''} erwartet ${expected} ± ${tolerance}, war ${actual}`);
}

// ---------- Distanzen ----------

test('Distanz stimmt mit den Referenzwerten des Geraets ueberein', () => {
  // Dieselben Werte wie in test/test_geo (GeographicLib, WGS84).
  near(Geo.distance(47.3769, 8.5417, 47.37812, 8.54225), 141.8544, 0.01);
  near(Geo.distance(47.3769, 8.5417, 47.376922, 8.541730), 3.3340, 0.001);
  near(Geo.distance(0.0005, -78.45, -0.0008, -78.449), 181.8106, 0.01);
  near(Geo.distance(-33.89, 151.2, -33.8915, 151.202), 248.8165, 0.01);
  near(Geo.distance(-16.5, 179.9992, -16.5003, -179.9992), 174.0187, 0.01);
  assert.equal(Geo.distance(47, 8, 47, 8), 0);
});

test('Koordinaten in Zehnmillionstel Grad', () => {
  assert.equal(Geo.toE7(47.3769), 473769000);
  assert.equal(Geo.toE7(-8.5417), -85417000);
  assert.equal(Geo.fromE7(473769000), 47.3769);
  near(Geo.distanceE7([473769000, 85417000], [473781200, 85422500]), 141.8544, 0.01);
});

test('lokale Meter mit Vorzeichen', () => {
  const p = at(100, -50);
  const local = Geo.toLocal(p.lat, p.lon, BASE.lat, BASE.lon);
  near(local.north, 100, 0.2);
  near(local.east, -50, 0.2);
});

// ---------- OpenStreetMap: Abfragen ----------

test('Suchabfrage maskiert Sonderzeichen', () => {
  const q = Osm.courseSearchQuery('Golf "Club" St. Moritz (18)');
  assert.ok(q.includes('["leisure"="golf_course"]'));
  assert.ok(q.includes('["name"~"Golf Club St\\\\. Moritz \\\\(18\\\\)",i]'), q);
  assert.ok(!q.includes('"Club"'));
  assert.ok(q.startsWith('[out:json]'));
});

test('Bahnabfrage enthaelt Mittelpunkt und Radius', () => {
  const q = Osm.holesQuery(47.3769, 8.5417, 2000);
  assert.ok(q.includes('way["golf"="hole"](around:2000,47.376900,8.541700);'));
  assert.ok(q.includes('way["golf"="green"](around:2000,47.376900,8.541700);'));
  assert.ok(q.includes('out tags geom;'));
  assert.ok(Osm.holesQuery(47, 8).includes('around:2500'));
});

test('Trefferliste der Platzsuche', () => {
  const hits = Osm.parseCourseSearch({ elements: [
    { type: 'way', id: 1, center: { lat: 47.1, lon: 8.2 }, tags: { name: 'Golfclub A', 'addr:city': 'Ortikon' } },
    { type: 'node', id: 2, lat: 46.5, lon: 7.5, tags: { name: 'Golfclub B' } },
    { type: 'relation', id: 3, center: { lat: 46.0, lon: 7.0 }, tags: {} },   // ohne Namen
    { type: 'way', id: 4, tags: { name: 'Ohne Ort' } }                        // ohne Position
  ] });
  assert.deepEqual(hits, [
    { name: 'Golfclub A', place: 'Ortikon', lat: 47.1, lon: 8.2 },
    { name: 'Golfclub B', place: '', lat: 46.5, lon: 7.5 }
  ]);
  assert.deepEqual(Osm.parseCourseSearch({}), []);
  assert.deepEqual(Osm.parseCourseSearch(null), []);
});

test('Platzname wird auf 31 Bytes gekuerzt, auch mit Umlauten', () => {
  assert.equal(Osm.cleanName('  Golfclub\nBeispiel '), 'Golfclub Beispiel');
  const long = Osm.cleanName('Golf- und Landclub Zürich-Oberäägeri am See');
  assert.ok(new TextEncoder().encode(long).length <= 31);
  assert.ok(long.startsWith('Golf- und Landclub Zürich'));
  assert.equal(Osm.cleanName('ääääääääääääääääääää'), 'äääääääääääääää'); // 15 x 2 Bytes
  assert.equal(Osm.cleanName(null), '');
});

// ---------- OpenStreetMap: Platz bauen ----------

function holeWay(ref, par, points, extra = {}) {
  return { type: 'way', id: 100 + Number(ref || 0), tags: Object.assign({ golf: 'hole', ref: String(ref), par: String(par) }, extra), geometry: points };
}
// Rechteckiges Gruen um `center`, `depth` Meter in Nord-Sued-Richtung, `width` in Ost-West.
function greenArea(center, depth, width) {
  const c = (n, e) => ({ lat: center.lat + n / 111180.0, lon: center.lon + e / 75400.0 });
  const pts = [c(-depth / 2, -width / 2), c(-depth / 2, width / 2), c(depth / 2, width / 2), c(depth / 2, -width / 2)];
  pts.push(pts[0]);
  return { type: 'way', id: 900, tags: { golf: 'green' }, geometry: pts };
}

test('Platz mit drei Bahnen und Gruens', () => {
  const elements = [
    holeWay(1, 4, [at(0), at(200, 10), at(350)]),
    greenArea(at(352), 30, 20),
    holeWay(2, 3, [at(0, 200), at(150, 200)]),
    greenArea(at(148, 203), 18, 24),
    holeWay(3, 5, [at(0, 400), at(250, 420), at(480, 400)]),
    greenArea(at(485, 398), 36, 22)
  ];
  const result = Osm.courseFromOsm(elements, 'Testplatz', BASE);
  assert.equal(result.holesFound, 3);
  assert.equal(result.greensMatched, 3);
  const course = result.course;
  assert.equal(course.v, 1);
  assert.equal(course.name, 'Testplatz');
  assert.equal(course.holes.length, 9); // hoechste Nummer 3 -> 9-Loch-Platz
  assert.equal(course.holes[0].par, 4);
  assert.deepEqual(course.holes[0].tee, e7(at(0)));
  // Gruenmitte ist der Schwerpunkt der Flaeche, nicht das Ende der Linie.
  near(Geo.distanceE7(course.holes[0].green, e7(at(352))), 0, 0.05);
  // 30 m tief; die Bahn kommt leicht schraeg an, deshalb etwas mehr als 15 m.
  assert.equal(course.holes[0].r, 156);
  assert.equal(course.holes[1].r, 90); // gerade Bahn: 18 m tief -> 9 m = 90 dm
  assert.equal(course.holes[2].r, 189); // 36 m tief, wieder leicht schraeg
  near(Stats.holeLength(course.holes[0]), 352, 0.5);
  // Loecher 4 bis 9 fehlen in den Daten.
  assert.deepEqual(course.holes[3], { par: 0 });
  assert.ok(result.warnings.some(w => w.includes('Nicht erfasst: Loch 4, 5, 6, 7, 8, 9')));
});

test('Radius richtet sich nach der Spielrichtung', () => {
  // Bahn verlaeuft nach Osten; das Gruen ist in Ost-West-Richtung 40 m, in Nord-Sued 16 m.
  const elements = [holeWay(1, 4, [at(0, 0), at(0, 300)]), greenArea(at(0, 300), 16, 40)];
  const hole = Osm.courseFromOsm(elements, 'X', BASE).course.holes[0];
  near(hole.r, 200, 2);
});

test('Radius wird auf sinnvolle Werte begrenzt', () => {
  const tiny = Osm.courseFromOsm([holeWay(1, 3, [at(0), at(100)]), greenArea(at(100), 4, 4)], 'X', BASE).course.holes[0];
  assert.equal(tiny.r, 50);
  const huge = Osm.courseFromOsm([holeWay(1, 3, [at(0), at(100)]), greenArea(at(100), 110, 60)], 'X', BASE).course.holes[0];
  assert.equal(huge.r, 400);
});

test('Bahn ohne Gruenflaeche nimmt das Ende der Linie', () => {
  const result = Osm.courseFromOsm([holeWay(1, 4, [at(0), at(320)])], 'X', BASE);
  const hole = result.course.holes[0];
  assert.deepEqual(hole.green, e7(at(320)));
  assert.equal(hole.r, undefined); // das Geraet nimmt dann seinen Standardradius
  assert.equal(result.greensMatched, 0);
  assert.ok(result.warnings.some(w => w.includes('fehlt die Grünfläche')));
});

test('ein Gruen weit weg vom Bahnende wird nicht zugeordnet', () => {
  const result = Osm.courseFromOsm([holeWay(1, 4, [at(0), at(320)]), greenArea(at(420), 30, 20)], 'X', BASE);
  assert.deepEqual(result.course.holes[0].green, e7(at(320)));
});

test('verkehrt herum erfasste Bahn wird gedreht', () => {
  // Linie beginnt am Gruen und endet am Abschlag.
  const result = Osm.courseFromOsm([holeWay(1, 4, [at(350), at(150), at(0)]), greenArea(at(352), 30, 20)], 'X', BASE);
  const hole = result.course.holes[0];
  assert.deepEqual(hole.tee, e7(at(0)));
  near(Geo.distanceE7(hole.green, e7(at(352))), 0, 0.05);
  assert.equal(hole.r, 150);
});

test('18-Loch-Platz, fehlendes Par, doppelte und ungueltige Nummern', () => {
  const elements = [];
  for (let n = 1; n <= 18; n++) {
    if (n === 7) continue; // Loch 7 fehlt
    elements.push(holeWay(n, n === 5 ? 'x' : 4, [at(0, n * 200), at(300, n * 200)]));
  }
  elements.push(holeWay(3, 5, [at(5000, 0), at(5300, 0)]));          // zweites Loch 3, weit weg (Nachbarplatz)
  elements.push(holeWay('', 4, [at(0, 9000), at(300, 9000)]));       // ohne Nummer
  elements.push(holeWay(19, 4, [at(0, 9200), at(300, 9200)]));       // Nummer ausserhalb
  elements.push(holeWay('1A', 4, [at(0, 9400), at(300, 9400)]));
  elements.push({ type: 'way', tags: { golf: 'hole', ref: '2' }, geometry: [at(0)] }); // nur ein Punkt
  const result = Osm.courseFromOsm(elements, 'Gross', BASE);
  const course = result.course;
  assert.equal(course.holes.length, 18);
  assert.equal(result.holesFound, 17);
  assert.deepEqual(course.holes[6], { par: 0 });
  assert.equal(course.holes[4].par, 0);
  assert.equal(course.holes[2].par, 4); // das nahe Loch 3, nicht das vom Nachbarplatz
  const text = result.warnings.join(' | ');
  assert.ok(text.includes('Nicht erfasst: Loch 7.'), text);
  assert.ok(text.includes('Ohne Par: Loch 5.'), text);
  assert.ok(text.includes('Loch 3 gibt es mehrfach'), text);
  assert.ok(text.includes('3 Bahnen ohne gültige Lochnummer wurden übersprungen'), text);
});

test('keine Bahnen gefunden', () => {
  for (const elements of [[], null, [greenArea(at(100), 20, 20)], [{ type: 'way', tags: { golf: 'bunker' }, geometry: [at(0), at(1), at(2)] }]]) {
    const result = Osm.courseFromOsm(elements, 'Leer', BASE);
    assert.equal(result.course, null);
    assert.equal(result.holesFound, 0);
    assert.equal(result.warnings.length, 1);
  }
});

test('kaputte Geometrie fuehrt nicht zum Absturz', () => {
  const elements = [
    { type: 'way', tags: { golf: 'hole', ref: '1', par: '4' }, geometry: [null, { lat: 'x' }, at(0), at(300)] },
    { type: 'way', tags: { golf: 'green' } },
    { type: 'way' },
    {}
  ];
  const result = Osm.courseFromOsm(elements, 'X', BASE);
  assert.deepEqual(result.course.holes[0].tee, e7(at(0)));
});

test('erzeugter Platz haelt die Grenzen des Geraeteformats ein', () => {
  const elements = [];
  for (let n = 1; n <= 18; n++) {
    elements.push(holeWay(n, 3 + (n % 3), [at(0, n * 150), at(140 + n * 20, n * 150)]));
    elements.push(greenArea(at(142 + n * 20, n * 150), 10 + n * 3, 20));
  }
  const course = Osm.courseFromOsm(elements, 'Grenzen', BASE).course;
  assert.equal(course.holes.length, 18);
  for (const hole of course.holes) {
    assert.ok(hole.par === 0 || (hole.par >= 3 && hole.par <= 6));
    assert.ok(Number.isInteger(hole.green[0]) && Math.abs(hole.green[0]) <= 900000000);
    assert.ok(Number.isInteger(hole.green[1]) && Math.abs(hole.green[1]) <= 1800000000);
    assert.ok(Number.isInteger(hole.r) && hole.r >= 10 && hole.r <= 600);
  }
  assert.ok(JSON.stringify(course).length < 64 * 1024); // passt in eine Anfrage ans Geraet
});

// ---------- Auswertung ----------

function shot(north, club, east = 0) { return { p: e7(at(north, east)), c: club }; }
const ROUND = {
  v: 1, course: 'Testplatz', hole: 2, date: '2026-11-24T08:05',
  holes: [
    { par: 4, green: e7(at(350)), done: true, shots: [shot(0, 'Dr'), shot(210, 'E7'), shot(340, 'SW'), shot(350, 'Pt'), shot(350, 'Pt')] },
    { par: 3, green: e7(at(150, 200)), done: true, shots: [{ p: e7(at(0, 200)), c: 'E6' }, { c: 'Pt' }] },
    { par: 5, green: e7(at(480, 400)), done: false, shots: [{ p: e7(at(0, 400)), c: 'Dr' }, { p: e7(at(230, 400)) }] },
    { par: 4, done: false, shots: [] }
  ]
};

test('Schlagweiten eines Lochs', () => {
  const d = Stats.shotDistances(ROUND.holes[0]);
  near(d[0], 210, 0.3);
  near(d[1], 130, 0.3);
  near(d[2], 10, 0.3);
  assert.equal(d[3], null); // Putt
  assert.equal(d[4], null);
});

test('letzter Schlag misst bis zum Gruen, aber nur bei abgeschlossenem Loch', () => {
  // Loch 2: Abschlag, dann ein Putt ohne Position -> Weite des Abschlags unbekannt.
  assert.deepEqual(Stats.shotDistances(ROUND.holes[1]), [null, null]);
  const holedOut = { par: 3, green: e7(at(150)), done: true, shots: [shot(0, 'E6')] };
  near(Stats.shotDistances(holedOut)[0], 150, 0.3);
  const open = { par: 3, green: e7(at(150)), done: false, shots: [shot(0, 'E6')] };
  assert.deepEqual(Stats.shotDistances(open), [null]);
  const noGreen = { par: 3, done: true, shots: [shot(0, 'E6')] };
  assert.deepEqual(Stats.shotDistances(noGreen), [null]);
  assert.deepEqual(Stats.shotDistances({}), []);
});

test('Rundenergebnis zaehlt nur abgeschlossene Loecher', () => {
  const s = Stats.roundSummary(ROUND);
  assert.equal(s.holesPlayed, 2);
  assert.equal(s.strokes, 7);
  assert.equal(s.putts, 3);
  assert.equal(s.toPar, 0); // +1 und -1
  near(s.longest, 210, 0.3);
  assert.equal(s.perHole.length, 4);
  assert.deepEqual(s.perHole[0], { number: 1, par: 4, strokes: 5, putts: 2, done: true, diff: 1 });
  assert.deepEqual(s.perHole[2], { number: 3, par: 5, strokes: 2, putts: 0, done: false, diff: null });
});

test('Stand gegen Par nur, wenn jedes gespielte Loch ein Par hat', () => {
  const round = { holes: [{ par: 4, done: true, shots: [{ c: 'Dr' }, { c: 'Pt' }] }, { done: true, shots: [{ c: 'Dr' }] }] };
  const s = Stats.roundSummary(round);
  assert.equal(s.toPar, null);
  assert.equal(s.strokes, 3);
  assert.equal(Stats.roundSummary({ holes: [] }).toPar, null);
  assert.equal(Stats.roundSummary({}).holesPlayed, 0);
});

test('Darstellung des Stands gegen Par', () => {
  assert.equal(Stats.formatToPar(4), '+4');
  assert.equal(Stats.formatToPar(0), 'E');
  assert.equal(Stats.formatToPar(-2), '-2');
  assert.equal(Stats.formatToPar(null), '–');
  assert.equal(Stats.formatToPar(undefined), '–');
});

test('Schlaeger-Statistik ueber mehrere Runden', () => {
  const second = { holes: [{ par: 4, green: e7(at(350)), done: true, shots: [shot(0, 'Dr'), shot(190, 'E7'), shot(330, 'E7'), shot(350, 'Pt')] }] };
  const stats = Stats.clubStats([ROUND, second]);
  const byClub = Object.fromEntries(stats.map(s => [s.club, s]));
  assert.deepEqual(stats.map(s => s.club), ['Dr', 'E7', 'SW']); // Reihenfolge wie im Bag, ohne Putter
  assert.equal(byClub.Dr.count, 3); // 210, 230 (laufendes Loch 3) und 190
  near(byClub.Dr.average, 210, 0.3);
  near(byClub.Dr.max, 230, 0.3);
  assert.equal(byClub.Dr.name, 'Driver');
  assert.equal(byClub.E7.count, 3); // 130, 140, 20
  near(byClub.E7.average, (130 + 140 + 20) / 3, 0.3);
  assert.equal(byClub.Pt, undefined);
});

test('Schlaeger-Statistik laesst Messfehler und Schlaege ohne Schlaeger weg', () => {
  const round = { holes: [{ done: true, shots: [shot(0, 'Dr'), shot(5000, 'E7'), shot(5000.3, 'E7'), { p: e7(at(5100)) }, shot(5200, 'Pt')] }] };
  const stats = Stats.clubStats([round]);
  // 5 km "Drive" und 30 cm zaehlen nicht, der Schlag ohne Schlaeger auch nicht.
  // Uebrig bleibt ein Eisen 7 ueber rund 100 m.
  assert.equal(stats.length, 1);
  assert.equal(stats[0].club, 'E7');
  assert.equal(stats[0].count, 1);
  near(stats[0].average, 99.7, 0.3);
  assert.deepEqual(Stats.clubStats([]), []);
  assert.deepEqual(Stats.clubStats(null), []);
});

test('Datum in Ortszeit', () => {
  // 08:05 UTC ist im Winter 09:05 in der Schweiz, im Sommer 10:05.
  assert.equal(Stats.formatDate('2026-11-24T08:05', 'de-CH', 'Europe/Zurich'), '24.11.2026, 09:05');
  assert.equal(Stats.formatDate('2026-07-01T08:05', 'de-CH', 'Europe/Zurich'), '01.07.2026, 10:05');
  assert.equal(Stats.formatDate('2026-12-31T23:30', 'de-CH', 'Europe/Zurich'), '01.01.2027, 00:30');
  assert.equal(Stats.formatDate('', 'de-CH'), 'ohne Datum');
  assert.equal(Stats.formatDate(undefined), 'ohne Datum');
  assert.equal(Stats.formatDate('quatsch'), 'ohne Datum');
});

test('CSV-Export', () => {
  const lines = Stats.toCsv(ROUND).split('\r\n');
  assert.equal(lines[0], 'Loch;Par;Schlag;Schlaeger;Weite_m;Breite;Laenge');
  assert.equal(lines[1], '1;4;1;Driver;210;47.3769000;8.5417000');
  assert.equal(lines[4], '1;4;4;Putter;;47.3800480;8.5417000');
  assert.equal(lines[7], '2;3;2;Putter;;;');       // ohne Position
  assert.equal(lines[9], '3;5;2;–;;47.3789687;8.5470050'); // ohne Schlaeger
  assert.equal(lines.length, 11); // Kopf, 9 Schlaege, leere Zeile am Ende
  assert.equal(Stats.toCsv({ holes: [] }), 'Loch;Par;Schlag;Schlaeger;Weite_m;Breite;Laenge\r\n');
});

test('Schlaegernamen und Lochlaenge', () => {
  assert.equal(Stats.clubName('PW'), 'Pitching Wedge');
  assert.equal(Stats.clubName('??'), '??');
  assert.equal(Stats.clubName(undefined), '–');
  assert.equal(Stats.CLUBS.length, 11);
  near(Stats.holeLength({ tee: e7(at(0)), green: e7(at(342)) }), 342, 0.3);
  assert.equal(Stats.holeLength({ par: 4 }), null);
});
