// The bundle element in index.html holds one or more base64 gzip sections,
// separated by newlines. Each section decodes to a JSON object of payload
// keys. The first section carries every key; later sections carry parts of
// the large maps (files, binaryFiles) that merge into it. V8 refuses strings
// longer than 2^29-24 characters, and the payload's JSON passed that at about
// 26,000 bundled source files, so nothing here serializes or parses a single
// string near that size. Node tools and tests read the bundle with readBundle;
// src/app.js implements the same merge in the page.
import {gzipSync, gunzipSync} from 'node:zlib';

// Largest JSON text one section may hold, well under the string limit.
export const SECTION_LIMIT = 200 * 1048576;
const SPLIT_KEYS = ['files', 'binaryFiles'];
export const BUNDLE_PATTERN = /<script id="bundle" type="application\/octet-stream">([^<]*)<\/script>/;

// JSON text as a list of Buffers: no single JSON.stringify of a large object.
export function jsonChunks(value, chunks) {
  if (Array.isArray(value)) {
    chunks.push(Buffer.from('['));
    value.forEach((item, i) => { if (i) chunks.push(Buffer.from(',')); jsonChunks(item, chunks); });
    chunks.push(Buffer.from(']'));
  } else if (value && typeof value === 'object') {
    chunks.push(Buffer.from('{'));
    let first = true;
    for (const [key, item] of Object.entries(value)) {
      if (item === undefined) continue;
      chunks.push(Buffer.from((first ? '' : ',') + JSON.stringify(key) + ':'));
      first = false;
      jsonChunks(item, chunks);
    }
    chunks.push(Buffer.from('}'));
  } else chunks.push(Buffer.from(JSON.stringify(value === undefined ? null : value)));
}
export function jsonBuffer(value) { const chunks = []; jsonChunks(value, chunks); return Buffer.concat(chunks); }

// The bundle element's text for a payload.
export function packBundle(payload) {
  const head = {}, parts = [];
  for (const [key, value] of Object.entries(payload)) {
    if (SPLIT_KEYS.includes(key) && value && typeof value === 'object') {
      head[key] = {};
      let part = {}, size = 2;
      for (const [name, item] of Object.entries(value)) {
        const entry = JSON.stringify(name).length + JSON.stringify(item).length + 2;
        if (size + entry > SECTION_LIMIT && Object.keys(part).length) { parts.push({[key]: part}); part = {}; size = 2; }
        part[name] = item; size += entry;
      }
      if (Object.keys(part).length) parts.push({[key]: part});
    } else head[key] = value;
  }
  return [head, ...parts].map(section => gzipSync(jsonBuffer(section)).toString('base64')).join('\n');
}

// Merge one decoded section into the payload being assembled.
export function mergeSection(payload, section) {
  for (const [key, value] of Object.entries(section)) {
    payload[key] = key in payload && value && typeof value === 'object' && !Array.isArray(value) ? Object.assign(payload[key], value) : value;
  }
  return payload;
}
export function unpackBundle(text) {
  const payload = {};
  for (const section of text.trim().split(/\s+/)) mergeSection(payload, JSON.parse(gunzipSync(Buffer.from(section, 'base64')).toString('utf8')));
  return payload;
}
// The payload embedded in a built index.html, or null when it has none.
export function readBundle(html) {
  const match = BUNDLE_PATTERN.exec(html);
  return match ? unpackBundle(match[1]) : null;
}
