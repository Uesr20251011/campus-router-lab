import QtQuick
import QtQuick.Controls

Button {
    id: control
    property string symbol: ""
    property string hint: ""
    property bool selected: false
    property color accent: "#D7F2E9"
    implicitWidth: 36
    implicitHeight: 36
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    ToolTip.visible: hovered || activeFocus
    ToolTip.delay: 450
    ToolTip.text: hint

    background: Rectangle {
        radius: 11
        color: control.selected ? control.accent : control.down ? "#E8EDF9"
               : control.hovered || control.activeFocus ? "#F0F5FB" : "transparent"
        border.width: control.activeFocus ? 1 : 0
        border.color: "#8ABCB0"
        Behavior on color { ColorAnimation { duration: 150 } }
    }
    contentItem: Text {
        text: control.symbol
        color: control.enabled ? control.selected ? "#216F62" : "#526882" : "#B6C2D0"
        font.family: "Segoe UI Symbol"
        font.pixelSize: 19
        font.weight: control.selected ? Font.DemiBold : Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
