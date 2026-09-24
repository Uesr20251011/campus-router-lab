import QtQuick
import QtQuick.Controls

Button {
    id: control
    property string symbol: ""
    property bool selected: false
    property bool compact: false
    property color accent: "#7BCBB9"
    hoverEnabled: true
    implicitHeight: compact ? 36 : 42
    implicitWidth: compact
                   ? Math.max(42, label.implicitWidth + (symbolText.visible ? symbolText.implicitWidth + 8 : 0) + 24)
                   : Math.max(104, label.implicitWidth + (symbolText.visible ? symbolText.implicitWidth + 12 : 0) + 34)

    background: Rectangle {
        radius: 14
        color: control.selected ? control.accent : control.down ? "#E8EDF9" : control.hovered ? "#F4F6FC" : "#FFFFFF"
        border.width: control.selected ? 0 : 1
        border.color: "#E5EAF3"
        Behavior on color { ColorAnimation { duration: 150 } }
    }

    contentItem: Row {
        spacing: 8
        anchors.centerIn: parent
        Text {
            id: symbolText
            visible: control.symbol.length > 0
            text: control.symbol
            color: control.selected ? "#173D3B" : "#61738C"
            font.family: "Segoe UI Symbol"
            font.pixelSize: control.compact ? 17 : 16
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            id: label
            text: control.text
            visible: text.length > 0
            color: control.selected ? "#173D3B" : "#33445E"
            font.family: "Microsoft YaHei UI"
            font.pixelSize: control.compact ? 12 : 13
            font.weight: control.selected ? Font.DemiBold : Font.Medium
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
