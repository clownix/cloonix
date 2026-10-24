// cr-notes.js — item notes on the canvas (cnf <n> note=), and the items of
// the Comments step under the mouse outlined.

"use strict";

const NOTE_MAX_BYTES = 100;    // MAX_NOTE_LEN-1 of the switch, in utf-8 bytes
const NOTE_LINE = 28;          // characters per line of a note under an item

let g_notes = {};              // name -> text, from "itemnote"
let g_highlight = null;        // Set of names outlined, or null
let g_highlight_timer = null;

function applyItemNote(name, txt) {
  if (txt) g_notes[name] = txt; else delete g_notes[name];
  draw();
}

// Under the item: the whole note, cut in lines at the spaces.
function drawItemNote(name, x, yTop) {
  const t = g_notes[name];
  if (!t) return;
  const lines = [];
  let cur = "";
  t.split(" ").forEach(w => {
    while (w.length > NOTE_LINE) {
      if (cur) { lines.push(cur); cur = ""; }
      lines.push(w.slice(0, NOTE_LINE));
      w = w.slice(NOTE_LINE);
    }
    if (cur && (cur.length + 1 + w.length > NOTE_LINE)) { lines.push(cur); cur = ""; }
    cur = cur ? cur + " " + w : w;
  });
  if (cur) lines.push(cur);
  ctx.fillStyle = "#e3b341";
  ctx.font = "9px monospace";
  ctx.textAlign = "center"; ctx.textBaseline = "top";
  lines.forEach((l, i) => ctx.fillText(l, x, yTop + i * 11));
}

// A pulsing ring around an outlined item.
function drawItemRing(name, x, y, r) {
  if (!g_highlight || !g_highlight.has(name)) return;
  const k = 0.5 + 0.5 * Math.sin(Date.now() / 160);
  ctx.beginPath();
  ctx.arc(x, y, r + 4 + 4 * k, 0, 2 * Math.PI);
  ctx.strokeStyle = "#f78166";
  ctx.lineWidth = 3 / (g_zoom || 1);
  ctx.stroke();
}

// names: the words of the commands of a step; only the items drawn count.
function canvasHighlight(names) {
  const live = new Set();
  if (names) names.forEach(n => {
    if (g_nodes.some(x => x.name === n) || g_lans.some(x => x.name === n))
      live.add(n);
  });
  g_highlight = live.size ? live : null;
  if (g_highlight && !g_highlight_timer)
    g_highlight_timer = setInterval(draw, 50);
  if (!g_highlight && g_highlight_timer) {
    clearInterval(g_highlight_timer);
    g_highlight_timer = null;
  }
  draw();
}

// The words of a cloonix line that can name an item: before any quote, so
// the command an ssh carries is not read as names.
function cmdNames(line) {
  const head = line.split(/["']/)[0];
  return head.trim().split(/\s+/).filter(w => /^[A-Za-z0-9_][A-Za-z0-9_.-]*$/.test(w));
}

// Right-click, Note…: the server checks it again and tells every browser.
function noteEdit(name) {
  const t = window.prompt(`Note on ${name}, shown on the canvas ` +
                          `(${NOTE_MAX_BYTES} bytes at most, empty removes it):`,
                          g_notes[name] || "");
  if (t === null) return;
  const txt = t.trim();
  if (/[\x00-\x1f\x7f]/.test(txt)) { alert("a note is one line of text"); return; }
  const n = new TextEncoder().encode(txt).length;
  if (n > NOTE_MAX_BYTES) {
    alert(`a note holds ${NOTE_MAX_BYTES} bytes at most, this one has ${n}`);
    return;
  }
  post("/ctl/note", { name, txt });
}

// Over the border of a panel under the canvas, the cursor of its resize.
window.addEventListener("load", () => {
  if (!canvas) return;
  canvas.addEventListener("mousemove", e => {
    if (e.buttons) return;
    const b = panelBorderAtScreen(e.clientX, e.clientY);
    canvas.style.cursor = b ? getComputedStyle(b.panel.querySelector(
      `[data-rdir="${b.dir}"]`)).cursor : "";
  });
});
