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

from flask import Flask, Response, abort, jsonify, render_template_string, send_from_directory
from markdown_it import MarkdownIt

REPO_ROOT = Path(__file__).resolve().parent.parent
DOCS_DIR = REPO_ROOT / "docs"
DEFAULT_PORT = 8080
POLL_INTERVAL = 0.25
DEBOUNCE_SECONDS = 0.2
NO_CACHE_HEADERS = {
    "Cache-Control": "no-store, no-cache, must-revalidate",
    "Pragma": "no-cache",
}

md = MarkdownIt("commonmark", {"breaks": True, "html": True}).enable("table")

RELOAD_SCRIPT = """
(function () {
  let version = {{ version }};
  const status = document.getElementById("reload-status");

  async function checkVersion() {
    try {
      const resp = await fetch("/version", { cache: "no-store" });
      const data = await resp.json();
      if (data.version !== version) {
        if (status) status.textContent = "Docs updated — reloading…";
        window.location.reload();
      }
    } catch (_) {}
  }

  setInterval(checkVersion, 500);
})();
"""

PAGE_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{{ title }}</title>
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
    }
    @media (prefers-color-scheme: dark) {
      :root {
        --bg: #0d1117;
        --fg: #e6edf3;
        --muted: #8b949e;
        --border: #30363d;
        --code-bg: #161b22;
        --link: #4493f8;
        --table-stripe: #161b22;
      }
    }
    * { box-sizing: border-box; }
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
      gap: 1rem;
      flex-wrap: wrap;
    }
    header a { color: var(--link); text-decoration: none; }
    header a:hover { text-decoration: underline; }
    .status {
      margin-left: auto;
      color: var(--muted);
      font-size: 0.85rem;
    }
    main {
      max-width: 52rem;
      margin: 0 auto;
      padding: 2rem 1.5rem 4rem;
    }
    h1, h2, h3, h4 { line-height: 1.25; margin-top: 1.5em; }
    h1 { margin-top: 0; font-size: 2rem; border-bottom: 1px solid var(--border); padding-bottom: 0.3em; }
    h2 { font-size: 1.5rem; border-bottom: 1px solid var(--border); padding-bottom: 0.25em; }
    hr { border: 0; border-top: 1px solid var(--border); margin: 1.5rem 0; }
    a { color: var(--link); }
    code {
      font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
      font-size: 0.9em;
      background: var(--code-bg);
      padding: 0.15em 0.35em;
      border-radius: 4px;
    }
    pre {
      background: var(--code-bg);
      border: 1px solid var(--border);
      border-radius: 6px;
      padding: 1rem;
      overflow: auto;
    }
    pre code { background: none; padding: 0; }
    table {
      border-collapse: collapse;
      width: 100%;
      margin: 1rem 0;
      display: block;
      overflow-x: auto;
    }
    th, td {
      border: 1px solid var(--border);
      padding: 0.45rem 0.75rem;
      text-align: left;
    }
    th { background: var(--table-stripe); }
    tbody tr:nth-child(even) { background: var(--table-stripe); }
    ul.docs-list { list-style: none; padding: 0; }
    ul.docs-list li { margin: 0.5rem 0; }
    .mermaid { text-align: center; margin: 1.5rem 0; }
  </style>
  <script type="module">
    import mermaid from "https://cdn.jsdelivr.net/npm/mermaid@11/dist/mermaid.esm.min.mjs";
    mermaid.initialize({ startOnLoad: false, theme: "neutral" });

    async function renderMermaid() {
      const blocks = document.querySelectorAll("pre code.language-mermaid");
      for (const block of blocks) {
        const parent = block.parentElement;
        const diagram = document.createElement("div");
        diagram.className = "mermaid";
        diagram.textContent = block.textContent;
        parent.replaceWith(diagram);
      }
      await mermaid.run();
    }

    document.addEventListener("DOMContentLoaded", () => {
      renderMermaid();
    });
  </script>
  <script>{{ reload_script | safe }}</script>
</head>
<body>
  <header>
    <strong><a href="/">docs</a></strong>
    {% if breadcrumb %}
    <span>/</span>
    <a href="{{ breadcrumb }}">{{ breadcrumb_name }}</a>
    {% endif %}
    <span class="status" id="reload-status">Watching for changes</span>
  </header>
  <main>{{ content | safe }}</main>
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
    render_cache: dict[str, tuple[str, str]] = {}
    reload_version = 0
    watcher_stop = threading.Event()
    debounce_timer: threading.Timer | None = None
    debounce_lock = threading.Lock()
    snapshot_lock = threading.Lock()
    previous_snapshot = collect_snapshots(docs_dir)

    def render_markdown(path: Path) -> str:
        rel = str(path.relative_to(docs_dir)).replace("\\", "/")
        fingerprint = file_fingerprint(path)
        cached = render_cache.get(rel)
        if cached and cached[0] == fingerprint:
            return cached[1]

        body = path.read_text(encoding="utf-8")
        html = md.render(body)
        render_cache[rel] = (fingerprint, html)
        return html

    def bump_version() -> None:
        nonlocal reload_version
        render_cache.clear()
        reload_version += 1

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
        return render_template_string(RELOAD_SCRIPT, version=reload_version)

    @app.get("/version")
    def version():
        return jsonify(version=reload_version)

    @app.get("/")
    def index():
        return render_template_string(
            INDEX_TEMPLATE,
            docs=list_markdown_files(docs_dir),
            reload_script=reload_script(),
        )

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

        content = render_markdown(doc_file)
        title = doc_file.stem.replace("-", " ").replace("_", " ").title()
        return render_template_string(
            PAGE_TEMPLATE,
            title=title,
            content=content,
            breadcrumb=f"/{doc_path}",
            breadcrumb_name=doc_path,
            reload_script=reload_script(),
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
