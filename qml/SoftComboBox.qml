import QtQuick
import QtQuick.Controls

ComboBox {
    id: control
    implicitHeight: 40
    implicitWidth: 170
    font.family: "Microsoft YaHei UI"
    font.pixelSize: 12

    background: Rectangle {
        radius: 12
        color: "#FFFFFF"
        border.color: control.activeFocus ? "#79BFAE" : "#DDE6F2"
    }
    contentItem: Text {
        leftPadding: 12
        rightPadding: 28
        text: control.displayText
        color: "#344862"
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Text {
        x: control.width - width - 13
        anchors.verticalCenter: parent.verticalCenter
        text: "⌄"
        color: "#7890A8"
        font.pixelSize: 17
    }
    delegate: ItemDelegate {
        id: option
        required property int index
        width: control.width - 8
        implicitHeight: 36
        highlighted: control.highlightedIndex === index
        background: Rectangle {
            radius: 8
            color: option.highlighted || option.hovered ? "#EAF6F2" : "#FFFFFF"
        }
        contentItem: Text {
            leftPadding: 9
            rightPadding: 6
            text: control.textAt(option.index)
            color: "#40536E"
            font: control.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
    popup: Popup {
        y: control.height + 4
        width: control.width
        height: Math.min(contentItem.implicitHeight + 8, 228)
        padding: 4
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }
        background: Rectangle {
            radius: 12
            color: "#FFFFFF"
            border.color: "#DDE6F2"
        }
    }
}
