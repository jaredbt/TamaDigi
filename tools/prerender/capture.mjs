// Capture Time Stranger poses through the dsts-viewer's dev-only recording API
// (window.__viewerApi), which returns model-only, transparent frames rendered with
// the game's own cel shading, eye compositing and outlines.
//
// Needs the viewer's Vite dev server on :6103 with DSTS_MODELS pointing at
// tools/prerender/models (laid out v3/dsts-opt/digimon/<slug>/<slug>.glb), and
// playwright-core + an installed Google Chrome.
//
//   node tools/prerender/capture.mjs [--only agumon] [--out tools/prerender/renders/viewer]
//
// Writes <out>/<slug>_<pose>.png (full viewport, transparent) and <out>/<slug>.json
// with every frame's model bounds, so the caller can crop all of a Digimon's poses
// to one union box. Nothing here may be committed: renders/ is git-ignored.
import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';

// playwright-core is not a TamaDigi dependency: point PLAYWRIGHT_CORE at an
// install of it (its index.mjs) or have it resolvable from here.
const { chromium } = await import(process.env.PLAYWRIGHT_CORE ?? 'playwright-core');

const VIEWER = 'http://127.0.0.1:6103/';
const CLIP_PREFIX = { koromon: 'chr322', agumon: 'chr050', wargreymon: 'chr027', seadramon: 'chr132' };
// pose -> [clip code, eye-expression row (0 happy/neutral, 1 sad, 2 grin, 3 angry), clip fraction]
const POSES = {
  idle: ['bn01', 0, 0.0],
  angry: ['bn01', 3, 0.0],
  sad: ['bn01', 1, 0.0],
  happy: ['bv01', 2, 0.5],
  losing: ['bd02', 0, 0.6],
  attacking: ['ba01', 0, 0.45],
};

// Camera, in camera-controls terms: azimuth 0 = dead front (models face +Z), +90 =
// on the model's left, which shows it in profile facing screen-LEFT like the Vital
// Bracelet sprites. A little short of 90 lets the face read. Polar 90 = level.
// zoomOut backs the camera off the viewer's idle framing so wide poses (wings
// spread, lunges) stay in frame; frames are cropped to the model afterwards anyway.
const VIEW = { azimuth: 70, polar: 82, zoomOut: 1.5 };
// Per-Digimon overrides, for bodies the default angle doesn't suit.
const VIEW_FOR = JSON.parse(process.env.VIEW_FOR ?? "{}");

const args = process.argv.slice(2);
const only = args.includes('--only') ? [args[args.indexOf('--only') + 1]] : Object.keys(CLIP_PREFIX);
const out = args.includes('--out') ? args[args.indexOf('--out') + 1] : 'tools/prerender/renders/viewer';
mkdirSync(out, { recursive: true });

const browser = await chromium.launch({ channel: 'chrome', headless: true, args: ['--use-angle=metal', '--enable-gpu'] });
const page = await browser.newPage({ viewport: { width: 900, height: 900 } });
page.on('pageerror', (e) => console.error('pageerror', e.message));

await page.goto(VIEWER);
await page.waitForFunction(() => localStorage.getItem('viewer-settings'), null, { timeout: 60000 });

for (const slug of only) {
  const bounds = {};
  for (const [pose, [code, eyes, frac]] of Object.entries(POSES)) {
    const clip = `${CLIP_PREFIX[slug]}_${code}`;
    await page.evaluate(
      ({ slug, clip, eyes }) => {
        const s = JSON.parse(localStorage.getItem('viewer-settings'));
        const a = s.d.panes.A;
        a.selectedModel = `/models/v3/dsts-opt/digimon/${slug}/${slug}.glb`;
        Object.assign(a.animation, { clip, eyeExpression: eyes, animPaused: true, animFrame: null });
        Object.assign(a.render, { bloom: false, shadows: false });
        a.camera.state = null; // let the viewer frame the new model itself
        localStorage.setItem('viewer-settings', JSON.stringify(s));
      },
      { slug, clip, eyes },
    );
    await page.goto(`${VIEWER}?m=${slug}`);
    await page.waitForFunction(() => window.__viewerReady?.A === true, null, { timeout: 120000 });
    const view = { ...VIEW, ...VIEW_FOR[slug] };
    await page.evaluate(({ azimuth, polar, zoomOut }) => {
      const c = window.__viewerDebug.A.controls;
      c.rotateTo((azimuth * Math.PI) / 180, (polar * Math.PI) / 180, false);
      c.dollyTo(c.distance * zoomOut, false);
      c.update(0);
    }, view);
    const info = await page.evaluate(() => window.__viewerApi.beginRecording('A'));
    const t = (info?.duration ?? 0) * frac;
    const frame = await page.evaluate((t) => window.__viewerApi.captureRecordingFrame('A', t, t), t);
    await page.evaluate(() => window.__viewerApi.endRecording('A'));
    if (!frame) {
      console.error(`FAIL ${slug} ${pose}: no frame`);
      continue;
    }
    writeFileSync(path.join(out, `${slug}_${pose}.png`), Buffer.from(frame.png, 'base64'));
    bounds[pose] = frame.bounds;
    const vp = await page.evaluate(() => [innerWidth, innerHeight]);
    const fb = frame.bounds;
    if (fb && (fb.x <= 1 || fb.y <= 1 || fb.x + fb.w >= vp[0] - 1 || fb.y + fb.h >= vp[1] - 1))
      console.warn(`WARN ${slug} ${pose}: model touches the frame edge, may be clipped`);
    console.log(`done ${slug} ${pose} (${clip}, eyes ${eyes}, t=${t.toFixed(2)}s of ${info?.duration?.toFixed(2)}s)`);
  }
  writeFileSync(path.join(out, `${slug}.json`), JSON.stringify(bounds, null, 1));
}
await browser.close();
