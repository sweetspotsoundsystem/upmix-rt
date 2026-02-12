#!/bin/bash
# Install UpmixRT plugins to system plugin directories
# Usage: ./install-plugins.sh [--release] [--sign-identity "Developer ID Application: ..."]
#
# For local development (Debug): ./install-plugins.sh
# For distribution: ./install-plugins.sh --release --sign-identity "Developer ID Application: Your Name (XXXXXXXXXX)"
#
# Note: Ad-hoc signed plugins (default) only work on the machine where they were signed.
#       To distribute to other Macs, use a Developer ID certificate or re-sign on the target machine.

set -e

# Parse arguments
BUILD_DIR="build"
BUILD_TYPE="Debug"
SIGN_IDENTITY="-"  # Default to ad-hoc signing

while [[ $# -gt 0 ]]; do
    case $1 in
        --release)
            BUILD_DIR="release-build"
            BUILD_TYPE="Release"
            shift
            ;;
        --sign-identity)
            SIGN_IDENTITY="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: ./install-plugins.sh [--release] [--sign-identity \"Developer ID Application: ...\"]"
            exit 1
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ARTEFACTS_DIR="$PROJECT_ROOT/$BUILD_DIR/plugin/AudioPlugin_artefacts/$BUILD_TYPE"

# Plugin paths
AU_PLUGIN="$ARTEFACTS_DIR/AU/UpmixRT.component"
VST3_PLUGIN="$ARTEFACTS_DIR/VST3/UpmixRT.vst3"

# System plugin directories
AU_DEST="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3"

echo "Installing UpmixRT plugins from $BUILD_DIR..."
echo ""

# Install AU plugin
if [[ -d "$AU_PLUGIN" ]]; then
    echo "Installing AU plugin..."
    mkdir -p "$AU_DEST"
    rm -rf "$AU_DEST/UpmixRT.component"
    cp -R "$AU_PLUGIN" "$AU_DEST/"
    echo "  ✓ Installed to $AU_DEST/UpmixRT.component"

    # Sign the plugin (required for DAW compatibility on macOS)
    echo "  Signing AU plugin..."
    # Sign embedded frameworks first
    if [[ -d "$AU_DEST/UpmixRT.component/Contents/Frameworks" ]]; then
        find "$AU_DEST/UpmixRT.component/Contents/Frameworks" -type f \( -name "*.dylib" -o -perm +111 \) -exec codesign --force --sign "$SIGN_IDENTITY" {} \;
    fi
    # Sign the main bundle
    codesign --force --sign "$SIGN_IDENTITY" "$AU_DEST/UpmixRT.component"
    if [[ "$SIGN_IDENTITY" == "-" ]]; then
        echo "  ✓ AU plugin signed (ad-hoc - local use only)"
    else
        echo "  ✓ AU plugin signed with: $SIGN_IDENTITY"
    fi
else
    echo "  ✗ AU plugin not found at $AU_PLUGIN"
fi

# Install VST3 plugin
if [[ -d "$VST3_PLUGIN" ]]; then
    echo "Installing VST3 plugin..."
    mkdir -p "$VST3_DEST"
    rm -rf "$VST3_DEST/UpmixRT.vst3"
    cp -R "$VST3_PLUGIN" "$VST3_DEST/"
    echo "  ✓ Installed to $VST3_DEST/UpmixRT.vst3"

    # Sign the plugin (required for DAW compatibility on macOS)
    echo "  Signing VST3 plugin..."
    # Sign embedded frameworks first
    if [[ -d "$VST3_DEST/UpmixRT.vst3/Contents/Frameworks" ]]; then
        find "$VST3_DEST/UpmixRT.vst3/Contents/Frameworks" -type f \( -name "*.dylib" -o -perm +111 \) -exec codesign --force --sign "$SIGN_IDENTITY" {} \;
    fi
    # Sign the main bundle
    codesign --force --sign "$SIGN_IDENTITY" "$VST3_DEST/UpmixRT.vst3"
    if [[ "$SIGN_IDENTITY" == "-" ]]; then
        echo "  ✓ VST3 plugin signed (ad-hoc - local use only)"
    else
        echo "  ✓ VST3 plugin signed with: $SIGN_IDENTITY"
    fi
else
    echo "  ✗ VST3 plugin not found at $VST3_PLUGIN"
fi

# Clear AU cache so DAWs pick up the updated plugin
echo ""
echo "Clearing AU cache..."
killall -9 AudioComponentRegistrar 2>/dev/null || true
rm -f "$HOME/Library/Caches/AudioUnitCache/com.apple.audiounits.cache" 2>/dev/null || true
echo "  ✓ AU cache cleared"

echo ""
echo "Done! Restart your DAW to detect the new plugins."
echo ""
if [[ "$SIGN_IDENTITY" == "-" ]]; then
    echo "⚠️  Plugins were ad-hoc signed (local use only)."
    echo "   To distribute to other Macs, either:"
    echo "   1. Re-sign on the target Mac: codesign --force --deep --sign - <plugin>"
    echo "   2. Use a Developer ID: ./install-plugins.sh --sign-identity \"Developer ID Application: ...\""
fi
