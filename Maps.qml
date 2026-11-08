// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine

ApplicationWindow {
    id: maps
    title: "Google Maps"
    visible: true
    width: 840
    height: 680
    color: "#151126"
    property bool typing: false

    function findPlace() {
        const query = place.text.trim()
        if (query !== "")
            mapView.url = "https://www.google.com/maps/search/?api=1&query=" + encodeURIComponent(query)
        maps.typing = false
        mapView.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ToolBar {
            Layout.fillWidth: true
            background: Rectangle { color: "#2b2342" }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 5
                spacing: 6

                Button {
                    text: "Back"
                    Layout.minimumWidth: 62
                    Layout.preferredHeight: 48
                    enabled: mapView.canGoBack
                    onClicked: mapView.goBack()
                }
                Label {
                    text: "Google Maps"
                    color: "#f7f2ff"
                    font.pixelSize: 17
                    Layout.fillWidth: true
                }
                Button {
                    text: "Reload"
                    Layout.minimumWidth: 68
                    Layout.preferredHeight: 48
                    onClicked: mapView.reload()
                }
                Button {
                    text: "Close"
                    Layout.minimumWidth: 62
                    Layout.preferredHeight: 48
                    onClicked: maps.close()
                }
            }
        }

        WebEngineView {
            id: mapView
            Layout.fillWidth: true
            Layout.fillHeight: true
            url: "https://www.google.com/maps"
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 6
            TextField {
                id: place
                placeholderText: "Search for a place"
                Accessible.name: placeholderText
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                TapHandler { onTapped: maps.typing = true }
                onAccepted: maps.findPlace()
            }
            Button {
                text: "Search"
                Layout.preferredWidth: 68
                Layout.preferredHeight: 48
                onClicked: maps.findPlace()
            }
        }
        TouchKeys {
            visible: maps.typing
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(maps.width - 12, 520)
            Layout.bottomMargin: 6
            field: place
            onDone: maps.findPlace()
        }
    }
}
