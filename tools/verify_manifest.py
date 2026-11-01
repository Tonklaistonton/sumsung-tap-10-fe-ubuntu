#!/usr/bin/env python3
"""Verify pinned build inputs against manifest.json (sha256)."""
import hashlib
import json
import sys
from pathlib import Path


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    base = Path(sys.argv[1]).parent
    manifest = json.loads(Path(sys.argv[1]).read_text())
    inputs = manifest["inputs"]
    failures = 0

    def check(rel: str, want: str) -> None:
        nonlocal failures
        p = base / rel
        if not p.exists():
            print(f"  [MISSING] {rel}")
            failures += 1
            return
        got = sha256(p)
        if got == want:
            print(f"  [OK]      {rel}")
        else:
            print(f"  [MISMATCH] {rel}\n            want {want}\n            got  {got}")
            failures += 1

    for key in ("ubuntu_base", "busybox"):
        item = inputs[key]
        check(item["path"], item["sha256"])

    for rel, want in inputs["firmware"]["files"].items():
        check(f'{inputs["firmware"]["root"]}/{rel}', want)

    if failures:
        print(f"verify_manifest: {failures} input(s) failed")
        return 1
    print("verify_manifest: all inputs match")
    return 0


if __name__ == "__main__":
    sys.exit(main())
