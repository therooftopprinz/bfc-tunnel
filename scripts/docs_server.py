#!/usr/bin/env python3
"""Serve docs/ as rendered HTML with live reload on file changes."""

from __future__ import annotations

import argparse
import hashlib
import os
import select
import struct
import sys
import threading
import time
from collections.abc import Callable
from pathlib import Path

from flask import Flask, Response, abort, jsonify, render_template_string, request, send_from_directory, stream_with_context

REPO_ROOT = Path(__file__).resolve().parent.parent
DOCS_DIR = REPO_ROOT / "docs"
DEFAULT_PORT = 8080
POLL_INTERVAL = 0.25
DEBOUNCE_SECONDS = 0.2
NO_CACHE_HEADERS = {
    "Cache-Control": "no-store, no-cache, must-revalidate",
    "Pragma": "no-cache",
}

INDEX_RELOAD_SCRIPT = """
(function () {
  let version = {{ version }};
  const status = document.getElementById("reload-status");

  function reloadNow() {
    if (status) status.textContent = "Docs updated — reloading…";
    window.location.reload();
  }

  function connectEvents() {
    const source = new EventSource("/events");
    source.addEventListener("version", (event) => {
      const next = Number(event.data);
      if (!Number.isNaN(next) && next !== version) {
        source.close();
        reloadNow();
      }
    });
    source.onerror = () => {
      source.close();
      setTimeout(connectEvents, 1000);
    };
  }

  async function checkVersion() {
    try {
      const resp = await fetch("/version", { cache: "no-store" });
      const data = await resp.json();
      if (data.version !== version) reloadNow();
    } catch (_) {}
  }

  if (window.EventSource) {
    connectEvents();
  } else {
    setInterval(checkVersion, 500);
  }
})();
"""

PAGE_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{{ title }}</title>
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/codemirror@5.65.18/lib/codemirror.min.css">
  <style>
    :root {
      color-scheme: light dark;
      --bg: #ffffff;
      --fg: #1f2328;
      --muted: #656d76;
      --border: #d0d7de;
      --code-bg: #f6f8fa;
      --link: #0969da;
      --table-stripe: #f6f8fa;
      --pane-bg: #f6f8fa;
      --editor-bg: #ffffff;
      --cm-fg: #24292f;
      --cm-gutter-bg: #f6f8fa;
      --cm-gutter-fg: #8c959f;
      --cm-selection: #add6ff66;
      --cm-cursor: #24292f;
      --cm-header: #0550ae;
      --cm-strong: #24292f;
      --cm-em: #24292f;
      --cm-link: #0969da;
      --cm-url: #0a3069;
      --cm-quote: #1a7f37;
      --cm-comment: #6e7781;
      --cm-code: #cf222e;
      --cm-hr: #656d76;
      --cm-list: #0550ae;
      --cm-formatting: #8c959f;
      --cm-keyword: #cf222e;
      --cm-tag: #116329;
      --cm-attribute: #0550ae;
      --cm-string: #0a3069;
      --cm-atom: #953800;
      --cm-number: #0550ae;
      --cm-meta: #6e7781;
      --cm-def: #8250df;
    }
    @media (prefers-color-scheme: dark) {
      :root {
        --bg: #1e1e1e;
        --fg: #d4d4d4;
        --muted: #8b949e;
        --border: #30363d;
        --code-bg: #252526;
        --link: #3794ff;
        --table-stripe: #252526;
        --pane-bg: #181818;
        --editor-bg: #1e1e1e;
        --cm-fg: #d4d4d4;
        --cm-gutter-bg: #1e1e1e;
        --cm-gutter-fg: #858585;
        --cm-selection: #264f78;
        --cm-cursor: #aeafad;
        --cm-header: #569cd6;
        --cm-strong: #569cd6;
        --cm-em: #c586c0;
        --cm-link: #3794ff;
        --cm-url: #ce9178;
        --cm-quote: #6a9955;
        --cm-comment: #6a9955;
        --cm-code: #ce9178;
        --cm-hr: #808080;
        --cm-list: #569cd6;
        --cm-formatting: #808080;
        --cm-keyword: #569cd6;
        --cm-tag: #569cd6;
        --cm-attribute: #9cdcfe;
        --cm-string: #ce9178;
        --cm-atom: #b5cea8;
        --cm-number: #b5cea8;
        --cm-meta: #808080;
        --cm-def: #dcdcaa;
      }
    }
    * { box-sizing: border-box; }
    html, body {
      height: 100%;
      margin: 0;
    }
    body {
      background: var(--bg);
      color: var(--fg);
      font: 16px/1.6 -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
      display: flex;
      flex-direction: column;
      overflow: hidden;
    }
    header {
      border-bottom: 1px solid var(--border);
      padding: 0.6rem 1rem;
      display: flex;
      align-items: center;
      gap: 0.75rem;
      flex-wrap: wrap;
      flex-shrink: 0;
    }
    header a { color: var(--link); text-decoration: none; }
    header a:hover { text-decoration: underline; }
    .status {
      margin-left: auto;
      color: var(--muted);
      font-size: 0.85rem;
    }
    .status.dirty { color: #bf8700; }
    .status.error { color: #cf222e; }
    .status.saved { color: #1a7f37; }
    @media (prefers-color-scheme: dark) {
      .status.error { color: #f85149; }
      .status.saved { color: #3fb950; }
      .status.dirty { color: #d29922; }
    }
    button.tool-btn {
      appearance: none;
      border: 1px solid var(--border);
      background: var(--code-bg);
      color: var(--fg);
      border-radius: 6px;
      padding: 0.3rem 0.7rem;
      font: inherit;
      font-size: 0.85rem;
      cursor: pointer;
    }
    button.tool-btn:hover { border-color: var(--link); }
    button.tool-btn.active {
      border-color: var(--link);
      color: var(--link);
      background: color-mix(in srgb, var(--link) 12%, transparent);
    }
    .split {
      flex: 1;
      display: grid;
      grid-template-columns: 1fr 1fr;
      min-height: 0;
    }
    .pane {
      min-width: 0;
      min-height: 0;
      display: flex;
      flex-direction: column;
      border-right: 1px solid var(--border);
    }
    .pane:last-child { border-right: none; }
    .pane-label {
      flex-shrink: 0;
      padding: 0.35rem 0.75rem;
      font-size: 0.75rem;
      text-transform: uppercase;
      letter-spacing: 0.04em;
      color: var(--muted);
      background: var(--pane-bg);
      border-bottom: 1px solid var(--border);
    }
    #editor-pane { background: var(--editor-bg); }
    #preview-pane { background: var(--bg); }
    .CodeMirror {
      flex: 1;
      height: auto;
      font-family: "Cascadia Code", "Fira Code", ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
      font-size: 14px;
      line-height: 1.55;
      background: var(--editor-bg);
      color: var(--cm-fg);
    }
    .CodeMirror-scroll { min-height: 100%; }
    .CodeMirror-gutters {
      background: var(--cm-gutter-bg);
      border-right: 1px solid var(--border);
    }
    .CodeMirror-linenumber { color: var(--cm-gutter-fg); }
    .CodeMirror-cursor { border-left: 2px solid var(--cm-cursor); }
    .CodeMirror-selected { background: var(--cm-selection); }
    .CodeMirror-focused .CodeMirror-selected { background: var(--cm-selection); }
    .cm-s-cursor .cm-header,
    .cm-s-cursor .cm-header-1,
    .cm-s-cursor .cm-header-2,
    .cm-s-cursor .cm-header-3,
    .cm-s-cursor .cm-header-4,
    .cm-s-cursor .cm-header-5,
    .cm-s-cursor .cm-header-6 { color: var(--cm-header); font-weight: 700; }
    .cm-s-cursor .cm-strong { color: var(--cm-strong); font-weight: 700; }
    .cm-s-cursor .cm-em { color: var(--cm-em); font-style: italic; }
    .cm-s-cursor .cm-link { color: var(--cm-link); text-decoration: underline; }
    .cm-s-cursor .cm-url { color: var(--cm-url); }
    .cm-s-cursor .cm-quote { color: var(--cm-quote); }
    .cm-s-cursor .cm-comment { color: var(--cm-comment); }
    .cm-s-cursor .cm-string,
    .cm-s-cursor .cm-string-2 { color: var(--cm-code); }
    .cm-s-cursor .cm-hr { color: var(--cm-hr); }
    .cm-s-cursor .cm-variable-2,
    .cm-s-cursor .cm-variable-3 { color: var(--cm-list); }
    .cm-s-cursor .cm-formatting,
    .cm-s-cursor .cm-formatting-header,
    .cm-s-cursor .cm-formatting-list,
    .cm-s-cursor .cm-formatting-quote,
    .cm-s-cursor .cm-formatting-strong,
    .cm-s-cursor .cm-formatting-em,
    .cm-s-cursor .cm-formatting-link { color: var(--cm-formatting); font-weight: 400; }
    .cm-s-cursor .cm-keyword { color: var(--cm-keyword); }
    .cm-s-cursor .cm-tag { color: var(--cm-tag); }
    .cm-s-cursor .cm-attribute { color: var(--cm-attribute); }
    .cm-s-cursor .cm-atom { color: var(--cm-atom); }
    .cm-s-cursor .cm-number { color: var(--cm-number); }
    .cm-s-cursor .cm-meta { color: var(--cm-meta); }
    .cm-s-cursor .cm-def { color: var(--cm-def); }
    .cm-s-cursor .cm-builtin { color: var(--cm-header); }
    .cm-s-cursor .cm-property { color: var(--cm-attribute); }
    .cm-s-cursor .cm-operator { color: var(--cm-fg); }
    #preview {
      flex: 1;
      overflow: auto;
      padding: 1.5rem 1.75rem 3rem;
    }
    #preview h1, #preview h2, #preview h3, #preview h4 { line-height: 1.25; margin-top: 1.5em; }
    #preview h1 { margin-top: 0; font-size: 2rem; border-bottom: 1px solid var(--border); padding-bottom: 0.3em; }
    #preview h2 { font-size: 1.5rem; border-bottom: 1px solid var(--border); padding-bottom: 0.25em; }
    #preview hr { border: 0; border-top: 1px solid var(--border); margin: 1.5rem 0; }
    #preview a { color: var(--link); }
    #preview code {
      font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
      font-size: 0.9em;
      background: var(--code-bg);
      padding: 0.15em 0.35em;
      border-radius: 4px;
    }
    #preview pre {
      background: var(--code-bg);
      border: 1px solid var(--border);
      border-radius: 6px;
      padding: 1rem;
      overflow: auto;
    }
    #preview pre code { background: none; padding: 0; }
    #preview table {
      border-collapse: collapse;
      width: 100%;
      margin: 1rem 0;
      display: block;
      overflow-x: auto;
    }
    #preview th, #preview td {
      border: 1px solid var(--border);
      padding: 0.45rem 0.75rem;
      text-align: left;
    }
    #preview th { background: var(--table-stripe); }
    #preview tbody tr:nth-child(even) { background: var(--table-stripe); }
    #preview .mermaid { text-align: center; margin: 1.5rem 0; }
    #preview .src-block {
      border-radius: 4px;
      transition: background-color 80ms ease;
    }
    #preview .src-block.src-highlight {
      background: color-mix(in srgb, var(--cm-selection) 85%, transparent);
      outline: 1px solid color-mix(in srgb, var(--link) 35%, transparent);
    }
    @media (max-width: 900px) {
      .split { grid-template-columns: 1fr; grid-template-rows: 1fr 1fr; }
      .pane { border-right: none; border-bottom: 1px solid var(--border); }
    }
  </style>
</head>
<body>
  <header>
    <strong><a href="/">docs</a></strong>
    {% if breadcrumb %}
    <span>/</span>
    <span>{{ breadcrumb_name }}</span>
    {% endif %}
    <button type="button" class="tool-btn" id="wrap-btn" title="Toggle line wrap">Wrap</button>
    <span class="status" id="reload-status">Autosave on</span>
  </header>
  <div class="split">
    <section class="pane" id="editor-pane">
      <div class="pane-label">Markdown</div>
      <textarea id="source">{{ markdown_source }}</textarea>
    </section>
    <section class="pane" id="preview-pane">
      <div class="pane-label">Preview</div>
      <div id="preview"></div>
    </section>
  </div>

  <script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.18/lib/codemirror.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.18/mode/markdown/markdown.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.18/mode/xml/xml.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.18/addon/mode/overlay.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.18/mode/gfm/gfm.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/marked@15.0.12/marked.min.js"></script>
  <script type="module">
    import mermaid from "https://cdn.jsdelivr.net/npm/mermaid@11/dist/mermaid.esm.min.mjs";

    const DOC_PATH = {{ doc_path | tojson }};
    const INITIAL_VERSION = {{ version }};
    const WRAP_KEY = "docs-editor-linewrap";
    const AUTOSAVE_MS = 700;
    const dark = window.matchMedia("(prefers-color-scheme: dark)").matches;
    const statusEl = document.getElementById("reload-status");
    const wrapBtn = document.getElementById("wrap-btn");
    const previewEl = document.getElementById("preview");
    const lineWrapping = localStorage.getItem(WRAP_KEY) !== "0";

    mermaid.initialize({ startOnLoad: false, theme: dark ? "dark" : "neutral" });
    marked.setOptions({ gfm: true, breaks: true });

    const editor = CodeMirror.fromTextArea(document.getElementById("source"), {
      mode: "gfm",
      lineNumbers: true,
      lineWrapping,
      theme: "cursor",
      indentUnit: 2,
      tabSize: 2,
      viewportMargin: Infinity,
    });

    let savedValue = editor.getValue();
    let dirty = false;
    let saving = false;
    let ignoreNextVersion = null;
    let version = INITIAL_VERSION;
    let previewTimer = null;
    let autosaveTimer = null;
    let mermaidId = 0;
    let syncingScroll = false;
    let syncingSelection = false;

    function lineAtOffset(src, offset) {
      if (offset <= 0) return 1;
      let lines = 1;
      const limit = Math.min(offset, src.length);
      for (let i = 0; i < limit; i++) {
        if (src.charCodeAt(i) === 10) lines++;
      }
      return lines;
    }

    function renderMarkdownWithSourceMap(src) {
      const tokens = marked.lexer(src);
      let pos = 0;
      const parts = [];
      for (const token of tokens) {
        const raw = token.raw || "";
        const start = pos;
        const end = start + raw.length;
        pos = end;
        if (token.type === "space" || !raw.trim()) {
          parts.push(marked.parser([token]));
          continue;
        }
        let endPos = end;
        while (endPos > start) {
          const code = src.charCodeAt(endPos - 1);
          if (code !== 10 && code !== 13) break;
          endPos--;
        }
        const startLine = lineAtOffset(src, start);
        const endLine = lineAtOffset(src, Math.max(start, endPos - 1));
        const html = marked.parser([token]);
        parts.push(
          '<div class="src-block" data-source-start="' +
            startLine +
            '" data-source-end="' +
            endLine +
            '">' +
            html +
            "</div>"
        );
      }
      return parts.join("");
    }

    function clearPreviewHighlight() {
      previewEl.querySelectorAll(".src-block.src-highlight").forEach((el) => {
        el.classList.remove("src-highlight");
      });
    }

    function highlightPreviewRange(startLine, endLine) {
      clearPreviewHighlight();
      if (startLine > endLine) {
        const tmp = startLine;
        startLine = endLine;
        endLine = tmp;
      }
      previewEl.querySelectorAll(".src-block").forEach((el) => {
        const s = Number(el.dataset.sourceStart);
        const e = Number(el.dataset.sourceEnd);
        if (e >= startLine && s <= endLine) el.classList.add("src-highlight");
      });
    }

    function selectEditorRange(startLine, endLine) {
      if (startLine > endLine) {
        const tmp = startLine;
        startLine = endLine;
        endLine = tmp;
      }
      const lastLine = editor.lineCount() - 1;
      const from = { line: Math.max(0, startLine - 1), ch: 0 };
      const toLine = Math.min(lastLine, Math.max(0, endLine - 1));
      const to = { line: toLine, ch: editor.getLine(toLine).length };
      syncingSelection = true;
      editor.setSelection(from, to);
      requestAnimationFrame(() => { syncingSelection = false; });
    }

    function syncPreviewFromEditorSelection() {
      if (syncingSelection) return;
      const from = editor.getCursor("from");
      const to = editor.getCursor("to");
      if (from.line === to.line && from.ch === to.ch) {
        clearPreviewHighlight();
        return;
      }
      highlightPreviewRange(from.line + 1, to.line + 1);
    }

    function blockFromNode(node) {
      if (!node) return null;
      if (node.nodeType === Node.TEXT_NODE) node = node.parentElement;
      return node && node.closest ? node.closest(".src-block") : null;
    }

    function syncEditorFromPreviewSelection() {
      if (syncingSelection) return;
      const sel = window.getSelection();
      if (!sel || sel.rangeCount === 0 || sel.isCollapsed) {
        clearPreviewHighlight();
        return;
      }
      if (!previewEl.contains(sel.anchorNode) && !previewEl.contains(sel.focusNode)) return;

      const a = blockFromNode(sel.anchorNode);
      const b = blockFromNode(sel.focusNode);
      if (!a && !b) return;
      const startLine = Math.min(
        Number((a || b).dataset.sourceStart),
        Number((b || a).dataset.sourceStart)
      );
      const endLine = Math.max(
        Number((a || b).dataset.sourceEnd),
        Number((b || a).dataset.sourceEnd)
      );
      highlightPreviewRange(startLine, endLine);
      selectEditorRange(startLine, endLine);
    }

    function scrollRatio(el) {
      const max = el.scrollHeight - el.clientHeight;
      return max > 0 ? el.scrollTop / max : 0;
    }

    function applyScrollRatio(el, ratio) {
      const max = el.scrollHeight - el.clientHeight;
      el.scrollTop = Math.max(0, max) * ratio;
    }

    function syncPreviewFromEditor() {
      if (syncingScroll) return;
      syncingScroll = true;
      const info = editor.getScrollInfo();
      const max = info.height - info.clientHeight;
      const ratio = max > 0 ? info.top / max : 0;
      applyScrollRatio(previewEl, ratio);
      requestAnimationFrame(() => { syncingScroll = false; });
    }

    function syncEditorFromPreview() {
      if (syncingScroll) return;
      syncingScroll = true;
      const ratio = scrollRatio(previewEl);
      const info = editor.getScrollInfo();
      const max = info.height - info.clientHeight;
      editor.scrollTo(null, Math.max(0, max) * ratio);
      requestAnimationFrame(() => { syncingScroll = false; });
    }

    function setStatus(text, kind) {
      statusEl.textContent = text;
      statusEl.className = "status" + (kind ? " " + kind : "");
    }

    function updateWrapButton() {
      const on = editor.getOption("lineWrapping");
      wrapBtn.classList.toggle("active", on);
      wrapBtn.setAttribute("aria-pressed", on ? "true" : "false");
      wrapBtn.title = on ? "Disable line wrap" : "Enable line wrap";
    }

    function updateDirty() {
      dirty = editor.getValue() !== savedValue;
      if (saving) return;
      if (dirty) setStatus("Unsaved changes…", "dirty");
      else setStatus("Autosave on");
    }

    async function renderPreview() {
      const source = editor.getValue();
      const ratio = scrollRatio(previewEl);
      previewEl.innerHTML = renderMarkdownWithSourceMap(source);

      const blocks = previewEl.querySelectorAll("pre code.language-mermaid");
      const nodes = [];
      for (const block of blocks) {
        const parent = block.parentElement;
        const diagram = document.createElement("div");
        diagram.className = "mermaid";
        diagram.id = "mermaid-" + (++mermaidId);
        diagram.textContent = block.textContent;
        parent.replaceWith(diagram);
        nodes.push(diagram);
      }
      if (nodes.length) {
        try {
          await mermaid.run({ nodes });
        } catch (err) {
          for (const node of nodes) {
            if (!node.querySelector("svg")) {
              node.innerHTML = "<pre>" + node.textContent + "</pre>";
            }
          }
        }
      }
      applyScrollRatio(previewEl, ratio);
      syncPreviewFromEditor();
      syncPreviewFromEditorSelection();
    }

    function schedulePreview() {
      clearTimeout(previewTimer);
      previewTimer = setTimeout(renderPreview, 120);
    }

    function scheduleAutosave() {
      clearTimeout(autosaveTimer);
      autosaveTimer = setTimeout(() => { saveDoc(); }, AUTOSAVE_MS);
    }

    async function saveDoc() {
      if (!dirty || saving) return;
      saving = true;
      setStatus("Saving…");
      const body = editor.getValue();
      try {
        const resp = await fetch("/api/doc/" + DOC_PATH, {
          method: "PUT",
          headers: { "Content-Type": "text/markdown; charset=utf-8" },
          body,
        });
        if (!resp.ok) {
          const data = await resp.json().catch(() => ({}));
          throw new Error(data.error || ("HTTP " + resp.status));
        }
        const data = await resp.json();
        // Keep dirty if more edits arrived while saving.
        if (editor.getValue() === body) {
          savedValue = body;
          dirty = false;
        } else {
          dirty = true;
        }
        if (typeof data.version === "number") {
          ignoreNextVersion = data.version;
          version = data.version;
        }
        if (!dirty) {
          setStatus("Saved", "saved");
          setTimeout(() => {
            if (!dirty && !saving) setStatus("Autosave on");
          }, 900);
        } else {
          setStatus("Unsaved changes…", "dirty");
          scheduleAutosave();
        }
      } catch (err) {
        setStatus("Autosave failed: " + err.message, "error");
      } finally {
        saving = false;
        if (dirty && statusEl.textContent.startsWith("Autosave failed")) {
          /* keep error visible briefly */
        } else if (dirty) {
          updateDirty();
        }
      }
    }

    async function reloadFromServer() {
      try {
        const resp = await fetch("/api/doc/" + DOC_PATH, { cache: "no-store" });
        if (!resp.ok) throw new Error("HTTP " + resp.status);
        const data = await resp.json();
        const cursor = editor.getCursor();
        const scroll = editor.getScrollInfo();
        editor.setValue(data.source);
        editor.setCursor(cursor);
        editor.scrollTo(scroll.left, scroll.top);
        savedValue = data.source;
        dirty = false;
        updateDirty();
        schedulePreview();
        setStatus("Reloaded from disk");
        setTimeout(() => {
          if (!dirty) setStatus("Autosave on");
        }, 1200);
      } catch (err) {
        setStatus("Reload failed: " + err.message, "error");
      }
    }

    function onExternalChange(next) {
      version = next;
      if (ignoreNextVersion !== null && next === ignoreNextVersion) {
        ignoreNextVersion = null;
        return;
      }
      if (dirty) {
        setStatus("File changed on disk (unsaved local edits)", "dirty");
        return;
      }
      reloadFromServer();
    }

    function connectEvents() {
      const source = new EventSource("/events");
      source.addEventListener("version", (event) => {
        const next = Number(event.data);
        if (!Number.isNaN(next) && next !== version) onExternalChange(next);
      });
      source.onerror = () => {
        source.close();
        setTimeout(connectEvents, 1000);
      };
    }

    async function checkVersion() {
      try {
        const resp = await fetch("/version", { cache: "no-store" });
        const data = await resp.json();
        if (data.version !== version) onExternalChange(data.version);
      } catch (_) {}
    }

    editor.on("change", () => {
      updateDirty();
      schedulePreview();
      scheduleAutosave();
    });

    editor.on("cursorActivity", syncPreviewFromEditorSelection);
    editor.on("scroll", syncPreviewFromEditor);
    previewEl.addEventListener("scroll", syncEditorFromPreview, { passive: true });
    previewEl.addEventListener("mouseup", syncEditorFromPreviewSelection);
    previewEl.addEventListener("keyup", syncEditorFromPreviewSelection);

    wrapBtn.addEventListener("click", () => {
      const next = !editor.getOption("lineWrapping");
      editor.setOption("lineWrapping", next);
      localStorage.setItem(WRAP_KEY, next ? "1" : "0");
      updateWrapButton();
    });

    window.addEventListener("keydown", (event) => {
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "s") {
        event.preventDefault();
        clearTimeout(autosaveTimer);
        saveDoc();
      }
    });

    if (window.EventSource) connectEvents();
    else setInterval(checkVersion, 500);

    updateWrapButton();
    renderPreview();
    updateDirty();
  </script>
</body>
</html>
"""

INDEX_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>docs</title>
  <style>
    :root {
      color-scheme: light dark;
      --bg: #ffffff;
      --fg: #1f2328;
      --muted: #656d76;
      --border: #d0d7de;
      --link: #0969da;
    }
    @media (prefers-color-scheme: dark) {
      :root {
        --bg: #0d1117;
        --fg: #e6edf3;
        --muted: #8b949e;
        --border: #30363d;
        --link: #4493f8;
      }
    }
    body {
      margin: 0;
      background: var(--bg);
      color: var(--fg);
      font: 16px/1.6 -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
    }
    header {
      border-bottom: 1px solid var(--border);
      padding: 0.75rem 1.5rem;
      display: flex;
      align-items: center;
    }
    .status { margin-left: auto; color: var(--muted); font-size: 0.85rem; }
    main { max-width: 40rem; margin: 0 auto; padding: 2rem 1.5rem; }
    a { color: var(--link); text-decoration: none; }
    a:hover { text-decoration: underline; }
    ul { list-style: none; padding: 0; }
    li { margin: 0.6rem 0; }
  </style>
  <script>{{ reload_script | safe }}</script>
</head>
<body>
  <header>
    <strong>docs</strong>
    <span class="status" id="reload-status">Watching for changes</span>
  </header>
  <main>
    <h1>Documentation</h1>
    <ul>
      {% for doc in docs %}
      <li><a href="/{{ doc }}">{{ doc }}</a></li>
      {% endfor %}
    </ul>
  </main>
</body>
</html>
"""


def resolve_doc_path(docs_dir: Path, relative: str) -> Path | None:
    candidate = (docs_dir / relative).resolve()
    try:
        candidate.relative_to(docs_dir.resolve())
    except ValueError:
        return None
    if not candidate.is_file():
        return None
    return candidate


def list_markdown_files(docs_dir: Path) -> list[str]:
    return sorted(
        str(path.relative_to(docs_dir)).replace("\\", "/")
        for path in docs_dir.rglob("*.md")
        if path.is_file()
    )


def file_fingerprint(path: Path) -> str:
    """Hash file contents so changes are detected even when mtime is coarse."""
    digest = hashlib.md5()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def collect_snapshots(root: Path) -> dict[str, str]:
    snapshots: dict[str, str] = {}
    if not root.exists():
        return snapshots
    for path in root.rglob("*"):
        if path.is_file():
            rel = str(path.relative_to(root)).replace("\\", "/")
            try:
                snapshots[rel] = file_fingerprint(path)
            except OSError:
                continue
    return snapshots


def watch_with_inotify(
    root: Path,
    on_activity: Callable[[], None],
    stop: threading.Event,
) -> bool:
    """Return True when inotify watching starts successfully."""
    if sys.platform != "linux":
        return False

    import ctypes

    libc = ctypes.CDLL("libc.so.6", use_errno=True)
    libc.inotify_init.restype = ctypes.c_int
    libc.inotify_add_watch.restype = ctypes.c_int
    libc.inotify_add_watch.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_uint32]

    in_close_write = 0x00000008
    in_moved_to = 0x00000080
    in_create = 0x00000100
    in_delete = 0x00000200
    in_modify = 0x00000002
    in_moved_from = 0x00000040
    in_isdir = 0x40000000
    mask = in_close_write | in_moved_to | in_create | in_delete | in_modify | in_moved_from

    fd = libc.inotify_init()
    if fd < 0:
        return False

    wd_to_path: dict[int, Path] = {}

    def add_watch(path: Path) -> None:
        watch_descriptor = libc.inotify_add_watch(fd, str(path).encode(), mask)
        if watch_descriptor >= 0:
            wd_to_path[watch_descriptor] = path

    def add_tree(path: Path) -> None:
        add_watch(path)
        for child in path.rglob("*"):
            if child.is_dir():
                add_watch(child)

    add_tree(root)

    try:
        while not stop.is_set():
            ready, _, _ = select.select([fd], [], [], 0.5)
            if not ready:
                continue

            try:
                data = os.read(fd, 65536)
            except OSError:
                continue

            offset = 0
            while offset + 16 <= len(data):
                watch_descriptor, event_mask, _cookie, name_len = struct.unpack_from(
                    "iIII", data, offset
                )
                offset += 16
                name = data[offset : offset + name_len].split(b"\0", 1)[0].decode(
                    errors="replace"
                )
                offset += name_len

                base = wd_to_path.get(watch_descriptor, root)
                if event_mask & in_isdir and (event_mask & in_create) and name:
                    new_dir = base / name
                    if new_dir.is_dir():
                        add_watch(new_dir)

                if event_mask & (
                    in_close_write
                    | in_moved_to
                    | in_delete
                    | in_modify
                    | in_moved_from
                    | in_create
                ):
                    on_activity()
    finally:
        os.close(fd)

    return True


def create_app(docs_dir: Path) -> Flask:
    app = Flask(__name__)
    docs_dir = docs_dir.resolve()
    reload_version = 0
    version_condition = threading.Condition()
    watcher_stop = threading.Event()
    debounce_timer: threading.Timer | None = None
    debounce_lock = threading.Lock()
    snapshot_lock = threading.Lock()
    previous_snapshot = collect_snapshots(docs_dir)

    def bump_version() -> int:
        nonlocal reload_version
        with version_condition:
            reload_version += 1
            version_condition.notify_all()
            return reload_version

    def apply_if_changed() -> None:
        nonlocal previous_snapshot
        current = collect_snapshots(docs_dir)
        with snapshot_lock:
            if current != previous_snapshot:
                previous_snapshot = current
                bump_version()

    def schedule_check() -> None:
        nonlocal debounce_timer
        with debounce_lock:
            if debounce_timer is not None:
                debounce_timer.cancel()
            debounce_timer = threading.Timer(DEBOUNCE_SECONDS, apply_if_changed)
            debounce_timer.daemon = True
            debounce_timer.start()

    def poll_docs() -> None:
        while not watcher_stop.is_set():
            time.sleep(POLL_INTERVAL)
            apply_if_changed()

    def inotify_docs() -> None:
        watch_with_inotify(docs_dir, schedule_check, watcher_stop)

    threading.Thread(target=poll_docs, daemon=True, name="docs-poll").start()
    threading.Thread(target=inotify_docs, daemon=True, name="docs-inotify").start()

    @app.after_request
    def disable_html_cache(response: Response):
        if response.content_type and "text/html" in response.content_type:
            response.headers.update(NO_CACHE_HEADERS)
        return response

    def reload_script() -> str:
        with version_condition:
            current = reload_version
        return render_template_string(INDEX_RELOAD_SCRIPT, version=current)

    def normalize_markdown_path(doc_path: str) -> str:
        if doc_path.endswith("/"):
            abort(404)
        if not doc_path.endswith(".md"):
            doc_path = f"{doc_path}.md"
        return doc_path

    @app.get("/version")
    def version():
        with version_condition:
            current = reload_version
        return jsonify(version=current)

    @app.get("/events")
    def events():
        @stream_with_context
        def event_stream():
            last_sent = -1
            while not watcher_stop.is_set():
                with version_condition:
                    if reload_version == last_sent:
                        version_condition.wait(timeout=15.0)
                    current = reload_version
                if current != last_sent:
                    last_sent = current
                    yield f"event: version\ndata: {current}\n\n"
                else:
                    yield ": keepalive\n\n"

        return Response(
            event_stream(),
            mimetype="text/event-stream",
            headers={
                **NO_CACHE_HEADERS,
                "Connection": "keep-alive",
                "X-Accel-Buffering": "no",
            },
        )

    @app.get("/")
    def index():
        return render_template_string(
            INDEX_TEMPLATE,
            docs=list_markdown_files(docs_dir),
            reload_script=reload_script(),
        )

    @app.get("/api/doc/<path:doc_path>")
    def get_doc(doc_path: str):
        doc_path = normalize_markdown_path(doc_path)
        doc_file = resolve_doc_path(docs_dir, doc_path)
        if doc_file is None:
            abort(404)
        return jsonify(source=doc_file.read_text(encoding="utf-8"), path=doc_path)

    @app.put("/api/doc/<path:doc_path>")
    def put_doc(doc_path: str):
        doc_path = normalize_markdown_path(doc_path)
        doc_file = resolve_doc_path(docs_dir, doc_path)
        if doc_file is None:
            abort(404)

        body = request.get_data(as_text=True)
        if body is None:
            return jsonify(error="empty body"), 400

        try:
            doc_file.write_text(body, encoding="utf-8")
        except OSError as exc:
            return jsonify(error=str(exc)), 500

        fingerprint = file_fingerprint(doc_file)
        with snapshot_lock:
            previous_snapshot[doc_path] = fingerprint
        new_version = bump_version()
        return jsonify(ok=True, version=new_version, path=doc_path)

    @app.get("/<path:doc_path>")
    def serve_doc(doc_path: str):
        if doc_path.endswith("/"):
            abort(404)

        if not doc_path.endswith(".md"):
            static_path = resolve_doc_path(docs_dir, doc_path)
            if static_path is not None:
                return send_from_directory(static_path.parent, static_path.name)
            doc_path = f"{doc_path}.md"

        doc_file = resolve_doc_path(docs_dir, doc_path)
        if doc_file is None:
            abort(404)

        source = doc_file.read_text(encoding="utf-8")
        title = doc_file.stem.replace("-", " ").replace("_", " ").title()
        with version_condition:
            current = reload_version
        return render_template_string(
            PAGE_TEMPLATE,
            title=title,
            markdown_source=source,
            doc_path=doc_path,
            version=current,
            breadcrumb=f"/{doc_path}",
            breadcrumb_name=doc_path,
        )

    return app


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1", help="Bind address (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"Port (default: {DEFAULT_PORT})")
    parser.add_argument(
        "--docs-dir",
        type=Path,
        default=DOCS_DIR,
        help=f"Documentation root (default: {DOCS_DIR})",
    )
    args = parser.parse_args()

    docs_dir = args.docs_dir.resolve()
    if not docs_dir.is_dir():
        raise SystemExit(f"Docs directory not found: {docs_dir}")

    app = create_app(docs_dir)
    print(f"Serving {docs_dir} at http://{args.host}:{args.port}/")
    app.run(host=args.host, port=args.port, debug=False, use_reloader=False, threaded=True)


if __name__ == "__main__":
    main()
