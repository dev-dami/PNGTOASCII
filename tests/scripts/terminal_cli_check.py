#!/usr/bin/env python3
from __future__ import annotations

import os
from pathlib import Path
import subprocess


def run_capture(cmd: list[str], env: dict[str, str] | None = None) -> str:
    result = subprocess.run(
        cmd,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=env,
    )
    return result.stdout


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    bin_path = Path(os.environ.get("FIB_BIN", str(root.parent / "fib")))
    fixture = root / "fixtures" / "checker.png"
    out_dir = root / "output"
    out_dir.mkdir(parents=True, exist_ok=True)

    always_text = run_capture(
        [str(bin_path), "--color", "always", str(fixture), "8", "4"]
    )
    assert "\x1b[" in always_text, "expected ANSI sequence with --color always"

    never_text = run_capture(
        [str(bin_path), "--color", "never", str(fixture), "8", "4"]
    )
    assert "\x1b[" not in never_text, "did not expect ANSI sequence with --color never"

    alias_text = run_capture([str(bin_path), "--ansi", str(fixture), "8", "4"])
    assert "\x1b[" in alias_text, "expected ANSI sequence with --ansi alias"

    no_alias_text = run_capture([str(bin_path), "--no-ansi", str(fixture), "8", "4"])
    assert "\x1b[" not in no_alias_text, (
        "did not expect ANSI sequence with --no-ansi alias"
    )

    file_out = out_dir / "color_auto_file.txt"
    subprocess.run(
        [str(bin_path), "--color", "auto", str(fixture), "8", "4", str(file_out)],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    file_text = file_out.read_text()
    assert "\x1b[" not in file_text, "auto mode should avoid ANSI when writing to file"

    env = os.environ.copy()
    env["NO_COLOR"] = "1"
    no_color_text = run_capture(
        [str(bin_path), "--color", "auto", str(fixture), "8", "4"], env=env
    )
    assert "\x1b[" not in no_color_text, "NO_COLOR should disable ANSI in auto mode"

    print("terminal cli checks passed")


if __name__ == "__main__":
    main()
