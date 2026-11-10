// SPDX-License-Identifier: GPL-3.0-or-later
// Task 2: CRT effect chain contract test.
//
// Saves grabbed frames of the EffectChain to known paths (effects off, and
// curvature on). Pixel assertions live in scripts/run_qml_tests.sh (which
// decodes the PNGs), because Qt 6 QML exposes no pixel-read API here.
import QtQuick
import QtTest
import src

TestCase {
    id: tc
    name: "EffectChain"
    width: 320
    height: 240
    visible: true

    // A known source: left half red, right half blue.
    Rectangle {
        id: src
        anchors.fill: parent
        color: "transparent"
        Rectangle { width: parent.width/2; height: parent.height; color: "#ff0000" }
        Rectangle { x: parent.width/2; width: parent.width/2; height: parent.height; color: "#0000ff" }
    }

    EffectChain {
        id: chain
        anchors.fill: parent
        sourceItem: src
    }

    property var grabResult
    property bool grabDone: false

    // Grab `item` and save to `path` (file:// URL); returns true on success.
    function grabToFile(item, path) {
        grabDone = false
        item.grabToImage(function(res) { grabResult = res; grabDone = true })
        tryCompare(tc, "grabDone", true, 2000)
        verify("grab produced no result", grabResult !== null)
        var ok = grabResult.saveToFile(path)
        verify("saveToFile failed for " + path, ok)
        return ok
    }

    function resetEffects() {
        chain.scanlinesEnabled = false
        chain.glowEnabled = false
        chain.curvatureEnabled = false
        chain.settings.screenCurvature = 0
        chain.settings.brightness = 0.5
        chain.settings.staticNoise = 0
        chain.settings.flickering = 0
        chain.settings.rgbShift = 0
        chain.settings.jitter = 0
        chain.settings.horizontalSync = 0
    }

    // All effects off => output equals source (identity).
    function test_save_identity_when_off() {
        resetEffects()
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_identity.png"))
    }

    // Curvature on => frame warps (corner masked). Saved for the shell check.
    function test_save_curvature_on() {
        resetEffects()
        chain.settings.screenCurvature = 0.6
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_curvature.png"))
    }
}
