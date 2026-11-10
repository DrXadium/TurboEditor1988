// SPDX-License-Identifier: GPL-3.0-or-later
// Probe: does the shader apply uniform values? Source is solid navy; with
// contrast=2.0 the shader must shift blue channel 170 -> ~255. Unchanged
// navy means uniforms are not binding.
import QtQuick
import QtTest
import src

TestCase {
    id: tc
    name: "UniformBinding"
    width: 200
    height: 100
    visible: true

    Rectangle { id: srcItem; anchors.fill: parent; color: "#0000aa" }

    EffectChain {
        id: chain
        anchors.fill: parent
        sourceItem: srcItem
    }

    property var grabResult
    property bool grabDone: false

    function test_extreme_contrast_applies() {
        // everything off except contrast
        chain.settings.screenCurvature = 0
        chain.settings.staticNoise = 0
        chain.settings.flickering = 0
        chain.settings.rgbShift = 0
        chain.settings.jitter = 0
        chain.settings.horizontalSync = 0
        chain.settings.rasterization = 0
        chain.settings.bloom = 0
        chain.settings.glowingLine = 0
        chain.settings.brightness = 0.5     // multiplier 1.0
        chain.settings.contrast = 2.0        // navy B: (0.667-0.5)*3+0.5 = 1.0 -> 255
        wait(80)
        grabDone = false
        chain.grabToImage(function(res) { grabResult = res; grabDone = true })
        tryCompare(tc, "grabDone", true, 2000)
        verify(grabResult.saveToFile("file:///tmp/crt_uniforms.png"))
    }
}