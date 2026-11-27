// SPDX-License-Identifier: GPL-3.0-or-later
// CRT effect chain pixel probes (2026-11-26 spec).
//
// Qt 6 QML exposes no pixel-read API, so each test saves a frame and
// scripts/check_fx_pixels.py decodes it and asserts the contract. Run with:
//   bash scripts/run_qml_tests.sh tst_effectchain
import QtQuick
import QtTest
import src

TestCase {
    id: tc
    name: "EffectChain"
    width: 320
    height: 240
    visible: true

    // Configurable source. Each test shows exactly the field it needs.
    Item {
        id: src
        anchors.fill: parent

        Item {
            id: redBlue
            anchors.fill: parent
            Rectangle { width: parent.width / 2; height: parent.height; color: "#ff0000" }
            Rectangle { x: parent.width / 2; width: parent.width / 2; height: parent.height; color: "#0000ff" }
        }
        Rectangle { id: blackField; anchors.fill: parent; color: "black"; visible: false }
        Rectangle { id: greyField; anchors.fill: parent; color: "#808080"; visible: false }
        Rectangle { id: darkField; anchors.fill: parent; color: "#202020"; visible: false }
        Rectangle { id: whiteField; anchors.fill: parent; color: "white"; visible: false }
        Rectangle { id: greenField; anchors.fill: parent; color: "#00ff00"; visible: false }
        Rectangle {
            id: whiteBlock
            x: 150; y: 110; width: 20; height: 20
            color: "white"
            visible: false
        }
        Item {
            id: edgeField
            anchors.fill: parent
            visible: false
            Rectangle { anchors.fill: parent; color: "black" }
            Rectangle { x: 160; width: 160; height: 240; color: "white" }
        }
    }

    EffectChain {
        id: chain
        anchors.fill: parent
        sourceItem: src
    }

    property var grabResult
    property bool grabDone: false

    function showOnly(name) {
        redBlue.visible = (name === "redblue")
        blackField.visible = (name === "black")
        greyField.visible = (name === "grey")
        darkField.visible = (name === "dark")
        whiteField.visible = (name === "white")
        greenField.visible = (name === "green")
        edgeField.visible = (name === "edge")
    }

    // Grab `item` and save to `path` (file:// URL); returns true on success.
    function grabToFile(item, path) {
        grabDone = false
        item.grabToImage(function(res) { grabResult = res; grabDone = true })
        tryCompare(tc, "grabDone", true, 3000)
        verify("grab produced no result", grabResult !== null)
        var ok = grabResult.saveToFile(path)
        verify("saveToFile failed for " + path, ok)
        return ok
    }

    function resetEffects() {
        chain.time = -1
        chain.sweepTime = 0
        chain.scanlinesEnabled = false
        chain.glowEnabled = false
        chain.settings.screenCurvature = 0
        chain.settings.brightness = 0.5
        chain.settings.contrast = 0.8
        chain.settings.staticNoise = 0
        chain.settings.flickering = 0
        chain.settings.rgbShift = 0
        chain.settings.jitter = 0
        chain.settings.horizontalSync = 0
        chain.settings.burnIn = 0
        chain.settings.bloom = 0
        chain.settings.glowingLine = 0
        chain.settings.ambientLight = 0
        chain.settings.chromaColor = 0
        chain.settings.saturationColor = 0
        chain.settings.phosphorEnabled = false
        chain.settings.screenRadius = 0
        chain.settings.screenInset = 0
        chain.settings.glassSheen = 0
        chain.settings.bloomQuality = 0.5
        chain.settings.burnInQuality = 0.5
        chain.settings.scanlinePitch = 2
        chain.settings.rasterization = 0
        chain.settings.sweep = 0
        whiteBlock.visible = false
    }

    // All effects off => output equals source (identity).
    function test_save_identity_when_off() {
        showOnly("redblue")
        resetEffects()
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_identity.png"))
    }

    // Curvature on => frame warps (corner masked). Saved for the shell check.
    function test_save_curvature_on() {
        showOnly("redblue")
        resetEffects()
        chain.settings.screenCurvature = 0.6
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_curvature.png"))
    }

    // Bloom is a spatial blur: a small white block puts ink well outside its
    // own bounds (a per-pixel brightness lift cannot).
    function test_save_bloom_halo() {
        showOnly("black")
        resetEffects()
        whiteBlock.visible = true
        chain.settings.bloom = 1.0
        chain.settings.bloomQuality = 0.5
        chain.time = 1.0
        wait(300)
        verify(grabToFile(chain, "file:///tmp/crt_bloom.png"))
    }

    // Burn-in leaves a decaying ghost after the source block is removed.
    function test_save_burn_in_ghost() {
        showOnly("black")
        resetEffects()
        whiteBlock.visible = true
        chain.settings.burnIn = 1.0
        chain.settings.burnInQuality = 0.5
        wait(700)
        whiteBlock.visible = false
        wait(100)
        verify(grabToFile(chain, "file:///tmp/crt_burnin_a.png"))
        wait(900)
        verify(grabToFile(chain, "file:///tmp/crt_burnin_b.png"))
    }

    // Rasterization modes: scanline row band, 2-D pixel grid, channel stripes.
    function test_save_raster_modes() {
        showOnly("grey")
        resetEffects()
        chain.settings.rasterizationIntensity = 1.0
        chain.settings.scanlinePitch = 2
        chain.time = 1.0

        chain.settings.rasterization = 0
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_raster0.png"))

        chain.settings.rasterization = 1
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_raster1.png"))

        chain.settings.rasterization = 2
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_raster2.png"))

        chain.settings.rasterization = 3
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_raster3.png"))
    }

    // True RGB shift: red and blue sample at different offsets, so their
    // half-brightness edges sit at different columns.
    function test_save_rgb_shift() {
        showOnly("edge")
        resetEffects()
        chain.settings.rgbShift = 1.0
        chain.time = 1.0
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_rgbshift.png"))
    }

    // Glow line: a bright band exists and its Y position changes with time.
    function test_save_glow_line() {
        showOnly("dark")
        resetEffects()
        chain.settings.glowingLine = 1.0
        chain.time = 1.0
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_glow_a.png"))
        chain.time = 4.0
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_glow_b.png"))
    }

    // Phosphor: off preserves the source hue; on converges to fontColor.
    function test_save_phosphor() {
        showOnly("green")
        resetEffects()
        chain.time = 1.0
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_phosphor_off.png"))

        chain.settings.phosphorEnabled = true
        chain.settings.chromaColor = 0
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_phosphor_on.png"))
    }

    // Rounded glass corners and screen inset both mask the frame black.
    function test_save_screen_radius() {
        showOnly("white")
        resetEffects()
        chain.settings.screenRadius = 1.0
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_radius.png"))
    }

    function test_save_screen_inset() {
        showOnly("white")
        resetEffects()
        chain.settings.screenInset = 1.0
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_inset.png"))
    }

    // Ambient light lifts the glass edge; sheen is a diagonal gradient.
    function test_save_ambient_and_sheen() {
        showOnly("grey")
        resetEffects()
        chain.time = 1.0
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_ambient_off.png"))

        chain.settings.ambientLight = 1.0
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_ambient_on.png"))

        chain.settings.ambientLight = 0
        chain.settings.glassSheen = 0
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_sheen_off.png"))

        chain.settings.glassSheen = 1.0
        wait(80)
        verify(grabToFile(chain, "file:///tmp/crt_sheen_on.png"))
    }
}
