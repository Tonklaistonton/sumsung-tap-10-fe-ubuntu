#!/usr/bin/env bash
# Regenerate manifest.json hashes from the files on disk.
# Use only when you intentionally changed a pinned input.
set -euo pipefail

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$BASE_DIR"

python3 - << 'EOF'
import hashlib, json
from pathlib import Path

def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()

m = json.loads(Path("manifest.json").read_text())
for key in ("ubuntu_base", "busybox"):
    p = Path(m["inputs"][key]["path"])
    m["inputs"][key]["sha256"] = sha256(p)
    print(f"updated {m['inputs'][key]['path']}")
fw = m["inputs"]["firmware"]
root = Path(fw["root"])
fw["files"] = {}
for p in sorted(root.rglob("*")):
    if p.is_file():
        fw["files"][str(p.relative_to(root))] = sha256(p)
        print(f"updated {p}")
Path("manifest.json").write_text(json.dumps(m, indent=2) + "\n")
print("manifest.json rewritten")
EOF
