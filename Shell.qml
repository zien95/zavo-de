// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

ApplicationWindow {
    id: shell
    required property var desktop
    required property var installedApps
    required property bool startFullscreen

    readonly property color backgroundColor: "#151126"
    readonly property color card: "#2b2342"
    readonly property color mint: "#91f5c8"
    readonly property color ink: "#f7f2ff"
    readonly property color muted: "#cec2e2"
    property date now: new Date()
    property string feedback: ""
    property bool typing: false

    title: "Zavo DE"
    visible: true
    width: 420
    height: Math.min(780, Screen.desktopAvailableHeight - 42)
    minimumWidth: 310
    minimumHeight: 360
    color: "#0c0a14"

    Component.onCompleted: if (startFullscreen && !desktop.inSession) showFullScreen()

    function selectPage(index) {
        shell.typing = false
        pages.setCurrentIndex(index)
        pages.forceActiveFocus()
    }

    function notify(message) {
        feedback = message
        messageTimer.restart()
    }

    function launchPinned(name) {
        if (name === "Settings")
            selectPage(2)
        else if (name === "Maps") {
            if (!desktop.openMaps())
                notify("Couldn’t open Google Maps")
        } else if (!desktop.launchPinned(name)) {
            if (name === "Camera")
                cameraPopup.open()
            else
                notify(name + " is unavailable. Check the app’s installation.")
        }
    }

    function toggleFullscreen() {
        if (visibility === Window.FullScreen)
            showNormal()
        else
            showFullScreen()
    }

    component Tile: Button {
        id: tile
        property bool selected: false
        property bool compact: false
        property string subtitle: ""
        property color tileColor: shell.card
        implicitWidth: 76
        implicitHeight: 64
        hoverEnabled: true
        padding: 7

        background: Rectangle {
            radius: 16
            color: tile.selected ? shell.mint : tile.tileColor
            opacity: tile.down ? 0.77 : 1
            border.width: tile.visualFocus ? 2 : 0
            border.color: tile.selected ? "#14553b" : shell.mint
        }
        contentItem: ColumnLayout {
            spacing: 3
            Image {
                visible: tile.icon.name !== "" || tile.icon.source.toString() !== ""
                source: tile.icon.source.toString() !== "" ? tile.icon.source
                        : tile.icon.name === "" ? ""
                        : "image://theme/" + (tile.selected ? "active/" : "normal/") + tile.icon.name
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: tile.compact ? 19 : 25
                Layout.preferredHeight: Layout.preferredWidth
                sourceSize.width: 44
                sourceSize.height: 44
                fillMode: Image.PreserveAspectFit
            }
            Text {
                visible: tile.text !== ""
                text: tile.text
                color: tile.selected ? "#122920" : shell.ink
                font.pixelSize: tile.compact ? 12 : 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
            Text {
                visible: tile.subtitle !== ""
                text: tile.subtitle
                color: shell.muted
                font.pixelSize: 11
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
        }
    }

    Timer { interval: 1000; running: true; repeat: true; onTriggered: shell.now = new Date() }
    Timer { id: messageTimer; interval: 4000; onTriggered: shell.feedback = "" }
    Shortcut { sequence: "F11"; enabled: !desktop.inSession; onActivated: shell.toggleFullscreen() }
    Shortcut {
        sequence: "Escape"
        enabled: !cameraPopup.opened && !leaveDialog.opened
        onActivated: {
            if (shell.typing) {
                shell.typing = false
                pages.forceActiveFocus()
            } else {
                shell.selectPage(0)
            }
        }
    }

    Rectangle {
        id: phone
        width: Math.min(parent.width, 520)
        height: parent.height
        anchors.horizontalCenter: parent.horizontalCenter
        color: shell.backgroundColor

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 12
                Layout.preferredHeight: 53
                Text {
                    text: "ZAVO DE"
                    color: shell.ink
                    font.pixelSize: 15
                    font.letterSpacing: 3
                    font.weight: Font.Bold
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: Qt.formatTime(shell.now, "HH:mm")
                    color: shell.muted
                    font.pixelSize: 14
                }
                Tile {
                    visible: !desktop.inSession
                    compact: true
                    text: ""
                    Accessible.name: "Toggle fullscreen"
                    icon.name: shell.visibility === Window.FullScreen ? "view-restore" : "view-fullscreen"
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 48
                    tileColor: shell.backgroundColor
                    onClicked: shell.toggleFullscreen()
                }
            }

            SwipeView {
                id: pages
                objectName: "pages"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                interactive: !shell.typing

                Item {
                    Image {
                        anchors.fill: parent
                        source: "wallpaper.svg"
                        fillMode: Image.PreserveAspectCrop
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 24
                        anchors.top: parent.top
                        anchors.topMargin: Math.min(38, parent.height / 10)
                        spacing: 3
                        Text {
                            text: Qt.formatTime(shell.now, "HH:mm")
                            color: shell.ink
                            font.pixelSize: Math.min(76, pages.height / 4)
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: Qt.formatDate(shell.now, "dddd, MMMM d")
                            color: shell.ink
                            font.pixelSize: 19
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 14
                        height: 77
                        radius: 22
                        color: "#ec1a152c"
                        border.color: "#514565"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 7
                            spacing: 6
                            Repeater {
                                model: [
                                    { name: "Files", icon: "folder" },
                                    { name: "Firefox", icon: "internet-web-browser" },
                                    { name: "Terminal", icon: "utilities-terminal" },
                                    { name: "Camera", icon: "camera-photo" }
                                ]
                                Tile {
                                    required property var modelData
                                    text: modelData.name
                                    icon.name: modelData.icon
                                    compact: true
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    onClicked: shell.launchPinned(text)
                                }
                            }
                        }
                    }
                }

                Item {
                    id: appsPage
                    readonly property string query: search.text.trim().toLowerCase()
                    readonly property var matchingApps: shell.installedApps.filter(
                        app => app.name.toLowerCase().includes(query)
                               || app.appId.toLowerCase().includes(query)
                    )

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        Flickable {
                            id: appScroll
                            objectName: "appScroll"
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            clip: true
                            contentWidth: width
                            contentHeight: appList.height
                            flickableDirection: Flickable.VerticalFlick
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}
                            ColumnLayout {
                                id: appList
                                width: appScroll.width
                                spacing: 8
                                Text {
                                    text: "Apps"
                                    color: shell.ink
                                    font.pixelSize: 29
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    visible: desktop.inSession && !appsPage.query
                                    text: "OPEN WINDOWS"
                                    color: shell.mint
                                    font.pixelSize: 12
                                    font.letterSpacing: 1.1
                                    Layout.topMargin: 8
                                }
                                Text {
                                    visible: desktop.inSession && !appsPage.query && desktop.windows.length === 0
                                    text: "No open windows"
                                    color: shell.muted
                                    font.pixelSize: 14
                                }
                                Repeater {
                                    model: appsPage.query ? [] : desktop.windows
                                    RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Tile {
                                            text: modelData.name
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 52
                                            subtitle: modelData.appId
                                            onClicked: if (!desktop.focusWindow(modelData.windowId))
                                                shell.notify("That window has closed")
                                        }
                                        Tile {
                                            text: "×"
                                            Accessible.name: "Close " + modelData.name
                                            Layout.preferredWidth: 52
                                            Layout.preferredHeight: 52
                                            onClicked: if (!desktop.closeWindow(modelData.windowId))
                                                shell.notify("That window has closed")
                                        }
                                    }
                                }
                                Text {
                                    visible: desktop.inSession && !appsPage.query
                                    text: "INSTALLED APPS"
                                    color: shell.mint
                                    font.pixelSize: 12
                                    font.letterSpacing: 1.1
                                    Layout.topMargin: 8
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: appScroll.width >= 370 ? 4 : 3
                                    columnSpacing: 7
                                    rowSpacing: 7
                                    Repeater {
                                        model: appsPage.matchingApps
                                        Tile {
                                            required property var modelData
                                            text: modelData.name
                                            icon.source: modelData.iconUrl || ""
                                            icon.name: modelData.iconName
                                            compact: true
                                            Layout.preferredWidth: 68
                                            Layout.preferredHeight: 65
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 56
                                            onClicked: {
                                                if (modelData.shortcut)
                                                    shell.launchPinned(text)
                                                else if (!desktop.launch(modelData.appId))
                                                    shell.notify("Couldn’t launch " + text)
                                            }
                                        }
                                    }
                                }
                                Text {
                                    visible: appsPage.matchingApps.length === 0
                                    text: "No matching apps"
                                    color: shell.muted
                                    font.pixelSize: 15
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            TextField {
                                id: search
                                objectName: "search"
                                Layout.fillWidth: true
                                Layout.preferredHeight: 52
                                leftPadding: 15
                                rightPadding: 15
                                color: shell.ink
                                placeholderText: "Search installed apps"
                                placeholderTextColor: shell.muted
                                Accessible.name: placeholderText
                                font.pixelSize: 16
                                selectByMouse: true
                                inputMethodHints: Qt.ImhNoPredictiveText
                                TapHandler { onTapped: shell.typing = true }
                                onAccepted: {
                                    shell.typing = false
                                    pages.forceActiveFocus()
                                }
                                onTextChanged: appScroll.contentY = 0
                                background: Rectangle {
                                    radius: 16
                                    color: shell.card
                                    border.width: search.activeFocus ? 2 : 0
                                    border.color: shell.mint
                                }
                            }
                            Tile {
                                visible: !shell.typing && search.text !== ""
                                compact: true
                                text: "Clear"
                                Layout.preferredWidth: 60
                                Layout.preferredHeight: 52
                                onClicked: {
                                    search.clear()
                                    pages.forceActiveFocus()
                                }
                            }
                        }
                    }
                }

                Item {
                    Flickable {
                        anchors.fill: parent
                        anchors.margins: 16
                        contentWidth: width
                        contentHeight: Math.max(height, settingsColumn.implicitHeight)
                        clip: true
                        flickableDirection: Flickable.VerticalFlick
                        boundsBehavior: Flickable.StopAtBounds
                        ColumnLayout {
                            id: settingsColumn
                            width: parent.width
                            height: Math.max(parent.height, implicitHeight)
                            spacing: 10
                            Text {
                                text: "Quick Settings"
                                color: shell.ink
                                font.pixelSize: 29
                                font.weight: Font.DemiBold
                            }
                            Text {
                                text: desktop.inSession ? "ZAVO SESSION" : "KUBUNTU SETTINGS"
                                color: shell.mint
                                font.pixelSize: 12
                                font.letterSpacing: 1.4
                            }
                            Item { Layout.fillHeight: true }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 10
                                rowSpacing: 10
                                Repeater {
                                    model: desktop.inSession ? [
                                        { name: "Open windows", icon: "view-grid", action: () => shell.selectPage(1) },
                                        { name: "Camera", icon: "camera-photo", action: () => cameraPopup.open() },
                                        { name: "Home", icon: "go-home", action: () => shell.selectPage(0) },
                                        { name: "End session", icon: "system-log-out", action: () => leaveDialog.open() }
                                    ] : [
                                        { name: "Network", icon: "network-wireless" },
                                        { name: "Bluetooth", icon: "preferences-system-bluetooth" },
                                        { name: "Sound", icon: "audio-volume-high" },
                                        { name: "Display", icon: "preferences-desktop-display" },
                                        { name: "Power", icon: "battery" },
                                        { name: "Notifications", icon: "preferences-desktop-notification" }
                                    ]
                                    Tile {
                                        required property var modelData
                                        text: modelData.name
                                        icon.name: modelData.icon
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 69
                                        onClicked: {
                                            if (modelData.action)
                                                modelData.action()
                                            else if (!desktop.openSetting(text))
                                                shell.notify("That settings panel isn’t available")
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Text {
                visible: shell.feedback !== ""
                text: shell.feedback
                color: shell.ink
                font.pixelSize: 14
                Layout.fillWidth: true
                Layout.margins: 10
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            RowLayout {
                visible: !shell.typing
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.topMargin: 7
                Layout.bottomMargin: 9
                spacing: 6
                Repeater {
                    model: [
                        { name: "Home", icon: "go-home" },
                        { name: "Apps", icon: "view-grid" },
                        { name: "Quick Settings", icon: "preferences-system" }
                    ]
                    Tile {
                        required property var modelData
                        required property int index
                        text: modelData.name
                        icon.name: modelData.icon
                        selected: pages.currentIndex === index
                        compact: true
                        Layout.fillWidth: true
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 57
                        onClicked: shell.selectPage(index)
                    }
                }
            }

            TouchKeys {
                objectName: "keyboard"
                visible: shell.typing
                Layout.fillWidth: true
                Layout.margins: 8
                field: search
                onDone: {
                    shell.typing = false
                    pages.forceActiveFocus()
                }
            }
        }
    }

    Popup {
        id: cameraPopup
        objectName: "cameraPopup"
        anchors.centerIn: Overlay.overlay
        width: Math.min(shell.width - 24, 520)
        height: Math.min(shell.height - 24, 470)
        padding: 12
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        background: Rectangle {
            color: shell.backgroundColor
            border.color: shell.card
            radius: 22
        }
        contentItem: ColumnLayout {
            spacing: 10
            Text {
                text: "Camera preview"
                color: shell.ink
                font.pixelSize: 22
                font.weight: Font.DemiBold
            }
            Loader {
                active: cameraPopup.visible
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: Item {
                    property string failure: ""
                    MediaDevices { id: devices }
                    Rectangle { anchors.fill: parent; color: "#0c0a14"; radius: 12 }
                    VideoOutput { id: preview; anchors.fill: parent }
                    CaptureSession {
                        videoOutput: preview
                        camera: Camera {
                            active: devices.videoInputs.length > 0 && failure === ""
                            cameraDevice: devices.defaultVideoInput
                            onErrorOccurred: (error, errorString) => failure = errorString || "Camera unavailable"
                        }
                    }
                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 24
                        visible: failure !== "" || devices.videoInputs.length === 0
                        text: failure || "No accessible camera found"
                        color: shell.ink
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }
            Tile {
                text: "Close camera"
                Layout.fillWidth: true
                onClicked: cameraPopup.close()
            }
        }
    }

    Dialog {
        id: leaveDialog
        anchors.centerIn: Overlay.overlay
        width: Math.min(shell.width - 30, 400)
        modal: true
        focus: true
        padding: 15
        background: Rectangle { color: shell.backgroundColor; radius: 22 }
        contentItem: ColumnLayout {
            spacing: 14
            Text {
                text: "End Zavo session?"
                color: shell.ink
                font.pixelSize: 22
            }
            Text {
                text: "Open apps will close."
                color: shell.muted
                font.pixelSize: 15
            }
            RowLayout {
                Layout.fillWidth: true
                Tile { text: "Cancel"; Layout.fillWidth: true; onClicked: leaveDialog.close() }
                Tile {
                    text: "End session"
                    selected: true
                    Layout.fillWidth: true
                    onClicked: {
                        leaveDialog.close()
                        if (!desktop.endSession())
                            shell.notify("Couldn’t end the session")
                    }
                }
            }
        }
    }
}
