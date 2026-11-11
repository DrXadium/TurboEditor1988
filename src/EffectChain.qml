// SPDX-License-Identifier: GPL-3.0-or-later
// Generalized CRT effect chain: takes a source item (any widget/scene content),
// renders it to a texture, and runs it through the CRT fragment shader.
// Edge-to-edge, no bezel. Properties mirror cool-retro-term's effect model.
import QtQuick

Item {
    id: root

    // The content to be CRT'd.
    property Item sourceItem

    // Effect model. `crt` is the QML default (cool-retro-term defaults; used
    // directly by Task 2 tests, which instantiate EffectChain without any C++
    // context). The app overrides `settings` with the C++ CrtSettings context
    // property `crtSettings` (QSettings-backed, loaded before engine.load()),
    // so the settings dialog drives the shader live.
    CrtSettings { id: crt }
    property var settings: crt

    // Convenience aliases so tests/short UI can set these directly.
    property alias scanlinesEnabled: crt.scanlinesEnabled
    property alias curvatureEnabled: crt.curvatureEnabled
    property alias glowEnabled: crt.glowEnabled
    property alias flickerEnabled: crt.flickerEnabled
    property alias motionEnabled: crt.motionEnabled
    property real screenCurvature: crt.screenCurvature
    onScreenCurvatureChanged: crt.screenCurvature = screenCurvature
    // TV-revision2 sweep orientation: the shader's sweep band moves along
    // screen X (left→right); the real CRT sweep must move top→bottom, so the
    // composed window rotates the pass 90° CW. `sweepTime` is the phase the
    // shader samples (app: the live clock; tests: a fixed value). Defaulting
    // to the internal `time` would trip Qt 6.10 component-id scoping in
    // initial bindings — QML resolves the id from the OBJECT's scope only —
    // so the default is an inert 0 and the app sets it explicitly.
    property real sweepTime: 0

    // Render the source item to a texture the shader samples.
    ShaderEffectSource {
        id: srcTex
        anchors.fill: parent
        sourceItem: root.sourceItem
        hideSource: true   // source renders only through the CRT pass
        live: true
        recursive: true
    }

    ShaderEffect {
        id: effect
        anchors.fill: parent
        property var source: srcTex
        property real time: clock.time
        property vector2d resolution: Qt.vector2d(width, height)
        property real screenCurvature: root.settings.screenCurvature
        property real rasterizationIntensity:
            root.settings.scanlinesEnabled ? root.settings.rasterizationIntensity : 0.0
        property real glow: root.settings.glowEnabled ? root.settings.bloom : 0.0
        property real rgbShift: root.settings.chromaColor
        property real jitter: root.settings.motionEnabled ? root.settings.jitter : 0.0
        property real horizontalSync: root.settings.horizontalSync
        property real flickering: root.settings.flickerEnabled ? root.settings.flickering : 0.0
        property real brightness: root.settings.brightness
        property real contrast: root.settings.contrast
        property real staticNoise: root.settings.staticNoise
        property real scanlineDensity: 1.0
        property real sweep: root.sweepTime
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
