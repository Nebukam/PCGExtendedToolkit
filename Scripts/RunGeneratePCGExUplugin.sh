#!/bin/sh
SCRIPT_LOCATION="$(cd "$(dirname "$0")" && pwd)"

# Check if scripts are in current directory (running from Scripts folder)
if [ -f "$SCRIPT_LOCATION/generate-uplugin.js" ] || [ -f "$SCRIPT_LOCATION/generate-uplugin.py" ]; then
    SCRIPT_DIR="$SCRIPT_LOCATION"
# Check if scripts are in plugin subfolder (running from project root)
elif [ -f "$SCRIPT_LOCATION/Plugins/PCGExtendedToolkit/Scripts/generate-uplugin.js" ] || \
     [ -f "$SCRIPT_LOCATION/Plugins/PCGExtendedToolkit/Scripts/generate-uplugin.py" ]; then
    SCRIPT_DIR="$SCRIPT_LOCATION/Plugins/PCGExtendedToolkit/Scripts"
else
    echo "ERROR: Could not find generate-uplugin scripts."
    echo "Run this from either:"
    echo "  - Project root (containing Plugins/PCGExtendedToolkit/)"
    echo "  - Plugins/PCGExtendedToolkit/Scripts/"
    exit 1
fi

if command -v node >/dev/null 2>&1; then
    node "$SCRIPT_DIR/generate-uplugin.js"
    RESULT=$?
elif command -v python3 >/dev/null 2>&1; then
    python3 "$SCRIPT_DIR/generate-uplugin.py"
    RESULT=$?
elif command -v python >/dev/null 2>&1; then
    python "$SCRIPT_DIR/generate-uplugin.py"
    RESULT=$?
else
    echo ""
    echo "ERROR: Neither Node.js nor Python found."
    echo "Please install one of:"
    echo "  - Node.js: https://nodejs.org"
    echo "  - Python:  https://python.org"
    RESULT=1
fi

echo ""
if [ $RESULT -eq 0 ]; then
    echo "Done!"
else
    echo "Generation failed with error code $RESULT"
fi

read -p "Press Enter to continue..."
exit $RESULT