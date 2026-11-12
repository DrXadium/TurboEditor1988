// SPDX-License-Identifier: GPL-3.0-or-later
// CRT-Kate root window: the bridged Borland shell frame, run edge-to-edge
// through the CRT effect chain. This is the only visible window (no bezel).
import QtQuick
import QtQuick.Window
import src

Window {
    id: root
    width: 1280
    height: 800
    title: "CRT-Kate"
    visible: true
    color: "black"

    // Bridged ContentBridge frame (ShellWindow rendered offscreen each tick).
    Image {
        id: frame
        anchors.fill: parent
        source: "image://bridge/frame?n=" + bridge.frameSerial
        cache: false
        fillMode: Image.Stretch
        Component.onCompleted: {
            bridge.sourceItem = frame
            bridge.resizeShell(width, height)
        }
        onWidthChanged: bridge.resizeShell(width, height)
        onHeightChanged: bridge.resizeShell(width, height)
    }

    // CRT effect chain. OFF by default while the Turbo Vision chrome is being
    // nailed down (scanlines + chroma shift make border/colour defects hard to
    // read); set CRT_KATE_FX=1 to bring the full effect stack back. A Loader
    // keeps the chain out of the scene entirely when disabled - the chain's
    // ShaderEffectSource uses hideSource, so merely hiding the chain would
    // still suppress the plain frame underneath.
    Loader {
        id: crtLoader
        objectName: "crtEffectChainLoader"
        anchors.fill: parent
        active: bridge.effectsEnabled
        sourceComponent: crtChainComponent
    }

    Component {
        id: crtChainComponent
        EffectChain {
            objectName: "crtEffectChain"
            anchors.fill: parent
            sourceItem: bridge.sourceItem
            settings: crtSettings
            fragmentShaderUrl: "qrc:/crt-kate/shaders/crt_effect.frag.qsb"
            sweepTime: clockTime
            // Phase clock (fixed 16 ms per tick), exposed for the sweepTime
            // binding - the internal EffectChain `time` is scope-hidden here.
            property real clockTime: 0
            Timer {
                interval: 16; repeat: true; running: true
                // CRT_KATE_NOSWEEP=1 freezes the phase so verification captures
                // are not caught mid-band.
                onTriggered: if (!bridge.noSweep) parent.clockTime += 0.016
            }
        }
    }

    // Input capture: the visible window is the CRT glass; real mouse/key events
    // land here and are forwarded to the offscreen ShellWindow (1:1 coords), so
    // the app is interactive through the CRT layer. Transparent — draws nothing,
    // only routes input. Above EffectChain so it receives the events.
    Item {
        id: inputLayer
        objectName: "inputLayer"
        anchors.fill: parent
        focus: true

        MouseArea {
            id: mouseForwarder
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            propagateComposedEvents: true
            onPressed: (mouse) => bridge.forwardMouseEvent(
                2 /*QEvent::MouseButtonPress*/, mouse.button, mouse.buttons,
                mouse.modifiers, mouse.x, mouse.y)
            onReleased: (mouse) => bridge.forwardMouseEvent(
                3 /*QEvent::MouseButtonRelease*/, mouse.button, mouse.buttons,
                mouse.modifiers, mouse.x, mouse.y)
            onPositionChanged: (mouse) => {
                if (mouse.buttons !== Qt.NoButton)
                    bridge.forwardMouseEvent(
                        5 /*QEvent::MouseMove*/, mouse.button, mouse.buttons,
                        mouse.modifiers, mouse.x, mouse.y)
            }
            onWheel: (wheel) => { /* scroll: not forwarded in this pass */ }
        }

        Keys.onPressed: (event) => {
            bridge.forwardKeyEvent(
                6 /*QEvent::KeyPress*/, event.key, event.modifiers,
                event.text)
            event.accepted = true   // we consumed it on the shell's behalf
        }
        Keys.onReleased: (event) => {
            bridge.forwardKeyEvent(
                7 /*QEvent::KeyRelease*/, event.key, event.modifiers,
                event.text)
            event.accepted = true
        }
    }
}