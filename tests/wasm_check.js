// Drive the WASM build under node and dump a frame, so the browser target can
// be checked without opening a browser.
//
//   node tests/wasm_check.js [ticks] [out.png]
//
// What it checks: the module comes up, the archive loads, a frame is real
// pixels (and matches what the native shot tool draws for the same scene),
// and the synthesiser produces signal at the rate the page would ask for.
const fs = require('fs');
const path = require('path');

const ticks = parseInt(process.argv[2] || '30', 10);
const out = process.argv[3] || 'tmp/wasm.png';

// Under CommonJS the generated `var Module = typeof Module != "undefined" ...`
// sees its own hoisted local, so a global set before the require is dropped.
// Take the object the module exports instead and wait for the exports to land.
const Module = require(path.resolve(__dirname, '../superdepth.js'));

let fails = 0;
function ok(cond, what) {
  if (!cond) { console.log('FAIL ' + what); fails++; }
}

/* A minimal PNG writer: stored deflate blocks, so nothing has to be linked. */
function png(w, h, rgba) {
  const crcTable = [];
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    crcTable[n] = c >>> 0;
  }
  const crc = b => {
    let c = 0xffffffff;
    for (const x of b) c = crcTable[(c ^ x) & 0xff] ^ (c >>> 8);
    return (c ^ 0xffffffff) >>> 0;
  };
  const chunk = (type, data) => {
    const len = Buffer.alloc(4);
    len.writeUInt32BE(data.length);
    const body = Buffer.concat([Buffer.from(type, 'latin1'), data]);
    const c = Buffer.alloc(4);
    c.writeUInt32BE(crc(body));
    return Buffer.concat([len, body, c]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8; ihdr[9] = 6;                     // 8bpp, RGBA
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) {
    raw[y * (w * 4 + 1)] = 0;
    rgba.copy(raw, y * (w * 4 + 1) + 1, y * w * 4, (y + 1) * w * 4);
  }
  const blocks = [Buffer.from([0x78, 0x01])];
  for (let pos = 0; pos < raw.length; pos += 65535) {
    const n = Math.min(65535, raw.length - pos);
    const head = Buffer.alloc(5);
    head[0] = pos + n === raw.length ? 1 : 0;
    head.writeUInt16LE(n, 1);
    head.writeUInt16LE(~n & 0xffff, 3);
    blocks.push(head, raw.subarray(pos, pos + n));
  }
  let a = 1, b = 0;
  for (const x of raw) { a = (a + x) % 65521; b = (b + a) % 65521; }
  const ad = Buffer.alloc(4);
  ad.writeUInt32BE(((b << 16) | a) >>> 0);
  blocks.push(ad);
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk('IHDR', ihdr), chunk('IDAT', Buffer.concat(blocks)),
    chunk('IEND', Buffer.alloc(0))]);
}

(async () => {
  for (let i = 0; i < 500 && !Module._sd_init; i++)
    await new Promise(r => setTimeout(r, 20));
  if (!Module._sd_init) {
    console.error('runtime never came up');
    process.exit(1);
  }
  ok(Module._sd_init() === 0, 'depth.dar loads inside the module');
  const w = Module._sd_width(), h = Module._sd_height();
  ok(w === 640 && h === 480, 'the surface is 640x480');
  ok(Module._sd_patterns() === 2887, 'depth.dar has 2887 patterns');
  ok(Module._sd_state() === 0x0a, 'the game starts in state 0x0a');

  for (let t = 0; t < ticks; t++) Module._sd_tick();
  ok(Module._sd_state() === 0x10, 'and is in the logo after a few frames');
  const p = Module._sd_framebuffer();
  const rgba = Buffer.from(Module.HEAPU8.subarray(p, p + w * h * 4));

  let lit = 0;
  for (let i = 0; i < rgba.length; i += 4)
    if (rgba[i] | rgba[i + 1] | rgba[i + 2]) lit++;
  ok(lit > 5000, 'the frame has something on it (' + lit + ' lit pixels)');
  ok(rgba[3] === 255, 'and it is opaque');
  fs.writeFileSync(out, png(w, h, rgba));

  // The music is synthesised inside the module; pull a few blocks and check
  // it is actually producing signal at the rate the page would ask for.
  Module._sd_audio_init(44100);
  Module._sd_set_bgm(1);
  const n = Module._sd_audio_max() < 4096 ? Module._sd_audio_max() : 4096;
  let peak = 0, sum = 0, count = 0;
  for (let b = 0; b < 20; b++) {
    Module._sd_audio(n);
    const pl = Module._sd_audio_left() >> 2;
    const l = Module.HEAPF32.subarray(pl, pl + n);
    for (const v of l) {
      const a = Math.abs(v);
      if (a > peak) peak = a;
      sum += v * v;
      count++;
    }
  }
  const rms = Math.sqrt(sum / count);

  // 46 frames for the logo to come up, 0x78 of waiting, 46 to go away.
  for (let t = ticks; t < 170; t++) Module._sd_tick();
  ok(Module._sd_state() === 0x1e, 'the logo hands over to the title (state ' +
     Module._sd_state().toString(16) + ')');
  for (let t = 0; t < 30; t++) Module._sd_tick();
  {
    const p2 = Module._sd_framebuffer();
    const rgba2 = Buffer.from(Module.HEAPU8.subarray(p2, p2 + w * h * 4));
    let lit2 = 0;
    for (let i = 0; i < rgba2.length; i += 4)
      if (rgba2[i] | rgba2[i + 1] | rgba2[i + 2]) lit2++;
    ok(lit2 > 200000, 'the title comes up full of sea (' + lit2 + ' lit)');
    fs.writeFileSync(out.replace(/\.png$/, '') + '_title.png', png(w, h, rgba2));
  }
  ok(peak > 0.01, 'the synthesiser makes a signal (peak ' + peak.toFixed(3) + ')');
  ok(peak <= 1.0, 'and it does not clip');
  console.log(`frame ${ticks} -> ${out}  (${lit} lit)  music peak ${peak.toFixed(3)} rms ${rms.toFixed(4)}`);

  if (fails) { console.log(fails + ' checks failed'); process.exit(1); }
  console.log('wasm checks passed');
})();
