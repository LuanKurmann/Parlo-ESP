// Browser-Tests der Web-Oberflaeche gegen den Simulator:  node --test web/test/*.test.js
//
// Der Simulator laeuft als lokaler Webserver mit einem frischen Datenordner,
// ein Chromium in Handygroesse bedient die Seite. OpenStreetMap wird durch
// feste Antworten ersetzt, damit der Test kein Internet braucht.
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const { spawn } = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { chromium } = require('playwright');

const ROOT = path.resolve(__dirname, '..', '..');
const SIMULATOR = path.join(ROOT, '.pio', 'build', 'simulator', 'program');
const SHOTS_DIR = process.env.WEB_SHOTS_DIR || '';

// Zwei Runden ins Archiv spielen (Loch 1: Bogey mit 2 Putts; dann Loch 1: Birdie)
// und in der dritten Runde einen Schlag stehen lassen.
const PLAY = [
  'b1,b1,pos:210,b1,b1,pos:330,b1,click:275:178,pos:336,b1,b1,pos:341,b1,b1,b2,b2', // Runde 1, Loch 1: 5 Schlaege
  'B2,b2,b2,b1,B1',                                                                  // Runde beenden, neue Runde
  'b1,b1,pos:230,b1,b1,pos:338,b1,b1,b2,b2',                                         // Runde 2, Loch 1: 3 Schlaege
  'B2,b2,b2,b1,B1',
  'b1,b1'                                                                            // Runde 3 laeuft
].join(',');

let server, browser, baseUrl, dataDir;

async function startSimulator() {
  dataDir = fs.mkdtempSync(path.join(os.tmpdir(), 'golf-web-'));
  const port = 20000 + Math.floor(Math.random() * 20000);
  server = spawn(SIMULATOR, ['--headless', '--data', dataDir, '--serve', String(port), '--web', path.join(ROOT, 'web'), '--do', PLAY],
    { stdio: ['ignore', 'pipe', 'inherit'] });
  baseUrl = `http://127.0.0.1:${port}`;
  for (let attempt = 0; attempt < 100; attempt++) {
    try {
      const response = await fetch(baseUrl + '/api/status');
      const status = await response.json();
      if (status.rounds === 2) return; // die Befehle sind durchgelaufen
    } catch (e) { /* Server noch nicht bereit */ }
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  throw new Error('Simulator-Server wurde nicht bereit');
}

async function getJson(pathname) {
  return (await fetch(baseUrl + pathname)).json();
}

async function newPage() {
  // Groesse eines ueblichen Android-Handys.
  const context = await browser.newContext({ viewport: { width: 412, height: 915 }, locale: 'de-CH', timezoneId: 'Europe/Zurich', acceptDownloads: true });
  const page = await context.newPage();
  const errors = [];
  page.on('pageerror', error => errors.push(String(error)));
  page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
  page.errors = errors;
  return page;
}

async function shot(page, name) {
  if (SHOTS_DIR) await page.screenshot({ path: path.join(SHOTS_DIR, name + '.png'), fullPage: true });
}

async function noSidewaysScroll(page) {
  const overflow = await page.evaluate(() => document.documentElement.scrollWidth - window.innerWidth);
  assert.ok(overflow <= 0, `Seite ist ${overflow} px breiter als der Bildschirm`);
}

test.before(async () => {
  assert.ok(fs.existsSync(SIMULATOR), 'Simulator fehlt. Zuerst bauen: pio run -e simulator');
  await startSimulator();
  browser = await chromium.launch();
});

test.after(async () => {
  if (browser) await browser.close();
  if (server) server.kill();
  if (dataDir) fs.rmSync(dataDir, { recursive: true, force: true });
});

test('Uebersicht zeigt laufende Runde, Archiv und Schlagweiten', async () => {
  const page = await newPage();
  await page.goto(baseUrl + '/');
  await page.waitForSelector('#round-list .list-item');
  assert.match(await page.textContent('#status'), /Verbunden · Version 0\.1\.0/);
  assert.match(await page.textContent('#current-round'), /Beispielplatz · Loch 1/);

  const items = page.locator('#round-list .list-item');
  assert.equal(await items.count(), 2);
  // Neueste zuerst: Runde 2 (3 Schlaege, -1), dann Runde 1 (5 Schlaege, +1).
  assert.match(await items.nth(0).textContent(), /Beispielplatz/);
  assert.match(await items.nth(0).locator('.score').textContent(), /^3\s*-1$/);
  assert.match(await items.nth(1).locator('.score').textContent(), /^5\s*\+1$/);
  assert.match(await items.nth(1).textContent(), /1 Loch · 2 Putts/);
  assert.match(await items.nth(0).textContent(), /1 Loch · 1 Putt3/); // "1 Putt", nicht "1 Putts"; danach folgt der Score
  assert.match(await page.textContent('#current-round'), /1 Schlag auf diesem Loch/);

  // Schlaeger-Statistik aus beiden Runden: Driver 210 m und 230 m.
  await page.waitForSelector('#club-stats table');
  const driver = page.locator('#club-stats tbody tr', { hasText: 'Driver' });
  assert.deepEqual(await driver.locator('td').allTextContents(), ['Driver', '2', '220 m', '230 m']);

  await noSidewaysScroll(page);
  await shot(page, '1-uebersicht');
  assert.deepEqual(page.errors, []);
  await page.context().close();
});

test('Rundendetail stimmt mit dem Geraet ueberein und laesst sich exportieren', async () => {
  const page = await newPage();
  await page.goto(baseUrl + '/');
  await page.click('#round-list .list-item[data-round="1"]');
  await page.waitForSelector('#round-detail .tiles');

  // Die Seite rechnet selbst; das Ergebnis muss dem des Geraets entsprechen.
  const fromDevice = (await getJson('/api/rounds')).rounds.find(r => r.n === 1);
  const tiles = await page.locator('#round-detail .tile .value').allTextContents();
  assert.equal(tiles[0], String(fromDevice.strokes));
  assert.equal(tiles[1], '+' + fromDevice.to_par);
  assert.equal(tiles[2], String(fromDevice.putts));
  assert.equal(tiles[3], '210 m');

  assert.deepEqual(await page.locator('#round-detail table').first().locator('tbody tr td').allTextContents(), ['1', '4', '5', '2']);
  assert.ok((await page.locator('#round-detail table').first().locator('tbody td.over').count()) === 1);

  await page.click('#round-detail details summary');
  const shots = await page.locator('#round-detail details tbody tr').allTextContents();
  assert.equal(shots.length, 5);
  assert.match(shots[0], /1\s*Driver\s*210 m/);
  assert.match(shots[1], /2\s*Eisen 8\s*120 m/); // 132 m bis zur Mitte: Eisen 8 liegt am naechsten
  assert.match(shots[2], /3\s*Hybrid\s*6 m/); // per Touch gewaehlter Schlaeger
  assert.match(shots[3], /4\s*Putter\s*Putt/);

  const [csv] = await Promise.all([page.waitForEvent('download'), page.click('#export-csv')]);
  assert.equal(csv.suggestedFilename(), 'runde-1.csv');
  const csvText = fs.readFileSync(await csv.path(), 'utf8');
  assert.ok(csvText.startsWith('Loch;Par;Schlag;Schlaeger;Weite_m;Breite;Laenge\r\n1;4;1;Driver;210;47.3769000;8.5417000\r\n'));
  assert.equal(csvText.trim().split('\r\n').length, 6);

  const [json] = await Promise.all([page.waitForEvent('download'), page.click('#export-json')]);
  assert.equal(json.suggestedFilename(), 'runde-1.json');
  assert.deepEqual(JSON.parse(fs.readFileSync(await json.path(), 'utf8')), await getJson('/api/rounds/1'));

  await noSidewaysScroll(page);
  await shot(page, '2-runde');
  await page.click('#round-back');
  assert.ok(await page.isVisible('#round-list'));
  assert.deepEqual(page.errors, []);
  await page.context().close();
});

test('Schlagweiten aendern wirkt auf dem Geraet', async () => {
  const page = await newPage();
  await page.goto(baseUrl + '/');
  await page.click('[data-tab="clubs"]');
  await page.waitForSelector('#carry-Dr');
  assert.equal(await page.inputValue('#carry-Dr'), '210');
  assert.equal(await page.locator('#club-inputs input').count(), 10); // ohne Putter
  await page.waitForFunction(() => document.querySelector('#club-inputs').textContent.includes('gemessen'));
  assert.match(await page.textContent('#club-inputs'), /Driver\s*gemessen Ø 220 m \(2 Schläge\)/);

  await page.fill('#carry-Dr', '195');
  await page.fill('#carry-E7', '128');
  await page.click('#clubs-form button[type="submit"]');
  await page.waitForFunction(() => document.querySelector('#toast').textContent.includes('gespeichert'));
  const settings = await getJson('/api/settings');
  assert.equal(settings.carry.Dr, 195);
  assert.equal(settings.carry.E7, 128);
  assert.equal(settings.carry.SW, 70);

  // Ungueltige Eingabe schickt der Browser gar nicht erst ab.
  await page.fill('#carry-Dr', '900');
  await page.click('#clubs-form button[type="submit"]');
  await page.waitForTimeout(300);
  assert.equal((await getJson('/api/settings')).carry.Dr, 195);

  await noSidewaysScroll(page);
  await shot(page, '3-schlaeger');
  assert.deepEqual(page.errors, []);
  await page.context().close();
});

// Feste Antworten statt OpenStreetMap: ein Platz mit drei erfassten Bahnen.
function osmPoint(north, east) { return { lat: 46.8 + north / 111180.0, lon: 8.2 + east / 75400.0 }; }
function osmGreen(north, east) {
  const corner = (n, e) => osmPoint(north + n, east + e);
  const pts = [corner(-12, -9), corner(-12, 9), corner(12, 9), corner(12, -9)];
  return { type: 'way', id: 1, tags: { golf: 'green' }, geometry: pts.concat([pts[0]]) };
}
const OSM_SEARCH = { elements: [
  { type: 'way', id: 7, center: { lat: 46.8, lon: 8.2 }, tags: { leisure: 'golf_course', name: 'Golfclub Musterwiese', 'addr:city': 'Musterdorf' } },
  { type: 'relation', id: 8, center: { lat: 47.5, lon: 9.1 }, tags: { leisure: 'golf_course', name: 'Golfpark Musterau' } }
] };
const OSM_HOLES = { elements: [
  { type: 'way', id: 11, tags: { golf: 'hole', ref: '1', par: '4' }, geometry: [osmPoint(0, 0), osmPoint(310, 0)] },
  osmGreen(312, 0),
  { type: 'way', id: 12, tags: { golf: 'hole', ref: '2', par: '3' }, geometry: [osmPoint(0, 150), osmPoint(140, 150)] },
  osmGreen(141, 151),
  { type: 'way', id: 13, tags: { golf: 'hole', ref: '3', par: '5' }, geometry: [osmPoint(0, 300), osmPoint(455, 300)] }
] };

test('Platz aus OpenStreetMap suchen, pruefen und aufs Geraet laden', async () => {
  const page = await newPage();
  const queries = [];
  await page.route('https://overpass-api.de/api/interpreter', async route => {
    const query = new URLSearchParams(route.request().postData()).get('data');
    queries.push(query);
    await route.fulfill({ contentType: 'application/json', body: JSON.stringify(query.includes('golf_course') ? OSM_SEARCH : OSM_HOLES) });
  });
  await page.goto(baseUrl + '/');
  await page.click('[data-tab="course"]');
  await page.waitForSelector('#course-name');
  assert.equal(await page.textContent('#course-name'), 'Beispielplatz');

  await page.fill('#search-name', 'Muster');
  await page.click('#search-form button[type="submit"]');
  await page.waitForSelector('#search-results .list-item');
  assert.ok(queries[0].includes('["name"~"Muster",i]'));
  const hits = page.locator('#search-results .list-item');
  assert.equal(await hits.count(), 2);
  assert.match(await hits.nth(0).textContent(), /Golfclub Musterwiese\s*Musterdorf/);

  await hits.nth(0).click();
  await page.waitForSelector('#import-send');
  assert.ok(queries[1].includes('(around:2500,46.800000,8.200000)'));
  const preview = await page.textContent('#import-preview');
  assert.match(preview, /Golfclub Musterwiese/);
  assert.match(preview, /3 von 9 Löchern gefunden, 2 mit Grünfläche/);
  assert.match(preview, /Nicht erfasst: Loch 4, 5, 6, 7, 8, 9/);
  assert.match(preview, /Bei 1 Loch fehlt die Grünfläche/);
  const firstRow = await page.locator('#import-preview tbody tr').first().locator('td').allTextContents();
  assert.deepEqual(firstRow, ['1', '4', '312 m', 'bekannt']);
  await noSidewaysScroll(page);
  await shot(page, '4-platz-vorschau');

  // Noch ist nichts auf dem Geraet geaendert.
  assert.equal((await getJson('/api/course')).name, 'Beispielplatz');

  await page.click('#import-send');
  await page.waitForFunction(() => document.querySelector('#toast').textContent.includes('Platz geladen'));
  assert.match(await page.textContent('#toast'), /laufende Runde liegt jetzt im Archiv/);

  const course = await getJson('/api/course');
  assert.equal(course.name, 'Golfclub Musterwiese');
  assert.equal(course.holes.length, 9);
  assert.equal(course.holes[0].par, 4);
  assert.equal(course.holes[0].r, 120);
  assert.deepEqual(course.holes[5], { par: 0 });
  const status = await getJson('/api/status');
  assert.equal(status.holes, 9);
  assert.equal(status.rounds, 3); // die laufende Runde wurde archiviert
  await page.waitForFunction(() => document.querySelector('#course-name').textContent === 'Golfclub Musterwiese');
  assert.deepEqual(page.errors, []);
  await page.context().close();
});

test('Suche ohne Treffer und ohne Internet zeigt verstaendliche Meldungen', async () => {
  const page = await newPage();
  let mode = 'empty';
  await page.route('https://overpass-api.de/api/interpreter', async route => {
    if (mode === 'empty') await route.fulfill({ contentType: 'application/json', body: '{"elements":[]}' });
    else if (mode === 'busy') await route.fulfill({ status: 429, body: 'rate limited' });
    else await route.abort();
  });
  await page.goto(baseUrl + '/');
  await page.click('[data-tab="course"]');
  await page.fill('#search-name', 'Nirgendwo');
  await page.click('#search-form button[type="submit"]');
  await page.waitForFunction(() => document.querySelector('#search-results').textContent.includes('Kein Golfplatz'));

  mode = 'busy';
  await page.click('#search-form button[type="submit"]');
  await page.waitForFunction(() => document.querySelector('#search-results').textContent.includes('überlastet'));

  mode = 'offline';
  await page.click('#search-form button[type="submit"]');
  await page.waitForFunction(() => document.querySelector('#search-results').textContent.includes('Hat das Handy Internet?'));
  await page.context().close();
});

test('Platzname mit HTML wird als Text gezeigt, nicht ausgefuehrt', async () => {
  const page = await newPage();
  const evil = { elements: [{ type: 'way', id: 1, center: { lat: 46.8, lon: 8.2 }, tags: { name: '<img src=x onerror="window.hacked=1">Platz' } }] };
  await page.route('https://overpass-api.de/api/interpreter', route => route.fulfill({ contentType: 'application/json', body: JSON.stringify(evil) }));
  await page.goto(baseUrl + '/');
  await page.click('[data-tab="course"]');
  await page.fill('#search-name', 'Platz');
  await page.click('#search-form button[type="submit"]');
  await page.waitForSelector('#search-results .list-item');
  assert.match(await page.textContent('#search-results .title'), /<img src=x/);
  assert.equal(await page.locator('#search-results img').count(), 0);
  assert.equal(await page.evaluate(() => window.hacked), undefined);
  await page.context().close();
});

test('Platzdatei herunterladen und wieder einspielen', async () => {
  const page = await newPage();
  await page.goto(baseUrl + '/');
  await page.click('[data-tab="course"]');
  await page.waitForSelector('#course-name');
  const [file] = await Promise.all([page.waitForEvent('download'), page.click('#course-export')]);
  assert.equal(file.suggestedFilename(), 'platz.json');
  const saved = JSON.parse(fs.readFileSync(await file.path(), 'utf8'));
  assert.deepEqual(saved, await getJson('/api/course'));

  // Geaenderte Datei zurueckspielen.
  saved.name = 'Aus Datei';
  const upload = path.join(dataDir, 'upload.json');
  fs.writeFileSync(upload, JSON.stringify(saved));
  await page.setInputFiles('#course-file', upload);
  await page.waitForSelector('#import-send');
  await page.click('#import-send');
  await page.waitForFunction(() => document.querySelector('#course-name').textContent === 'Aus Datei');
  assert.equal((await getJson('/api/course')).name, 'Aus Datei');

  // Unsinn wird abgelehnt, bevor etwas ans Geraet geht.
  fs.writeFileSync(upload, 'kein json');
  await page.setInputFiles('#course-file', upload);
  await page.waitForFunction(() => document.querySelector('#toast').textContent.includes('keine Platzdatei'));
  assert.equal((await getJson('/api/course')).name, 'Aus Datei');
  await page.context().close();
});

test('Runde loeschen braucht zwei Tipps', async () => {
  const page = await newPage();
  await page.goto(baseUrl + '/');
  await page.click('#round-list .list-item[data-round="2"]');
  await page.click('#round-delete');
  assert.match(await page.textContent('#round-delete'), /Wirklich löschen/);
  assert.equal((await fetch(baseUrl + '/api/rounds/2')).status, 200); // nach dem ersten Tipp noch da
  await page.click('#round-delete');
  await page.waitForFunction(() => !document.querySelector('#round-list [data-round="2"]') && !document.querySelector('#rounds-overview').hidden);
  assert.equal((await fetch(baseUrl + '/api/rounds/2')).status, 404);
  assert.ok(await page.isVisible('#round-list .list-item[data-round="1"]'));
  assert.deepEqual(page.errors.filter(e => !e.includes('404')), []);
  await page.context().close();
});

test('WLAN des Geraets einstellen', async () => {
  const page = await newPage();
  await page.goto(baseUrl + '/');
  await page.waitForSelector('#device-tab-button:not([hidden])');
  await page.click('[data-tab="device"]');
  assert.match(await page.textContent('#wifi-status'), /eigenes WLAN auf:\s*Golf-Tracker/);
  assert.match(await page.textContent('#wifi-saved'), /Noch kein WLAN gespeichert/);
  assert.ok(await page.isHidden('#wifi-forget'));

  // Zu kurzes Passwort schickt der Browser nicht ab.
  await page.fill('#wifi-ssid', 'Heimnetz');
  await page.fill('#wifi-password', 'kurz');
  await page.click('#wifi-form button[type="submit"]');
  await page.waitForTimeout(300);
  assert.equal((await getJson('/api/wifi')).saved, false);

  await page.fill('#wifi-password', 'streng geheim 1');
  await page.click('#wifi-form button[type="submit"]');
  await page.waitForFunction(() => document.querySelector('#wifi-saved').textContent.includes('Ein WLAN ist gespeichert'));
  assert.equal(await page.inputValue('#wifi-password'), ''); // Passwort bleibt nicht im Feld stehen
  const wifi = await getJson('/api/wifi');
  assert.equal(wifi.saved, true);
  assert.equal(wifi.ssid, 'Heimnetz');
  assert.equal(JSON.stringify(wifi).includes('geheim'), false);
  await noSidewaysScroll(page);
  await shot(page, '5-geraet');

  await page.click('#wifi-forget');
  await page.waitForFunction(() => document.querySelector('#wifi-saved').textContent.includes('Noch kein WLAN'));
  assert.equal((await getJson('/api/wifi')).saved, false);
  assert.deepEqual(page.errors, []);
  await page.context().close();
});

test('Geraet nicht erreichbar zeigt eine klare Meldung', async () => {
  const page = await newPage();
  await page.route('**/api/**', route => route.abort());
  await page.goto(baseUrl + '/');
  await page.waitForFunction(() => document.querySelector('#status').textContent.includes('nicht erreichbar'));
  await page.context().close();
});
