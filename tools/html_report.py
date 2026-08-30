#!/usr/bin/env python3
# Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
# SPDX-License-Identifier: MIT
#
# html_report.py - Self-contained HTML report generator (ADR-0005)
#
# Reads a benchmark output directory (results.json + the PNG artifacts a
# `vgcpu-benchmark run --png` pass wrote) and, optionally, the correctness
# census JSON emitted by `VGCPU_ORACLE_JSON=... vgcpu_tests`, and publishes
# an HTML file plus a `png/` asset folder next to it (renders and
# amplified difference maps, linked relatively -- owner decision
# 2026-08-30, replacing base64 inlining that ballooned with the MPVG
# corpus): performance leaderboards, a rendering gallery with
# cross-backend SSIM against a chosen reference backend, amplified
# difference images, and the correctness-oracle census matrix.
#
# Design constraints (mirrors the reporting-harness discipline adopted in
# AGENTS.md): Python 3 stdlib only, no network, no CDN asset, deterministic
# (same inputs -> same bytes; the only timestamp shown is the run's own),
# read-only with respect to the benchmark outputs.
#
# Usage:
#   tools/html_report.py <results_dir> [-o report.html]
#                        [--reference skia] [--oracle-json census.json]

import argparse
import base64
import json
import math
import re
import struct
import sys
import zlib
from pathlib import Path

# ----------------------------------------------------------------------------
# Minimal PNG codec (sufficient for stb_image_write output: 8-bit, non-
# interlaced, color types 0/2/4/6).
# ----------------------------------------------------------------------------

_PNG_SIG = b"\x89PNG\r\n\x1a\n"


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def decode_png(data):
    """Return (width, height, rgba_bytes). Raises ValueError on unsupported input."""
    if data[:8] != _PNG_SIG:
        raise ValueError("not a PNG")
    pos = 8
    width = height = None
    color_type = bit_depth = None
    idat = bytearray()
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos : pos + 4])
        ctype = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            width, height, bit_depth, color_type, _comp, _filt, interlace = struct.unpack(
                ">IIBBBBB", chunk
            )
            if bit_depth != 8 or interlace != 0 or color_type not in (0, 2, 4, 6):
                raise ValueError(
                    f"unsupported PNG (depth={bit_depth} color={color_type} interlace={interlace})"
                )
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break
    if width is None:
        raise ValueError("missing IHDR")
    channels = {0: 1, 2: 3, 4: 2, 6: 4}[color_type]
    stride = width * channels
    raw = zlib.decompress(bytes(idat))
    if len(raw) < (stride + 1) * height:
        raise ValueError("truncated PNG data")
    out = bytearray(stride * height)
    prev = bytearray(stride)
    for y in range(height):
        row_start = y * (stride + 1)
        filt = raw[row_start]
        line = bytearray(raw[row_start + 1 : row_start + 1 + stride])
        if filt == 1:  # Sub
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 0xFF
        elif filt == 2:  # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif filt == 3:  # Average
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif filt == 4:  # Paeth
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                ul = prev[i - channels] if i >= channels else 0
                line[i] = (line[i] + _paeth(left, prev[i], ul)) & 0xFF
        elif filt != 0:
            raise ValueError(f"bad PNG filter {filt}")
        out[y * stride : (y + 1) * stride] = line
        prev = line
    # Convert to RGBA
    if color_type == 6:
        return width, height, bytes(out)
    rgba = bytearray(width * height * 4)
    if color_type == 2:
        for i in range(width * height):
            rgba[i * 4 : i * 4 + 3] = out[i * 3 : i * 3 + 3]
            rgba[i * 4 + 3] = 255
    elif color_type == 0:
        for i in range(width * height):
            g = out[i]
            rgba[i * 4 : i * 4 + 4] = bytes((g, g, g, 255))
    else:  # 4: gray+alpha
        for i in range(width * height):
            g, a = out[i * 2], out[i * 2 + 1]
            rgba[i * 4 : i * 4 + 4] = bytes((g, g, g, a))
    return width, height, bytes(rgba)


def encode_png_rgb(width, height, rgb):
    """Encode tightly-packed RGB bytes as a PNG (filter 0, deterministic)."""

    def chunk(ctype, payload):
        c = struct.pack(">I", len(payload)) + ctype + payload
        return c + struct.pack(">I", zlib.crc32(ctype + payload) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    stride = width * 3
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw += rgb[y * stride : (y + 1) * stride]
    idat = zlib.compress(bytes(raw), 9)
    return _PNG_SIG + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b"")


# ----------------------------------------------------------------------------
# Image comparison: SSIM (luma over white, 8x8 tiles) + pixel diff stats +
# amplified difference image.
# ----------------------------------------------------------------------------


def _luma_over_white(rgba, n):
    """Premultiplied RGBA composited over white, then Rec.601 luma (floats)."""
    luma = [0.0] * n
    for i in range(n):
        r, g, b, a = rgba[i * 4 : i * 4 + 4]
        inv = 255 - a
        luma[i] = 0.299 * (r + inv) + 0.587 * (g + inv) + 0.114 * (b + inv)
    return luma


AE_TOLERANCE = 8  # "fuzz" per-channel tolerance for the AE bad-pixel count


def compare_images(width, height, rgba_a, rgba_b):
    """Return dict(ssim, pae, ae_pct, diff_pct, diff_png_bytes).

    SSIM is structural and AVERAGES: one badly-wrong pixel hides inside a
    ~1.0 score. The industry-standard complement (cf. ImageMagick metrics)
    is the pair PAE + AE:
    - pae: Peak Absolute Error = L-infinity norm, max per-channel |a-b|
      over every pixel (worst case, 0..255);
    - ae_pct: percentage of pixels whose worst channel diff exceeds
      AE_TOLERANCE (how WIDESPREAD the disagreement is, ignoring
      AA/rounding noise below the tolerance).
    """
    n = width * height
    la = _luma_over_white(rgba_a, n)
    lb = _luma_over_white(rgba_b, n)

    c1 = (0.01 * 255.0) ** 2
    c2 = (0.03 * 255.0) ** 2
    tile = 8
    ssim_sum = 0.0
    tiles = 0
    for ty in range(0, height, tile):
        for tx in range(0, width, tile):
            xs = range(tx, min(tx + tile, width))
            ys = range(ty, min(ty + tile, height))
            va, vb = [], []
            for y in ys:
                base = y * width
                for x in xs:
                    va.append(la[base + x])
                    vb.append(lb[base + x])
            m = len(va)
            ma = sum(va) / m
            mb = sum(vb) / m
            var_a = sum((v - ma) ** 2 for v in va) / m
            var_b = sum((v - mb) ** 2 for v in vb) / m
            cov = sum((va[i] - ma) * (vb[i] - mb) for i in range(m)) / m
            ssim_sum += ((2 * ma * mb + c1) * (2 * cov + c2)) / (
                (ma * ma + mb * mb + c1) * (var_a + var_b + c2)
            )
            tiles += 1
    ssim = ssim_sum / tiles if tiles else 1.0

    pae = 0
    diff_count = 0
    ae_count = 0
    diff_rgb = bytearray(n * 3)
    for i in range(n):
        d = max(
            abs(rgba_a[i * 4 + c] - rgba_b[i * 4 + c]) for c in range(4)
        )
        if d:
            diff_count += 1
            if d > pae:
                pae = d
            if d > AE_TOLERANCE:
                ae_count += 1
        amp = min(255, d * 8)
        diff_rgb[i * 3] = 255
        diff_rgb[i * 3 + 1] = 255 - amp
        diff_rgb[i * 3 + 2] = 255 - amp
    return {
        "ssim": ssim,
        "pae": pae,
        "ae_pct": 100.0 * ae_count / n if n else 0.0,
        "diff_pct": 100.0 * diff_count / n if n else 0.0,
        "diff_png": encode_png_rgb(width, height, bytes(diff_rgb)),
    }


# ----------------------------------------------------------------------------
# Input loading
# ----------------------------------------------------------------------------


def sanitize(name):
    """Mirror artifacts::generate_artifact_path component sanitization."""
    return re.sub(r"[^a-z0-9]", "_", name.lower())


def artifact_name(backend, scene):
    return f"{sanitize(backend)}_{sanitize(scene)}.png"


def load_inputs(results_dir, oracle_json, memory_json=None):
    results_path = results_dir / "results.json"
    if not results_path.is_file():
        sys.exit(f"error: {results_path} not found")
    results = json.loads(results_path.read_text(encoding="utf-8"))
    oracle = None
    if oracle_json:
        opath = Path(oracle_json)
        if not opath.is_file():
            sys.exit(f"error: oracle JSON {opath} not found")
        oracle = json.loads(opath.read_text(encoding="utf-8"))
    else:
        default = results_dir / "oracle.json"
        if default.is_file():
            oracle = json.loads(default.read_text(encoding="utf-8"))

    memory = None
    if memory_json:
        mpath = Path(memory_json)
        if not mpath.is_file():
            sys.exit(f"error: memory JSON {mpath} not found")
        memory = json.loads(mpath.read_text(encoding="utf-8"))
    else:
        default_mem = results_dir / "memory.json"
        if default_mem.is_file():
            memory = json.loads(default_mem.read_text(encoding="utf-8"))

    return results, oracle, memory

# ----------------------------------------------------------------------------
# HTML rendering
# ----------------------------------------------------------------------------

_CSS = """
:root{
  --bg:#f6f7f9; --fg:#1c2128; --muted:#59636e; --card:#ffffff;
  --line:#d8dee4; --accent:#0969da; --ok:#1a7f37; --okbg:#dafbe1;
  --warn:#9a6700; --warnbg:#fff8c5; --bad:#cf222e; --badbg:#ffebe9;
  --na:#59636e; --nabg:#eaeef2; --ref:#8250df; --refbg:#fbefff;
}
@media (prefers-color-scheme: dark){
  :root{
    --bg:#0d1117; --fg:#e6edf3; --muted:#9198a1; --card:#161b22;
    --line:#30363d; --accent:#4493f8; --ok:#3fb950; --okbg:#12261e;
    --warn:#d29922; --warnbg:#272115; --bad:#f85149; --badbg:#25171c;
    --na:#9198a1; --nabg:#21262d; --ref:#ab7df8; --refbg:#221a32;
  }
}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);
  font:18px/1.55 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
main{max-width:1440px;margin:0 auto;padding:28px 24px 96px}
h1{font-size:31px;margin:8px 0 2px}
h2{font-size:24px;margin:48px 0 14px;padding-top:22px;border-top:1px solid var(--line)}
h3{font-size:19px;margin:26px 0 10px}
.sub{color:var(--muted);margin:0 0 22px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:18px 22px;margin:14px 0}
.meta{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:7px 28px;font-size:16px}
.meta b{color:var(--muted);font-weight:600;margin-right:6px}
table{border-collapse:collapse;width:100%;font-size:16px}
th,td{padding:7px 12px;text-align:left;border-bottom:1px solid var(--line);white-space:nowrap}
th{color:var(--muted);font-weight:600}
td.num{text-align:right;font-variant-numeric:tabular-nums}
tr.best td{background:var(--okbg)}
.chip{display:inline-block;padding:2px 11px;border-radius:999px;font-size:14px;font-weight:600}
.chip.ok{color:var(--ok);background:var(--okbg)}
.chip.warn{color:var(--warn);background:var(--warnbg)}
.chip.bad{color:var(--bad);background:var(--badbg)}
.chip.na{color:var(--na);background:var(--nabg)}
.chip.ref{color:var(--ref);background:var(--refbg)}
.bar-row{display:grid;grid-template-columns:132px 1fr 190px;align-items:center;gap:12px;margin:4px 0}
.bar-name{font-size:15.5px;font-weight:600;text-align:right}
.bar-track{background:var(--nabg);border-radius:6px;height:22px;position:relative}
.bar-fill{background:linear-gradient(90deg,var(--accent),color-mix(in srgb,var(--accent) 60%,transparent));
  height:100%;border-radius:6px;min-width:2px}
.bar-row.is-best .bar-fill{background:linear-gradient(90deg,var(--ok),color-mix(in srgb,var(--ok) 60%,transparent))}
.bar-val{font-size:15px;color:var(--muted);font-variant-numeric:tabular-nums}
.gallery{display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:14px}
.shot{background:var(--card);border:1px solid var(--line);border-radius:8px;overflow:hidden;text-align:center}
.shot img{width:100%;height:156px;object-fit:cover;display:block;cursor:zoom-in;background:#fff}
.shot .cap{padding:7px 10px 10px;font-size:15px}
.shot .cap b{display:block;font-size:15.5px}
dialog{border:none;border-radius:12px;padding:0;max-width:92vw;background:var(--card);color:var(--fg)}
dialog::backdrop{background:rgba(0,0,0,.55)}
dialog .dwrap{padding:16px}
dialog img{max-width:86vw;max-height:70vh;display:block;margin:8px auto;background:#fff;border:1px solid var(--line)}
dialog .drow{display:flex;gap:16px;flex-wrap:wrap;justify-content:center}
dialog h4{margin:0 0 4px;text-align:center}
dialog p{margin:2px 0;text-align:center;font-size:15.5px;color:var(--muted)}
dialog button{margin:12px auto 0;display:block;padding:7px 22px;border-radius:6px;border:1px solid var(--line);
  background:var(--nabg);color:var(--fg);cursor:pointer;font-size:16px}
.legend{font-size:15px;color:var(--muted);margin:10px 0 0}
.footnote{font-size:15.5px;color:var(--muted)}
code{background:var(--nabg);border-radius:4px;padding:1px 6px;font-size:15px}
.tab-nav{display:flex;gap:10px;margin:20px 0 16px;border-bottom:1px solid var(--line);padding-bottom:12px}
.tab-btn{background:var(--card);border:1px solid var(--line);color:var(--fg);padding:9px 20px;border-radius:8px;cursor:pointer;font-weight:600;font-size:16px;transition:all .15s ease}
.tab-btn:hover{background:var(--nabg)}
.tab-btn.active{background:var(--accent);color:#fff;border-color:var(--accent)}
.tab-panel{display:none}
.tab-panel.active{display:block}
"""

_JS = """
function zoom(id){document.getElementById(id).showModal();}
document.addEventListener('click',e=>{
  if(e.target.tagName==='DIALOG')e.target.close();
});
function showTab(panelId, btn){
  const parent = btn.closest('.tab-container');
  parent.querySelectorAll('.tab-btn').forEach(b=>b.classList.remove('active'));
  parent.querySelectorAll('.tab-panel').forEach(p=>p.classList.remove('active'));
  btn.classList.add('active');
  const target = parent.querySelector('#' + panelId);
  if(target) target.classList.add('active');
}
"""


def esc(s):
    return (
        str(s)
        .replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )



def fmt_ms(ns):
    return f"{ns / 1e6:.3f}"

def fmt_kb(bytes_val):
    if bytes_val < 1024:
        return f"{bytes_val} B"
    elif bytes_val < 1024 * 1024:
        return f"{bytes_val / 1024.0:.1f} KiB"
    else:
        return f"{bytes_val / (1024.0 * 1024.0):.2f} MiB"


def ssim_chip(score):
    if score >= 0.999:
        cls, label = "ok", f"SSIM {score:.4f}"
    elif score >= 0.99:
        cls, label = "ok", f"SSIM {score:.4f}"
    elif score >= 0.95:
        cls, label = "warn", f"SSIM {score:.4f}"
    else:
        cls, label = "bad", f"SSIM {score:.4f}"
    return f'<span class="chip {cls}">{label}</span>'


def pae_chip(pae, ae_pct):
    """Worst-case badge: PAE (L-infinity) colored by severity.

    <= AE_TOLERANCE: within the AA/rounding fuzz -> green;
    <= 64: visible localized divergence (typically edge AA) -> amber;
    >  64: at least one pixel is badly wrong (e.g. a channel off by 25%+)
           -> red, regardless of how good SSIM looks.
    """
    if pae <= AE_TOLERANCE:
        cls = "ok"
    elif pae <= 64:
        cls = "warn"
    else:
        cls = "bad"
    return f'<span class="chip {cls}">L∞ {pae}</span>'


def classification_chip(cls_name):
    mapping = {
        "exact-union": ("ok", "exact-union"),
        "sum-then-saturate": ("warn", "sum+saturate"),
        "other-divergence": ("bad", "divergent"),
        "pass": ("ok", "pass"),
        "FAIL": ("bad", "FAIL"),
    }
    c, label = mapping.get(cls_name, ("na", cls_name))
    return f'<span class="chip {c}">{esc(label)}</span>'


def build_report(results, oracle, memory, results_dir, reference, assets_dir):
    meta = results.get("run_metadata", {})
    env = meta.get("environment", {})
    policy = meta.get("policy", {})
    cases = [c for c in results.get("cases", [])]

    backends = sorted({c["backend_id"] for c in cases})
    scenes = sorted({c["scene_id"] for c in cases})
    real_backends = [b for b in backends if b != "null"]

    if reference is None:
        # Default preference (owner decision, 2026-08-30): skia -- closest to
        # the analytic float expectation on interior blends (0.3-1.6/255 vs
        # cairo's 3.9 truncation drift) and exact-union class on overlap.
        for candidate in ("skia", "cairo"):
            if candidate in real_backends:
                reference = candidate
                break
        else:
            reference = real_backends[0] if real_backends else None
    if reference not in real_backends:
        sys.exit(f"error: reference backend '{reference}' not in results ({', '.join(real_backends)})")
    by_key = {(c["backend_id"], c["scene_id"]): c for c in cases}

    # Load artifacts + compute comparisons
    images = {}  # (backend, scene) -> (w,h,rgba,png_bytes)
    for c in cases:
        b, s = c["backend_id"], c["scene_id"]
        if b == "null":
            continue
        path = results_dir / artifact_name(b, s)
        if not path.is_file() and c.get("artifact_path"):
            path = Path(c["artifact_path"])
        if path.is_file():
            data = path.read_bytes()
            try:
                w, h, rgba = decode_png(data)
                images[(b, s)] = (w, h, rgba, data)
            except ValueError as e:
                print(f"warning: cannot decode {path.name}: {e}", file=sys.stderr)

    comparisons = {}  # (backend, scene) -> compare dict
    for s in scenes:
        ref_img = images.get((reference, s))
        if not ref_img:
            continue
        for b in real_backends:
            if b == reference:
                continue
            img = images.get((b, s))
            if not img:
                continue
            if img[0] != ref_img[0] or img[1] != ref_img[1]:
                continue
            comparisons[(b, s)] = compare_images(img[0], img[1], ref_img[2], img[2])

    # ---------------- HTML assembly ----------------
    out = []
    w = out.append
    w("<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>")
    w("<meta name='viewport' content='width=device-width,initial-scale=1'>")
    w("<title>VGCPU-Benchmark Report</title>")
    w(f"<style>{_CSS}</style></head><body><main>")

    # Header
    w("<h1>VGCPU-Benchmark Report</h1>")
    w(
        f"<p class='sub'>CPU-only 2D vector graphics: performance, rendering "
        f"gallery, cross-backend SSIM (reference: <b>{esc(reference)}</b>), "
        f"correctness census.</p>"
    )
    w("<div class='card'><div class='meta'>")
    rows = [
        ("Run", meta.get("timestamp", "?")),
        ("Suite", meta.get("suite_version", "?")),
        ("Commit", meta.get("git_commit", "") or "n/a"),
        ("Schema", results.get("schema_version", "?")),
        ("OS", f"{env.get('os_name','?')} {env.get('os_version','')}".strip()),
        ("CPU", env.get("cpu_model", "?")),
        ("Arch", env.get("arch", "?")),
        ("Cores", env.get("cpu_cores", "?")),
        ("Compiler", f"{env.get('compiler_name','?')} {env.get('compiler_version','')}".strip()),
        (
            "Discipline",
            (
                f"pinned to CPU {env['pinned_cpus']}"
                if env.get("pinned_cpus")
                else "not pinned"
            )
            + (f" · governor {env['cpu_governor']}" if env.get("cpu_governor") else ""),
        ),
        (
            "Policy",
            f"warmup {policy.get('warmup_iterations','?')} / iters "
            f"{policy.get('measurement_iterations','?')} / reps "
            f"{policy.get('repetitions','?')} / threads {policy.get('thread_count','?')}",
        ),
        ("Backends", f"{len(real_backends)} + null baseline"),
        ("Scenes", str(len(scenes))),
    ]
    for k, v in rows:
        w(f"<div><b>{esc(k)}</b>{esc(v)}</div>")
    w("</div></div>")

    # ---------------- Performance ----------------
    w("<h2>Performance</h2>")
    w(
        "<p class='sub'>Median wall-clock time per frame (p50, lower is "
        "better); p90 alongside. Two distinct benchmarking modes below:</p>"
    )

    def render_perf_tab(stat_key, desc_text):
        w(f"<p class='sub'>{desc_text}</p>")
        w("<div class='card' style='overflow-x:auto'><table><thead><tr><th>scene \\ backend</th>")
        for b in real_backends:
            mark = " (ref)" if b == reference else ""
            w(f"<th>{esc(b)}{esc(mark)}</th>")
        w("</tr></thead><tbody>")
        for s in scenes:
            vals = {}
            for b in real_backends:
                c = by_key.get((b, s))
                if c and c.get("decision") == "EXECUTE" and stat_key in c and c[stat_key].get("wall_p50_ns", 0) > 0:
                    vals[b] = c[stat_key]["wall_p50_ns"]
            best = min(vals.values()) if vals else None
            w(f"<tr><td>{esc(s)}</td>")
            for b in real_backends:
                if b in vals:
                    cell = fmt_ms(vals[b]) + " ms"
                    if vals[b] == best:
                        cell = f"<b style='color:var(--ok)'>{cell}</b>"
                    w(f"<td class='num'>{cell}</td>")
                else:
                    w("<td class='num'>—</td>")
            w("</tr>")
        w("</tbody></table>")
        w("<p class='legend'>Bold green = fastest for the scene. The <code>null</code> backend is excluded.</p></div>")

        for s in scenes:
            entries = []
            for b in real_backends:
                c = by_key.get((b, s))
                if c and c.get("decision") == "EXECUTE" and stat_key in c and c[stat_key].get("wall_p50_ns", 0) > 0:
                    entries.append((b, c[stat_key]["wall_p50_ns"], c[stat_key]["wall_p90_ns"]))
            if not entries:
                continue
            entries.sort(key=lambda e: e[1])
            max_ns = max(e[1] for e in entries)
            w(f"<h3>{esc(s)}</h3><div class='card'>")
            for i, (b, p50, p90) in enumerate(entries):
                pct = 100.0 * p50 / max_ns if max_ns else 0
                best_cls = " is-best" if i == 0 else ""
                w(
                    f"<div class='bar-row{best_cls}'><div class='bar-name'>{esc(b)}</div>"
                    f"<div class='bar-track'><div class='bar-fill' style='width:{pct:.1f}%'></div></div>"
                    f"<div class='bar-val'>{fmt_ms(p50)} ms <span style='opacity:.65'>(p90 {fmt_ms(p90)})</span></div></div>"
                )
            w("</div>")

    w("<div class='tab-container'>")
    w("<div class='tab-nav'>")
    w("<button class='tab-btn active' onclick=\"showTab('tab-prebaked', this)\">Pre-baked (Draw time only)</button>")
    w("<button class='tab-btn' onclick=\"showTab('tab-lifecycle', this)\">Full-lifecycle (Create + Draw + Destroy)</button>")
    w("</div>")

    w("<div id='tab-prebaked' class='tab-panel active'>")
    render_perf_tab("stats", "<b>Mode A — Pre-baked geometry (Retained):</b> All path and geometry objects are pre-created during <code>Prepare()</code> outside the benchmark loop. The timed measurement contains <b>only the drawing/rasterization loop</b> (pure drawing throughput).")
    w("</div>")

    w("<div id='tab-lifecycle' class='tab-panel'>")
    render_perf_tab("lifecycle_stats", "<b>Mode B — Full lifecycle:</b> Benchmark measures the complete frame lifecycle executed in 3 sequential loops: <b>Loop 1: Create all native paths</b> &rarr; <b>Loop 2: Draw all</b> &rarr; <b>Loop 3: Destroy all</b> (metric is the sum $T_{\\text{create}} + T_{\\text{draw}} + T_{\\text{destroy}}$).")
    w("</div>")
    w("</div>")
    # ---------------- Gallery + SSIM ----------------
    if images:
        w("<h2>Rendering gallery — SSIM &amp; PAE (L∞)</h2>")
        w(
            f"<p class='sub'>Every backend's untimed render per scene, compared "
            f"against <b>{esc(reference)}</b>'s render of the same scene. Two "
            f"complementary metrics (industry pair, cf. ImageMagick): "
            f"<b>SSIM</b> is structural and averages — a single badly-wrong "
            f"pixel hides inside a ~1.0 score; <b>PAE</b> (peak absolute "
            f"error, the L∞/Chebyshev norm) is the worst per-channel "
            f"difference of any single pixel — it catches exactly that. "
            f"Chips: L∞ ≤ {AE_TOLERANCE} within AA/rounding fuzz, ≤ 64 "
            f"localized edge divergence, &gt; 64 at least one pixel badly "
            f"wrong. Both measure agreement with the reference, not "
            f"correctness. Click an image for the amplified difference map "
            f"and the AE bad-pixel percentage.</p>"
        )
        dlg_id = 0
        dialogs = []
        for s in scenes:
            have = [(b, images.get((b, s))) for b in real_backends]
            have = [(b, img) for b, img in have if img]
            if not have:
                continue
            w(f"<h3>{esc(s)}</h3><div class='gallery'>")
            for b, img in have:
                dlg_id += 1
                did = f"d{dlg_id}"
                cmp_res = comparisons.get((b, s))
                if b == reference:
                    badge = "<span class='chip ref'>reference</span>"
                elif cmp_res:
                    badge = ssim_chip(cmp_res["ssim"]) + pae_chip(
                        cmp_res["pae"], cmp_res["ae_pct"]
                    )
                else:
                    badge = "<span class='chip na'>no compare</span>"
                c = by_key.get((b, s))
                ms = (
                    fmt_ms(c["stats"]["wall_p50_ns"]) + " ms"
                    if c and c.get("decision") == "EXECUTE"
                    else "—"
                )
                # External assets (owner decision, 2026-08-30): images live in
                # <output_dir>/png/ and are linked relatively, replacing the
                # base64 inlining that ballooned the report to ~200 MB once
                # the MPVG map corpus landed.
                name = f"{b}_{s.replace('/', '_')}.png"
                (assets_dir / name).write_bytes(img[3])
                src = f"png/{name}"
                w(
                    f"<figure class='shot' style='margin:0'>"
                    f"<img src='{src}' alt='{esc(b)} / {esc(s)}' loading='lazy' onclick=\"zoom('{did}')\">"
                    f"<figcaption class='cap'><b>{esc(b)}</b>{badge}<br>"
                    f"<span style='color:var(--muted)'>{ms}</span></figcaption></figure>"
                )
                # dialog content
                d = [f"<dialog id='{did}'><div class='dwrap'><h4>{esc(b)} — {esc(s)}</h4>"]
                if b == reference:
                    d.append("<p>Reference backend for SSIM comparisons.</p>")
                elif cmp_res:
                    d.append(
                        f"<p>SSIM {cmp_res['ssim']:.5f} vs {esc(reference)} · "
                        f"PAE (L∞) {cmp_res['pae']}/255 · "
                        f"AE@{AE_TOLERANCE}: {cmp_res['ae_pct']:.3f}% px · "
                        f"{cmp_res['diff_pct']:.2f}% px differ at all</p>"
                    )
                d.append("<div class='drow'><div>")
                d.append(f"<p>render</p><img src='{src}' alt='render'>")
                d.append("</div>")
                if cmp_res:
                    diff_name = f"diff_{b}_{s.replace('/', '_')}.png"
                    (assets_dir / diff_name).write_bytes(cmp_res["diff_png"])
                    d.append(
                        f"<div><p>difference ×8 vs {esc(reference)}</p>"
                        f"<img src='png/{diff_name}' alt='diff'></div>"
                    )
                d.append("</div><button onclick='this.closest(\"dialog\").close()'>Close</button>")
                d.append("</div></dialog>")
                dialogs.append("".join(d))
            w("</div>")
        out.extend(dialogs)

    # ---------------- Correctness census ----------------
    if oracle:
        w("<h2>Correctness census — self-overlap coverage oracle</h2>")
        w(
            "<p class='sub'>Exact-value oracle (ADR-0004): four "
            "single-drawcall cases with hand-derivable expected coverage. "
            "Control cases (A, D) have one correct answer and are hard "
            "requirements; overlap cases (B, C) classify the backend's "
            "architecture. <span class='chip ok'>exact-union</span> computes "
            "the true coverage of the geometric union; "
            "<span class='chip warn'>sum+saturate</span> sums per-contour "
            "coverage and clamps — up to 2× error on self-overlapping paths.</p>"
        )
        case_names = sorted({r["case"] for r in oracle})
        o_backends = sorted({r["backend"] for r in oracle})
        by_bc = {(r["backend"], r["case"]): r for r in oracle}
        w("<div class='card' style='overflow-x:auto'><table><thead><tr><th>backend</th>")
        for cn in case_names:
            w(f"<th>{esc(cn)}</th>")
        w("</tr></thead><tbody>")
        for b in o_backends:
            w(f"<tr><td><b>{esc(b)}</b></td>")
            for cn in case_names:
                r = by_bc.get((b, cn))
                if not r:
                    w("<td>—</td>")
                    continue
                chip = classification_chip(r["classification"])
                w(
                    f"<td>{chip}<br><span style='color:var(--muted);font-size:12px'>"
                    f"{r['measured']:.3f} <span style='opacity:.7'>(exp "
                    f"{r['expected_union']:.3f})</span></span></td>"
                )
            w("</tr>")
        w("</tbody></table>")
        w(
            "<p class='footnote'>measured = alpha coverage read back at the "
            "probe pixel; exp = analytically correct union coverage. A FAIL "
            "on a control case flags either a genuine backend defect or an "
            "oracle-calibration gap (e.g. a backend with a documented coarse "
            "sub-pixel lattice) — see ADR-0004 for the open items.</p>"
        )
        w("</div>")

    # ---------------- Working memory profile ----------------
    if memory:
        w("<h2>Working memory profile — Heap allocations &amp; Peak live working set</h2>")
        w(
            "<p class='sub'>Isolated single-frame heap memory profiling (measured "
            "separately via <code>vgcpu-benchmark profile-memory</code> to ensure zero "
            "timing contamination on the performance benchmarks): <b>allocs/frame</b> is "
            "the count of dynamic allocation calls (malloc/new) during a single render pass; "
            "<b>peak heap</b> is the maximum live working heap memory; <b>total churn</b> "
            "is the cumulative memory requested.</p>"
        )
        mem_cases = memory.get("cases", [])
        by_mem = {(c["backend_id"], c["scene_id"]): c for c in mem_cases}

        w("<div class='card' style='overflow-x:auto'><table><thead><tr><th>scene \\ backend</th>")
        for b in real_backends:
            w(f"<th>{esc(b)}</th>")
        w("</tr></thead><tbody>")
        for s in scenes:
            w(f"<tr><td>{esc(s)}</td>")
            for b in real_backends:
                mc = by_mem.get((b, s))
                if mc and mc.get("decision") == "EXECUTE" and "memory" in mc:
                    m = mc["memory"]
                    allocs = m.get("alloc_count", 0)
                    peak = m.get("peak_heap_bytes", 0)
                    churn = m.get("total_alloc_bytes", 0)
                    w(
                        f"<td class='num'><b>{fmt_kb(peak)}</b><br><span style='color:var(--muted);font-size:12px'>"
                        f"{allocs:,} allocs<br>{fmt_kb(churn)} churn</span></td>"
                    )
                elif mc and mc.get("decision") == "SKIP":
                    w("<td class='num'><span class='chip na'>skip</span></td>")
                else:
                    w("<td class='num'>—</td>")
            w("</tr>")
        w("</tbody></table></div>")

    # Footer
    w(
        "<p class='footnote' style='margin-top:36px'>Generated by "
        "<code>tools/html_report.py</code> (ADR-0005) from "
        "<code>results.json</code> + PNG artifacts"
        + (" + <code>oracle.json</code>" if oracle else "")
        + ". Self-contained file: images embedded, no external assets.</p>"
    )
    w(f"<script>{_JS}</script>")
    w("</main></body></html>")
    return "".join(out)


def main():
    ap = argparse.ArgumentParser(
        description="Generate an HTML report (with a png/ asset folder) from a vgcpu-benchmark output directory."
    )
    ap.add_argument("results_dir", help="directory containing results.json and PNG artifacts")
    ap.add_argument("-o", "--output", default=None, help="output HTML path (default: <results_dir>/report.html)")
    ap.add_argument("--reference", default=None, help="reference backend for SSIM (default: skia, else cairo, else first)")
    ap.add_argument("--oracle-json", default=None, help="correctness census JSON (default: <results_dir>/oracle.json if present)")
    ap.add_argument("--memory-json", default=None, help="working memory profile JSON (default: <results_dir>/memory.json if present)")
    args = ap.parse_args()

    results_dir = Path(args.results_dir)
    results, oracle, memory = load_inputs(results_dir, args.oracle_json, args.memory_json)

    out_path = Path(args.output) if args.output else results_dir / "report.html"
    # Owner decision (2026-08-30): gallery images are NOT inlined; they are
    # written to <output_dir>/png/ and linked relatively. Keeps the HTML
    # small with the heavyweight MPVG corpus in the suite.
    assets_dir = out_path.parent / "png"
    assets_dir.mkdir(parents=True, exist_ok=True)

    html = build_report(results, oracle, memory, results_dir, args.reference, assets_dir)
    out_path.write_text(html, encoding="utf-8")
    size_kb = out_path.stat().st_size / 1024
    n_assets = len(list(assets_dir.glob("*.png")))
    print(f"report: {out_path} ({size_kb:.0f} KiB, {n_assets} images in {assets_dir})")


if __name__ == "__main__":
    main()
