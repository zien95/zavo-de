// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: keys
    required property TextField field
    signal done
    property bool numbers: false
    spacing: 4

    function typeKey(value) {
        const first = field.selectionStart
        const last = field.selectionEnd
        field.remove(first, last)
        field.insert(first, value)
        field.cursorPosition = first + value.length
        field.forceActiveFocus()
    }

    component Key: Button {
        id: button
        focusPolicy: Qt.NoFocus
        Layout.fillWidth: true
        Layout.minimumWidth: 20
        Layout.preferredWidth: 30
        Layout.preferredHeight: 43
        background: Rectangle {
            color: button.down ? "#91f5c8" : "#302748"
            radius: 8
        }
        contentItem: Text {
            text: button.text
            color: button.down ? "#122920" : "#f7f2ff"
            font.pixelSize: 15
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    Repeater {
        model: keys.numbers ? ["1234567890", "-/:@&'().", "?!,+#"] : ["qwertyuiop", "asdfghjkl", "zxcvbnm"]
        RowLayout {
            required property string modelData
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: keys.width * modelData.length / 10
            spacing: 3
            Repeater {
                model: modelData.split("")
                Key {
                    required property string modelData
                    text: modelData
                    onClicked: keys.typeKey(modelData)
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 4
        Key {
            text: keys.numbers ? "ABC" : "123"
            Layout.preferredWidth: 57
            onClicked: keys.numbers = !keys.numbers
        }
        Key {
            text: "Space"
            Layout.preferredWidth: 130
            onClicked: keys.typeKey(" ")
        }
        Key {
            text: "⌫"
            Accessible.name: "Backspace"
            Layout.preferredWidth: 55
            onClicked: {
                const first = keys.field.selectionStart
                const last = keys.field.selectionEnd
                keys.field.remove(first === last ? Math.max(0, first - 1) : first, last)
                keys.field.forceActiveFocus()
            }
        }
        Key {
            text: "Done"
            Layout.preferredWidth: 60
            onClicked: keys.done()
        }
    }
}
