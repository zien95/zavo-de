// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.layershell as LayerShell

Window {
    id: navigation
    required property var desktop
    property bool overview: false
    property double pressTime: 0
    property real reveal: 0
    readonly property bool showingSheet: overview || reveal > 0
    readonly property int barHeight: 44

    title: "Zavo Navigation"
    width: 400
    height: showingSheet ? Math.min(380, Screen.height - 20) : barHeight
    color: "transparent"

    LayerShell.Window.anchors: LayerShell.Window.AnchorBottom | LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorRight
    LayerShell.Window.layer: LayerShell.Window.LayerOverlay
    LayerShell.Window.exclusionZone: barHeight
    LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityNone
    LayerShell.Window.activateOnShow: false
    LayerShell.Window.scope: "zavo-navigation"

    onOverviewChanged: {
        sheetMotion.stop()
        sheetMotion.from = reveal
        sheetMotion.to = overview ? 1 : 0
        sheetMotion.duration = !desktop.appearance.animations ? 0 : overview ? 220 : 160
        sheetMotion.start()
    }
    NumberAnimation {
        id: sheetMotion
        target: navigation
        property: "reveal"
        easing.type: Easing.OutCubic
    }

    function showOverview() {
        desktop.refreshWindows()
        overview = true
    }

    function activate() {
        if (overview)
            overview = false
        else if (!desktop.closeFocusedWindow(navigation.screen.name))
            showOverview()
    }

    Rectangle {
        anchors.fill: parent
        visible: navigation.showingSheet
        opacity: navigation.reveal
        color: "#600a0913"
        TapHandler { onTapped: navigation.overview = false }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: bar.top
        anchors.bottomMargin: -28 * (1 - navigation.reveal)
        width: Math.min(parent.width - 16,
            desktop.tabletLayout(navigation.screen.width, navigation.screen.height - navigation.barHeight,
                                 desktop.appearance.layoutMode) ? 680 : 460)
        height: navigation.height - navigation.barHeight
        visible: navigation.showingSheet
        enabled: navigation.overview
        opacity: navigation.reveal
        color: "#151126"
        radius: 18
        border.color: "#443758"
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: "Active apps"
                    color: "#f7f2ff"
                    font.pixelSize: 23
                    font.weight: Font.DemiBold
                }
                Button {
                    text: "Home"
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 48
                    onClicked: {
                        desktop.showHome()
                        navigation.overview = false
                    }
                }
            }

            Label {
                visible: desktop.windows.length === 0
                text: "No active apps"
                color: "#cec2e2"
                Layout.alignment: Qt.AlignHCenter
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6
                boundsBehavior: Flickable.StopAtBounds
                model: desktop.windows
                delegate: RowLayout {
                    required property var modelData
                    width: ListView.view.width
                    height: 54
                    spacing: 6
                    Button {
                        id: entry
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: modelData.name
                        Accessible.description: modelData.appId
                        onClicked: {
                            if (desktop.focusWindow(modelData.windowId))
                                navigation.overview = false
                            else
                                desktop.refreshWindows()
                        }
                        background: Rectangle {
                            color: entry.down ? "#453963" : "#2b2342"
                            radius: 12
                            Behavior on color { ColorAnimation { duration: 110 } }
                        }
                        contentItem: Label {
                            text: entry.text
                            color: "#f7f2ff"
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    Button {
                        id: closeButton
                        text: "×"
                        Accessible.name: "Close " + modelData.name
                        Layout.preferredWidth: 52
                        Layout.fillHeight: true
                        onClicked: {
                            desktop.closeWindow(modelData.windowId)
                            desktop.refreshWindows()
                        }
                        background: Rectangle {
                            color: closeButton.down ? "#6d3d60" : "#32263f"
                            radius: 12
                            Behavior on color { ColorAnimation { duration: 110 } }
                        }
                        contentItem: Label {
                            text: closeButton.text
                            color: "#f7f2ff"
                            font.pixelSize: 25
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
                ScrollBar.vertical: ScrollBar {}
            }
            Button {
                visible: desktop.canChangeLayout
                text: desktop.wideLayout ? "Use portrait layout" : "Use wide layout"
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                onClicked: {
                    desktop.toggleLayout()
                    navigation.overview = false
                }
            }
        }
    }

    Rectangle {
        id: bar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: navigation.barHeight
        color: "#151126"

        Item {
            width: Math.min(200, bar.width)
            height: bar.height
            anchors.centerIn: parent
            Accessible.role: Accessible.Button
            Accessible.name: "Close app or show active apps"
            Accessible.onPressAction: navigation.activate()

            Rectangle {
                anchors.centerIn: parent
                width: 126
                height: 5
                radius: 3
                color: press.pressed || swipe.active ? desktop.appearance.accent : "#e7dcf5"
                Behavior on color { ColorAnimation { duration: 120 } }
            }

            TapHandler {
                id: press
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onPressedChanged: if (pressed) navigation.pressTime = Date.now()
                onTapped: (point, button) => {
                    if (button === Qt.RightButton)
                        navigation.overview ? navigation.overview = false : navigation.showOverview()
                    else
                        navigation.activate()
                }
            }

            DragHandler {
                id: swipe
                enabled: !navigation.overview
                target: null
                xAxis.enabled: false
                onGrabChanged: (transition, point) => {
                    if (transition !== PointerDevice.UngrabExclusive)
                        return
                    const distance = point.scenePressPosition.y - point.scenePosition.y
                    const across = Math.abs(point.scenePressPosition.x - point.scenePosition.x)
                    if (distance < 40 || across > distance)
                        return
                    const elapsed = Date.now() - navigation.pressTime
                    if (elapsed < 320 && distance * 1000 / Math.max(1, elapsed) >= 180)
                        navigation.activate()
                    else
                        navigation.showOverview()
                }
            }
        }
    }
}
