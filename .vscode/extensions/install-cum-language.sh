#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXT_SRC="$ROOT/cum-language"
EXT_ID="bfc-tunnel.cum-language"
EXT_VERSION="0.1.0"
EXT_DIR_NAME="${EXT_ID}-${EXT_VERSION}"
TS="$(date +%s)000"

install_to() {
  local target_root="$1"
  local dest="$target_root/$EXT_DIR_NAME"
  local registry="$target_root/extensions.json"

  mkdir -p "$target_root"
  rm -rf "$dest"
  cp -a "$EXT_SRC" "$dest"

  python3 - "$registry" "$dest" "$EXT_ID" "$EXT_VERSION" "$TS" <<'PY'
import json
import sys
from pathlib import Path

registry = Path(sys.argv[1])
dest = Path(sys.argv[2]).resolve()
ext_id = sys.argv[3]
version = sys.argv[4]
ts = int(sys.argv[5])
relative = dest.name

entry = {
    "identifier": {"id": ext_id},
    "version": version,
    "location": {
        "$mid": 1,
        "path": str(dest),
        "scheme": "file",
    },
    "relativeLocation": relative,
    "metadata": {
        "installedTimestamp": ts,
        "source": "vsix",
    },
}

entries = []
if registry.exists():
    entries = json.loads(registry.read_text())
    entries = [e for e in entries if e.get("identifier", {}).get("id") != ext_id]

entries.append(entry)
registry.write_text(json.dumps(entries, indent=2) + "\n")
PY

  echo "Installed $EXT_ID to $dest"
}

install_to "$HOME/.cursor-server/extensions"
install_to "$HOME/.cursor/extensions"

echo
echo "Done. Reload the Cursor window:"
echo "  Ctrl+Shift+P -> Developer: Reload Window"
