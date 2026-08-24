#!/usr/bin/env python3
"""Swap PU2CLR <Wire.h> for sketch TwiFastWire (copied into patched lib src)."""
import pathlib
import sys

OLD = "#include <Wire.h>"
NEW = '#include "twi_fast_wire.h"'


def patch_file(path: pathlib.Path, required: bool) -> None:
    text = path.read_text()
    if NEW in text and OLD not in text:
        print(f"si4735 wire shim: {path.name} already patched")
        return
    if OLD not in text:
        if required:
            print(f"si4735 wire shim: {OLD} not in {path}", file=sys.stderr)
            sys.exit(1)
        print(f"si4735 wire shim: {path.name} has no {OLD} (ok)")
        return
    path.write_text(text.replace(OLD, NEW, 1))
    print(f"si4735 wire shim: {path.name} -> twi_fast_wire.h")


def main() -> int:
    root = pathlib.Path(sys.argv[1])
    if root.is_file():
        patch_file(root, required=True)
        return 0
    patch_file(root / "SI4735.h", required=True)
    patch_file(root / "SI4735.cpp", required=False)
    return 0


if __name__ == "__main__":
    sys.exit(main())
