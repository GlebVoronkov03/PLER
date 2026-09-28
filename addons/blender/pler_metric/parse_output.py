"""Parse pler CLI stdout into a result dict."""
from __future__ import annotations

import re
from typing import Dict, Optional


_LINE = re.compile(
    r"^(PLER|MSE|Peak|Mean err|Miss ref|Miss test|Rays|Backend|Align|Verts|Time|TSI|Combined|Align IoU):\s*(.+)$"
)


def parse_pler_stdout(text: str) -> Dict[str, str]:
    out: Dict[str, str] = {}
    for raw in text.splitlines():
        line = raw.strip()
        m = _LINE.match(line)
        if not m:
            continue
        key, val = m.group(1), m.group(2).strip()
        out[key] = val
    return out


def pler_db(parsed: Dict[str, str]) -> Optional[float]:
    v = parsed.get("PLER", "")
    # "30.1147 dB"
    try:
        return float(v.split()[0])
    except (IndexError, ValueError):
        return None


def mse(parsed: Dict[str, str]) -> Optional[float]:
    try:
        return float(parsed.get("MSE", "").split()[0])
    except (IndexError, ValueError):
        return None


def tsi(parsed: Dict[str, str]) -> Optional[float]:
    if "TSI" not in parsed:
        return None
    try:
        return float(parsed["TSI"].split()[0])
    except (IndexError, ValueError):
        return None
