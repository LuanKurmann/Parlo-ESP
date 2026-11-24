// Web-Oberflaeche des Golf-Trackers: holt Daten vom Geraet (/api/...) und
// zeigt Runden, Platz und Schlagweiten. Alle Texte werden als Text eingesetzt,
// nie als HTML, weil Platznamen aus fremder Quelle stammen.
(function () {
  'use strict';

  var Stats = window.GolfStats, Osm = window.GolfOsm;
  var state = { rounds: [], roundData: {}, bag: null, course: null, importCourse: null };

  // ---------- Hilfen ----------

  function $(id) { return document.getElementById(id); }

  // el('td', {class: 'x'}, 'Text' | [Kinder])
  function el(tag, attrs, children) {
    var node = document.createElement(tag);
    Object.keys(attrs || {}).forEach(function (key) {
      if (key === 'class') node.className = attrs[key];
      else if (key === 'onclick') node.addEventListener('click', attrs[key]);
      else node.setAttribute(key, attrs[key]);
    });
    (Array.isArray(children) ? children : [children]).forEach(function (child) {
      if (child === null || child === undefined) return;
      node.appendChild(typeof child === 'object' ? child : document.createTextNode(String(child)));
    });
    return node;
  }

  function replace(container, children) {
    container.textContent = '';
    (Array.isArray(children) ? children : [children]).forEach(function (c) { if (c) container.appendChild(c); });
  }

  var toastTimer = null;
  function toast(text, isError) {
    var box = $('toast');
    box.textContent = text;
    box.className = isError ? 'error' : '';
    box.hidden = false;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(function () { box.hidden = true; }, isError ? 6000 : 3500);
  }

  // Anfrage ans Geraet. Liefert das JSON der Antwort oder wirft einen Fehler mit lesbarem Text.
  function api(method, path, body) {
    var options = { method: method };
    if (body !== undefined) {
      options.body = typeof body === 'string' ? body : JSON.stringify(body);
      options.headers = { 'Content-Type': 'application/json' };
    }
    return fetch(path, options).then(function (response) {
      return response.json().catch(function () { return {}; }).then(function (data) {
        if (!response.ok) throw new Error(data.error || ('Fehler ' + response.status));
        return data;
      });
    }, function () {
      throw new Error('Das Gerät ist nicht erreichbar.');
    });
  }

  function download(name, text, type) {
    var url = URL.createObjectURL(new Blob([text], { type: type }));
    var link = el('a', { href: url, download: name });
    document.body.appendChild(link);
    link.click();
    link.remove();
    setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
  }

  // count(2, 'Schlag', 'Schläge') -> "2 Schläge"
  function count(n, one, many) { return n + ' ' + (n === 1 ? one : many); }

  function toneClass(diff) { return diff === null || diff === undefined || diff === 0 ? '' : (diff < 0 ? 'under' : 'over'); }
  function meters(value) { return value === null || value === undefined ? '–' : Math.round(value) + ' m'; }

  // `extraClass` z. B. 'text-second', wenn die zweite Spalte Text statt Zahlen enthaelt.
  function table(head, rows, foot, extraClass) {
    var t = el('table', { class: extraClass || '' }, [
      el('thead', {}, el('tr', {}, head.map(function (h) { return el('th', { scope: 'col' }, h); }))),
      el('tbody', {}, rows)
    ]);
    if (foot) t.appendChild(el('tfoot', {}, foot));
    return el('div', { class: 'table-wrap' }, t);
  }

  // ---------- Tabs ----------

  function showTab(name) {
    ['rounds', 'course', 'clubs'].forEach(function (tab) {
      $('tab-' + tab).hidden = tab !== name;
      document.querySelector('[data-tab="' + tab + '"]').setAttribute('aria-pressed', String(tab === name));
    });
  }

  // ---------- Runden ----------

  function renderCurrentRound(round) {
    var s = Stats.roundSummary(round);
    var started = round.holes.some(function (h) { return (h.shots || []).length > 0; });
    if (!started) {
      replace($('current-round'), el('p', { class: 'muted' }, 'Noch kein Schlag erfasst. Auf dem Gerät Knopf 1 drücken, um zu beginnen.'));
      return;
    }
    var onThisHole = ((round.holes[round.hole] || {}).shots || []).length;
    var progress = s.holesPlayed === 0
      ? count(onThisHole, 'Schlag', 'Schläge') + ' auf diesem Loch'
      : count(s.holesPlayed, 'Loch', 'Löcher') + ' abgeschlossen: ' + count(s.strokes, 'Schlag', 'Schläge') + ', ' +
        Stats.formatToPar(s.toPar) + ' gegen Par, ' + count(s.putts, 'Putt', 'Putts');
    replace($('current-round'), [
      el('p', {}, [el('strong', {}, round.course || 'Platz ohne Namen'), ' · Loch ' + (round.hole + 1)]),
      el('p', { class: 'muted' }, progress),
      el('button', { type: 'button', onclick: function () { showRound(round, null); } }, 'Ansehen')
    ]);
  }

  function renderRoundList() {
    if (state.rounds.length === 0) {
      replace($('round-list'), el('p', { class: 'muted' }, 'Noch keine abgeschlossene Runde. Eine Runde wandert hierher, sobald du auf dem Gerät eine neue beginnst.'));
      return;
    }
    replace($('round-list'), el('div', { class: 'list' }, state.rounds.map(function (r) {
      return el('button', { type: 'button', class: 'list-item', 'data-round': r.n, onclick: function () { openRound(r.n); } }, [
        el('span', { class: 'grow' }, [
          el('span', { class: 'title' }, r.course || 'Platz ohne Namen'),
          el('br'),
          el('span', { class: 'muted small' }, Stats.formatDate(r.date) + ' · ' + count(r.holes, 'Loch', 'Löcher') + ' · ' + count(r.putts, 'Putt', 'Putts'))
        ]),
        el('span', { class: 'score' }, [
          String(r.strokes), ' ',
          el('span', { class: 'small ' + toneClass(r.to_par) }, r.to_par === undefined ? '' : Stats.formatToPar(r.to_par))
        ])
      ]);
    })));
  }

  function renderClubStats() {
    var rounds = state.rounds.map(function (r) { return state.roundData[r.n]; }).filter(Boolean);
    var stats = Stats.clubStats(rounds);
    if (stats.length === 0) {
      replace($('club-stats'), el('p', { class: 'muted' }, 'Sobald Runden mit GPS-Positionen vorliegen, stehen hier Durchschnitt und Bestweite pro Schläger.'));
      return;
    }
    replace($('club-stats'), table(['Schläger', 'Schläge', 'Ø', 'Beste'], stats.map(function (s) {
      return el('tr', {}, [el('td', {}, s.name), el('td', {}, s.count), el('td', {}, meters(s.average)), el('td', {}, meters(s.max))]);
    })));
    renderClubInputs(); // die gemessenen Werte erscheinen dort als Hinweis
  }

  function openRound(number) {
    var cached = state.roundData[number];
    (cached ? Promise.resolve(cached) : api('GET', '/api/rounds/' + number)).then(function (round) {
      state.roundData[number] = round;
      showRound(round, number);
    }).catch(function (e) { toast(e.message, true); });
  }

  // `number` ist die Archivnummer, null bei der laufenden Runde.
  function showRound(round, number) {
    var s = Stats.roundSummary(round);
    var fileBase = 'runde-' + (number === null ? 'laufend' : number);

    var scoreRows = s.perHole.filter(function (h) { return h.strokes > 0; }).map(function (h) {
      return el('tr', {}, [
        el('td', {}, h.number + (h.done ? '' : ' (läuft)')),
        el('td', {}, h.par || '–'),
        el('td', { class: toneClass(h.diff) }, h.strokes),
        el('td', {}, h.putts)
      ]);
    });
    var holeDetails = (round.holes || []).map(function (hole, index) {
      if (!(hole.shots || []).length) return null;
      var distances = Stats.shotDistances(hole);
      return el('details', {}, [
        el('summary', {}, 'Loch ' + (index + 1) + ': ' + count(hole.shots.length, 'Schlag', 'Schläge')),
        table(['Schlag', 'Schläger', 'Weite'], hole.shots.map(function (shot, i) {
          return el('tr', {}, [el('td', {}, i + 1), el('td', {}, Stats.clubName(shot.c)), el('td', {}, shot.c === 'Pt' ? 'Putt' : meters(distances[i]))]);
        }), null, 'text-second')
      ]);
    });

    var deleteButton = null;
    if (number !== null) {
      deleteButton = el('button', { type: 'button', class: 'danger', id: 'round-delete' }, 'Runde löschen');
      deleteButton.addEventListener('click', function () {
        if (deleteButton.dataset.armed !== '1') { // zweiter Tipp bestaetigt
          deleteButton.dataset.armed = '1';
          deleteButton.textContent = 'Wirklich löschen? Nochmals tippen';
          return;
        }
        api('DELETE', '/api/rounds/' + number).then(function () {
          delete state.roundData[number];
          toast('Runde gelöscht');
          closeRound();
          return loadRounds();
        }).catch(function (e) { toast(e.message, true); });
      });
    }

    replace($('round-detail'), [
      el('button', { type: 'button', id: 'round-back', onclick: closeRound }, '← Zurück zur Übersicht'),
      el('h2', {}, number === null ? 'Laufende Runde' : 'Runde ' + number),
      el('p', {}, [el('strong', {}, round.course || 'Platz ohne Namen'), el('br'), el('span', { class: 'muted' }, Stats.formatDate(round.date))]),
      el('div', { class: 'tiles' }, [
        tile(s.strokes, 'Schläge'),
        tile(Stats.formatToPar(s.toPar), 'gegen Par', toneClass(s.toPar)),
        tile(s.putts, 'Putts'),
        tile(s.longest ? meters(s.longest) : '–', 'längster Schlag')
      ]),
      el('h2', {}, 'Scorecard'),
      scoreRows.length ? table(['Loch', 'Par', 'Schläge', 'Putts'], scoreRows, el('tr', {}, [
        el('td', {}, 'Gesamt'), el('td', {}, ''), el('td', { class: toneClass(s.toPar) }, s.strokes), el('td', {}, s.putts)
      ])) : el('p', { class: 'muted' }, 'Keine Schläge erfasst.'),
      el('h2', {}, 'Schläge pro Loch'),
      el('div', {}, holeDetails),
      el('h2', {}, 'Exportieren'),
      el('div', { class: 'row' }, [
        el('button', { type: 'button', id: 'export-csv', onclick: function () { download(fileBase + '.csv', Stats.toCsv(round), 'text/csv'); } }, 'Als Tabelle (CSV)'),
        el('button', { type: 'button', id: 'export-json', onclick: function () { download(fileBase + '.json', JSON.stringify(round), 'application/json'); } }, 'Als Datei (JSON)'),
        deleteButton
      ])
    ]);
    $('rounds-overview').hidden = true;
    $('round-detail').hidden = false;
    window.scrollTo(0, 0);
  }

  function tile(value, name, tone) {
    return el('div', { class: 'tile' }, [el('div', { class: 'value ' + (tone || '') }, value), el('div', { class: 'name' }, name)]);
  }

  function closeRound() {
    $('round-detail').hidden = true;
    $('rounds-overview').hidden = false;
  }

  function loadRounds() {
    return Promise.all([api('GET', '/api/round'), api('GET', '/api/rounds')]).then(function (results) {
      renderCurrentRound(results[0]);
      state.rounds = results[1].rounds || [];
      renderRoundList();
      // Fuer die Schlaeger-Statistik alle Runden nachladen, eine nach der anderen.
      return state.rounds.reduce(function (chain, r) {
        return chain.then(function () {
          if (state.roundData[r.n]) return null;
          return api('GET', '/api/rounds/' + r.n).then(function (round) { state.roundData[r.n] = round; }, function () {});
        });
      }, Promise.resolve()).then(renderClubStats);
    });
  }

  // ---------- Platz ----------

  function courseTable(course) {
    return table(['Loch', 'Par', 'Länge', 'Grün'], course.holes.map(function (hole, index) {
      return el('tr', {}, [
        el('td', {}, index + 1), el('td', {}, hole.par || '–'), el('td', {}, meters(Stats.holeLength(hole))),
        el('td', {}, hole.green ? 'bekannt' : 'fehlt')
      ]);
    }));
  }

  function renderCourse() {
    var course = state.course;
    var known = course.holes.filter(function (h) { return h.green; }).length;
    replace($('course-current'), [
      el('p', {}, [el('strong', { id: 'course-name' }, course.name || 'Platz ohne Namen'), el('br'),
        el('span', { class: 'muted' }, count(course.holes.length, 'Loch', 'Löcher') + ', bei ' + known + ' ist das Grün bekannt')]),
      el('details', {}, [el('summary', {}, 'Löcher anzeigen'), courseTable(course)])
    ]);
  }

  function loadCourse() {
    return api('GET', '/api/course').then(function (course) { state.course = course; renderCourse(); });
  }

  function overpass(query) {
    return fetch(Osm.OVERPASS_URL, { method: 'POST', body: new URLSearchParams({ data: query }) }).then(function (response) {
      if (response.status === 429 || response.status === 504) throw new Error('OpenStreetMap ist gerade überlastet. Bitte in einer Minute nochmals versuchen.');
      if (!response.ok) throw new Error('OpenStreetMap antwortet mit Fehler ' + response.status + '.');
      return response.json();
    }, function () {
      throw new Error('OpenStreetMap ist nicht erreichbar. Hat das Handy Internet?');
    });
  }

  function searchCourses(name) {
    $('import-preview').hidden = true;
    replace($('search-results'), el('p', { class: 'muted' }, 'Suche …'));
    overpass(Osm.courseSearchQuery(name)).then(function (response) {
      var hits = Osm.parseCourseSearch(response);
      if (hits.length === 0) {
        replace($('search-results'), el('p', { class: 'muted' }, 'Kein Golfplatz mit diesem Namen gefunden. Versuche einen kürzeren Suchbegriff, zum Beispiel nur den Ort.'));
        return;
      }
      replace($('search-results'), el('div', { class: 'list' }, hits.map(function (hit) {
        return el('button', { type: 'button', class: 'list-item', onclick: function () { loadHoles(hit); } }, [
          el('span', { class: 'grow' }, [el('span', { class: 'title' }, hit.name), hit.place ? el('br') : null,
            hit.place ? el('span', { class: 'muted small' }, hit.place) : null]),
          el('span', { class: 'muted' }, 'Laden')
        ]);
      })));
    }).catch(function (e) { replace($('search-results'), el('p', { class: 'error' }, e.message)); });
  }

  function loadHoles(hit) {
    var preview = $('import-preview');
    preview.hidden = false;
    replace(preview, el('p', { class: 'muted' }, 'Lade Spielbahnen von „' + hit.name + '“ …'));
    overpass(Osm.holesQuery(hit.lat, hit.lon)).then(function (response) {
      showImportPreview(Osm.courseFromOsm(response.elements, hit.name, hit));
    }).catch(function (e) { replace(preview, el('p', { class: 'error' }, e.message)); });
  }

  function showImportPreview(result) {
    var preview = $('import-preview');
    preview.hidden = false;
    var warnings = result.warnings.length ? el('div', { class: 'notice' }, el('ul', {}, result.warnings.map(function (w) { return el('li', {}, w); }))) : null;
    if (!result.course) {
      state.importCourse = null;
      replace(preview, warnings);
      return;
    }
    state.importCourse = result.course;
    replace(preview, [
      el('h2', {}, 'Vorschau'),
      el('p', {}, [el('strong', {}, result.course.name), el('br'),
        el('span', { class: 'muted' }, result.holesFound + ' von ' + result.course.holes.length + ' Löchern gefunden, ' + result.greensMatched + ' mit Grünfläche')]),
      warnings,
      courseTable(result.course),
      el('p', { class: 'muted small' }, 'Der Platz auf dem Gerät wird ersetzt. Eine laufende Runde wird vorher ins Archiv gelegt.'),
      el('button', { type: 'button', class: 'primary', id: 'import-send', onclick: function () { sendCourse(result.course); } }, 'Auf das Gerät laden')
    ]);
  }

  function sendCourse(course) {
    api('PUT', '/api/course', course).then(function (answer) {
      toast(answer.archived_round ? 'Platz geladen. Die laufende Runde liegt jetzt im Archiv.' : 'Platz geladen.');
      $('import-preview').hidden = true;
      replace($('search-results'), []);
      return Promise.all([loadCourse(), loadRounds(), loadStatus()]);
    }).catch(function (e) { toast(e.message, true); });
  }

  // ---------- Schlaeger ----------

  function renderClubInputs() {
    if (!state.bag) return;
    var measured = {};
    Stats.clubStats(state.rounds.map(function (r) { return state.roundData[r.n]; }).filter(Boolean)).forEach(function (s) { measured[s.club] = s; });
    replace($('club-inputs'), Stats.CLUBS.filter(function (c) { return c[0] !== 'Pt'; }).map(function (c) {
      var hint = measured[c[0]] ? 'gemessen Ø ' + meters(measured[c[0]].average) + ' (' + count(measured[c[0]].count, 'Schlag', 'Schläge') + ')' : '';
      var current = document.getElementById('carry-' + c[0]);
      return el('div', { class: 'club-row' }, [
        el('label', { for: 'carry-' + c[0] }, [c[1], hint ? el('br') : null, hint ? el('span', { class: 'muted small' }, hint) : null]),
        el('input', { id: 'carry-' + c[0], type: 'number', inputmode: 'numeric', min: '1', max: '400', step: '1', required: '',
          value: current ? current.value : state.bag.carry[c[0]] }),
        el('span', { class: 'unit' }, 'm')
      ]);
    }));
  }

  function loadBag() {
    return api('GET', '/api/settings').then(function (bag) { state.bag = bag; renderClubInputs(); });
  }

  function saveBag() {
    var carry = {};
    Stats.CLUBS.forEach(function (c) {
      if (c[0] !== 'Pt') carry[c[0]] = parseInt($('carry-' + c[0]).value, 10);
    });
    api('PUT', '/api/settings', { v: 1, carry: carry }).then(function () {
      state.bag = { v: 1, carry: carry };
      toast('Schlagweiten gespeichert');
    }).catch(function (e) { toast(e.message, true); });
  }

  // ---------- Start ----------

  function loadStatus() {
    return api('GET', '/api/status').then(function (status) {
      $('status').textContent = 'Verbunden · Version ' + status.version + (status.storage_ok ? '' : ' · Achtung: Speicher meldet Fehler');
    });
  }

  document.querySelectorAll('[data-tab]').forEach(function (button) {
    button.addEventListener('click', function () { showTab(button.dataset.tab); });
  });
  $('search-form').addEventListener('submit', function (event) {
    event.preventDefault();
    var name = $('search-name').value.trim();
    if (name.length >= 3) searchCourses(name);
  });
  $('clubs-form').addEventListener('submit', function (event) { event.preventDefault(); saveBag(); });
  $('course-export').addEventListener('click', function () {
    if (state.course) download('platz.json', JSON.stringify(state.course), 'application/json');
  });
  $('course-file').addEventListener('change', function (event) {
    var file = event.target.files[0];
    if (!file) return;
    file.text().then(function (text) {
      var course;
      try { course = JSON.parse(text); } catch (e) { course = null; }
      if (!course || !Array.isArray(course.holes)) { toast('Das ist keine Platzdatei.', true); return; }
      showImportPreview({ course: course, warnings: [], holesFound: course.holes.filter(function (h) { return h.green; }).length,
        greensMatched: course.holes.filter(function (h) { return h.green; }).length });
    });
    event.target.value = '';
  });

  Promise.all([loadStatus(), loadCourse(), loadBag(), loadRounds()]).catch(function (e) {
    $('status').textContent = e.message;
    $('status').className = 'error';
  });
})();
