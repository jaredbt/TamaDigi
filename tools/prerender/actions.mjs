// Capture a Digimon's full TamaDigi action set as animation frames, through the
// dsts-viewer's dev-only recording API (see capture.mjs for the setup: the viewer's
// Vite dev server on :6103 with DSTS_MODELS -> tools/prerender/models, and
// PLAYWRIGHT_CORE -> an install of playwright-core).
//
//   node tools/prerender/actions.mjs --only agumon [--action walk,eat] [--fps 24] [--out tools/prerender/renders/actions]
//                                    [--models tools/prerender/models] [--prefix chr050]
//
// Any roster form works: the clip prefix (chrNNN) and the clips a model has are read
// from the model's own .glb (its animation names, <prefix>_<code>), found under
// --models (the folder the viewer serves as DSTS_MODELS). --prefix gives the
// expected prefix (batch.py passes the viewer index's chr_id); the glb wins if they
// disagree, with a warning. An action whose clip (and fallbacks) the model lacks is
// skipped, never staged, and listed under bounds.json's "_missing".
//
// Each action is one Time Stranger clip, sampled at --fps across one play of the
// clip (default 24, the game's authored rate; thinning to a lower device rate is
// done later), with an eye-expression row. Writes
// <out>/<slug>/<action>_NN.png (transparent, full viewport) and <out>/<slug>/bounds.json.
// Nothing here may be committed: renders/ is git-ignored.
import { existsSync, mkdirSync, readdirSync, readFileSync, unlinkSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));

const { chromium } = await import(process.env.PLAYWRIGHT_CORE ?? 'playwright-core');

const VIEWER = 'http://127.0.0.1:6103/';
// The prototype's five, kept as the default --only set; any other slug works too.
const PROTOTYPE = ['koromon', 'agumon', 'greymon', 'wargreymon', 'seadramon'];
const VIEW = { azimuth: 70, polar: 82, zoomOut: 1.5 };
// Per-Digimon camera overrides (merged over VIEW), plus any in the VIEW_FOR env var
// as JSON, e.g. VIEW_FOR='{"wargreymon":{"azimuth":35}}'.
const VIEW_FOR = { wargreymon: { azimuth: 35 }, ...JSON.parse(process.env.VIEW_FOR ?? '{}') };

// TamaDigi action -> [clip code, eyes, speed]
// eyes: an eye-atlas row (0 open, 1 closed, 2 smile, 3 angry), or a timeline
//   [[fromFraction, row], ...] when the expression changes mid-clip. The viewer
//   holds one row for a whole clip (the body clips don't animate the eyes), so a
//   timeline is captured once per row and spliced by frame.
// speed (default 1) slows a clip by sampling it more finely, so it stays as smooth
//   as the rest at the same playback rate: 0.5 = twice the frames, half the pace.
// The game's own clip names (from the viewer's data/animation_names.json) line up
// with TamaDigi's actions almost one to one. Sleep and sad are stand-ins with the
// eyes shut, picked by eye: the low-HP idle reads as sleep, a head-shake as sulking.
const ACTIONS = {
  idle: ['bn01', 0],
  walk: ['br01', 0],          // run; mirrored on the device for the other direction
  sleep: ['bn02', 1],         // the game's low-HP idle, eyes shut
  sad: ['fq02', 1],           // "No", eyes shut; a new TamaDigi slot (sad borrows hurt today)
  yawn: ['fe03', [[0, 0], [0.3, 1], [0.76, 0]]],   // eyes shut while the mouth is wide open
  eat: ['fe01', 0],
  hurt: ['bd01', 1],          // a wince
  // Across the roster ba01 tends to be the physical attack and ba02 the special
  // (elemental/magic) one, matching TamaDigi's MC_PHYS / MC_SPEC move categories.
  // A trend, not a rule: swap per Digimon in ACTIONS_FOR where it runs the other way.
  attack: ['ba01', 3],        // physical moves
  attack2: ['ba02', 3],       // special moves; a new TamaDigi slot
  cheer: ['fe02', 2],
  victory: ['bv01', 2],       // TamaDigi's Hop slot
  nod: ['fq01', 0],
  refuse: ['fq02', 0],
  knockdown: ['bd02', 1],
  getup: ['bd03', 0],
};

// Fallback clips, tried in order when a model lacks an action's own clip. Sleep is
// a stand-in anyway: the plain idle with the eyes shut reads the same.
const FALLBACK = { sleep: ['bn01'] };

// Per-Digimon picks that beat the generic mapping above. Some models carry an
// event clip (the eNNN codes, unnamed in the viewer) that suits an action better.
const ACTIONS_FOR = {
  agumon: { walk: ['e011', 0] },
  koromon: { walk: ['br01', 0, 0.6] },   // no walk clip: its run, slowed to a stroll (picked by eye from 0.5-0.75)
  greymon: { yawn: null },               // no fe03 on this model; null = skip (the device falls back to idle)
};

const args = process.argv.slice(2);
const arg = (k, d) => (args.includes(k) ? args[args.indexOf(k) + 1] : d);
const onlyActions = args.includes('--action') ? arg('--action').split(',') : null;
// --speed overrides every captured action's speed, for trying a pace before
// writing it into ACTIONS_FOR (pair it with --action and a separate --out).
const SPEED = args.includes('--speed') ? Number(arg('--speed')) : null;
// --measure records each action's capture scale (view.pxPerUnit) into an existing
// bounds.json without re-capturing any frames.
const MEASURE = args.includes('--measure');
const slugs = args.includes('--only') ? arg('--only').split(',') : PROTOTYPE;
const FPS = Number(arg('--fps', 24));
const OUT = arg('--out', path.join(HERE, 'renders', 'actions'));
const MODELS = arg('--models', process.env.DSTS_MODELS ?? path.join(HERE, 'models'));
const PREFIX = arg('--prefix', null);

// A glb's animation names, from its JSON chunk (no three.js needed).
function glbClips(file) {
  const b = readFileSync(file);
  if (b.toString('latin1', 0, 4) !== 'glTF') throw new Error(`${file}: not a glb`);
  const len = b.readUInt32LE(12);
  const j = JSON.parse(b.toString('utf8', 20, 20 + len));
  return (j.animations ?? []).map((a) => a.name).filter(Boolean);
}

// The model's clip prefix: the chrNNN whose idle (bn01) it carries, else its most
// common chrNNN_ prefix. Returns { prefix, codes } (codes: Set of clip codes).
function clipsFor(slug, expected) {
  const names = glbClips(path.join(MODELS, 'v3', 'dsts-opt', 'digimon', slug, `${slug}.glb`));
  const idle = names.map((n) => n.match(/^(chr\d+)_bn01$/)?.[1]).filter(Boolean);
  let prefix = expected && names.includes(`${expected}_bn01`) ? expected : idle[0];
  if (!prefix) {
    const count = {};
    for (const n of names) { const m = n.match(/^(chr\d+)_/); if (m) count[m[1]] = (count[m[1]] ?? 0) + 1; }
    prefix = Object.entries(count).sort((a, b) => b[1] - a[1])[0]?.[0] ?? expected;
  }
  if (expected && prefix !== expected) console.warn(`WARN ${slug}: index prefix ${expected}, glb clips use ${prefix}`);
  const codes = new Set(names.filter((n) => n.startsWith(`${prefix}_`)).map((n) => n.slice(prefix.length + 1)));
  return { prefix, codes };
}

const browser = await chromium.launch({ channel: 'chrome', headless: true, args: ['--use-angle=metal', '--enable-gpu'] });
const page = await browser.newPage({ viewport: { width: 900, height: 900 } });
page.on('pageerror', (e) => console.error('pageerror', e.message));
await page.goto(VIEWER);
await page.waitForFunction(() => localStorage.getItem('viewer-settings'), null, { timeout: 60000 });

// Load slug with clip and eye row, turn the camera, and start a recording.
async function stage(slug, clip, eyes, viewFor = VIEW) {
  await page.evaluate(
    ({ slug, clip, eyes }) => {
      const s = JSON.parse(localStorage.getItem('viewer-settings'));
      const a = s.d.panes.A;
      a.selectedModel = `/models/v3/dsts-opt/digimon/${slug}/${slug}.glb`;
      Object.assign(a.animation, { clip, eyeExpression: eyes, animPaused: true, animFrame: null });
      Object.assign(a.render, { bloom: false, shadows: false });
      a.camera.state = null;
      localStorage.setItem('viewer-settings', JSON.stringify(s));
    },
    { slug, clip, eyes },
  );
  await page.goto(`${VIEWER}?m=${slug}`);
  await page.waitForFunction(() => window.__viewerReady?.A === true, null, { timeout: 120000 });
  // The viewer frames every load to its model's pose, so each action's capture has
  // its own scale. Record it as pixels per world unit at the orbit target: with a
  // vertical fov, the world height visible at distance d is 2 d tan(fov / 2).
  const view = await page.evaluate(({ azimuth, polar, zoomOut }) => {
    const { controls: c, camera, renderer } = window.__viewerDebug.A;
    c.rotateTo((azimuth * Math.PI) / 180, (polar * Math.PI) / 180, false);
    c.dollyTo(c.distance * zoomOut, false);
    c.update(0);
    const h = renderer.domElement.clientHeight;
    return { distance: c.distance, fov: camera.fov, canvasH: h,
             pxPerUnit: h / (2 * c.distance * Math.tan((camera.fov * Math.PI) / 360)) };
  }, viewFor);
  const info = await page.evaluate(() => window.__viewerApi.beginRecording('A'));
  return { info, view };
}

for (const slug of slugs) {
  const dir = path.join(OUT, slug);
  mkdirSync(dir, { recursive: true });
  const boundsFile = path.join(dir, 'bounds.json');
  // Re-capturing a subset (--action) keeps the other actions' entries.
  const bounds = (onlyActions || MEASURE) && existsSync(boundsFile) ? JSON.parse(readFileSync(boundsFile, 'utf8')) : {};
  const plan = { ...ACTIONS, ...ACTIONS_FOR[slug] };
  const { prefix, codes } = clipsFor(slug, PREFIX);
  const viewFor = { ...VIEW, ...VIEW_FOR[slug] };
  const missing = { ...(bounds._missing ?? {}) };
  for (const [action, spec] of Object.entries(plan)) {
    if (!spec) continue;
    if (onlyActions && !onlyActions.includes(action)) continue;
    let [code, eyes, planned = 1] = spec;
    if (!codes.has(code)) {
      const alt = (FALLBACK[action] ?? []).find((c) => codes.has(c));
      if (!alt) {
        missing[action] = code;
        delete bounds[action];
        if (!MEASURE) for (const f of readdirSync(dir)) if (f.startsWith(`${action}_`)) unlinkSync(path.join(dir, f));
        console.warn(`MISSING ${slug} ${action}: no ${prefix}_${code} on the model; slot left empty`);
        continue;
      }
      console.warn(`FALLBACK ${slug} ${action}: no ${prefix}_${code}, using ${prefix}_${alt}`);
      code = alt;
    }
    delete missing[action];
    const speed = SPEED ?? planned;
    if (!MEASURE) for (const f of readdirSync(dir)) if (f.startsWith(`${action}_`)) unlinkSync(path.join(dir, f));
    const clip = `${prefix}_${code}`;
    const timeline = Array.isArray(eyes) ? eyes : [[0, eyes]];
    const rowAt = (frac) => timeline.filter(([from]) => frac >= from).at(-1)[1];
    const rows = [...new Set(timeline.map(([, row]) => row))];
    if (MEASURE) {
      // Scale only: stage the clip and record its view, keep the frames already on disk.
      const { view } = await stage(slug, clip, rows[0], viewFor);
      await page.evaluate(() => window.__viewerApi.endRecording('A'));
      if (bounds[action]) bounds[action].view = view;
      console.log(`${slug} ${action.padEnd(9)} ${view.pxPerUnit.toFixed(1)} px/unit`);
      continue;
    }
    const b = [];
    let info = null;
    let view = null;
    let n = 0;
    // One pass per eye row: the viewer applies the row at load, not per frame.
    for (const row of rows) {
      ({ info, view } = await stage(slug, clip, row, viewFor));
      n = Math.max(1, Math.round(((info?.duration ?? 0) * FPS) / speed));
      for (let i = 0; i < n; i++) {
        if (rowAt(i / n) !== row) continue;
        const t = (i / FPS) * speed;
        const f = await page.evaluate((t) => window.__viewerApi.captureRecordingFrame('A', t, t), t);
        if (!f) continue;
        writeFileSync(path.join(dir, `${action}_${String(i).padStart(2, '0')}.png`), Buffer.from(f.png, 'base64'));
        b[i] = f.bounds;
      }
      await page.evaluate(() => window.__viewerApi.endRecording('A'));
    }
    const frames = b.filter(Boolean).length;
    if (frames !== n) console.warn(`WARN ${slug} ${action}: ${frames} of ${n} frames captured`);
    bounds[action] = { clip, eyes, speed, frames, ms: Math.round(1000 / FPS), view, bounds: b };
    console.log(`${slug} ${action.padEnd(9)} ${clip} ${frames} frames (${info?.duration?.toFixed(2)}s clip` +
      `${speed === 1 ? '' : `, x${speed} -> ${(info.duration / speed).toFixed(2)}s`}` +
      `${rows.length > 1 ? `, eyes ${JSON.stringify(timeline)}` : ''})`);
  }
  bounds._missing = missing;
  bounds._prefix = prefix;
  writeFileSync(boundsFile, JSON.stringify(bounds, null, 1));
}
await browser.close();
