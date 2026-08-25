"""Local, dependency-free browser explorer for generated asset manifests."""

from __future__ import annotations

import json
import mimetypes
import struct
import webbrowser
import zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, unquote, urlparse

from .common import resolve

_INDEX_HTML = r"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>OpenTony Asset Explorer</title>
<style>
:root { color-scheme: dark; --bg:#101217; --panel:#171a21; --panel2:#20242d; --line:#303642; --text:#e7eaf0; --muted:#9da6b5; --accent:#77d4c5; --warn:#efbd70; }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--text); font:14px/1.45 system-ui,sans-serif; }
header { min-height:58px; display:flex; align-items:center; gap:16px; padding:10px 22px; border-bottom:1px solid var(--line); background:#13161c; }
header h1 { margin:0; font:600 18px/1 system-ui,sans-serif; letter-spacing:.02em; }
header h1 a { color:var(--text); text-decoration:none; }
header span { flex:1 1 220px; min-width:0; color:var(--muted); overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
.library-link { flex:0 0 auto; color:var(--accent); text-decoration:none; }
.nav-toggle { display:none; flex:0 0 auto; }
main { display:grid; grid-template-columns:280px minmax(0,1fr) 330px; height:calc(100vh - 58px); }
aside, section { min-width:0; overflow:auto; }
aside { padding:18px 14px; border-right:1px solid var(--line); background:var(--panel); }
section { padding:20px; }
#details { padding:18px 16px; border-left:1px solid var(--line); background:var(--panel); }
h2,h3 { margin:0 0 12px; font-weight:600; }
h2 { font-size:20px; } h3 { font-size:14px; color:var(--muted); text-transform:uppercase; letter-spacing:.08em; }
.muted { color:var(--muted); }
.nav-group { margin:20px 0 24px; }
button, .link-button { border:1px solid var(--line); background:var(--panel2); color:var(--text); border-radius:6px; padding:8px 10px; cursor:pointer; text-align:left; }
button:hover, .link-button:hover { border-color:var(--accent); color:var(--accent); }
.nav-button { display:block; width:100%; margin:5px 0; }
.cards { display:grid; grid-template-columns:repeat(auto-fit,minmax(150px,1fr)); gap:10px; margin:16px 0 26px; }
.card { padding:14px; border:1px solid var(--line); border-radius:8px; background:var(--panel); }
.card strong { display:block; font-size:24px; color:var(--accent); }
.card small { color:var(--muted); }
.texture-grid { display:grid; grid-template-columns:repeat(auto-fill,minmax(130px,1fr)); gap:12px; }
.texture { overflow:hidden; border:1px solid var(--line); border-radius:8px; background:var(--panel); }
.texture img { display:block; width:100%; aspect-ratio:1.5; object-fit:contain; image-rendering:auto; background:#090b0e; }
.texture div { padding:8px; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; color:var(--muted); font-size:12px; }
.model-list { display:grid; grid-template-columns:repeat(auto-fill,minmax(200px,1fr)); gap:8px; }
.model-list button { overflow:hidden; white-space:nowrap; text-overflow:ellipsis; }
.filter-row { display:flex; gap:8px; align-items:center; flex-wrap:wrap; margin:14px 0; }
.filter-row input { flex:1 1 260px; min-width:180px; }
input, select { border:1px solid var(--line); border-radius:6px; padding:8px 10px; background:var(--panel2); color:var(--text); font:inherit; }
input:focus, select:focus, button:focus-visible, a:focus-visible { outline:2px solid var(--accent); outline-offset:2px; }
canvas { display:block; width:100%; height:420px; border:1px solid var(--line); border-radius:8px; background:radial-gradient(circle at 50% 40%,#252b35,#0d0f13 75%); }
#blockmap-canvas { height:220px; margin-top:12px; }
pre { max-height:520px; overflow:auto; padding:12px; border:1px solid var(--line); border-radius:8px; background:#0c0e12; color:#c7d0db; white-space:pre-wrap; word-break:break-word; }
.toolbar { display:flex; gap:8px; align-items:center; flex-wrap:wrap; margin-bottom:12px; }
a { color:var(--accent); } .pill { display:inline-block; padding:2px 7px; border-radius:999px; background:var(--panel2); color:var(--muted); font-size:12px; }
@media (max-width:1000px) { main { grid-template-columns:220px minmax(0,1fr); } #details { display:none; } }
@media (max-width:700px) {
  header { align-items:flex-start; flex-wrap:wrap; gap:8px 12px; padding:12px 14px; }
  header h1 { flex:1 1 190px; line-height:1.2; }
  header span { order:3; flex-basis:100%; white-space:normal; overflow:visible; text-overflow:clip; overflow-wrap:anywhere; }
  .library-link { margin-left:auto; }
  .nav-toggle { display:block; order:2; }
  main { display:block; height:auto; }
  #nav { display:none; border:0; }
  main.nav-open #nav { display:block; }
  aside, section { border:0; }
  #nav { max-height:none; }
  canvas { height:300px; }
}
</style>
</head>
<body>
<header><h1><a href="/">OpenTony Asset Explorer</a></h1><span id="source">Loading manifest…</span><a class="library-link" href="/">Asset Library</a><button class="nav-toggle" type="button" onclick="toggleNav()" aria-expanded="false">Menu</button></header>
<main id="layout">
<aside id="nav"></aside>
<section id="content"><p class="muted">Loading…</p></section>
<aside id="details"><h3>Selection</h3><p class="muted">Choose a scene, model, texture, or blockmap from the left.</p></aside>
</main>
<script>
const PACKAGE_PATH = null;
const state = { manifest:null, object:null, animation:0, paused:false, frame:null };
const $ = id => document.getElementById(id);
const esc = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const packageQuery = () => PACKAGE_PATH ? `&package=${encodeURIComponent(PACKAGE_PATH)}` : '';
const fileUrl = path => '/files/' + path.split('/').map(encodeURIComponent).join('/') + (PACKAGE_PATH ? `?package=${encodeURIComponent(PACKAGE_PATH)}` : '');
const apiUrl = path => '/api/obj?path=' + encodeURIComponent(path) + packageQuery();
function card(label, value) { return `<div class="card"><strong>${esc(value)}</strong><small>${esc(label)}</small></div>`; }
function navButton(label, action) { return `<button class="nav-button" onclick="${action}">${esc(label)}</button>`; }
function toggleNav() {
  const layout = $('layout'), button = document.querySelector('.nav-toggle');
  const open = layout.classList.toggle('nav-open');
  button.setAttribute('aria-expanded', String(open));
  button.textContent = open ? 'Close' : 'Menu';
}

async function init() {
  state.manifest = await fetch('/api/manifest' + (PACKAGE_PATH ? `?package=${encodeURIComponent(PACKAGE_PATH)}` : '')).then(r => r.json());
  const m = state.manifest;
  $('source').textContent = m.source?.path || 'generated asset directory';
  renderNav();
  showOverview();
}

function renderNav() {
  const m = state.manifest;
  let html = '<div class="nav-group"><h3>Explore</h3>';
  html += navButton('Overview', 'showOverview()');
  if (m.scene) html += navButton('Placed scene', `showObj('${m.scene.path}', 'Placed scene')`);
  if (m.collision) html += navButton('Collision mesh', `showObj('${m.collision.path}', 'Collision mesh')`);
  if (m.blockmap) html += navButton('Blockmap grid', 'showBlockmap()');
  html += '</div><div class="nav-group"><h3>Collections</h3>';
  html += navButton(`Models (${m.models?.length || 0})`, 'showModels()');
  html += navButton(`Textures (${m.textures?.length || 0})`, 'showTextures()');
  html += '</div>';
  $('nav').innerHTML = html;
}

function showOverview() {
  const m = state.manifest, f = m.format || {};
  $('content').innerHTML = `<h2>Asset overview</h2>
    <p class="muted">${esc(m.extracted_path || 'Generated PSX asset package')}</p>
    <div class="cards">
      ${card('models', m.models?.length || f.model_count || 0)}
      ${card('placed objects', m.objects?.length || f.object_count || 0)}
      ${card('textures', m.textures?.length || f.texture_count || 0)}
      ${card('blockmap references', f.blockmap_object_references || 0)}
      ${card('collision faces', m.collision?.face_count || 0)}
    </div>
    <h3>Generated files</h3>
    <p>${m.scene ? `<a href="${fileUrl(m.scene.path)}">scene.obj</a> · ` : ''}${m.collision ? `<a href="${fileUrl(m.collision.path)}">collision.obj</a> · <a href="${fileUrl(m.blockmap.path)}">blockmap.json</a>` : 'No collision blockmap in this package.'}</p>
    <h3>Format</h3><pre>${esc(JSON.stringify(f, null, 2))}</pre>`;
  $('details').innerHTML = '<h3>Selection</h3><p class="muted">Choose an asset from the left to inspect it.</p>';
}

function showModels() {
  const models = state.manifest.models || [];
  $('content').innerHTML = `<h2>Models</h2><p class="muted">Select a model for a rotating preview. Search by index, name, or face count.</p>
    <div class="filter-row"><input id="model-filter" type="search" placeholder="Filter models…" aria-label="Filter models"><span id="model-count" class="pill"></span></div>
    <div id="model-list" class="model-list"></div>`;
  const filter = $('model-filter');
  filter.addEventListener('input', () => renderModels(models, filter.value));
  renderModels(models, '');
}

function renderModels(models, query) {
  const needle = query.trim().toLowerCase();
  const filtered = models.filter(model => {
    const label = `model_${String(model.index).padStart(4,'0')} ${model.face_count} ${model.name == null ? 'unnamed' : Number(model.name).toString(16)}`;
    return !needle || label.toLowerCase().includes(needle);
  });
  $('model-count').textContent = `${filtered.length} of ${models.length}`;
  $('model-list').innerHTML = filtered.length ? filtered.map(model => `<button onclick="showObj('${model.path}', 'Model ${model.index}')">model_${String(model.index).padStart(4,'0')} · ${model.face_count} faces · ${model.name == null ? 'unnamed' : '0x' + Number(model.name).toString(16).padStart(8,'0')}</button>`).join('') : '<p class="empty">No models match this filter.</p>';
}

function showTextures() {
  const textures = state.manifest.textures || [];
  $('content').innerHTML = `<h2>Textures</h2><p class="muted">Decoded PPM textures are converted to browser-compatible PNG responses on demand.</p>
    <div class="texture-grid">${textures.map(texture => `<a class="texture" href="${fileUrl(texture.path)}" target="_blank"><img src="${fileUrl(texture.path)}" loading="lazy" alt="texture ${texture.index}"><div>0x${Number(texture.name).toString(16).padStart(8,'0')} · ${texture.width}×${texture.height}</div></a>`).join('')}</div>`;
}

async function showObj(path, title) {
  if (state.frame) cancelAnimationFrame(state.frame);
  state.frame = null;
  $('content').innerHTML = `<h2>${esc(title)}</h2><p class="muted">Parsing ${esc(path)}…</p>`;
  const data = await fetch(apiUrl(path)).then(r => r.json());
  state.object = data;
  $('content').innerHTML = `<div class="toolbar"><h2>${esc(title)}</h2><button type="button" onclick="toggleRotation()" id="rotate-toggle">Pause rotation</button><button type="button" onclick="resetObjectView()">Reset view</button><a href="${fileUrl(path)}" target="_blank">open raw OBJ</a></div>
    <canvas id="preview" width="1000" height="600"></canvas>
    <div class="cards">${card('vertices', data.vertex_count)}${card('faces', data.face_count)}${card('materials', data.materials.length)}${card('objects', data.objects.length)}</div>`;
  $('details').innerHTML = `<h3>OBJ metadata</h3><pre>${esc(JSON.stringify({path, bounds:data.bounds, vertex_count:data.vertex_count, face_count:data.face_count, materials:data.materials, objects:data.objects}, null, 2))}</pre>`;
  drawObject();
}

function colorFor(name) {
  let h = 0; for (const c of String(name || 'surface')) h = (h * 31 + c.charCodeAt(0)) >>> 0;
  return `hsl(${h % 360} 55% 62%)`;
}
function drawObject() {
  const canvas = $('preview'); if (!canvas || !state.object) return;
  const ctx = canvas.getContext('2d'), data = state.object;
  const bounds = data.bounds, center = [(bounds[0]+bounds[3])/2,(bounds[1]+bounds[4])/2,(bounds[2]+bounds[5])/2];
  const span = Math.max(bounds[3]-bounds[0], bounds[4]-bounds[1], bounds[5]-bounds[2], 1);
  const angle = state.animation / 160;
  if (!state.paused) state.animation++;
  const projected = data.vertices.map(v => {
    const x=v[0]-center[0], y=v[1]-center[1], z=v[2]-center[2];
    const rx=x*Math.cos(angle)-z*Math.sin(angle), rz=x*Math.sin(angle)+z*Math.cos(angle);
    return [canvas.width/2 + rx/span*canvas.width*.9, canvas.height/2 - y/span*canvas.height*.9, rz];
  });
  ctx.clearRect(0,0,canvas.width,canvas.height);
  const faces = data.faces.flatMap(face => face.vertices.length === 3 ? [face] : face.vertices.slice(1,-1).map((_,i) => ({vertices:[face.vertices[0],face.vertices[i+1],face.vertices[i+2]], material:face.material})));
  faces.sort((a,b) => ((projected[b.vertices[0]][2]+projected[b.vertices[1]][2]+projected[b.vertices[2]][2]) - (projected[a.vertices[0]][2]+projected[a.vertices[1]][2]+projected[a.vertices[2]][2])));
  for (const face of faces) {
    ctx.beginPath(); face.vertices.forEach((index,i) => { const p=projected[index]; i ? ctx.lineTo(p[0],p[1]) : ctx.moveTo(p[0],p[1]); }); ctx.closePath();
    ctx.fillStyle=colorFor(face.material); ctx.globalAlpha=.36; ctx.fill(); ctx.globalAlpha=.85; ctx.strokeStyle=colorFor(face.material); ctx.stroke();
  }
  state.frame = requestAnimationFrame(drawObject);
}

function toggleRotation() {
  state.paused = !state.paused;
  const button = $('rotate-toggle');
  if (button) button.textContent = state.paused ? 'Resume rotation' : 'Pause rotation';
}

function resetObjectView() {
  state.animation = 0;
}

async function showBlockmap() {
  const m=state.manifest; const data=await fetch(fileUrl(m.blockmap.path)).then(r=>r.json()); const b=data.blockmaps[0];
  $('content').innerHTML=`<h2>Blockmap grid</h2><p class="muted">${b.cell_counts[0]}×${b.cell_counts[1]} cells; cell colors indicate referenced object count.</p><canvas id="blockmap-canvas" width="800" height="500"></canvas><p><a href="${fileUrl(m.blockmap.path)}" target="_blank">open blockmap.json</a></p>`;
  $('details').innerHTML=`<h3>Blockmap metadata</h3><pre>${esc(JSON.stringify({bounds:b.bounds, bounds_fixed:b.bounds_fixed, cell_counts:b.cell_counts},null,2))}</pre>`;
  const canvas=$('blockmap-canvas'),ctx=canvas.getContext('2d'), max=Math.max(...b.cells.map(c=>c.objects.length),1), w=canvas.width/b.cell_counts[0], h=canvas.height/b.cell_counts[1];
  for(const cell of b.cells){ const alpha=.12+.88*cell.objects.length/max; ctx.fillStyle=`rgba(119,212,197,${alpha})`; ctx.fillRect(cell.x*w,cell.z*h,w-1,h-1); if(cell.objects.length){ctx.fillStyle='#d8fff8';ctx.font='12px system-ui';ctx.fillText(cell.objects.length,cell.x*w+4,cell.z*h+15);} }
}

init().catch(error => { $('content').innerHTML=`<h2>Explorer error</h2><pre>${esc(error)}</pre>`; });
</script>
</body>
</html>"""

_DASHBOARD_HTML = r"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>OpenTony Asset Library</title>
<style>
:root { color-scheme:dark; --bg:#101217; --panel:#171a21; --panel2:#20242d; --line:#303642; --text:#e7eaf0; --muted:#9da6b5; --accent:#77d4c5; }
* { box-sizing:border-box; } body { margin:0; background:var(--bg); color:var(--text); font:14px/1.45 system-ui,sans-serif; }
header { border-bottom:1px solid var(--line); background:#13161c; }
.header-inner, main { max-width:1200px; margin:0 auto; }
.header-inner { padding:24px 22px; }
h1 { margin:0; font-size:24px; } header p { margin:6px 0 0; color:var(--muted); }
main { padding:24px 22px; }
.library-toolbar { padding:16px; margin-bottom:18px; border:1px solid var(--line); border-radius:10px; background:var(--panel); }
.toolbar-heading { display:flex; align-items:flex-start; justify-content:space-between; gap:16px; }
.toolbar-heading h2 { margin:0; font-size:18px; }
.toolbar-heading p { margin:4px 0 0; color:var(--muted); }
.filter-row { display:flex; gap:10px; align-items:end; flex-wrap:wrap; margin-top:16px; }
.filter-row label { display:flex; flex-direction:column; gap:5px; color:var(--muted); font-size:12px; }
.filter-row .search-field { flex:1 1 300px; }
input, select { border:1px solid var(--line); border-radius:6px; padding:9px 10px; background:var(--panel2); color:var(--text); font:inherit; }
input:focus, select:focus, button:focus-visible, a:focus-visible { outline:2px solid var(--accent); outline-offset:2px; }
.check { flex-direction:row !important; align-items:center; padding:9px 0; }
button { border:1px solid var(--line); border-radius:6px; padding:9px 10px; background:var(--panel2); color:var(--text); cursor:pointer; font:inherit; }
button:hover { border-color:var(--accent); color:var(--accent); }
.grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(270px,1fr)); gap:14px; }
.package { display:block; min-width:0; padding:18px; border:1px solid var(--line); border-radius:10px; background:var(--panel); color:var(--text); text-decoration:none; transition:border-color .15s, transform .15s; }
.package:hover { border-color:var(--accent); transform:translateY(-1px); }
.package.empty-package { opacity:.72; }
.package-top { display:flex; align-items:center; justify-content:space-between; gap:10px; }
.package h2 { min-width:0; margin:0 0 6px; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; font-size:17px; }
.package p { margin:4px 0 14px; color:var(--muted); overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
.stats { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:5px 12px; color:var(--muted); }
.stats span strong { color:var(--accent); font-weight:600; }
.empty-label { color:var(--muted); font-size:13px; }
.pill { display:inline-block; flex:0 0 auto; padding:3px 8px; border-radius:999px; background:var(--panel2); color:var(--muted); font-size:12px; }
.empty { padding:24px; border:1px dashed var(--line); border-radius:10px; color:var(--muted); }
code { color:var(--accent); }
@media (max-width:700px) { .header-inner, main { padding-left:14px; padding-right:14px; } .toolbar-heading { display:block; } .toolbar-heading button { margin-top:12px; } .grid { grid-template-columns:1fr; } }
</style>
</head>
<body><header><div class="header-inner"><h1>OpenTony Asset Library</h1><p>Generated asset packages under <code id="root">build/assets</code></p></div></header>
<main>
  <section class="library-toolbar" aria-label="Asset library filters">
    <div class="toolbar-heading"><div><h2>Packages</h2><p id="summary">Scanning manifests…</p></div><button id="show-empty" type="button">Show empty packages</button></div>
    <div class="filter-row">
      <label class="search-field">Search packages<input id="search" type="search" placeholder="Name or source path…"></label>
      <label>Sort by<select id="sort"><option value="assets">Most assets</option><option value="name">Name</option><option value="source">Source path</option></select></label>
    </div>
  </section>
  <div id="packages" class="grid"><p class="empty">Scanning manifests…</p></div>
</main>
<script>
const esc = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const state = { items:[], showEmpty:false, query:'', sort:'assets' };
const assetTotal = item => item.models + item.objects + item.textures + item.collision_faces;
function card(item) {
  const label = item.has_assets ? 'generated package' : 'empty manifest';
  const stats = item.has_assets
    ? `<div class="stats"><span><strong>${item.models}</strong> models</span><span><strong>${item.objects}</strong> objects</span><span><strong>${item.textures}</strong> textures</span><span><strong>${item.collision_faces}</strong> collision faces</span></div>`
    : '<div class="empty-label">No exported assets · likely an internal/source manifest</div>';
  return `<a class="package${item.has_assets ? '' : ' empty-package'}" href="/package?path=${encodeURIComponent(item.path)}"><div class="package-top"><h2>${esc(item.name)}</h2><span class="pill">${label}</span></div><p title="${esc(item.source || item.path)}">${esc(item.source || item.path)}</p>${stats}</a>`;
}
function render() {
  const target = document.getElementById('packages');
  const query = state.query.toLowerCase();
  const usable = state.items.some(item => item.has_assets);
  let items = state.items.filter(item => (state.showEmpty || !usable || item.has_assets));
  items = items.filter(item => `${item.name} ${item.source || ''}`.toLowerCase().includes(query));
  items.sort((a, b) => state.sort === 'name' ? a.name.localeCompare(b.name) : state.sort === 'source' ? (a.source || a.path).localeCompare(b.source || b.path) : assetTotal(b) - assetTotal(a) || a.name.localeCompare(b.name));
  const emptyCount = state.items.filter(item => !item.has_assets).length;
  document.getElementById('summary').textContent = `${items.length} shown · ${state.items.length} total${emptyCount && !state.showEmpty ? ` · ${emptyCount} empty hidden` : ''}`;
  document.getElementById('show-empty').textContent = state.showEmpty ? 'Hide empty packages' : `Show empty packages${emptyCount ? ` (${emptyCount})` : ''}`;
  if (!items.length) { target.innerHTML = '<div class="empty">No packages match this filter.</div>'; return; }
  target.innerHTML = items.map(card).join('');
}
document.getElementById('search').addEventListener('input', event => { state.query = event.target.value.trim(); render(); });
document.getElementById('sort').addEventListener('change', event => { state.sort = event.target.value; render(); });
document.getElementById('show-empty').addEventListener('click', event => { state.showEmpty = !state.showEmpty; render(); });
fetch('/api/catalog').then(response => response.json()).then(items => { state.items = items; render(); }).catch(error => { document.getElementById('packages').innerHTML = `<div class="empty">Explorer error: ${esc(error)}</div>`; });
</script>
</body></html>"""


def explorer_root(path: str | Path) -> Path:
    root = resolve(path).resolve()
    if not root.is_dir():
        raise SystemExit(f"asset explorer path is not a directory: {root}")
    if not (root / "manifest.json").is_file():
        raise SystemExit(f"asset explorer requires a generated manifest.json: {root}")
    return root


def asset_workspace_root(path: str | Path) -> Path:
    root = resolve(path).resolve()
    if not root.is_dir():
        raise SystemExit(f"asset explorer workspace is not a directory: {root}")
    return root


def _png_chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)


def ppm_to_png(data: bytes) -> bytes:
    """Convert the extractor's P6/255 PPM output to a browser-friendly PNG."""

    index = 0

    def token() -> bytes:
        nonlocal index
        while index < len(data) and data[index] in b" \t\r\n":
            index += 1
        if index < len(data) and data[index] == ord("#"):
            end = data.find(b"\n", index)
            index = len(data) if end < 0 else end + 1
            return token()
        start = index
        while index < len(data) and data[index] not in b" \t\r\n":
            index += 1
        if start == index:
            raise ValueError("invalid PPM header")
        return data[start:index]

    if token() != b"P6":
        raise ValueError("only binary P6 PPM files are supported")
    width, height, maximum = (int(token()) for _ in range(3))
    if maximum != 255 or width <= 0 or height <= 0:
        raise ValueError("unsupported PPM dimensions or color depth")
    if index >= len(data) or data[index] not in b" \t\r\n":
        raise ValueError("PPM header is missing its pixel separator")
    if data[index] == ord("\r") and index + 1 < len(data) and data[index + 1] == ord("\n"):
        index += 2
    else:
        index += 1
    pixels = data[index : index + width * height * 3]
    if len(pixels) != width * height * 3:
        raise ValueError("truncated PPM pixel data")
    scanlines = b"".join(b"\0" + pixels[row * width * 3 : (row + 1) * width * 3] for row in range(height))
    return b"".join(
        [
            b"\x89PNG\r\n\x1a\n",
            _png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)),
            _png_chunk(b"IDAT", zlib.compress(scanlines, 6)),
            _png_chunk(b"IEND", b""),
        ]
    )


def parse_obj(data: str, *, limit: int = 250_000) -> dict:
    vertices: list[tuple[float, float, float]] = []
    faces: list[dict] = []
    materials: set[str] = set()
    objects: list[str] = []
    current_material = "untextured"
    for raw_line in data.splitlines():
        line = raw_line.strip()
        if line.startswith("v "):
            values = line.split()
            vertices.append(tuple(float(value) for value in values[1:4]))
            if len(vertices) > limit:
                raise ValueError("OBJ has too many vertices for the explorer")
        elif line.startswith("f "):
            values = line.split()[1:]
            indices = []
            for value in values:
                index = int(value.split("/", 1)[0])
                indices.append(index - 1 if index > 0 else len(vertices) + index)
            if len(indices) >= 3:
                faces.append({"vertices": indices, "material": current_material})
                if len(faces) > limit:
                    raise ValueError("OBJ has too many faces for the explorer")
        elif line.startswith("usemtl "):
            current_material = line[7:].strip() or "untextured"
            materials.add(current_material)
        elif line.startswith("o "):
            name = line[2:].strip()
            if name:
                objects.append(name)
    if vertices:
        bounds = [
            min(vertex[axis] for vertex in vertices) for axis in range(3)
        ] + [max(vertex[axis] for vertex in vertices) for axis in range(3)]
    else:
        bounds = [0, 0, 0, 0, 0, 0]
    return {
        "vertex_count": len(vertices),
        "face_count": len(faces),
        "vertices": vertices,
        "faces": faces,
        "materials": sorted(materials),
        "objects": objects,
        "bounds": bounds,
    }


def _safe_child(root: Path, relative: str) -> Path:
    candidate = (root / unquote(relative)).resolve()
    try:
        candidate.relative_to(root.resolve())
    except ValueError as exc:
        raise ValueError("asset path escapes explorer root") from exc
    if not candidate.is_file():
        raise FileNotFoundError(relative)
    return candidate


def _safe_directory(root: Path, relative: str) -> Path:
    candidate = (root / unquote(relative or ".")).resolve()
    try:
        candidate.relative_to(root.resolve())
    except ValueError as exc:
        raise ValueError("asset package escapes explorer workspace") from exc
    if not candidate.is_dir() or not (candidate / "manifest.json").is_file():
        raise FileNotFoundError(relative)
    return candidate


def _catalog(root: Path) -> list[dict]:
    packages = []
    for manifest_path in sorted(root.rglob("manifest.json")):
        if not manifest_path.is_file():
            continue
        try:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError):
            continue
        package_path = manifest_path.parent.relative_to(root).as_posix()
        if package_path == ".":
            package_path = ""
        collision = manifest.get("collision") or {}
        packages.append(
            {
                "path": package_path or ".",
                "name": manifest_path.parent.name,
                "source": (manifest.get("source") or {}).get("path"),
                "models": len(manifest.get("models") or []),
                "objects": len(manifest.get("objects") or []),
                "textures": len(manifest.get("textures") or []),
                "collision_faces": collision.get("face_count", 0),
            }
        )
    return packages


def _package_html(package_path: str | None) -> bytes:
    value = json.dumps(package_path) if package_path else "null"
    return _INDEX_HTML.replace("const PACKAGE_PATH = null;", f"const PACKAGE_PATH = {value};").encode("utf-8")


def _handler_for(root: Path, *, package_mode: bool):
    class ExplorerHandler(BaseHTTPRequestHandler):
        def _send(self, body: bytes, content_type: str, status: int = 200) -> None:
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            self.wfile.write(body)

        def _error(self, status: int, message: str) -> None:
            self._send(json.dumps({"error": message}).encode("utf-8"), "application/json", status)

        def _package_root(self, query: dict[str, list[str]]) -> Path:
            if package_mode:
                return root
            package_path = query.get("package", [""])[0]
            return _safe_directory(root, package_path)

        def do_GET(self) -> None:
            request = urlparse(self.path)
            query = parse_qs(request.query)
            try:
                if request.path == "/":
                    if package_mode:
                        self._send(_package_html(None), "text/html; charset=utf-8")
                    else:
                        self._send(_DASHBOARD_HTML.encode("utf-8"), "text/html; charset=utf-8")
                elif request.path == "/package":
                    package_path = query.get("path", [""])[0]
                    _safe_directory(root, package_path)
                    self._send(_package_html(package_path or "."), "text/html; charset=utf-8")
                elif request.path == "/api/catalog":
                    self._send(json.dumps(_catalog(root)).encode("utf-8"), "application/json")
                elif request.path == "/api/manifest":
                    self._send((self._package_root(query) / "manifest.json").read_bytes(), "application/json")
                elif request.path == "/api/obj":
                    relative = query.get("path", [""])[0]
                    target = _safe_child(self._package_root(query), relative)
                    self._send(
                        json.dumps(parse_obj(target.read_text(encoding="utf-8"))).encode("utf-8"),
                        "application/json",
                    )
                elif request.path.startswith("/files/"):
                    relative = request.path.removeprefix("/files/")
                    target = _safe_child(self._package_root(query), relative)
                    body = target.read_bytes()
                    content_type = mimetypes.guess_type(target.name)[0] or "application/octet-stream"
                    if target.suffix.lower() == ".ppm":
                        body = ppm_to_png(body)
                        content_type = "image/png"
                    self._send(body, content_type)
                else:
                    self._error(404, "not found")
            except (FileNotFoundError, ValueError, UnicodeDecodeError) as exc:
                self._error(400, str(exc))

        def log_message(self, format: str, *args) -> None:
            return

    return ExplorerHandler


def run_asset_explorer(
    root: Path,
    *,
    package_mode: bool,
    host: str = "127.0.0.1",
    port: int = 8765,
    open_browser: bool = False,
) -> int:
    server = ThreadingHTTPServer((host, port), _handler_for(root, package_mode=package_mode))
    url = f"http://{host}:{server.server_address[1]}/"
    print(f"Asset explorer: {url}")
    print(f"Serving: {root}")
    if open_browser:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nAsset explorer stopped")
    finally:
        server.server_close()
    return 0


def assets_explore(args) -> int:
    if args.path:
        candidate = resolve(args.path).resolve()
        if (candidate / "manifest.json").is_file():
            root = explorer_root(candidate)
            package_mode = True
        else:
            root = asset_workspace_root(candidate)
            package_mode = False
    else:
        root = resolve("build/assets").resolve()
        root.mkdir(parents=True, exist_ok=True)
        package_mode = False
    return run_asset_explorer(
        root,
        package_mode=package_mode,
        host=args.host,
        port=args.port,
        open_browser=args.open_browser,
    )
