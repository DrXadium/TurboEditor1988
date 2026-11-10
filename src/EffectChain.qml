// SPDX-License-Identifier: GPL-3.0-or-later
// Generalized CRT effect chain: takes a source item (any widget/scene content),
// renders it to a texture, and runs it through the CRT fragment shader.
// Edge-to-edge, no bezel. Properties mirror cool-retro-term's effect model.
import QtQuick

Item {
    id: root

    // The content to be CRT'd.
    property Item sourceItem

    // Effect model (defaults = cool-retro-term). Declared with an id so
    // aliases and the shader can bind to it.
    CrtSettings { id: crt }
    property CrtSettings settings: crt

    // Convenience aliases so tests/short UI can set these directly.
    property alias scanlinesEnabled: crt.scanlinesEnabled
    property alias curvatureEnabled: crt.curvatureEnabled
    property alias glowEnabled: crt.glowEnabled
    property alias flickerEnabled: crt.flickerEnabled
    property alias motionEnabled: crt.motionEnabled
    property real screenCurvature: crt.screenCurvature
    onScreenCurvatureChanged: crt.screenCurvature = screenCurvature

    // Render the source item to a texture the shader samples.
    ShaderEffectSource {
        id: srcTex
        anchors.fill: parent
        sourceItem: root.sourceItem
        hideSource: false
        live: true
        recursive: true
    }

    ShaderEffect {
        id: effect
        anchors.fill: parent
        property var source: srcTex
        property real time: clock.time
        property vector2d resolution: Qt.vector2d(width, height)
        property real screenCurvature: settings.screenCurvature
        property real rasterizationIntensity:
            settings.scanlinesEnabled ? settings.rasterizationIntensity : 0.0
        property real glow: settings.glowEnabled ? settings.bloom : 0.0
        property real rgbShift: settings.chromaColor
        property real jitter: settings.motionEnabled ? settings.jitter : 0.0
        property real horizontalSync: settings.horizontalSync
        property real flickering: settings.flickerEnabled ? settings.flickering : 0.0
        property real brightness: settings.brightness
        property real contrast: settings.contrast
        property real staticNoise: settings.staticNoise
        property real scanlineDensity: 1.0
        // Compiled shader path (absolute file URL). Overridable for the app
        // build (qrc path) in Task 4; the test runner sets this to the
        // .superpowers/qmlbuild/shaders output.
        fragmentShader: root.fragmentShaderUrl
    }

    // Default resolves to the compiled .qsb the run_qml_tests.sh produced.
    property url fragmentShaderUrl:
        Qt.resolvedUrl("../.superpowers/qmlbuild/shaders/crt_effect.frag.qsb")

    // Simple time source for animated effects.
    Timer {
        id: clock
        property real time: 0
        interval: 16; repeat: true; running: true
        onTriggered: time += 0.016
    }
}
