#!/usr/bin/env python3
"""Batch the Time Stranger sprite capture over the whole roster: for each of the
469 forms (ts_dex.h's TS_DEX_TBL rows, the firmware's dex ids), download the
dsts-viewer model if it is missing, capture the action set with actions.mjs, pack
it into the firmware's TPK2 format with pack_tpk2.py, then delete the raw PNG
frames (bounds.json stays, so a form can be re-packed without re-capturing).

    python3 -I tools/prerender/batch.py run    --viewer DIR [--root DIR] [--only slug,..] [--limit N] [--retry-all]
    python3 -I tools/prerender/batch.py fetch  --viewer DIR [--root DIR]     # downloads only, no viewer server
    python3 -I tools/prerender/batch.py audit  --viewer DIR [--root DIR]     # offline clip check of the models on disk
    python3 -I tools/prerender/batch.py report --viewer DIR [--root DIR]

--viewer is a clone of Nuffle's dsts-viewer (MIT, tangled.org/nuffle.me/dsts-viewer):
its data/digimon_index.json gives each form's model slug and chrNNN clip prefix,
matched on Time Stranger's dex number (TsDexEntry::tsDex). `run` also needs that
viewer's Vite dev server on :6103 with DSTS_MODELS=<root>/models, and
PLAYWRIGHT_CORE pointing at playwright-core's index.mjs (see actions.mjs).

--root is the prerender folder whose git-ignored models/ and renders/ are used
(default: this script's folder). Outputs, all under <root>/renders/ts/:
  tpk2/pNNN.bin   packed sprites, NNN = the firmware dex id (copy to the SD's /mons)
  work/<slug>/    bounds.json per form (frames are deleted once packed)
  log.jsonl       one record per attempt; the latest per form is its state
  roster.json     dex id -> name, slug, prefix, public art, boss
  REPORT.md       written by `report` (and at the end of every `run`)
Models land in <root>/models/v3/dsts-opt/digimon/<slug>/<slug>.glb.

Resumable and idempotent: a form with a pNNN.bin is skipped, as are forms whose
last attempt ended in a permanent state (no model on the CDN, no idle clip) unless
--retry-all; anything else that failed is retried on the next run. Order: the 73
forms with public art (noart.h), then the 13 bosses, then the rest by dex id.
Downloads are sequential with a pause between them (a hobbyist's CDN). The run
stops when free disk space drops below 5 GB.

Everything this writes is built from Bandai Namco's assets: never commit or
publish it (models/ and renders/ are git-ignored).
"""
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
CDN = 'https://cdn.nuffle.me/v3/dsts-opt/digimon/{slug}/{slug}.glb'
VIEWER_URL = 'http://127.0.0.1:6103/'
DOWNLOAD_PAUSE_S = 3
MIN_FREE = 5 * 1024 ** 3
CAPTURE_TIMEOUT_S = 20 * 60
PACK_TIMEOUT_S = 10 * 60
# Only the actions pack_tpk2.py has a firmware slot for; sad, attack2, refuse,
# knockdown and getup are captured by actions.mjs but have no slot yet, so the batch
# skips them (re-run actions.mjs on a form when the slots exist).
PACKED = ['idle', 'walk', 'sleep', 'eat', 'hurt', 'attack', 'cheer', 'victory', 'nod', 'yawn']
# The same clip codes actions.mjs uses for PACKED (its ACTIONS / FALLBACK), for the offline audit.
CLIPS = {'idle': ['bn01'], 'walk': ['br01'], 'sleep': ['bn02', 'bn01'], 'eat': ['fe01'],
         'hurt': ['bd01'], 'attack': ['ba01'], 'cheer': ['fe02'], 'victory': ['bv01'],
         'nod': ['fq01'], 'yawn': ['fe03']}
CLIPS_FOR = {'agumon': {'walk': ['e011']}}   # actions.mjs ACTIONS_FOR picks
# Frame caps tried in turn when a pack goes over the 3 MB file limit.
FRAME_CAPS = [24, 16, 12]
# over_limit: still over 3 MB at the smallest frame cap; retry with --retry-all once
# the firmware limit or FRAME_CAPS change.
PERMANENT = {'no_model', 'no_idle', 'over_limit'}


def opt(args, k, d=None):
    return args[args.index(k) + 1] if k in args else d


class Paths:
    def __init__(self, root):
        self.root = root
        self.models = os.path.join(root, 'models')
        self.ts = os.path.join(root, 'renders', 'ts')
        self.out = os.path.join(self.ts, 'tpk2')
        self.work = os.path.join(self.ts, 'work')
        self.log = os.path.join(self.ts, 'log.jsonl')

    def glb(self, slug):
        return os.path.join(self.models, 'v3', 'dsts-opt', 'digimon', slug, f'{slug}.glb')

    def bin(self, dex):
        return os.path.join(self.out, f'p{dex:03d}.bin')


# ---------------------------------------------------------------- roster

def load_roster(viewer):
    rows = []
    for line in open(os.path.join(REPO, 'ts_dex.h')):
        m = re.match(r'\s*\{ "([^"]+)", (\d+), ', line)
        if m and m.group(2) != '0':
            rows.append((m.group(1), int(m.group(2))))
    count = int(re.search(r'#define TS_DEX_COUNT (\d+)', open(os.path.join(REPO, 'ts_dex.h')).read()).group(1))
    assert len(rows) == count, f'{len(rows)} TS_DEX_TBL rows, TS_DEX_COUNT {count}'
    noart = open(os.path.join(REPO, 'noart.h')).read()
    bits = [int(x, 16) for x in re.findall(r'0x([0-9A-F]{2})', noart.split('HAS_ART_BITS', 1)[1].split('};', 1)[0])]
    has_art = lambda d: d < len(bits) * 8 and (bits[d >> 3] >> (d & 7)) & 1
    dex_h = open(os.path.join(REPO, 'ts_dex.h')).read()
    bosses = [int(x) for x in re.findall(r'\{ (\d+), \d+, TS_ST_\w+, TS_BOSS_\w+ \}', dex_h)]
    index = {e['dex_number']: e for e in json.load(open(os.path.join(viewer, 'data', 'digimon_index.json')))}
    roster = []
    for dex, (name, ts) in enumerate(rows, 1):
        e = index.get(ts)
        roster.append({'dex': dex, 'tsDex': ts, 'name': name,
                       'slug': e['slug'] if e else None, 'chr': e['chr_id'] if e else None,
                       'viewerName': e['name'] if e else None,
                       'art': bool(has_art(dex)), 'boss': dex in bosses})
    art = sum(r['art'] for r in roster)
    print(f'roster: {len(roster)} forms, {art} with public art, {len(bosses)} bosses, '
          f'{sum(not r["slug"] for r in roster)} not in the viewer index')
    return roster


def ordered(roster):
    first = [r for r in roster if r['art']]
    second = [r for r in roster if r['boss'] and not r['art']]
    rest = [r for r in roster if not r['art'] and not r['boss']]
    return first + second + rest


# ---------------------------------------------------------------- state

def log(P, rec):
    rec['t'] = time.strftime('%Y-%m-%dT%H:%M:%S')
    with open(P.log, 'a') as fh:
        fh.write(json.dumps(rec) + '\n')


def latest(P):
    state = {}
    if os.path.exists(P.log):
        for line in open(P.log):
            if line.strip():
                r = json.loads(line)
                state[r['dex']] = r
    return state


def free_bytes():
    return shutil.disk_usage(os.path.expanduser('~')).free


def disk_ok():
    f = free_bytes()
    if f < MIN_FREE:
        print(f'STOP: {f / 1024 ** 3:.1f} GB free, below the 5 GB floor')
        return False
    return True


# ---------------------------------------------------------------- models

def download(P, slug):
    """Fetch slug's glb if missing. Returns 'ok', 'cached', 'no_model' or raises."""
    dst = P.glb(slug)
    if os.path.exists(dst):
        return 'cached'
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    part = f'{dst}.part{os.getpid()}'
    req = urllib.request.Request(CDN.format(slug=slug), headers={'User-Agent': 'TamaDigi-prerender/1 (personal batch)'})
    try:
        with urllib.request.urlopen(req, timeout=120) as r, open(part, 'wb') as fh:
            shutil.copyfileobj(r, fh)
        os.replace(part, dst)
        return 'ok'
    except urllib.error.HTTPError as e:
        if os.path.exists(part):
            os.unlink(part)
        if e.code in (403, 404):
            return 'no_model'
        raise
    finally:
        time.sleep(DOWNLOAD_PAUSE_S)


def glb_clips(path):
    with open(path, 'rb') as fh:
        head = fh.read(20)
        if head[:4] != b'glTF':
            raise ValueError(f'{path}: not a glb')
        n = int.from_bytes(head[12:16], 'little')
        j = json.loads(fh.read(n))
    return [a.get('name') for a in j.get('animations', []) if a.get('name')]


def clip_audit(path, slug, expected):
    """Same prefix rule as actions.mjs clipsFor(): the expected chrNNN if the model
    has its bn01, else the chrNNN whose bn01 it has, else its commonest prefix."""
    names = glb_clips(path)
    idle = [m.group(1) for n in names if (m := re.match(r'^(chr\d+)_bn01$', n))]
    prefix = expected if expected and f'{expected}_bn01' in names else (idle[0] if idle else None)
    if not prefix:
        count = {}
        for n in names:
            if (m := re.match(r'^(chr\d+)_', n)):
                count[m.group(1)] = count.get(m.group(1), 0) + 1
        prefix = max(count, key=count.get) if count else expected
    codes = {n[len(prefix) + 1:] for n in names if prefix and n.startswith(prefix + '_')}
    missing, fallback = {}, {}
    for action in PACKED:
        chain = CLIPS_FOR.get(slug, {}).get(action, CLIPS[action])
        hit = next((c for c in chain if c in codes), None)
        if not hit:
            missing[action] = chain[0]
        elif hit != chain[0]:
            fallback[action] = hit
    return {'prefix': prefix, 'prefixMismatch': bool(expected and prefix != expected),
            'missing': missing, 'fallback': fallback, 'clips': len(names)}


# ---------------------------------------------------------------- capture + pack

def run_group(cmd, timeout, env=None):
    """Run cmd in its own process group; kill the whole group (Chrome too) on timeout."""
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                         start_new_session=True, env=env)
    try:
        out, _ = p.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        os.killpg(p.pid, signal.SIGKILL)
        out, _ = p.communicate()
        return None, (out or '') + f'\nTIMEOUT after {timeout} s'
    return p.returncode, out


def capture(P, r, node):
    cmd = [node, os.path.join(HERE, 'actions.mjs'), '--only', r['slug'], '--action', ','.join(PACKED),
           '--out', P.work, '--models', P.models]
    if r['chr']:
        cmd += ['--prefix', r['chr']]
    return run_group(cmd, CAPTURE_TIMEOUT_S)


def pack(P, r):
    out = ''
    for cap in FRAME_CAPS:
        code, out = run_group([sys.executable, '-I', os.path.join(HERE, 'pack_tpk2.py'), r['slug'], str(r['dex']),
                               P.out, '--src', P.work, '--max-frames', str(cap)], PACK_TIMEOUT_S)
        if code == 0:
            return cap, out
        if '3 MB' not in out:
            break
        if os.path.exists(P.bin(r['dex'])):   # the over-size file it wrote
            os.unlink(P.bin(r['dex']))
    return None, out


def clear_frames(P, slug):
    d = os.path.join(P.work, slug)
    if os.path.isdir(d):
        for f in os.listdir(d):
            if f.endswith('.png'):
                os.unlink(os.path.join(d, f))


def viewer_up():
    try:
        urllib.request.urlopen(VIEWER_URL, timeout=5).close()
        return True
    except Exception:
        return False


def do_run(P, roster, args):
    if not viewer_up():
        sys.exit(f'the dsts-viewer dev server is not answering on {VIEWER_URL} (see the docstring)')
    if not os.environ.get('PLAYWRIGHT_CORE'):
        sys.exit('set PLAYWRIGHT_CORE to playwright-core/index.mjs')
    node = opt(args, '--node', shutil.which('node'))
    if not node:
        sys.exit('no node on PATH (source nvm first) or pass --node')
    only = set(opt(args, '--only').split(',')) if '--only' in args else None
    limit = int(opt(args, '--limit', 10 ** 6))
    retry_all = '--retry-all' in args
    state = latest(P)
    done = 0
    for r in ordered(roster):
        if only and r['slug'] not in only and r['name'] not in only and str(r['dex']) not in only:
            continue
        last = state.get(r['dex'])
        if os.path.exists(P.bin(r['dex'])) and last and last['status'] == 'packed':
            continue
        if last and last['status'] in PERMANENT and not retry_all:
            continue
        if done >= limit:
            break
        if not disk_ok():
            break
        done += 1
        rec = {'dex': r['dex'], 'name': r['name'], 'slug': r['slug']}
        t0 = time.time()
        if not r['slug']:
            log(P, {**rec, 'status': 'no_model', 'error': 'not in the viewer index'})
            continue
        try:
            got = download(P, r['slug'])
        except Exception as e:
            log(P, {**rec, 'status': 'download_failed', 'error': str(e)[:300]})
            print(f'{r["dex"]:3d} {r["slug"]}: download failed: {e}')
            continue
        if got == 'no_model':
            log(P, {**rec, 'status': 'no_model', 'error': 'CDN 404'})
            print(f'{r["dex"]:3d} {r["slug"]}: no model on the CDN')
            continue
        audit = clip_audit(P.glb(r['slug']), r['slug'], r['chr'])
        rec.update(prefix=audit['prefix'], missing=audit['missing'], fallback=audit['fallback'])
        if 'idle' in audit['missing']:
            log(P, {**rec, 'status': 'no_idle'})
            print(f'{r["dex"]:3d} {r["slug"]}: no {audit["prefix"]}_bn01 idle clip; cannot pack')
            continue
        if not disk_ok():
            break
        code, out = capture(P, r, node)
        warns = [l for l in out.splitlines() if l.startswith(('WARN', 'pageerror'))]
        if code != 0:
            clear_frames(P, r['slug'])
            log(P, {**rec, 'status': 'capture_failed', 'error': out[-600:], 'warn': warns})
            print(f'{r["dex"]:3d} {r["slug"]}: capture failed\n{out[-600:]}')
            continue
        cap, pout = pack(P, r)
        clear_frames(P, r['slug'])
        if cap is None:
            over = '3 MB' in pout
            log(P, {**rec, 'status': 'over_limit' if over else 'pack_failed', 'error': pout[-600:], 'warn': warns})
            print(f'{r["dex"]:3d} {r["slug"]}: pack failed\n{pout[-600:]}')
            continue
        size = os.path.getsize(P.bin(r['dex']))
        log(P, {**rec, 'status': 'packed', 'bytes': size, 'maxFrames': cap, 'warn': warns,
                'secs': round(time.time() - t0), 'pack': pout.splitlines()[0] if pout else '',
                'notes': [l.strip() for l in pout.splitlines() if 'note:' in l]})
        print(f'{r["dex"]:3d} {r["slug"]}: packed {size // 1024} KB'
              f'{"" if cap == 24 else f" (max {cap} frames)"} in {time.time() - t0:.0f} s, '
              f'missing {",".join(audit["missing"]) or "none"}; {free_bytes() / 1024 ** 3:.1f} GB free', flush=True)
    write_report(P, roster)


def do_fetch(P, roster):
    for r in ordered(roster):
        if not r['slug'] or os.path.exists(P.glb(r['slug'])):
            continue
        if not disk_ok():
            break
        try:
            got = download(P, r['slug'])
        except Exception as e:
            print(f'{r["dex"]:3d} {r["slug"]}: download failed: {e}', flush=True)
            continue
        if got == 'no_model':
            log(P, {'dex': r['dex'], 'name': r['name'], 'slug': r['slug'], 'status': 'no_model', 'error': 'CDN 404'})
        print(f'{r["dex"]:3d} {r["slug"]}: {got}', flush=True)


# ---------------------------------------------------------------- report

def write_report(P, roster):
    state = latest(P)
    by_status = {}
    for r in roster:
        s = state.get(r['dex'], {}).get('status', 'not_attempted')
        if s == 'packed' and not os.path.exists(P.bin(r['dex'])):
            s = 'pack_lost'
        by_status.setdefault(s, []).append(r)
    audits, missing_by_action, fallbacks, mismatch, unreadable = {}, {}, [], [], []
    for r in roster:
        if r['slug'] and os.path.exists(P.glb(r['slug'])):
            try:
                a = audits[r['dex']] = clip_audit(P.glb(r['slug']), r['slug'], r['chr'])
            except Exception as e:
                unreadable.append((r, str(e)))
                continue
            for act, code in a['missing'].items():
                missing_by_action.setdefault(act, []).append((r, code))
            for act, code in a['fallback'].items():
                fallbacks.append((r, act, code))
            if a['prefixMismatch']:
                mismatch.append((r, a['prefix']))
    sz = lambda d: sum(os.path.getsize(os.path.join(dp, f)) for dp, _, fs in os.walk(d) for f in fs) if os.path.isdir(d) else 0
    name = lambda r: f'{r["dex"]} {r["name"]} (`{r["slug"]}`)'
    L = ['# Time Stranger sprite batch: report', '',
         f'Generated {time.strftime("%Y-%m-%d %H:%M")} by `tools/prerender/batch.py`. Private: built from '
         'Bandai Namco assets, never commit or publish.', '',
         f'- Forms: {len(roster)} (dex ids 1-{len(roster)})',
         f'- Packed: **{len(by_status.get("packed", []))}** ({sum(r["art"] for r in by_status.get("packed", []))} of '
         f'{sum(r["art"] for r in roster)} with public art, {sum(r["boss"] for r in by_status.get("packed", []))} of '
         f'{sum(r["boss"] for r in roster)} bosses)',
         *[f'- {s}: {len(v)}' for s, v in sorted(by_status.items()) if s != 'packed'],
         f'- Size: packs {sz(P.out) / 1024 ** 2:.0f} MB in `{P.out}`; models {sz(P.models) / 1024 ** 2:.0f} MB; '
         f'{free_bytes() / 1024 ** 3:.1f} GB free', '']
    reduced = [state[r['dex']] for r in by_status.get('packed', []) if state.get(r['dex'], {}).get('maxFrames', 24) != 24]
    if reduced:
        L += ['## Packed with fewer frames (over 3 MB at 24)', '',
              *[f'- {s["dex"]} {s["name"]}: max {s["maxFrames"]} frames' for s in reduced], '']
    dropped = [(s, n) for r in by_status.get('packed', []) for s in [state.get(r['dex'], {})]
               for n in s.get('notes', []) if 'in view' in n or '255 px' in n]
    if dropped:
        L += ['## Slots left empty at pack (clip out of frame, or over 255 px)', '',
              *[f'- {s["dex"]} {s["name"]}: {n.removeprefix("note: ")}' for s, n in dropped], '']
    over = by_status.get('over_limit', [])
    if over:
        L += [f'## Over the 3 MB file limit even at {FRAME_CAPS[-1]} frames ({len(over)}; not retried without --retry-all)', '',
              *[f'- {name(r)}: {(m.group(1) if (m := re.search(r"(\d+) bytes >", state[r["dex"]].get("error", ""))) else "?")} bytes'
                f'{" (public art)" if r["art"] else ""}{" (boss)" if r["boss"] else ""}' for r in over], '']
    L += ['## No model on the CDN', '']
    L += [f'- {name(r)}: {state.get(r["dex"], {}).get("error", "")}' for r in by_status.get('no_model', [])] or ['- none so far']
    L += ['', '## No idle clip (cannot pack)', '']
    L += [f'- {name(r)}' for r in by_status.get('no_idle', [])] or ['- none so far']
    L += ['', f'## Missing clips per action ({len(audits)} models audited)', '',
          'The slot is left empty and the device falls back to idle.', '']
    for act in PACKED:
        v = missing_by_action.get(act, [])
        L.append(f'- **{act}** (`{CLIPS[act][0]}`): {len(v)}' + (': ' + ', '.join(f'{r["name"]}' for r, _ in v) if v else ''))
    if fallbacks:
        L += ['', '## Fallback clips used', '', *[f'- {name(r)}: {act} <- `{code}`' for r, act, code in fallbacks]]
    if mismatch:
        L += ['', '## Clip prefix differs from the viewer index', '', *[f'- {name(r)}: index `{r["chr"]}`, glb `{p}`' for r, p in mismatch]]
    fails = [(s, r) for s in ('download_failed', 'capture_failed', 'pack_failed') for r in by_status.get(s, [])]
    L += ['', '## Failures (retried on the next run)', '']
    L += [f'- {name(r)}: {s}: `{state[r["dex"]].get("error", "")[-200:].strip().splitlines()[-1] if state[r["dex"]].get("error") else ""}`'
          for s, r in fails] or ['- none']
    if unreadable:
        L += ['', '## Unreadable models', '', *[f'- {name(r)}: {e}' for r, e in unreadable]]
    nat = by_status.get('not_attempted', [])
    if nat:
        L += ['', f'## Not attempted yet ({len(nat)})', '', ', '.join(r['name'] for r in nat)]
    os.makedirs(P.ts, exist_ok=True)
    path = os.path.join(P.ts, 'REPORT.md')
    open(path, 'w').write('\n'.join(L) + '\n')
    print(f'report: {path}')


def main():
    args = sys.argv[1:]
    if not args or args[0] not in ('run', 'fetch', 'audit', 'report') or '--viewer' not in args:
        sys.exit(__doc__)
    P = Paths(os.path.abspath(os.path.expanduser(opt(args, '--root', HERE))))
    for d in (P.out, P.work):
        os.makedirs(d, exist_ok=True)
    roster = load_roster(os.path.expanduser(opt(args, '--viewer')))
    with open(os.path.join(P.ts, 'roster.json'), 'w') as fh:
        json.dump(roster, fh, indent=1)
    lock = os.path.join(P.ts, f'batch-{args[0]}.lock')   # a run and a fetch may overlap
    if args[0] in ('run', 'fetch'):
        if os.path.exists(lock):
            pid = int(open(lock).read() or 0)
            try:
                os.kill(pid, 0)
                sys.exit(f'another batch is running (pid {pid}); remove {lock} if it is not')
            except (ProcessLookupError, ValueError):
                pass
        open(lock, 'w').write(str(os.getpid()))
    try:
        if args[0] == 'run':
            do_run(P, roster, args)
        elif args[0] == 'fetch':
            do_fetch(P, roster)
        else:
            write_report(P, roster)
    finally:
        if args[0] in ('run', 'fetch') and os.path.exists(lock):
            os.unlink(lock)


if __name__ == '__main__':
    main()
