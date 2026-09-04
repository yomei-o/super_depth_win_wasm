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

  // The same number of ticks with no keys at all must come out identical to
  // what the native build drew - the port has one copy of the game, and this
  // is what says so.
  const want = 'tmp/native_60.bin';
  if (fs.existsSync(want)) {
    const ref = fs.readFileSync(want);
    Module._sd_init();                          // start over from frame 0
    // The native shot sets the clock by hand; the page reads the real one,
    // and the "%2dFPS" in the corner counts frames inside a second - so a
    // second turning over mid-run would change two digits and nothing else.
    // Pin the same clock the native tool uses.
    Module._sd_set_clock(0, 1999, 2, 14);
    for (let t = 0; t < 60; t++) Module._sd_tick();
    const sp = Module._sd_surface();
    const got = Buffer.from(Module.HEAPU8.subarray(sp, sp + w * h));
    ok(ref.length === got.length, 'the native surface is the same size');
    let diff = 0;
    for (let i = 0; i < got.length && i < ref.length; i++)
      if (got[i] !== ref[i]) diff++;
    ok(diff === 0, 'and identical to the wasm one (' + diff + ' pixels differ)');
    console.log('native vs wasm at frame 60: ' +
                (diff === 0 ? 'identical' : diff + ' pixels differ'));
  } else {
    console.log('(no ' + want + ', skipping the native comparison)');
  }

  // Every stage has to start inside the module, not just in the native
  // build: the native tools read disk/ off the filesystem, the page only has
  // what --embed-file put in.  disk/stage3.bin was missing once, and the
  // space stage gave up and dropped back to the title (state 0x1e) exactly
  // as FUN_0040f970 does when it cannot read its script.
  Module._sd_init();
  Module._sd_debug(0x86e);                      // to the title
  Module._sd_tick();
  Module._sd_set_pad(0x0010);                   // Game Start, so the stage
  Module._sd_tick();                            // select is allowed (it only
  Module._sd_set_pad(0);                        // answers in 0x32/0x33/0x34)
  Module._sd_tick();
  ok(Module._sd_state() === 0x32, 'the button starts a game in the module');
  for (const [cmd, name, hook] of [[0x84e, 'sea', 1], [0x84f, 'air', 4],
                                   [0x850, 'space', 7], [0x851, 'boss', 8]]) {
    Module._sd_debug(cmd);
    Module._sd_tick();
    Module._sd_tick();
    ok(Module._sd_state() === 0x32, 'the ' + name + ' stage stays in 0x32 (' +
       Module._sd_state().toString(16) + ')');
  }

  // The debug menu's commands go through the same export the page's
  // buttons use, and the page's own script has to parse.
  Module._sd_init();
  ok(Module._sd_debug(0x86f) === 1, 'the ending command is taken');
  Module._sd_tick();
  ok(Module._sd_state() === 0x32, 'and it puts the game in state 0x32');
  ok(Module._sd_debug(0x123) === 0, 'an unknown command is not');
  ok(Module._sd_music_on() === 1 && Module._sd_se_on() === 1 &&
     Module._sd_stereo() === 1, 'the three options start on');
  Module._sd_debug(0x85d);
  ok(Module._sd_music_on() === 0, 'and 音楽 turns the music off');
  Module._sd_debug(0x85d);
  {
    const html = fs.readFileSync(path.resolve(__dirname, '../index.html'),
                                 'utf8');
    const m = html.match(/<script>([\s\S]*?)<\/script>/);
    ok(!!m, 'index.html has its script');
    if (m) {
      let bad = null;
      try { new Function(m[1]); } catch (e) { bad = e.message; }
      ok(bad === null, 'and it parses (' + bad + ')');
      // every id the buttons send must be one the module answers
      const list = m[1].match(/const DBG = \[([\s\S]*?)\];/);
      ok(!!list, 'and the debug menu is in it');
      if (list) {
        const ids = [...list[1].matchAll(/0x8[0-9a-f]{2}/g)]
          .map(x => parseInt(x, 16));
        ok(ids.length > 15, 'with all its buttons (' + ids.length + ')');
        const unknown = ids.filter(id => Module._sd_debug(id) !== 1);
        ok(unknown.length === 0, 'and every button id is a command (' +
           unknown.map(x => x.toString(16)).join(' ') + ')');
      }
    }
  }

  if (fails) { console.log(fails + ' checks failed'); process.exit(1); }
  console.log('wasm checks passed');
})();
