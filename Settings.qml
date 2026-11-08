// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: settings
    required property var desktop
    readonly property color accent: desktop.appearance.accent
    readonly property var themes: desktop.cursorThemes()
    readonly property var rotations: [0, 90, 180, 270]
    property string feedback: ""

    title: "Zavo Settings"
    width: 435
    height: 690
    minimumWidth: 310
    visible: true
    color: "#151126"
    palette.highlight: accent

    component Section: Flickable {
        id: section
        property alias fields: form.data
        contentWidth: width
        contentHeight: form.implicitHeight + 26
        clip: true
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        ColumnLayout {
            id: form
            x: 14
            y: 12
            width: section.width - 28
            spacing: 10
        }
    }

    component Heading: Label {
        color: "#f7f2ff"
        font.pixelSize: 22
        font.weight: Font.DemiBold
        Layout.fillWidth: true
    }

    component SettingButton: Button {
        palette.buttonText: "#f7f2ff"
        Layout.fillWidth: true
        Layout.preferredHeight: 49
        background: Rectangle { color: "#2b2342"; radius: 12 }
    }

    Timer { interval: 3500; running: true; repeat: true; triggeredOnStart: true; onTriggered: desktop.refreshRadios() }
    Timer { id: feedbackTimer; interval: 5500; onTriggered: settings.feedback = "" }
    Connections {
        target: desktop
        function onSystemError(error) {
            settings.feedback = error
            feedbackTimer.restart()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            Heading { text: "Settings"; font.pixelSize: 28 }
            Button { text: "Close"; Layout.preferredHeight: 44; onClicked: settings.close() }
        }

        TabBar {
            id: tabs
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            currentIndex: sections.currentIndex
            onCurrentIndexChanged: sections.setCurrentIndex(currentIndex)
            Repeater {
                model: ["Display", "Network", "Style", "Power"]
                TabButton {
                    required property string modelData
                    text: modelData
                    id: tab
                    height: 46
                    font.pixelSize: 13
                    palette.buttonText: "#f7f2ff"
                    background: Rectangle {
                        color: tab.checked ? "#483b63" : "#2b2342"
                        radius: 9
                        Behavior on color {
                            ColorAnimation { duration: desktop.appearance.animations ? 150 : 0 }
                        }
                    }
                }
            }
        }

        SwipeView {
            id: sections
            interactive: !displayControls.adjusting
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Section {
                fields: [
                    Heading { text: "Display & sound" },
                    DeviceControls {
                        id: displayControls
                        desktop: settings.desktop
                        showPower: false
                        Layout.fillWidth: true
                    },
                    Heading { text: "Rotation"; visible: desktop.inSession },
                    SwitchDelegate {
                        palette.text: "#f7f2ff"
                        visible: desktop.inSession
                        Layout.fillWidth: true
                        Layout.preferredHeight: 54
                        text: "Rotation lock"
                        checked: desktop.appearance.rotationLock
                        onToggled: desktop.setRotationLock(checked)
                    },
                    Label {
                        visible: desktop.inSession
                        text: "Unlock to follow a supported accelerometer. Choosing an orientation locks it."
                        color: "#cec2e2"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    },
                    ComboBox {
                        visible: desktop.inSession
                        Accessible.name: "Display orientation"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        model: ["Normal", "90°", "180°", "270°"]
                        currentIndex: Math.max(0, settings.rotations.indexOf(desktop.appearance.rotation))
                        onActivated: index => desktop.setRotation(settings.rotations[index])
                    }
                ]
            }

            Section {
                fields: [
                    Heading { text: "Connectivity" },
                    Repeater {
                        model: [
                            { name: "Wi-Fi", key: "wifi" }, { name: "Bluetooth", key: "bluetooth" },
                            { name: "Mobile data", key: "mobile" }, { name: "GPS", key: "gps" }
                        ]
                        SwitchDelegate {
                            palette.text: "#f7f2ff"
                            required property var modelData
                            readonly property var radio: desktop.radios[modelData.key]
                            text: modelData.name + (radio === -1 ? " · Unavailable" : radio === 2 ? " · Hardware blocked" : "")
                            enabled: (radio === 0 || radio === 1) && !desktop.systemBusy
                            checked: radio === 1
                            Layout.fillWidth: true
                            Layout.preferredHeight: 54
                            onToggled: desktop.setRadio(modelData.key, checked)
                        }
                    }
                ]
            }

            Section {
                fields: [
                    Heading { text: "Personalization" },
                    Label { text: "Accent color"; color: "#cec2e2" },
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: ["#91f5c8", "#a8c8ff", "#ddbcff", "#ffc090", "#ffabc2"]
                            Button {
                                required property string modelData
                                width: 42
                                height: 42
                                Accessible.name: "Accent " + modelData
                                onClicked: desktop.saveAppearance("accent", modelData)
                                background: Rectangle {
                                    radius: 21
                                    color: parent.modelData
                                    border.width: settings.accent.toString() === parent.modelData ? 3 : 0
                                    border.color: "#f7f2ff"
                                }
                            }
                        }
                    },
                    SettingButton { text: "Pick a color…"; onClicked: { colorPicker.selectedColor = settings.accent; colorPicker.open() } },
                    Heading { text: "Wallpaper" },
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 108
                        radius: 12
                        color: "#2b2342"
                        clip: true
                        Image { anchors.fill: parent; source: "wallpaper.svg"; fillMode: Image.PreserveAspectCrop }
                        Image {
                            anchors.fill: parent
                            source: desktop.appearance.wallpaper
                            visible: status === Image.Ready
                            fillMode: Image.PreserveAspectCrop
                        }
                    },
                    RowLayout {
                        Layout.fillWidth: true
                        SettingButton { text: "Choose image…"; onClicked: wallpaperPicker.open() }
                        SettingButton { text: "Reset"; onClicked: desktop.saveAppearance("wallpaper", "") }
                    },
                    Heading { text: "Cursor" },
                    ComboBox {
                        Accessible.name: "Cursor theme"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        model: settings.themes
                        currentIndex: Math.max(0, settings.themes.indexOf(desktop.appearance.cursorTheme))
                        onActivated: index => desktop.saveAppearance("cursorTheme", settings.themes[index])
                    },
                    ComboBox {
                        readonly property var sizes: [16, 24, 32, 40, 48, 64]
                        Accessible.name: "Cursor size"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        model: ["16 px", "24 px", "32 px", "40 px", "48 px", "64 px"]
                        currentIndex: Math.max(0, sizes.indexOf(desktop.appearance.cursorSize))
                        onActivated: index => desktop.saveAppearance("cursorSize", sizes[index])
                    },
                    SwitchDelegate {
                        palette.text: "#f7f2ff"
                        text: "Shell animations"
                        checked: desktop.appearance.animations
                        Layout.fillWidth: true
                        Layout.preferredHeight: 53
                        onToggled: desktop.saveAppearance("animations", checked)
                    }
                ]
            }

            Section {
                fields: [
                    Heading { text: "Battery & power" },
                    DeviceControls {
                        desktop: settings.desktop
                        showSliders: false
                        Layout.fillWidth: true
                    }
                ]
            }
        }

        Label {
            visible: settings.feedback !== ""
            Layout.fillWidth: true
            Layout.margins: 10
            text: settings.feedback
            color: "#ffc090"
            wrapMode: Text.WordWrap
        }
    }

    FileDialog {
        id: wallpaperPicker
        title: "Choose a wallpaper"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.svg)", "All files (*)"]
        onAccepted: desktop.saveAppearance("wallpaper", selectedFile.toString())
    }
    ColorDialog {
        id: colorPicker
        title: "Choose an accent color"
        onAccepted: desktop.saveAppearance("accent", selectedColor.toString())
    }
}
