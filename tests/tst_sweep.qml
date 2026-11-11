// SPDX-License-Identifier: GPL-3.0-or-later
// TV-revision2 Task 1 (corrected): the CRT sweep must travel TOP→BOTTOM
// while the picture stays UPRIGHT.
//
// The band axis lives in the SHADER (uv.y). An earlier revision rotated the
// whole composed pass 90° CW to fake the direction, which rotated the entire
// UI with it — the user-visible defect this test now pins. Two assertions:
//   1. Upright: the source paints a RED block top-left and a BLUE block
//      bottom-right; after the pass they must still be top-left / bottom-right.
//   2. Sweep: the bright band's row (y) changes between two phases while its
//      column (x) stays put.
//
// Scene lives directly on the TestCase root: qmltestrunner renders that Item,
// while a scratch `Window` created from a test file never maps and grabs black.
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

    // Flat mid-grey field (something for the sweep band to lift) plus the
    // orientation markers: red top-left, blue bottom-right.
    Item {
        id: src
        anchors.fill: parent
        Rectangle { anchors.fill: parent; color: "#404040" }
        // Markers are hidden for the sweep-phase grabs: at full brightness the
        // red block out-shines the band and would dominate the centroid.
        Item {
            id: markers
            anchors.fill: parent
            Rectangle { x: 0; y: 0; width: tc.width / 4; height: tc.height / 4; color: "#FF0000" }
            Rectangle {
                x: tc.width * 3 / 4; y: tc.height * 3 / 4
                width: tc.width / 4; height: tc.height / 4; color: "#0000FF"
            }
        }
    }

    EffectChain {
        id: chain
        anchors.fill: parent
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

    function grabToFile(item, path) {
        grabDone = false
        item.grabToImage(function(res) {
            if (res.saveToFile(path)) grabDone = true
        })
        tryCompare(tc, "grabDone", true, 3000)
        verify("saveToFile failed for " + path, grabDone)
    }

    // Upright proof: no rotation anywhere in the pipeline.
    function test_upright_output() {
        markers.visible = true
        sweepAmp = 0
        chain.resetChain()
        wait(120)
        grabToFile(tc, "file:///tmp/crt_upright.png")
    }

    // Two phases → two PNGs; the shell asserts the band moved in Y only.
    function test_zz_save_two_sweep_phases() {
        markers.visible = false
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
