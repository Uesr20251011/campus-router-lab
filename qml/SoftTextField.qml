import QtQuick
import QtQuick.Controls

TextField {
    id: control
    implicitHeight: 40
    color: "#344862"
    placeholderTextColor: "#9AA9BB"
    font.family: "Microsoft YaHei UI"
    font.pixelSize: 13
    selectByMouse: true
    leftPadding: 12
    rightPadding: 12
    background: Rectangle {
        radius: 11
        color: "#FFFFFF"
        border.color: control.activeFocus ? "#79BFAE" : "#DDE6F2"
        border.width: control.activeFocus ? 2 : 1
    }
}
