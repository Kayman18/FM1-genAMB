#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Regenerate FM_FACTORY for the released 16 preset bank."""
import sys
from pathlib import Path
from fm16_bank import render
if __name__ == '__main__':
    Path(sys.argv[1]).write_text(render('expanded'))
