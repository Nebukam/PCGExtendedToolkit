#!/bin/sh
cd "$(dirname "$0")"

if command -v node >/dev/null 2>&1; then
    node generate-uplugin.js
    exit $?
fi

if command -v python3 >/dev/null 2>&1; then
    python3 generate-uplugin.py
    exit $?
fi

if command -v python >/dev/null 2>&1; then
    python generate-uplugin.py
    exit $?
fi

echo ""
echo "ERROR: Neither Node.js nor Python found."
echo "Please install one of:"
echo "  - Node.js: https://nodejs.org"
echo "  - Python:  https://python.org"
echo ""
exit 1