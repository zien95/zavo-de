// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

ColumnLayout {
    id: controls
    required property var desktop
    property bool showSliders: true
    property bool showPower: true
    readonly property bool adjusting: brightness.adjusting || volume.adjusting
    readonly property color accent: desktop.appearance.accent
    spacing: 8

    SoundEffect {
        id: volumeCue
        source: controls.showSliders ? "volume-tick.wav" : ""
        volume: 0.48
    }
    Connections {
        target: controls.desktop
        function onVolumeAdjusted(percent) {
            if (controls.showSliders && percent > 0 && volumeCue.status === SoundEffect.Ready)
                volumeCue.play()
        }
    }

    component LevelControl: ColumnLayout {
        id: level
        required property string label
        required property string key
        readonly property var currentLevel: controls.desktop.radios[key]
        property bool touched: false
        readonly property bool adjusting: slider.pressed
        Layout.fillWidth: true
        spacing: 1

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: level.label
                color: "#f7f2ff"
                font.pixelSize: 15
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            Label {
                text: level.currentLevel === undefined ? "Checking…"
                    : level.currentLevel < 0 ? "Unavailable"
                    : (Math.round(slider.value) === 0 ||
                        (level.currentLevel === 0 && !slider.pressed && !level.touched)) ? "Off"
                    : "On · " + Math.round(slider.value) + "%"
                color: "#cec2e2"
            }
        }

        Slider {
            id: slider
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            from: level.key === "brightness" ? 1 : 0
            to: 100
            stepSize: 1
            enabled: level.currentLevel >= 0
            Accessible.name: level.label
            onMoved: {
                level.touched = true
                if ((!pressed || level.key === "volume") && !commit.running)
                    commit.start()
            }
            onPressedChanged: if (!pressed && level.touched) commit.restart()
            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: slider.availableWidth
                height: 8
                radius: 4
                color: "#372d4d"
                Rectangle {
                    width: slider.visualPosition * parent.width
                    height: parent.height
                    radius: 4
                    color: controls.accent
                }
            }
            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: 25
                height: 25
                radius: 13
                color: slider.pressed ? controls.accent : "#f7f2ff"
                opacity: slider.enabled ? 1 : 0.45
            }
        }
        Binding {
            target: slider
            property: "value"
            value: level.currentLevel >= 0 ? level.currentLevel : 0
            when: !slider.pressed && !level.touched && !controls.desktop.systemBusy
            restoreMode: Binding.RestoreNone
        }
        Timer {
            id: commit
            interval: 150
            onTriggered: {
                if (controls.desktop.systemBusy) {
                    restart()
                    return
                }
                controls.desktop.setLevel(level.key, Math.round(slider.value))
                level.touched = false
            }
        }
    }

    LevelControl { id: brightness; visible: controls.showSliders; label: "Brightness"; key: "brightness" }
    LevelControl { id: volume; visible: controls.showSliders; label: "Volume"; key: "volume" }

    RowLayout {
        visible: controls.showPower
        Layout.fillWidth: true
        Label {
            text: "Battery"
            color: "#f7f2ff"
            font.pixelSize: 15
            font.weight: Font.DemiBold
            Layout.fillWidth: true
        }
        Label {
            readonly property var battery: controls.desktop.radios.battery
            text: battery === undefined ? "Checking…" : battery < 0 ? "Unavailable" : battery + "%"
            color: "#cec2e2"
        }
    }

    SwitchDelegate {
        visible: controls.showPower
        Layout.fillWidth: true
        Layout.preferredHeight: 52
        text: "Power saver"
        checked: controls.desktop.radios.saver === 1
        enabled: controls.desktop.radios.saver >= 0 && !controls.desktop.systemBusy
        onToggled: controls.desktop.setPowerSaver(checked)
        palette.text: "#f7f2ff"
        palette.windowText: "#f7f2ff"
        palette.highlight: controls.accent
        background: Rectangle { radius: 12; color: "#2b2342" }
    }
}
