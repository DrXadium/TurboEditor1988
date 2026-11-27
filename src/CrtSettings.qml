// SPDX-License-Identifier: GPL-3.0-or-later
// Mirrors cool-retro-term's ApplicationSettings effect model (real property
// names + defaults), generalized for a plain texture source (no terminal,
// no bezel). Effect knobs are continuous; "*Enabled" accessors map to value>0
// so settings UI / brief-style booleans work too.
import QtQuick

QtObject {
    id: root

    // --- Rasterization mode (cool-retro-term enum) ---
    readonly property int no_rasterization: 0
    readonly property int scanline_rasterization: 1
    readonly property int pixel_rasterization: 2
    readonly property int subpixel_rasterization: 3
    readonly property int modern_rasterization: 4

    // --- Cool-retro-term effect floats (defaults from ApplicationSettings.qml) ---
    property real contrast: 0.80
    property real brightness: 0.5
    property real ambientLight: 0.2
    property real staticNoise: 0.12
    property real screenCurvature: 0.0
    property real glowingLine: 0.2
    property real burnIn: 0.25
    property real bloom: 0.55
    property real chromaColor: 0.0      // keep the UI's own colours by default
    property real saturationColor: 0.0
    property real jitter: 0.2
    property real horizontalSync: 0.08
    property real flickering: 0.1
    property real rgbShift: 0.0
    property real sweep: 0.0    // TV-revision2 band amplitude (0 = off)
    // Master bypass; only main.qml's Loader reads it. Values stay put.
    property bool effectsEnabled: true

    // Phase-2 pipeline knobs (2026-11-26 spec).
    property bool phosphorEnabled: false   // gates convertWithChroma
    property int scanlinePitch: 2          // physical px per raster cell
    property real glassSheen: 0.0          // CRT frame shininess, adapted
    property real screenRadius: 0.0        // rounded glass corners
    property real screenInset: 0.0         // CRT frame size, adapted
    property real bloomQuality: 0.5        // blur resolution
    property real burnInQuality: 0.5       // feedback resolution

    // Bezel-era names kept as aliases for callers that still use them.
    property real frameShininess: glassSheen
    property real frameSize: screenInset

    // Rasterization intensity: 0 disables scanlines.
    property int rasterization: 1          // default scanline_rasterization
    // Scanline strength. Was 0.5, which on a 1-px period swung luminance by
    // ~75% and read as thick heavy banding rather than a CRT raster.
    property real rasterizationIntensity: 0.22

    // Colors (defaults match cool-retro-term amber-on-black)
    property color fontColor: "#ff8100"
    property color backgroundColor: "#000000"

    // --- Brief-named boolean accessors (map to the float model) ---
    property bool scanlinesEnabled: rasterization != no_rasterization && rasterizationIntensity > 0
    property bool curvatureEnabled: screenCurvature > 0
    property bool glowEnabled: bloom > 0 || glowingLine > 0
    property bool flickerEnabled: flickering > 0
    property bool motionEnabled: jitter > 0 || horizontalSync > 0
    property bool burnInEnabled: burnIn > 0
    property bool horizontalSyncEnabled: horizontalSync > 0
    property bool rgbShiftEnabled: rgbShift > 0
    property bool sheenEnabled: glassSheen > 0

    // Compose as cool-retro-term does (kept for parity/debug).
    function composeProfile() {
        return JSON.stringify({
            contrast: contrast, brightness: brightness,
            screenCurvature: screenCurvature, glowingLine: glowingLine,
            bloom: bloom, burnIn: burnIn, chromaColor: chromaColor,
            saturationColor: saturationColor, staticNoise: staticNoise,
            jitter: jitter, horizontalSync: horizontalSync,
            flickering: flickering, rgbShift: rgbShift,
            rasterization: rasterization, rasterizationIntensity: rasterizationIntensity,
            phosphorEnabled: phosphorEnabled, scanlinePitch: scanlinePitch,
            glassSheen: glassSheen, screenRadius: screenRadius,
            screenInset: screenInset, bloomQuality: bloomQuality,
            burnInQuality: burnInQuality, sweep: sweep
        })
    }
}
