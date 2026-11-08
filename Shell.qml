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
    readonly property color mint: desktop.appearance.accent
    readonly property color accentInk: 0.2126 * mint.r + 0.7152 * mint.g + 0.0722 * mint.b > 0.48
        ? "#122920" : "#f7f2ff"
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
        if (name === "Settings") {
            if (!desktop.openSettings())
                notify("Couldn’t open Settings")
        } else if (name === "Maps") {
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
        property bool light: false
        property string subtitle: ""
        property color tileColor: shell.card
        implicitWidth: 76
        implicitHeight: 64
        hoverEnabled: true
        padding: 7

        background: Rectangle {
            radius: 16
            color: tile.light ? (tile.selected ? "#dedee5" : "#f2f2f5") : tile.selected ? shell.mint : tile.tileColor
            opacity: tile.down ? 0.77 : 1
            Behavior on color { ColorAnimation { duration: desktop.appearance.animations ? 150 : 0 } }
            border.width: tile.visualFocus ? 2 : 0
            border.color: tile.light ? "#151515" : tile.selected ? "#14553b" : shell.mint
        }
        contentItem: ColumnLayout {
            spacing: 3
            Image {
                visible: tile.icon.name !== "" || tile.icon.source.toString() !== ""
                source: tile.icon.source.toString() !== "" ? tile.icon.source
                        : tile.icon.name === "" ? ""
                        : "image://theme/" + (tile.light ? "dialog/" : tile.selected ? "active/" : "normal/")
                          + tile.icon.name + "/" + shell.mint.toString().slice(1)
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
                color: tile.light ? "#111111" : tile.selected ? shell.accentInk : shell.ink
                font.pixelSize: tile.compact ? 12 : 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
            Text {
                visible: tile.subtitle !== ""
                text: tile.subtitle
                color: tile.light ? "#353535" : tile.selected ? shell.accentInk : shell.muted
                font.pixelSize: 11
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
        }
    }

    Timer { interval: 1000; running: true; repeat: true; onTriggered: shell.now = new Date() }
    Timer { id: messageTimer; interval: 6000; onTriggered: shell.feedback = "" }
    Timer {
        interval: pages.currentIndex === 2 ? 3500 : 12000
        running: true
        triggeredOnStart: true
        repeat: true
        onTriggered: desktop.refreshRadios()
    }
    Connections {
        target: desktop
        function onSystemError(error) { shell.notify(error) }
    }
    Shortcut { sequence: "F11"; enabled: !desktop.inSession; onActivated: shell.toggleFullscreen() }
    Shortcut {
        sequence: "Escape"
        enabled: !cameraPopup.opened && !powerDialog.opened && !confirmPower.opened
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
                Text {
                    visible: desktop.radios.battery >= 0
                    text: (desktop.radios.battery ?? 0) + "%"
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
                interactive: !shell.typing && !quickDevices.adjusting
                onCurrentIndexChanged: if (currentIndex === 2) desktop.refreshRadios()

                Item {
                    Image {
                        anchors.fill: parent
                        source: "wallpaper.svg"
                        fillMode: Image.PreserveAspectCrop
                    }
                    Image {
                        anchors.fill: parent
                        source: desktop.appearance.wallpaper
                        visible: status === Image.Ready
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
                                text: "CONNECTIVITY & POWER"
                                color: shell.mint
                                font.pixelSize: 12
                                font.letterSpacing: 1.4
                            }
                            DeviceControls {
                                id: quickDevices
                                desktop: shell.desktop
                                Layout.fillWidth: true
                            }
                            Item { Layout.fillHeight: true }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 10
                                rowSpacing: 10
                                Repeater {
                                    model: [
                                        { name: "Wi-Fi", key: "wifi", icon: "network-wireless" },
                                        { name: "Bluetooth", key: "bluetooth", icon: "preferences-system-bluetooth" },
                                        { name: "Mobile data", key: "mobile", icon: "network-mobile-100" },
                                        { name: "GPS", key: "gps", icon: "mark-location" }
                                    ]
                                    Tile {
                                        required property var modelData
                                        readonly property var radioState: desktop.radios[modelData.key]
                                        text: modelData.name
                                        icon.name: modelData.icon
                                        selected: radioState === 1
                                        subtitle: radioState === 1 ? "On" : radioState === 0 ? "Off"
                                                : radioState === 2 ? "Hardware blocked"
                                                : radioState === -1 ? "Unavailable" : "Checking…"
                                        enabled: (radioState === 0 || radioState === 1) && !desktop.systemBusy
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 82
                                        onClicked: desktop.setRadio(modelData.key, radioState === 0)
                                    }
                                }
                                Tile {
                                    text: "Power"
                                    icon.name: "system-shutdown"
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 74
                                    enabled: !desktop.systemBusy
                                    onClicked: powerDialog.open()
                                }
                                Tile {
                                    text: "Settings"
                                    icon.name: "preferences-system"
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 74
                                    onClicked: shell.launchPinned("Settings")
                                }
                                Tile {
                                    text: "Rotation lock"
                                    icon.name: "object-rotate-right"
                                    visible: desktop.inSession
                                    selected: desktop.appearance.rotationLock
                                    subtitle: selected ? "Locked" : "Auto"
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 74
                                    onClicked: desktop.setRotationLock(!selected)
                                }
                                Tile {
                                    text: "Camera"
                                    icon.name: "camera-photo"
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 74
                                    onClicked: cameraPopup.open()
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

    component DialogSurface: Rectangle {
        color: "#ffffff"
        border.color: "#d6d6dc"
        radius: 22
    }

    component DialogEnter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: desktop.appearance.animations ? 155 : 0
            easing.type: Easing.OutCubic
        }
    }

    component DialogExit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: desktop.appearance.animations ? 105 : 0
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
        background: DialogSurface {}
        enter: DialogEnter {}
        exit: DialogExit {}
        contentItem: ColumnLayout {
            spacing: 10
            Text {
                text: "Camera preview"
                color: "#111111"
                font.pixelSize: 22
                font.weight: Font.DemiBold
            }
            Loader {
                id: cameraLoader
                active: cameraPopup.visible
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: Item {
                    id: cameraView
                    property string failure: ""
                    property string captureStatus: ""
                    property bool saving: false
                    readonly property bool canTakePhoto: shutter.readyForCapture && !saving

                    function takePhoto() {
                        const path = desktop.photoPath()
                        if (path === "") {
                            captureStatus = "Couldn’t open your Pictures folder."
                            return
                        }
                        saving = true
                        captureStatus = "Saving…"
                        if (shutter.captureToFile(path) < 0) {
                            saving = false
                            captureStatus = shutter.errorString || "Couldn’t take the photo."
                        }
                    }

                    MediaDevices { id: devices }
                    Rectangle { anchors.fill: parent; color: "#0c0a14"; radius: 12 }
                    VideoOutput { id: preview; anchors.fill: parent }
                    CaptureSession {
                        videoOutput: preview
                        imageCapture: ImageCapture {
                            id: shutter
                            fileFormat: ImageCapture.JPEG
                            onImageSaved: (requestId, fileName) => {
                                cameraView.saving = false
                                cameraView.captureStatus = "Saved in Pictures · " + fileName.split("/").pop()
                            }
                            onErrorOccurred: (requestId, error, errorString) => {
                                cameraView.saving = false
                                cameraView.captureStatus = errorString || "Couldn’t save the photo."
                            }
                        }
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
            Text {
                visible: !!cameraLoader.item && cameraLoader.item.captureStatus !== ""
                text: cameraLoader.item ? cameraLoader.item.captureStatus : ""
                color: "#353535"
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                Tile {
                    light: true
                    text: "Close camera"
                    Layout.fillWidth: true
                    onClicked: cameraPopup.close()
                }
                Tile {
                    light: true
                    text: "Take photo"
                    icon.name: "camera-photo"
                    selected: true
                    Layout.fillWidth: true
                    enabled: cameraLoader.item ? cameraLoader.item.canTakePhoto : false
                    onClicked: cameraLoader.item.takePhoto()
                }
            }
        }
    }

    Dialog {
        id: powerDialog
        anchors.centerIn: Overlay.overlay
        width: Math.min(shell.width - 30, 400)
        modal: true
        focus: true
        padding: 15
        background: DialogSurface {}
        enter: DialogEnter {}
        exit: DialogExit {}
        contentItem: ColumnLayout {
            spacing: 8
            Text {
                text: "Power"
                color: "#111111"
                font.pixelSize: 23
                font.weight: Font.DemiBold
            }
            Repeater {
                model: [
                    { name: "Suspend", key: "suspend", question: "Suspend this device?" },
                    { name: "Restart", key: "reboot", question: "Restart this device?" },
                    { name: "Shut down", key: "poweroff", question: "Shut down this device?" },
                    { name: "End session", key: "logout", question: "End the Zavo session?" }
                ]
                Tile {
                    light: true
                    required property var modelData
                    visible: modelData.key !== "logout" || desktop.inSession
                    text: modelData.name
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    onClicked: {
                        confirmPower.action = modelData
                        powerDialog.close()
                        confirmPower.open()
                    }
                }
            }
            Tile {
                light: true
                text: "Cancel"
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                onClicked: powerDialog.close()
            }
        }
    }

    Dialog {
        id: confirmPower
        property var action: ({ name: "", key: "", question: "" })
        anchors.centerIn: Overlay.overlay
        width: Math.min(shell.width - 30, 400)
        modal: true
        focus: true
        padding: 15
        background: DialogSurface {}
        enter: DialogEnter {}
        exit: DialogExit {}
        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: confirmPower.action.question
                color: "#111111"
                font.pixelSize: 22
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Tile { light: true; text: "Cancel"; Layout.fillWidth: true; onClicked: confirmPower.close() }
                Tile {
                    light: true
                    text: confirmPower.action.name
                    selected: true
                    Layout.fillWidth: true
                    onClicked: {
                        const key = confirmPower.action.key
                        confirmPower.close()
                        if (key === "logout") {
                            if (!desktop.endSession())
                                shell.notify("Couldn’t end the session")
                        } else {
                            desktop.power(key)
                        }
                    }
                }
            }
        }
    }
}
