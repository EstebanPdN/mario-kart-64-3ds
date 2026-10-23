#!/bin/sh
set -eu
cd "$(dirname "$0")"
if [ ! -x .venv/bin/python ]; then python3 -m venv .venv; fi
.venv/bin/python -m pip install -r requirements.txt
if [ ! -x runtime/bin/mk64-extract ]; then .venv/bin/python build.py; fi
exec .venv/bin/python app.py
