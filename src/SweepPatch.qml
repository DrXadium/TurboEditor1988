// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// One-shot 90° clockwise transform for the CRT pass: the EffectChain sweeps
// its band along screen X; rotated CW, the band travels top→bottom (the user's
// headline fix for TV-revision2). Geometry: a (H×W) patch hosts the (W×H)
// chain rotated 90° CW about its center. To land the rotated display box at
// the parent's (0,0), place the patch at x = (parentW − parentH)/2,
// y = (parentH − parentW)/2 (for patch size (parentH × parentW)).
Item {
    id: patch
    property Item chain
    rotation: 90

    // Chain origin inside the patch (solves rotated-center == patch center).
    readonly property real chainX: (height - chain.width) / 2
    readonly property real chainY: (width - chain.height) / 2
    // The host slot that receives the chain — exposed so scenes that must
    // place additional items (e.g. the source) in the same subtree/window can
    // target it (items in different windows never composite together).
    // NOTE (Qt 6.10 scoping): an outer object cannot reference the delegate's
    // `id` from a binding — resolve through the Repeater's item list instead.
    property Item hostItem: patchRepeater.count > 0
        ? patchRepeater.itemAt(0) : null

    Repeater {
        id: patchRepeater
        model: patch.chain ? 1 : 0
        Item {
            x: patch.chainX
            y: patch.chainY
            width: patch.chain.width
            height: patch.chain.height
            objectName: "sweepHost"
            // Host the live EffectChain (reparented from its inline site).
            children: [patch.chain]
        }
    }
}
