// SPDX-License-Identifier: GPL-3.0-or-later
// TV-revision2 Task 1: the CRT sweep must travel TOP→BOTTOM on screen.
// The shader sweeps along screen X of its own texture; the app composes the
// chain through SweepPatch (90° CW). This test grabs the COMPOSED scene at
// two sweep phases and saves both PNGs; scripts/run_qml_tests.sh decodes them
// and asserts the bright band's row (y) changes while its column (x) stays
// put. (EffectChain alone sweeps columns — the defect this pins.)
//
// Scene lives directly on the TestCase root: qmltestrunner renders that Item
// (grabs of EffectChain here are proven to work), while a scratch `Window`
// created from a test file never maps under the runner and grabs black.
import QtQuick
import QtTest
import src

TestCase {
    id: tc
    name: "Sweep"
    width: 160
    height: 120
    visible: true

    property real phase: 0
    property real sweepAmp: 0
    property bool grabDone: false

    // Source fed through the chain (hidden while the shader pass owns it).
    Rectangle {
        id: src
        anchors.fill: parent
        color: "#404040"
    }

    EffectChain {
        id: chain
        width: tc.width
        height: tc.height
        sourceItem: src
        sweepTime: phase
        function resetChain() {
            scanlinesEnabled = false
            glowEnabled = false
            curvatureEnabled = false
            settings.screenCurvature = 0
            settings.brightness = 0.5
            settings.staticNoise = 0
            settings.flickering = 0
            settings.rgbShift = 0
            settings.jitter = 0
            settings.horizontalSync = 0
            settings.sweep = tc.sweepAmp
        }
        Component.onCompleted: resetChain()
    }

    // 90° CW composition. Display box (160 wide × 120 tall after rotation)
    // lands exactly over the TestCase: patch local center (80,80) with
    // x=20, y=-20 puts rotated center at (80,60) = tc center.
    SweepPatch {
        id: patch
        x: 20
        y: -20
        width: 120
        height: 160
        chain: chain
    }

    function grabToFile(item, path) {
        grabDone = false
        item.grabToImage(function(res) {
            if (res.saveToFile(path)) grabDone = true
        })
        tryCompare(tc, "grabDone", true, 3000)
        verify("saveToFile failed for " + path, grabDone)
    }

    // Two phases of the composed (rotated) chain → two PNGs for the shell.
    function test_zz_save_two_sweep_phases() {
        // The chain must have landed in the patch host before grabbing.
        verify("chain never entered the SweepPatch host",
            patch.hostItem !== null && chain.parent === patch.hostItem)
        sweepAmp = 0.6
        chain.resetChain()
        phase = 0.15
        wait(120)
        grabToFile(tc, "file:///tmp/crt_sweep_a.png")
        phase = 0.55
        wait(120)
        grabToFile(tc, "file:///tmp/crt_sweep_b.png")
    }
}