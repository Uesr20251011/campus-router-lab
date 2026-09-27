import QtQuick

Item {
    id: glyph
    property string kind: ""
    implicitWidth: 22
    implicitHeight: 22

    Image {
        anchors.fill: parent
        source: glyph.kind.length > 0 ? "qrc:/assets/toolbar/" + glyph.kind + ".png" : ""
        sourceClipRect: {
            switch (glyph.kind) {
            case "hand": return Qt.rect(180, 200, 890, 890)
            case "link": return Qt.rect(265, 275, 750, 750)
            case "pin": return Qt.rect(291, 304, 660, 660)
            case "cost": return Qt.rect(127, 128, 1000, 1000)
            case "gauge": return Qt.rect(200, 190, 850, 850)
            case "disk": return Qt.rect(220, 225, 810, 810)
            default: return Qt.rect(0, 0, 0, 0)
            }
        }
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        opacity: glyph.enabled ? 1 : 0.4
    }
}
