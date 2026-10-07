import QtQuick
import QtQuick.Controls

Button {
    id: control
    property string symbol: ""
    property string iconName: ""
    property string hint: ""
    property bool selected: false
    property color accent: "#D7F2E9"
    property int symbolSize: 19
    implicitWidth: 36
    implicitHeight: 36
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    ToolTip.visible: hovered
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
    contentItem: Item {
        Text {
            visible: control.iconName.length === 0
            anchors.centerIn: parent
            text: control.symbol
            color: control.enabled ? control.selected ? "#216F62" : "#526882" : "#B6C2D0"
            font.family: "Segoe UI Symbol"
            font.pixelSize: control.symbolSize
            font.weight: control.selected ? Font.DemiBold : Font.Normal
        }
        ToolbarGlyph {
            visible: control.iconName.length > 0
            anchors.centerIn: parent
            width: 26
            height: width
            kind: control.iconName
            enabled: control.enabled
        }
    }
}
