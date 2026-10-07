import QtQuick

Item {
    id: glyph
    property string kind: ""
    implicitWidth: 22
    implicitHeight: 22

    Image {
        anchors.fill: parent
        source: glyph.kind.length > 0 ? "qrc:/assets/toolbar/" + glyph.kind + ".png" : ""
        sourceSize: Qt.size(128, 128)
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        opacity: glyph.enabled ? 1 : 0.4
    }
}
