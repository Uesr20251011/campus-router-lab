import QtQuick
import QtQuick.Controls

SpinBox {
    id: control
    editable: true
    implicitWidth: 104
    implicitHeight: 38
    padding: 0
    leftPadding: 29
    rightPadding: 29
    font.family: "Microsoft YaHei UI"
    font.pixelSize: 13

    function commitInput() {
        const typed = Number(contentItem.text)
        if (Number.isInteger(typed) && typed >= Math.min(from, to) && typed <= Math.max(from, to))
            value = typed
        else
            contentItem.text = displayText
    }

    background: Rectangle {
        radius: 11
        color: "#FFFFFF"
        border.width: 1
        border.color: control.activeFocus ? "#74BFAA" : "#DDE6F2"
    }

    contentItem: TextInput {
        z: 2
        text: control.displayText
        color: "#344862"
        font: control.font
        clip: true
        selectByMouse: true
        horizontalAlignment: TextInput.AlignHCenter
        verticalAlignment: TextInput.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
        selectionColor: "#BDEAD9"
        selectedTextColor: "#344862"
    }

    down.indicator: Rectangle {
        x: 0
        width: 28
        height: control.height
        radius: 10
        color: control.down.pressed ? "#DCEFEA" : control.down.hovered ? "#EFF7F4" : "#F6F8FC"
        Text {
            anchors.centerIn: parent
            text: "−"
            color: control.value > Math.min(control.from, control.to) ? "#607993" : "#BBC7D5"
            font.pixelSize: 19
        }
    }
    up.indicator: Rectangle {
        x: control.width - width
        width: 28
        height: control.height
        radius: 10
        color: control.up.pressed ? "#DCEFEA" : control.up.hovered ? "#EFF7F4" : "#F6F8FC"
        Text {
            anchors.centerIn: parent
            text: "+"
            color: control.value < Math.max(control.from, control.to) ? "#607993" : "#BBC7D5"
            font.pixelSize: 17
        }
    }
}
