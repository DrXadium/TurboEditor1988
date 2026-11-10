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

    // CRT effect chain over the whole window (edge-to-edge, no bezel).
    EffectChain {
        objectName: "crtEffectChain"
        anchors.fill: parent
        sourceItem: bridge.sourceItem
    }
}