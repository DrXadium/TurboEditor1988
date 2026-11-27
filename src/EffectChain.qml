// SPDX-License-Identifier: GPL-3.0-or-later
// Multi-stage CRT effect chain: burn-in accumulation -> bloom -> dynamic pass
// -> static pass. Ported from cool-retro-term's four-stage pipeline, adapted for
// an edge-to-edge GUI (no bezel) as described in
// the design spec that introduced it.
//
// The public contract is unchanged: sourceItem / settings / convenience aliases.
// `shaderBaseUrl` points at the compiled .qsb files (the test harness uses its
// own qmlbuild output; the app points at its embedded qrc copies).
import QtQuick
import QtQuick.Window
import QtQuick.Effects

Item {
    id: root

    // The content to be CRT'd.
    property Item sourceItem

    // Effect model. `crt` is the QML default (cool-retro-term defaults; used
    // directly by the QML tests). The app overrides `settings` with the C++
    // CrtSettings context property so Settings drives the shader live.
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

    // TV-revision2 sweep phase: the shader samples this to place the rolling
    // band. Tests set a fixed value; the app advances it from its clock.
    property real sweepTime: 0

    // Animation time. -1 means "use the internal clock"; a non-negative value
    // pins it so pixel probes are deterministic.
    property real time: -1
    readonly property real clockTime: clock.time
    readonly property real shaderTime: time >= 0 ? time : clock.time

    // Bloom is passed pre-scaled, exactly as cool-retro-term does (×2.5).
    readonly property real bloomAmount: settings.bloom > 0 ? settings.bloom * 2.5 : 0

    // Compiled shader locations. Default resolves to the qmlbuild output the
    // test runner produces; main.qml points this at qrc.
    property url shaderBaseUrl:
        Qt.resolvedUrl("shaders/")

    // --- Source texture ------------------------------------------------------
    ShaderEffectSource {
        id: srcTex
        anchors.fill: parent
        sourceItem: root.sourceItem
        hideSource: true   // source renders only through the CRT pipeline
        live: true
        recursive: true
        smooth: true
    }

    // A tiny black texture for stages that are switched off, so the shaders
    // always have a valid sampler bound.
    Rectangle {
        id: blackRect
        width: 1; height: 1
        color: "black"
        visible: false
    }
    ShaderEffectSource {
        id: blackTex
        sourceItem: blackRect
        hideSource: true
        visible: false
    }

    // --- Stage 1: burn-in accumulation --------------------------------------
    Item {
        id: burnInContainer
        width: Math.max(1, Math.round(root.width * root.settings.burnInQuality))
        height: Math.max(1, Math.round(root.height * root.settings.burnInQuality))
        visible: true

        // Set false while resizing so the feedback buffer does not smear.
        property bool resizing: false
        property real lastUpdate: 0
        property real prevLastUpdate: 0

        readonly property var effectSource:
            burnInLoader.item ? burnInLoader.item.source : null

        function completelyUpdate() {
            const newTime = root.clockTime
            if (newTime > lastUpdate) {
                prevLastUpdate = lastUpdate
                lastUpdate = newTime
            }
            if (burnInLoader.item)
                burnInLoader.item.source.scheduleUpdate()
        }

        function restart() {
            prevLastUpdate = root.clockTime
            lastUpdate = prevLastUpdate
            completelyUpdate()
        }

        onWidthChanged: { resizing = true; resizeTimer.restart() }
        onHeightChanged: { resizing = true; resizeTimer.restart() }

        Timer {
            id: resizeTimer
            interval: 300
            onTriggered: burnInContainer.resizing = false
        }

        Timer {
            interval: 16
            repeat: true
            running: root.settings.burnIn > 0
            onTriggered: burnInContainer.completelyUpdate()
        }

        Connections {
            target: root.settings
            function onBurnInChanged() { burnInContainer.restart() }
            function onBurnInQualityChanged() { burnInContainer.restart() }
            function onRasterizationChanged() { burnInContainer.restart() }
        }

        Loader {
            id: burnInLoader
            anchors.fill: parent
            active: root.settings.burnIn > 0
            sourceComponent: burnInComponent
        }
    }

    Component {
        id: burnInComponent
        Item {
            property alias source: burnInEffectSource

            ShaderEffectSource {
                id: burnInEffectSource
                anchors.fill: parent
                sourceItem: burnInShader
                live: false
                recursive: !burnInContainer.resizing
                hideSource: true
                wrapMode: ShaderEffectSource.ClampToEdge
                format: ShaderEffectSource.RGBA
                smooth: true
                visible: false
            }

            ShaderEffect {
                id: burnInShader
                anchors.fill: parent
                property var txt_source: srcTex
                property var burnInSource: burnInEffectSource
                property real burnInTime:
                    1.0 / Math.max(0.16, Math.min(1.6, root.settings.burnIn))
                property real burnInLastUpdate: burnInContainer.lastUpdate
                property real prevLastUpdate: burnInContainer.prevLastUpdate
                fragmentShader: root.shaderBaseUrl + "burn_in.frag.qsb"
                blending: false
            }
        }
    }

    // --- Stage 2: bloom (downscale + blur) ----------------------------------
    Item {
        id: bloomContainer
        width: Math.max(1, Math.round(root.width * root.settings.bloomQuality))
        height: Math.max(1, Math.round(root.height * root.settings.bloomQuality))
        visible: true

        ShaderEffectSource {
            id: bloomTex
            anchors.fill: parent
            sourceItem: srcTex
            textureSize: Qt.size(bloomContainer.width, bloomContainer.height)
            hideSource: true
            live: root.settings.bloom > 0
            smooth: true
            visible: false
        }

        Loader {
            id: bloomLoader
            anchors.fill: parent
            active: root.settings.bloom > 0
            sourceComponent: bloomComponent
        }
    }

    Component {
        id: bloomComponent
        MultiEffect {
            source: bloomTex
            blurEnabled: true
            blur: 1.0
            blurMax: 16 + 48 * Math.max(0.0, Math.min(1.0, root.settings.bloomQuality))
            autoPaddingEnabled: false
        }
    }

    ShaderEffectSource {
        id: bloomSource
        anchors.fill: parent
        sourceItem: bloomLoader.item ? bloomLoader.item : blackTex
        hideSource: true
        wrapMode: ShaderEffectSource.Repeat
        smooth: true
        visible: false
    }

    // --- Stage 3: dynamic pass ----------------------------------------------
    ShaderEffect {
        id: dynamicEffect
        anchors.fill: parent
        visible: false
        blending: false

        property var screenBuffer: srcTex
        property var burnInSource:
            burnInContainer.effectSource ? burnInContainer.effectSource : blackTex
        property real time: root.shaderTime
        property color fontColor: root.settings.fontColor
        property color backgroundColor: root.settings.backgroundColor
        property vector2d virtualResolution: Qt.vector2d(
            Math.max(1, width * Screen.devicePixelRatio /
                        Math.max(1, root.settings.scanlinePitch)),
            Math.max(1, height * Screen.devicePixelRatio /
                        Math.max(1, root.settings.scanlinePitch)))
        property real rasterizationIntensity:
            (root.settings.rasterization !== 0 && root.settings.rasterizationIntensity > 0)
                ? root.settings.rasterizationIntensity : 0.0
        property int rasterizationMode: root.settings.rasterization
        property real scanlinePitch: root.settings.scanlinePitch
        property real burnInLastUpdate: burnInContainer.lastUpdate
        property real burnInTime:
            1.0 / Math.max(0.16, Math.min(1.6, root.settings.burnIn))
        property real burnIn: root.settings.burnIn
        property real staticNoise: root.settings.staticNoise
        property real screenCurvature: root.settings.screenCurvature
        property real glowingLine: root.settings.glowingLine
        property real chromaColor: root.settings.chromaColor
        property vector2d jitterDisplacement: Qt.vector2d(
            0.007 * root.settings.jitter, 0.002 * root.settings.jitter)
        property real jitter: root.settings.jitter
        property real horizontalSync: root.settings.horizontalSync
        property real horizontalSyncStrength:
            0.05 + 0.30 * Math.max(0.0, Math.min(1.0, root.settings.horizontalSync))
        property real flickering: root.settings.flickering
        property real frameSize: root.settings.screenInset
        property real bloom: root.bloomAmount
        property real phosphorEnabled: root.settings.phosphorEnabled ? 1.0 : 0.0
        property real contrast: root.settings.contrast
        property real sweep: root.settings.sweep
        property real sweepPhase: root.sweepTime

        fragmentShader: root.shaderBaseUrl + "crt_dynamic.frag.qsb"
    }

    ShaderEffectSource {
        id: frameBuffer
        anchors.fill: parent
        sourceItem: dynamicEffect
        hideSource: true
        live: true
        smooth: true
        visible: false
    }

    // --- Stage 4: static pass (the only visible item) -----------------------
    ShaderEffect {
        id: staticEffect
        anchors.fill: parent

        property var source: frameBuffer
        property var bloomSource: bloomSource
        property vector2d resolution: Qt.vector2d(width, height)
        property real screenCurvature: root.settings.screenCurvature
        property real rgbShift: root.settings.rgbShift
        property real frameShininess: root.settings.glassSheen
        property real frameSize: root.settings.screenInset
        property real brightness: root.settings.brightness
        property real bloom: root.bloomAmount
        property real staticNoise: root.settings.staticNoise
        property real screenRadius: root.settings.screenRadius
        property real ambientLight: root.settings.ambientLight
        property real glassSheen: root.settings.glassSheen

        fragmentShader: root.shaderBaseUrl + "crt_static.frag.qsb"
    }

    // Simple time source for animated effects.
    Timer {
        id: clock
        property real time: 0
        interval: 16
        repeat: true
        running: true
        onTriggered: time += 0.016
    }
}
