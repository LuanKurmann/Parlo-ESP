// Auswertung von Runden im Format des Geraets (siehe lib/core/src/core/serialize.cpp).
(function (root) {
  'use strict';

  var Geo = typeof require !== 'undefined' ? require('./geo.js') : root.GolfGeo;

  // Alle Schlaeger, die das Geraet kennt, in seiner Reihenfolge (lib/core/src/core/clubs.cpp).
  var CLUBS = [
    ['Dr', 'Driver'], ['H3', 'Holz 3'], ['H5', 'Holz 5'], ['H7', 'Holz 7'], ['Hy', 'Hybrid'], ['E3', 'Eisen 3'],
    ['E4', 'Eisen 4'], ['E5', 'Eisen 5'], ['E6', 'Eisen 6'], ['E7', 'Eisen 7'], ['E8', 'Eisen 8'], ['E9', 'Eisen 9'],
    ['PW', 'Pitching Wedge'], ['GW', 'Gap Wedge'], ['SW', 'Sand Wedge'], ['LW', 'Lob Wedge'], ['Pt', 'Putter']
  ];
  var MAX_BAG_CLUBS = 14; // mit Putter
  var MAX_PLAUSIBLE_M = 400; // laengere "Schlaege" sind Messfehler

  function clubName(short) {
    for (var i = 0; i < CLUBS.length; i++) if (CLUBS[i][0] === short) return CLUBS[i][1];
    return short || '–';
  }

  // Weite jedes Schlags eines Lochs in Metern (null, wenn unbekannt): bis zum
  // naechsten Schlag, beim letzten Schlag eines abgeschlossenen Lochs bis zur
  // Gruenmitte. Putts haben keine Weite.
  function shotDistances(hole) {
    var shots = hole.shots || [];
    return shots.map(function (shot, i) {
      if (shot.c === 'Pt' || !shot.p) return null;
      var end = null;
      if (i + 1 < shots.length) end = shots[i + 1].p || null;
      else if (hole.done && hole.green) end = hole.green;
      return end ? Geo.distanceE7(shot.p, end) : null;
    });
  }

  // Ergebnis einer Runde. Gezaehlt werden wie auf dem Geraet nur abgeschlossene Loecher.
  function roundSummary(round) {
    var summary = { holesPlayed: 0, strokes: 0, putts: 0, toPar: null, longest: 0, perHole: [] };
    var parKnown = 0, diffSum = 0;
    (round.holes || []).forEach(function (hole, index) {
      var shots = hole.shots || [];
      var putts = shots.filter(function (s) { return s.c === 'Pt'; }).length;
      var par = hole.par || 0;
      var entry = {
        number: index + 1, par: par, strokes: shots.length, putts: putts, done: !!hole.done,
        diff: hole.done && par ? shots.length - par : null
      };
      summary.perHole.push(entry);
      if (!hole.done) return;
      summary.holesPlayed++;
      summary.strokes += shots.length;
      summary.putts += putts;
      if (par) { parKnown++; diffSum += shots.length - par; }
      shotDistances(hole).forEach(function (d) {
        if (d !== null && d <= MAX_PLAUSIBLE_M && d > summary.longest) summary.longest = d;
      });
    });
    if (summary.holesPlayed > 0 && parKnown === summary.holesPlayed) summary.toPar = diffSum;
    return summary;
  }

  // "+4", "E", "-2" oder "–" ohne Par.
  function formatToPar(value) {
    if (value === null || value === undefined) return '–';
    return value === 0 ? 'E' : (value > 0 ? '+' + value : String(value));
  }

  // Durchschnitt, Maximum und Anzahl pro Schlaeger ueber mehrere Runden.
  function clubStats(rounds) {
    var byClub = {};
    (rounds || []).forEach(function (round) {
      (round.holes || []).forEach(function (hole) {
        var distances = shotDistances(hole);
        (hole.shots || []).forEach(function (shot, i) {
          var d = distances[i];
          if (d === null || d < 1 || d > MAX_PLAUSIBLE_M || !shot.c) return;
          var entry = byClub[shot.c] || (byClub[shot.c] = { sum: 0, count: 0, max: 0 });
          entry.sum += d;
          entry.count++;
          if (d > entry.max) entry.max = d;
        });
      });
    });
    return CLUBS.filter(function (c) { return byClub[c[0]]; }).map(function (c) {
      var e = byClub[c[0]];
      return { club: c[0], name: c[1], count: e.count, average: e.sum / e.count, max: e.max };
    });
  }

  // Die Startzeit einer Runde ist UTC (vom GPS). Anzeige in der Ortszeit des Handys.
  function formatDate(text, locale, timeZone) {
    if (!/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}$/.test(text || '')) return 'ohne Datum';
    var date = new Date(text + ':00Z');
    if (isNaN(date.getTime())) return 'ohne Datum';
    var options = { day: '2-digit', month: '2-digit', year: 'numeric', hour: '2-digit', minute: '2-digit' };
    if (timeZone) options.timeZone = timeZone;
    return date.toLocaleString(locale || 'de-CH', options);
  }

  // Alle Schlaege einer Runde als CSV (Semikolon, wie es Excel in der Schweiz erwartet).
  function toCsv(round) {
    var lines = ['Loch;Par;Schlag;Schlaeger;Weite_m;Breite;Laenge'];
    (round.holes || []).forEach(function (hole, h) {
      var distances = shotDistances(hole);
      (hole.shots || []).forEach(function (shot, i) {
        lines.push([
          h + 1,
          hole.par || '',
          i + 1,
          clubName(shot.c),
          distances[i] === null ? '' : Math.round(distances[i]),
          shot.p ? Geo.fromE7(shot.p[0]).toFixed(7) : '',
          shot.p ? Geo.fromE7(shot.p[1]).toFixed(7) : ''
        ].join(';'));
      });
    });
    return lines.join('\r\n') + '\r\n';
  }

  // Laenge eines Lochs im Platzformat (Abschlag bis Gruenmitte), null wenn unbekannt.
  function holeLength(hole) {
    return hole.tee && hole.green ? Geo.distanceE7(hole.tee, hole.green) : null;
  }

  var api = {
    CLUBS: CLUBS, MAX_BAG_CLUBS: MAX_BAG_CLUBS, clubName: clubName, shotDistances: shotDistances, roundSummary: roundSummary,
    formatToPar: formatToPar, clubStats: clubStats, formatDate: formatDate, toCsv: toCsv, holeLength: holeLength
  };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  root.GolfStats = api;
})(typeof window !== 'undefined' ? window : globalThis);
