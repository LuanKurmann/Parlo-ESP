// Tests des Bluetooth-Protokolls gegen den Simulator (ueber TCP):
//   node --test tools/link/*.test.js
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const { spawn } = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { LinkClient, Reassembler, fragment, crc32, encodeRequest } = require('./link_client.js');

const ROOT = path.resolve(__dirname, '..', '..');
const SIMULATOR = path.join(ROOT, '.pio', 'build', 'simulator', 'program');

let simulator, dataDir, linkPort, httpPort;

async function startSimulator(packet) {
  dataDir = fs.mkdtempSync(path.join(os.tmpdir(), 'golf-link-'));
  linkPort = 20000 + Math.floor(Math.random() * 20000);
  httpPort = linkPort + 1;
  simulator = spawn(SIMULATOR, ['--headless', '--data', dataDir, '--link', String(linkPort), '--link-packet', String(packet),
    '--serve', String(httpPort), '--web', path.join(ROOT, 'web'), '--do', 'b1,b1,pos:210,b1,b1'], { stdio: ['ignore', 'pipe', 'inherit'] });
  for (let attempt = 0; attempt < 100; attempt++) {
    try { if ((await fetch(`http://127.0.0.1:${httpPort}/api/status`)).ok) return; } catch (e) { /* noch nicht bereit */ }
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  throw new Error('Simulator wurde nicht bereit');
}

async function viaHttp(pathname) {
  return (await fetch(`http://127.0.0.1:${httpPort}${pathname}`)).text();
}

async function connected(packetSize) {
  const client = new LinkClient(linkPort, packetSize);
  await client.connect();
  return client;
}

test.before(async () => {
  assert.ok(fs.existsSync(SIMULATOR), 'Simulator fehlt. Zuerst bauen: pio run -e simulator');
  await startSimulator(180);
});

test.after(() => {
  if (simulator) simulator.kill();
  if (dataDir) fs.rmSync(dataDir, { recursive: true, force: true });
});

test('CRC-32 und Zerlegen stimmen mit der Firmware ueberein', () => {
  assert.equal(crc32(Buffer.from('123456789')), 0xCBF43926);
  assert.equal(crc32(Buffer.from('hallo')), 0xB97231D1);
  // Dieselben Bytes, die test/test_link fuer "hallo" erwartet.
  assert.deepEqual([...fragment(Buffer.from('hallo'), 100)[0]],
    [1, 0, 0, 5, 0, 0, 0, 0xD1, 0x31, 0x72, 0xB9, 0x68, 0x61, 0x6C, 0x6C, 0x6F]);
  const pieces = fragment(Buffer.alloc(1000, 7), 20);
  assert.ok(pieces.every(p => p.length <= 20));
  assert.equal(new Reassembler().add(fragment(Buffer.from('x'), 20)[0]).toString(), 'x');
});

test('Statusabfrage liefert dasselbe wie ueber HTTP', async () => {
  const client = await connected(180);
  const response = await client.request('GET', '/api/status');
  assert.equal(response.status, 200);
  assert.equal(response.body, await viaHttp('/api/status'));
  assert.equal(JSON.parse(response.body).api, 3);
  client.close();
});

test('grosse Antwort kommt in Stuecken der eingestellten Groesse', async () => {
  const client = await connected(180);
  const response = await client.request('GET', '/api/course');
  assert.equal(response.body, await viaHttp('/api/course'));
  assert.ok(response.body.length > 1000);
  assert.ok(client.receivedFragments.length > 5, 'Antwort sollte ueber mehrere Stuecke kommen');
  assert.ok(client.receivedFragments.every(length => length <= 180));
  assert.ok(client.receivedFragments.slice(0, -1).every(length => length === 180)); // alle bis auf das letzte voll
  client.close();
});

test('kleinste Stuecke (20 Bytes) in Senderichtung', async () => {
  const client = await connected(20);
  const course = JSON.parse(await viaHttp('/api/course'));
  course.name = 'Per Link';
  const put = await client.request('PUT', '/api/course', JSON.stringify(course));
  assert.equal(put.status, 200);
  assert.equal(JSON.parse(put.body).archived_round, true); // die laufende Runde des Simulators
  assert.equal(JSON.parse(await viaHttp('/api/course')).name, 'Per Link');
  client.close();
});

test('Abgleich wie ihn die App macht: Liste holen, Runde ueber die Kennung holen und loeschen', async () => {
  const client = await connected(180);
  const list = JSON.parse((await client.request('GET', '/api/rounds')).body).rounds;
  assert.equal(list.length, 1);
  const id = list[0].id;
  assert.match(id, /^[0-9a-f]{16}$/);

  const round = await client.request('GET', '/api/rounds/id/' + id);
  assert.equal(round.status, 200);
  const data = JSON.parse(round.body);
  assert.equal(data.id, id);
  assert.equal(data.holes[0].shots.length, 2);
  assert.equal(data.holes[0].shots[0].c, 'Dr');

  assert.equal((await client.request('DELETE', '/api/rounds/id/' + id)).status, 200);
  assert.equal((await client.request('GET', '/api/rounds/id/' + id)).status, 404);
  assert.equal(JSON.parse((await client.request('GET', '/api/rounds')).body).rounds.length, 0);
  client.close();
});

test('Einstellungen aendern und Umlaute im Inhalt', async () => {
  const client = await connected(64);
  const put = await client.request('PUT', '/api/settings', JSON.stringify({ v: 1, brightness: 120, clubs: ['Dr', 'E7', 'GW'] }));
  assert.equal(put.status, 200);
  const settings = JSON.parse((await client.request('GET', '/api/settings')).body);
  assert.equal(settings.brightness, 120);
  assert.deepEqual(settings.clubs, ['Dr', 'E7', 'GW', 'Pt']);

  const course = JSON.parse((await client.request('GET', '/api/course')).body);
  course.name = 'Zürich Höhe';
  assert.equal((await client.request('PUT', '/api/course', JSON.stringify(course))).status, 200);
  assert.equal(JSON.parse((await client.request('GET', '/api/course')).body).name, 'Zürich Höhe');
  client.close();
});

test('Fehler der Schnittstelle kommen mit ihrem Status an', async () => {
  const client = await connected(180);
  assert.equal((await client.request('GET', '/api/gibt-es-nicht')).status, 404);
  assert.equal((await client.request('PUT', '/api/course', 'kein json')).status, 400);
  assert.equal((await client.request('POST', '/api/status')).status, 405);
  assert.equal((await client.request('GET', '/api/status')).status, 200);
  client.close();
});

test('viele Anfragen hintereinander behalten ihre Nummern', async () => {
  const client = await connected(100);
  for (let i = 0; i < 100; i++) {
    const response = await client.request('GET', i % 2 ? '/api/status' : '/api/settings');
    assert.equal(response.status, 200);
  }
  assert.equal(client.nextId, 101);
  client.close();
});

test('verlorenes Stueck wird mit Status 400 und Nummer 0 gemeldet, danach geht es weiter', async () => {
  const client = await connected(50);
  const pieces = fragment(encodeRequest(77, 'PUT', '/api/course', await viaHttp('/api/course')), 50);
  assert.ok(pieces.length > 5);
  const answer = client.waitForResponse();
  client.sendFragments([pieces[0], ...pieces.slice(2)]); // Stueck 1 fehlt
  const error = await answer;
  assert.equal(error.id, 0);
  assert.equal(error.status, 400);
  assert.equal((await client.request('GET', '/api/status')).status, 200);
  client.close();
});

test('Unsinn statt einer Anfrage wird abgelehnt', async () => {
  const client = await connected(180);
  const answer = client.waitForResponse();
  client.sendFragments(fragment(Buffer.from('das ist keine Anfrage'), 180));
  const response = await answer;
  assert.equal(response.id, 0);
  assert.equal(response.status, 400);
  client.close();
});

test('abgebrochene Verbindung mitten in einer Anfrage stoert die naechste Verbindung nicht', async () => {
  const first = await connected(50);
  const pieces = fragment(encodeRequest(1, 'PUT', '/api/course', await viaHttp('/api/course')), 50);
  first.sendFragments(pieces.slice(0, 3)); // nur der Anfang
  await new Promise(resolve => setTimeout(resolve, 100));
  first.close();

  const second = await connected(180);
  const response = await second.request('GET', '/api/status');
  assert.equal(response.status, 200);
  assert.equal(response.id, 1); // neue Verbindung, neue Zaehlung
  second.close();
});

test('eine neue Verbindung ersetzt die bisherige', async () => {
  const first = await connected(180);
  assert.equal((await first.request('GET', '/api/status')).status, 200);
  const second = await connected(180);
  assert.equal((await second.request('GET', '/api/status')).status, 200);
  await assert.rejects(first.request('GET', '/api/status')); // die alte wurde getrennt
  second.close();
});
