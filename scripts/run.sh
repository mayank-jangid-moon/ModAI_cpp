#!/bin/bash

# ModAI Run Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
EXECUTABLE="$BUILD_DIR/ModAI"

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "ERROR: Executable not found: $EXECUTABLE"
    echo ""
    echo "Please build the project first:"
    echo "  ./scripts/build.sh"
    exit 1
fi

# Check if executable is actually executable
if [ ! -x "$EXECUTABLE" ]; then
    echo "Making executable..."
    chmod +x "$EXECUTABLE"
fi

echo "========================================="
echo "ModAI - Trust & Safety Dashboard"
echo "========================================="
echo ""
echo "Starting application..."
echo ""

# Check for API keys
if [ -z "$MODAI_HUGGINGFACE_TOKEN" ] && [ -z "$MODAI_HIVE_API_KEY" ]; then
    echo "Warning: API keys not set in environment variables"
    echo "   The app will look for keys in config file"
    echo "   See SETUP.md for configuration instructions"
    echo ""
fi

# Run the application
cd "$PROJECT_ROOT"
"$EXECUTABLE" "$@"

EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ]; then
    echo ""
    echo "ERROR: Application exited with error code: $EXIT_CODE"
    echo ""
    echo "Troubleshooting:"
    echo "1. Check that all dependencies are installed"
    echo "2. Verify API keys are configured (see SETUP.md)"
    echo "3. Check logs in: data/logs/system.log"
    exit $EXIT_CODE
fi

