#!/usr/bin/env python3
from __future__ import annotations

import os
from pathlib import Path
import subprocess
from PIL import Image


def mk_low_contrast(path: Path) -> None:
    w, h = 320, 96
    img = Image.new('L', (w, h))
    p = img.load()
    for y in range(h):
        for x in range(w):
            p[x, y] = 96 + int((x / (w - 1)) * 40)
    img.save(path)


def mk_diag_edge(path: Path) -> None:
    w, h = 200, 120
    img = Image.new('L', (w, h), 230)
    p = img.load()
    for y in range(h):
        for x in range(w):
            if x > y + 20:
                p[x, y] = 25
    img.save(path)


def run_fib(bin_path: Path, in_path: Path, out_path: Path, w: int, h: int) -> str:
    subprocess.run([str(bin_path), str(in_path), str(w), str(h), str(out_path)], check=True, stdout=subprocess.DEVNULL)
    return out_path.read_text()


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    bin_path = Path(os.environ.get('FIB_BIN', str(root.parent / 'fib')))
    out_dir = root / 'output'
    out_dir.mkdir(parents=True, exist_ok=True)

    lc_img = out_dir / 'low_contrast.png'
    lc_out = out_dir / 'low_contrast.txt'
    mk_low_contrast(lc_img)
    lc_txt = run_fib(bin_path, lc_img, lc_out, 160, 48)
    lc_unique = len(set(lc_txt) - {'\n'})
    assert lc_unique >= 24, f'expected >=24 unique chars for low-contrast image, got {lc_unique}'

    eg_img = out_dir / 'diag_edge.png'
    eg_out = out_dir / 'diag_edge.txt'
    mk_diag_edge(eg_img)
    eg_txt = run_fib(bin_path, eg_img, eg_out, 100, 50)
    eg_count = sum(eg_txt.count(c) for c in '/\\|-_')
    assert eg_count >= 25, f'expected visible edge glyphs, got {eg_count}'

    print('depth-edge checks passed')


if __name__ == '__main__':
    main()
