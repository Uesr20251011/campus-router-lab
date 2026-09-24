import QtQuick

Rectangle {
    id: control
    signal clicked()
    property string symbol: ""
    property string accessibleName: ""
    property bool destructive: false

    width: 36
    height: 34
    radius: 10
    color: pointer.containsMouse ? (destructive ? "#FCE5E5" : "#F0F5FA") : "transparent"

    Text {
        anchors.centerIn: parent
        text: control.symbol
        color: control.destructive && pointer.containsMouse ? "#C95F67" : "#65758D"
        font.pixelSize: control.symbol === "×" ? 23 : 19
        font.family: "Microsoft YaHei UI"
    }
    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        Accessible.name: control.accessibleName
        Accessible.role: Accessible.Button
        onClicked: control.clicked()
    }
}
