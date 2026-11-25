// Referenz-Client fuer das Bluetooth-Protokoll des Golf-Trackers, hier ueber
// den TCP-Zugang des Simulators. Beschreibung: docs/bluetooth-protokoll.md.
// Dient als Vorlage fuer die App und als Gegenprobe fuer die Firmware.
'use strict';
const net = require('node:net');

const FIRST = 1, NEXT = 2;
const FIRST_HEADER = 11, NEXT_HEADER = 3;
const MIN_FRAGMENT = 20;

// CRC-32 wie in lib/core/src/core/crc32.cpp.
function crc32(buffer) {
  let crc = 0xFFFFFFFF;
  for (const byte of buffer) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ (0xEDB88320 & -(crc & 1));
  }
  return (~crc) >>> 0;
}

// Zerlegt eine Nachricht in Stuecke von hoechstens `maxSize` Bytes.
function fragment(message, maxSize) {
  const size = Math.max(maxSize, MIN_FRAGMENT);
  const fragments = [];
  let offset = 0, sequence = 0;
  do {
    const first = sequence === 0;
    const header = Buffer.alloc(first ? FIRST_HEADER : NEXT_HEADER);
    header[0] = first ? FIRST : NEXT;
    header.writeUInt16LE(sequence, 1);
    if (first) {
      header.writeUInt32LE(message.length, 3);
      header.writeUInt32LE(crc32(message), 7);
    }
    const take = Math.min(size - header.length, message.length - offset);
    fragments.push(Buffer.concat([header, message.subarray(offset, offset + take)]));
    offset += take;
    sequence++;
  } while (offset < message.length);
  return fragments;
}

// Setzt Stuecke zusammen. add() liefert die fertige Nachricht oder null;
// bei einem Fehler wirft es.
class Reassembler {
  constructor() { this.reset(); }
  reset() { this.parts = []; this.received = 0; this.total = 0; this.crc = 0; this.expected = 0; this.receiving = false; }
  add(data) {
    if (data.length < NEXT_HEADER) throw new Error('Stueck zu kurz');
    const type = data[0], sequence = data.readUInt16LE(1);
    let header = NEXT_HEADER;
    if (type === FIRST) {
      this.reset();
      if (data.length < FIRST_HEADER || sequence !== 0) throw new Error('ungueltiges erstes Stueck');
      this.total = data.readUInt32LE(3);
      this.crc = data.readUInt32LE(7);
      this.receiving = true;
      header = FIRST_HEADER;
    } else if (type === NEXT) {
      if (!this.receiving || sequence !== this.expected) { this.reset(); throw new Error('Stueck ausser der Reihe'); }
    } else {
      throw new Error('unbekannte Art ' + type);
    }
    const payload = data.subarray(header);
    if (this.received + payload.length > this.total) { this.reset(); throw new Error('mehr Daten als angekuendigt'); }
    this.parts.push(payload);
    this.received += payload.length;
    this.expected = sequence + 1;
    if (this.received < this.total) return null;
    const message = Buffer.concat(this.parts);
    const expectedCrc = this.crc;
    this.reset();
    if (crc32(message) !== expectedCrc) throw new Error('Pruefsumme stimmt nicht');
    return message;
  }
}

function encodeRequest(id, method, path, body) {
  return Buffer.from(`${id}\n${method} ${path}\n${body || ''}`, 'utf8');
}

function decodeResponse(message) {
  const text = message.toString('utf8');
  const first = text.indexOf('\n'), second = text.indexOf('\n', first + 1);
  if (first < 0 || second < 0) throw new Error('ungueltige Antwort');
  return { id: Number(text.slice(0, first)), status: Number(text.slice(first + 1, second)), body: text.slice(second + 1) };
}

// Verbindung zum Simulator. `packetSize` ist die Groesse der Stuecke, die
// dieser Client sendet.
class LinkClient {
  constructor(port, packetSize = 180) {
    this.port = port;
    this.packetSize = packetSize;
    this.nextId = 1;
    this.reassembler = new Reassembler();
    this.buffer = Buffer.alloc(0);
    this.waiting = null;
    this.receivedFragments = [];
  }

  connect() {
    return new Promise((resolve, reject) => {
      this.socket = net.createConnection({ host: '127.0.0.1', port: this.port }, resolve);
      this.socket.setNoDelay(true);
      this.socket.on('error', error => { reject(error); this._fail(error); });
      this.socket.on('close', () => this._fail(new Error('Verbindung getrennt')));
      this.socket.on('data', data => this._onData(data));
    });
  }

  close() { if (this.socket) this.socket.destroy(); }

  _fail(error) {
    if (this.waiting) { const waiting = this.waiting; this.waiting = null; waiting.reject(error); }
  }

  // TCP: vor jedem Stueck steht seine Laenge als zwei Bytes.
  _onData(data) {
    this.buffer = Buffer.concat([this.buffer, data]);
    while (this.buffer.length >= 2) {
      const length = this.buffer.readUInt16LE(0);
      if (this.buffer.length < 2 + length) break;
      const piece = this.buffer.subarray(2, 2 + length);
      this.buffer = this.buffer.subarray(2 + length);
      this.receivedFragments.push(piece.length);
      let message;
      try { message = this.reassembler.add(piece); } catch (error) { this._fail(error); continue; }
      if (message && this.waiting) {
        const waiting = this.waiting;
        this.waiting = null;
        waiting.resolve(decodeResponse(message));
      }
    }
  }

  // Sendet rohe Stuecke (fuer Tests mit absichtlichen Fehlern).
  sendFragments(fragments) {
    for (const piece of fragments) {
      const length = Buffer.alloc(2);
      length.writeUInt16LE(piece.length, 0);
      this.socket.write(Buffer.concat([length, piece]));
    }
  }

  waitForResponse(timeoutMs = 5000) {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => { this.waiting = null; reject(new Error('keine Antwort')); }, timeoutMs);
      this.waiting = {
        resolve: value => { clearTimeout(timer); resolve(value); },
        reject: error => { clearTimeout(timer); reject(error); }
      };
    });
  }

  // Eine Anfrage wie ueber /api/...: liefert {id, status, body}.
  async request(method, path, body) {
    const id = this.nextId++;
    const answer = this.waitForResponse();
    this.sendFragments(fragment(encodeRequest(id, method, path, body), this.packetSize));
    const response = await answer;
    if (response.id !== id) throw new Error(`Antwort ${response.id} passt nicht zur Anfrage ${id}`);
    return response;
  }
}

module.exports = { LinkClient, Reassembler, fragment, crc32, encodeRequest, decodeResponse };
