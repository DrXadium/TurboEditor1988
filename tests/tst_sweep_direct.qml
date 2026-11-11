// SPDX-License-Identifier: GPL-3.0-or-later
// Companion to tst_sweep: grabs the UNROTATED EffectChain at two sweep
// phases. Pinpoints which stage misbehaves —
//   raw PNGs move in X   → shader+uniform OK (composed rotation responsible for Y)
//   raw PNGs black       → sweep uniform/shader stage broken.
import QtQuick
import QtTest
import src

TestCase {
    id: tc
    name: "SweepDirect"
    width: 160
    height: 120
    visible: true

    property real phase: 0
    property real sweepAmp: 0
    property bool grabDone: false

    Rectangle { id: src; anchors.fill: parent; color: "#404040" }

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

    function test_zz_two_phases_direct() {
        sweepAmp = 0.6
        chain.resetChain()
        phase = 0.15
        wait(120)
        grabToFile(chain, "file:///tmp/crt_sweepraw_a.png")
        phase = 0.65
        wait(120)
        grabToFile(chain, "file:///tmp/crt_sweepraw_b.png")
    }
}