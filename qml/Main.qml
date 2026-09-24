import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    width: 1080
    height: 680
    minimumWidth: 880
    minimumHeight: 560
    title: "校园路由实验室"
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "#F2F5FB"
    font.family: "Microsoft YaHei UI"

    property string activeTool: "select"
    property bool fixedNodes: false
    property string activeAlgorithm: ""
    property string metric: "cost"
    property var traceSteps: []
    property int stepIndex: -1
    property bool playing: false
    property bool treeOnly: false
    property int highlightedFlowPath: -1
    property int selectedNodeId: -1
    property int selectedLinkId: -1
    property string notice: ""
    readonly property var currentStep: stepIndex >= 0 && stepIndex < traceSteps.length ? traceSteps[stepIndex] : ({})
    readonly property bool treeReady: (activeAlgorithm === "prim" || activeAlgorithm === "kruskal")
                                      && currentStep.kind === "finish"
                                      && currentStep.selectedEdges
                                      && currentStep.selectedEdges.length === graphBackend.nodes.length - 1
    readonly property bool showFlowResult: activeAlgorithm === "flow" && currentStep.kind === "finish"
                                           && selectedNodeId < 0 && selectedLinkId < 0
    onStepIndexChanged: {
        if (treeOnly && stepIndex !== traceSteps.length - 1) treeOnly = false
        if (currentStep.kind !== "finish") highlightedFlowPath = -1
    }

    function resetTrace() {
        playing = false
        treeOnly = false
        traceSteps = []
        stepIndex = -1
        activeAlgorithm = ""
        highlightedFlowPath = -1
    }
    function exitDemo() {
        canvas.commitNow()
        resetTrace()
        chooseNode(-1)
        selectedLinkId = -1
        notice = ""
    }
    function chooseNode(id) {
        selectedNodeId = id
        selectedLinkId = -1
        if (id >= 0) nodeNameField.text = graphBackend.nodeName(id)
    }
    function chooseLink(id) {
        selectedLinkId = id
        selectedNodeId = -1
        const link = selectedLinkData()
        if (link) {
            costField.text = link.hasCost ? String(link.cost) : "0"
            capacityField.text = link.hasCapacity ? String(link.capacity) : "0"
            directionSwitch.checked = link.directed
        }
    }
    function selectedLinkData() {
        const links = graphBackend.links
        for (let i = 0; i < links.length; ++i) if (links[i].id === selectedLinkId) return links[i]
        return null
    }
    function runAlgorithm(which) {
        if (activeAlgorithm === which) { exitDemo(); return }
        canvas.commitNow()
        treeOnly = false
        highlightedFlowPath = -1
        sourceBox.commitInput()
        sinkBox.commitInput()
        const result = graphBackend.run(which, sourceBox.value, sinkBox.value)
        activeAlgorithm = which
        metric = which === "flow" ? "capacity" : "cost"
        traceSteps = result.steps
        stepIndex = traceSteps.length ? 0 : -1
        playing = traceSteps.length > 1
        notice = result.success ? "" : result.error
    }
    function loadExample(which) {
        canvas.commitNow()
        treeOnly = false
        chooseNode(-1)
        selectedLinkId = -1
        canvas.linkStart = -1
        if (which === "cost") { graphBackend.loadCostSample(); metric = "cost" }
        else if (which === "flow") { graphBackend.loadFlowSample(); metric = "capacity" }
        else { graphBackend.generateCampus(20); metric = "cost" }
        const nodes = graphBackend.nodes
        sourceBox.value = nodes.length ? nodes[0].id : 1
        sinkBox.value = nodes.length ? nodes[nodes.length - 1].id : 1
        canvas.fitView()
        notice = ""
    }
    function presetChoices() {
        const choices = ["校园网络 · 内置", "造价例题 · 内置", "吞吐例题 · 内置"]
        const names = graphBackend.presetNames
        for (let i = 0; i < names.length; ++i) choices.push("我的预设 · " + names[i])
        return choices
    }
    function loadChosenPreset() {
        const index = presetBox.currentIndex
        if (index < 0) return
        if (index < 3) {
            loadExample(["campus", "cost", "flow"][index])
        } else {
            canvas.commitNow()
            const names = graphBackend.presetNames
            if (index - 3 >= names.length || !graphBackend.loadPreset(names[index - 3])) {
                notice = graphBackend.error
                return
            }
            chooseNode(-1)
            selectedLinkId = -1
            canvas.linkStart = -1
            metric = "cost"
            const nodes = graphBackend.nodes
            sourceBox.value = nodes.length ? nodes[0].id : 1
            sinkBox.value = nodes.length ? nodes[nodes.length - 1].id : 1
            canvas.fitView()
            notice = "已载入预设：『" + names[index - 3] + "』"
        }
        networkPopup.close()
    }
    function generateRandomNetwork() {
        randomCount.commitInput()
        canvas.commitNow()
        graphBackend.generateRandom(randomCount.value)
        if (graphBackend.error.length > 0) { notice = graphBackend.error; return }
        chooseNode(-1)
        selectedLinkId = -1
        canvas.linkStart = -1
        metric = "cost"
        sourceBox.value = 1
        sinkBox.value = randomCount.value
        canvas.fitView()
        notice = "已生成 " + randomCount.value + " 个节点的随机网络"
        networkPopup.close()
    }
    function saveCurrentPreset() {
        canvas.commitNow()
        const name = presetNameInput.text.trim()
        if (!graphBackend.savePreset(name)) { notice = graphBackend.error; return }
        const names = graphBackend.presetNames
        presetBox.currentIndex = 3 + names.indexOf(name)
        notice = "已保存预设：『" + name + "』"
        savePresetPopup.close()
    }
    function applyLink() {
        const cost = Number(costField.text), capacity = Number(capacityField.text)
        if (!Number.isInteger(cost) || !Number.isInteger(capacity) || cost < 0 || capacity < 0) {
            notice = "造价与容量请输入非负整数"
            return
        }
        canvas.commitNow()
        if (graphBackend.updateLink(selectedLinkId, cost, capacity, directionSwitch.checked)) notice = "链路参数已更新"
        else notice = graphBackend.error
    }
    function removeSelection() {
        canvas.commitNow()
        if (selectedNodeId >= 0) graphBackend.removeNode(selectedNodeId)
        else if (selectedLinkId >= 0) graphBackend.removeLink(selectedLinkId)
        chooseNode(-1)
        selectedLinkId = -1
    }
    function removeNodeById(id) {
        canvas.commitNow()
        if (canvas.linkStart === id) canvas.linkStart = -1
        graphBackend.removeNode(id)
        if (selectedNodeId === id) chooseNode(-1)
    }

    Connections {
        target: graphBackend
        function onTopologyChanged() { window.resetTrace() }
    }
    Timer {
        interval: {
            const base = speedBox.currentIndex === 0 ? 1600 : speedBox.currentIndex === 2 ? 500 : 1050
            return (window.currentStep.kind === "path" || window.currentStep.kind === "update")
                ? Math.round(base * 1.25) : base
        }
        repeat: true
        running: window.playing
        onTriggered: {
            if (window.stepIndex < window.traceSteps.length - 1) window.stepIndex++
            else window.playing = false
        }
    }
    Shortcut { sequence: "Delete"; onActivated: window.removeSelection() }
    Shortcut { sequence: "Escape"; onActivated: { window.activeTool = "select"; canvas.linkStart = -1 } }
    Shortcut { sequence: "Space"; onActivated: window.playing = !window.playing && window.traceSteps.length > 0 }

    Rectangle {
        id: header
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: 84
        color: "#FFFFFF"
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#E7ECF4" }
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onPressed: window.startSystemMove()
            onDoubleClicked: {
                if (window.visibility === Window.Maximized) window.showNormal()
                else window.showMaximized()
            }
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 28; anchors.rightMargin: 28
            spacing: 12
            Image {
                source: "qrc:/assets/app-icon.png"
                sourceSize.width: 96; sourceSize.height: 96
                Layout.preferredWidth: 48; Layout.preferredHeight: 48
                Layout.maximumWidth: 48; Layout.maximumHeight: 48
                fillMode: Image.PreserveAspectFit
            }
            Column {
                Layout.leftMargin: 3
                spacing: 1
                Text { text: "校园路由实验室"; color: "#283A56"; font.pixelSize: 21; font.weight: Font.Bold }
                Text { text: "让每一条路径都看得见"; color: "#91A0B5"; font.pixelSize: 11 }
            }
            Item { Layout.fillWidth: true }
            SoftButton {
                id: networkButton
                text: "网络管理"; symbol: "◌"
                selected: networkPopup.visible
                onClicked: networkPopup.visible ? networkPopup.close() : networkPopup.open()
            }
            Rectangle { width: 1; height: 32; color: "#E8EDF4"; Layout.leftMargin: 7; Layout.rightMargin: 7 }
            Text {
                visible: window.width >= 1190
                text: graphBackend.nodes.length + " 个节点  ·  " + graphBackend.links.length + " 条链路"
                color: "#7F90A6"; font.pixelSize: 12
            }
            Row {
                Layout.leftMargin: 4
                spacing: 2
                WindowControl { symbol: "−"; accessibleName: "最小化"; onClicked: window.showMinimized() }
                WindowControl {
                    symbol: window.visibility === Window.Maximized ? "❐" : "□"
                    accessibleName: window.visibility === Window.Maximized ? "还原" : "最大化"
                    onClicked: {
                        if (window.visibility === Window.Maximized) window.showNormal()
                        else window.showMaximized()
                    }
                }
                WindowControl { symbol: "×"; accessibleName: "关闭"; destructive: true; onClicked: window.close() }
            }
        }
    }

    Popup {
        id: networkPopup
        objectName: "networkPopup"
        parent: Overlay.overlay
        x: Math.min(window.width - width - 16,
                    networkButton.mapToItem(window.contentItem, 0, 0).x)
        y: header.height - 3
        width: 270; height: 276
        padding: 14
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 18; color: "#FFFFFF"; border.color: "#E0E8F3"
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Text { text: "网络"; color: "#344862"; font.pixelSize: 15; font.weight: Font.DemiBold }
            Text { text: "内置例题和已保存的网络"; color: "#94A2B5"; font.pixelSize: 11 }
            SoftComboBox { id: presetBox; model: window.presetChoices(); Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true; spacing: 7
                SoftButton { text: "载入所选"; compact: true; Layout.fillWidth: true; onClicked: window.loadChosenPreset() }
                SoftButton { text: "保存当前"; compact: true; Layout.fillWidth: true; onClicked: { networkPopup.close(); savePresetPopup.open() } }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#EBEFF6"; Layout.topMargin: 4; Layout.bottomMargin: 3 }
            Text { text: "随机网络 · 节点数"; color: "#344862"; font.pixelSize: 12; font.weight: Font.DemiBold }
            RowLayout {
                Layout.fillWidth: true; spacing: 7
                SoftSpinBox { id: randomCount; from: 2; to: 200; value: 20; Layout.fillWidth: true }
                SoftButton { text: "生成"; compact: true; onClicked: window.generateRandomNetwork() }
            }
            Text { text: "预设会保存在本机，下次启动仍可使用"; color: "#9AA9BB"; font.pixelSize: 10 }
        }
    }

    Popup {
        id: savePresetPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 340; height: 198
        padding: 20
        modal: true; dim: true; focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Overlay.modal: Rectangle { color: "#550E2035" }
        background: Rectangle { radius: 20; color: "#FFFFFF"; border.color: "#E0E8F3" }
        onOpened: { presetNameInput.text = ""; window.notice = ""; presetNameInput.forceActiveFocus() }
        ColumnLayout {
            anchors.fill: parent; spacing: 10
            Text { text: "保存当前网络"; color: "#344862"; font.pixelSize: 16; font.weight: Font.DemiBold }
            Text { text: "节点名称、位置、链路和权值都会保存"; color: "#91A0B5"; font.pixelSize: 11 }
            TextField {
                id: presetNameInput
                Layout.fillWidth: true; implicitHeight: 40
                placeholderText: "输入预设名称"
                maximumLength: 32
                selectByMouse: true
                onAccepted: window.saveCurrentPreset()
                background: Rectangle { radius: 11; color: "#FFFFFF"; border.color: presetNameInput.activeFocus ? "#79BFAE" : "#DDE6F2" }
            }
            Text { visible: window.notice.length > 0; text: window.notice; color: "#C37D7F"; font.pixelSize: 11 }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Item { Layout.fillWidth: true }
                SoftButton { text: "取消"; compact: true; onClicked: savePresetPopup.close() }
                SoftButton { text: "保存"; compact: true; selected: true; onClicked: window.saveCurrentPreset() }
            }
        }
    }

    Menu {
        id: nodeMenu
        objectName: "nodeMenu"
        property int nodeId: -1
        popupType: Popup.Item
        width: 142
        background: Rectangle { radius: 12; color: "#FFFFFF"; border.color: "#E1E8F2" }
        MenuItem {
            id: deleteNodeAction
            text: "删除节点"
            implicitHeight: 44
            leftPadding: 14; rightPadding: 12
            background: Rectangle {
                radius: 10
                color: deleteNodeAction.highlighted ? "#FDEDEF" : "#FFFFFF"
            }
            contentItem: Row {
                spacing: 9
                Text { text: "×"; color: "#C96D78"; font.pixelSize: 20; anchors.verticalCenter: parent.verticalCenter }
                Text { text: "删除节点"; color: "#40536E"; font.pixelSize: 13; font.family: "Microsoft YaHei UI"; anchors.verticalCenter: parent.verticalCenter }
            }
            onTriggered: window.removeNodeById(nodeMenu.nodeId)
        }
    }

    Item {
        id: workspace
        anchors.top: header.bottom; anchors.bottom: playback.top
        anchors.left: parent.left; anchors.right: parent.right

        Rectangle {
            id: leftPane
            x: 16; y: 16; width: window.width < 1200 ? 190 : 216; height: parent.height - 32
            color: "#FFFFFF"; radius: 22
            border.color: "#E7ECF4"
            Flickable {
                anchors.fill: parent; anchors.margins: 18
                clip: true
                contentWidth: width
                contentHeight: leftColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                ColumnLayout {
                id: leftColumn
                width: parent.width
                spacing: 8
                Text { text: "编辑画布"; color: "#344862"; font.pixelSize: 15; font.weight: Font.DemiBold }
                Text { text: "移动、连接，自由搭建网络"; color: "#94A2B5"; font.pixelSize: 11 }
                Item { height: 6 }
                SoftButton { Layout.fillWidth: true; text: "选择与拖动"; symbol: "⌖"; selected: window.activeTool === "select"; onClicked: window.activeTool = "select" }
                SoftButton { Layout.fillWidth: true; text: "添加节点"; symbol: "＋"; selected: window.activeTool === "node"; onClicked: window.activeTool = "node" }
                SoftButton { Layout.fillWidth: true; text: "连接节点"; symbol: "↗"; selected: window.activeTool === "link"; onClicked: window.activeTool = "link" }
                SoftButton { Layout.fillWidth: true; text: "固定节点模式"; symbol: "▣"; selected: window.fixedNodes; accent: "#DCD4F9"; onClicked: window.fixedNodes = !window.fixedNodes }
                SoftButton { Layout.fillWidth: true; text: "自动整理"; symbol: "◎"; enabled: !window.fixedNodes; opacity: enabled ? 1 : 0.48; onClicked: { canvas.commitNow(); graphBackend.autoLayout(); canvas.fitView() } }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#EBEFF6"; Layout.topMargin: 11; Layout.bottomMargin: 8 }
                Text { text: "算法演示"; color: "#344862"; font.pixelSize: 15; font.weight: Font.DemiBold }
                Text { text: "再次点击当前算法可退出演示"; color: "#94A2B5"; font.pixelSize: 11 }
                SoftButton { Layout.fillWidth: true; text: "Prim 最小生成树"; symbol: "✳"; selected: window.activeAlgorithm === "prim"; accent: "#BDEAD9"; onClicked: window.runAlgorithm("prim") }
                SoftButton { Layout.fillWidth: true; text: "Kruskal 最小生成树"; symbol: "✳"; selected: window.activeAlgorithm === "kruskal"; accent: "#BDEAD9"; onClicked: window.runAlgorithm("kruskal") }
                SoftButton { Layout.fillWidth: true; text: "最大流"; symbol: "⇢"; selected: window.activeAlgorithm === "flow"; accent: "#DCD4F9"; onClicked: window.runAlgorithm("flow") }
                Item { height: 6 }
                Rectangle {
                    Layout.fillWidth: true; height: 88; radius: 16; color: "#F4F8FF"
                    Column {
                        anchors.fill: parent; anchors.margins: 12; spacing: 5
                        Text { text: "小提示"; color: "#6685AE"; font.pixelSize: 12; font.weight: Font.DemiBold }
                        Text { width: parent.width; wrapMode: Text.Wrap; text: window.fixedNodes ? "固定模式下自由排布，拖动只移动当前节点。" : "拖动节点会带动邻居；悬停节点可看清相连的边。"; color: "#7C8DA5"; font.pixelSize: 11 }
                    }
                }
            }
            }
        }

        Rectangle {
            id: canvasCard
            anchors.left: leftPane.right; anchors.leftMargin: 12
            anchors.right: rightPane.left; anchors.rightMargin: 12
            anchors.top: parent.top; anchors.topMargin: 16
            anchors.bottom: parent.bottom; anchors.bottomMargin: 16
            color: "#FFFFFF"; radius: 22; border.color: "#E7ECF4"
            GraphCanvas {
                id: canvas
                anchors.fill: parent; anchors.margins: 8
                nodes: graphBackend.nodes
                links: graphBackend.links
                step: window.currentStep
                algorithm: window.activeAlgorithm
                treeOnly: window.treeOnly
                metric: window.metric
                tool: window.activeTool
                fixedNodes: window.fixedNodes
                playing: window.playing
                selectedNode: window.selectedNodeId
                selectedLink: window.selectedLinkId
                flowPathFocus: window.highlightedFlowPath
                onNodeSelected: id => window.chooseNode(id)
                onLinkSelected: id => window.chooseLink(id)
                onNodeContextRequested: (id, x, y) => {
                    nodeMenu.nodeId = id
                    nodeMenu.popup(canvas, Qt.point(x, y))
                }
                onPositionsSettled: positions => graphBackend.setPositions(positions)
                onBlankClicked: (x, y) => {
                    canvas.commitNow()
                    const id = graphBackend.addNode(x, y)
                    if (id >= 0) window.chooseNode(id)
                }
                onLinkRequested: (fromId, toId) => {
                    canvas.commitNow()
                    const id = graphBackend.addLink(fromId, toId, 10, 15, false)
                    if (id >= 0) window.chooseLink(id)
                    else window.notice = graphBackend.error
                }
            }
            Row {
                anchors.right: parent.right; anchors.top: parent.top
                anchors.rightMargin: 20; anchors.topMargin: 20
                spacing: 8
                SoftButton { text: "造价"; compact: true; selected: window.metric === "cost"; onClicked: window.metric = "cost" }
                SoftButton { text: "容量"; compact: true; selected: window.metric === "capacity"; accent: "#DAD1F4"; onClicked: window.metric = "capacity" }
                SoftButton { text: "全部边权"; compact: true; selected: canvas.showAllWeights; onClicked: canvas.showAllWeights = !canvas.showAllWeights }
                SoftButton { text: "适应"; compact: true; onClicked: canvas.fitView() }
            }
            Rectangle {
                anchors.left: parent.left; anchors.bottom: parent.bottom
                anchors.leftMargin: 20; anchors.bottomMargin: 20
                width: legendRow.implicitWidth + 24; height: 36; radius: 12
                color: "#F5FFFFFF"; border.color: "#E7ECF5"
                Row {
                    id: legendRow; anchors.centerIn: parent; spacing: 13
                    Repeater {
                        model: window.showFlowResult
                            ? [{name:"彩色路径",tone:"#36AA99"}, {name:"未载流",tone:"#AEB8C8"}]
                            : window.activeAlgorithm === "flow"
                            ? [{name:"探索",tone:"#EAA668"}, {name:"通路",tone:"#50BDA7"},
                               {name:"逆向",tone:"#876BDB"}, {name:"余量0",tone:"#AEB8C8"}]
                            : [{name:"已选",tone:"#68C1AC"}, {name:"当前",tone:"#F0B477"}]
                        delegate: Row {
                            spacing: 5
                            Rectangle { width: 10; height: 10; radius: 5; color: modelData.tone; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: modelData.name; color: "#8292AA"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: rightPane
            width: window.width < 1200 ? 248 : 290; height: parent.height - 32; y: 16
            anchors.right: parent.right; anchors.rightMargin: 16
            color: "#FFFFFF"; radius: 22; border.color: "#E7ECF4"
            Flickable {
                anchors.fill: parent; anchors.margins: 18
                clip: true
                contentWidth: width
                contentHeight: rightColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                ColumnLayout {
                id: rightColumn
                width: parent.width
                spacing: 11
                Text { text: window.showFlowResult ? "最大流结果" : "属性与结果"; color: "#344862"; font.pixelSize: 16; font.weight: Font.DemiBold }
                Text { text: window.showFlowResult ? "总流量 " + window.currentStep.value + " 包/秒" : "点击节点或连线，即可在这里修改"; color: window.showFlowResult ? "#45A996" : "#94A2B5"; font.pixelSize: window.showFlowResult ? 13 : 11 }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#EBEFF6"; Layout.topMargin: 5 }
                ColumnLayout {
                    visible: window.selectedNodeId >= 0
                    Layout.fillWidth: true; spacing: 8
                    Text { text: "节点  R" + window.selectedNodeId; color: "#7285A0"; font.pixelSize: 12 }
                    TextField {
                        id: nodeNameField
                        Layout.fillWidth: true
                        placeholderText: "节点名称"
                        selectByMouse: true
                        onAccepted: { canvas.commitNow(); graphBackend.renameNode(window.selectedNodeId, text) }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        SoftButton { text: "保存名称"; Layout.fillWidth: true; selected: true; onClicked: { canvas.commitNow(); if (!graphBackend.renameNode(window.selectedNodeId, nodeNameField.text)) window.notice = graphBackend.error } }
                        SoftButton { text: "删除"; accent: "#F4C9C9"; onClicked: window.removeSelection() }
                    }
                }
                ColumnLayout {
                    visible: window.selectedLinkId >= 0
                    Layout.fillWidth: true; spacing: 8
                    Text {
                        text: {
                            const link = window.selectedLinkData()
                            return link ? "链路  R" + link.u + (link.directed ? " → R" : " — R") + link.v : "链路"
                        }
                        color: "#7285A0"; font.pixelSize: 12
                    }
                    Text { text: "建设造价"; color: "#8191A8"; font.pixelSize: 11 }
                    TextField { id: costField; Layout.fillWidth: true; placeholderText: "例如 10"; validator: IntValidator { bottom: 0 } selectByMouse: true }
                    Text { text: "最大传输容量（包/秒）"; color: "#8191A8"; font.pixelSize: 11 }
                    TextField { id: capacityField; Layout.fillWidth: true; placeholderText: "例如 15"; validator: IntValidator { bottom: 0 } selectByMouse: true }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: {
                                const link = window.selectedLinkData()
                                return link ? "单向 R" + link.u + " → R" + link.v : "单向链路"
                            }
                            color: "#8191A8"; font.pixelSize: 12; Layout.fillWidth: true
                        }
                        Switch { id: directionSwitch }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        SoftButton { text: "应用修改"; Layout.fillWidth: true; selected: true; onClicked: window.applyLink() }
                        SoftButton { text: "删除"; accent: "#F4C9C9"; onClicked: window.removeSelection() }
                    }
                }
                Rectangle {
                    visible: window.selectedNodeId < 0 && window.selectedLinkId < 0 && !window.showFlowResult
                    Layout.fillWidth: true; height: 105; radius: 16; color: "#F6F8FD"
                    Column {
                        anchors.centerIn: parent; spacing: 7
                        Text { text: "○"; font.pixelSize: 28; color: "#B5C7E2"; anchors.horizontalCenter: parent.horizontalCenter }
                        Text { text: "选择一个节点或链路"; color: "#93A1B5"; font.pixelSize: 12; anchors.horizontalCenter: parent.horizontalCenter }
                    }
                }
                Rectangle { visible: !window.showFlowResult; Layout.fillWidth: true; height: 1; color: "#EBEFF6"; Layout.topMargin: 4 }
                Text { visible: !window.showFlowResult; text: "计算范围"; color: "#344862"; font.pixelSize: 14; font.weight: Font.DemiBold }
                ColumnLayout {
                    visible: !window.showFlowResult
                    Layout.fillWidth: true
                    spacing: 7
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "起点"; color: "#8292A7"; font.pixelSize: 12 }
                        SoftSpinBox { id: sourceBox; from: 1; to: 200; value: 1; Layout.fillWidth: true }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "终点"; color: "#8292A7"; font.pixelSize: 12 }
                        SoftSpinBox { id: sinkBox; from: 1; to: 200; value: 20; Layout.fillWidth: true }
                    }
                }
                Rectangle { visible: !window.showFlowResult; Layout.fillWidth: true; height: 1; color: "#EBEFF6"; Layout.topMargin: 4 }
                Text { visible: !window.showFlowResult; text: "当前步骤"; color: "#344862"; font.pixelSize: 14; font.weight: Font.DemiBold }
                Rectangle {
                    visible: !window.showFlowResult
                    Layout.fillWidth: true; Layout.preferredHeight: window.activeAlgorithm === "flow" ? 166 : 146
                    radius: 17; color: "#F4F7FE"; border.color: "#E8ECF7"
                    Column {
                        anchors.fill: parent; anchors.margins: 15; spacing: 8
                        Text { text: window.stepIndex >= 0 ? "第 " + (window.stepIndex + 1) + " / " + window.traceSteps.length + " 步" : "等待运行"; color: "#8396B2"; font.pixelSize: 11 }
                        Text { width: parent.width; text: window.currentStep.title || "选择算法开始演示"; color: "#344964"; font.pixelSize: 16; font.weight: Font.DemiBold; wrapMode: Text.Wrap }
                        Text { width: parent.width; text: window.currentStep.detail || "算法经过的节点和链路会在画布上依次亮起。"; color: "#7D90A9"; font.pixelSize: 12; wrapMode: Text.Wrap }
                    }
                }
                Text { visible: !window.showFlowResult && window.stepIndex >= 0; text: (window.activeAlgorithm === "flow" ? "当前流量  " : "累计造价  ") + (window.currentStep.value || 0); color: "#57AF9C"; font.pixelSize: 17; font.weight: Font.Bold }
                Text {
                    visible: !window.showFlowResult && window.activeAlgorithm === "flow" && window.stepIndex >= 0
                    Layout.fillWidth: true; wrapMode: Text.Wrap
                    text: "边权 0/7 = 已用/总容量；灰边已耗尽，紫色箭头可反向回退。"
                    color: "#8494AA"; font.pixelSize: 11
                }
                ColumnLayout {
                    visible: window.showFlowResult
                    Layout.fillWidth: true; spacing: 7
                    Text {
                        text: "最终路径 · " + (window.currentStep.flowPaths ? window.currentStep.flowPaths.length : 0) + " 条"
                        color: "#344862"; font.pixelSize: 14; font.weight: Font.DemiBold
                    }
                    Text { text: "点击路径可在拓扑图上突出显示"; color: "#91A0B5"; font.pixelSize: 11 }
                    Text {
                        visible: !window.currentStep.flowPaths || window.currentStep.flowPaths.length === 0
                        text: "起点到终点没有可用流量"; color: "#91A0B5"; font.pixelSize: 12
                    }
                    Repeater {
                        model: window.currentStep.flowPaths || []
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 48
                            radius: 12
                            color: window.highlightedFlowPath === index ? "#EDEBFA" : "#F6F8FD"
                            border.color: window.highlightedFlowPath === index ? canvas.flowColor(index) : "#E6EBF5"
                            Rectangle {
                                x: 11; y: 8; width: 5; height: parent.height - 16; radius: 3
                                color: canvas.flowColor(index)
                            }
                            Column {
                                x: 25; y: 4; width: parent.width - 37; spacing: 1
                                Text {
                                    text: "路径 " + (index + 1) + "  ·  " + modelData.amount + " 包/秒"
                                    color: canvas.flowColor(index); font.pixelSize: 12; font.weight: Font.DemiBold
                                }
                                Text {
                                    id: routeText
                                    width: parent.width; wrapMode: Text.WrapAnywhere
                                    maximumLineCount: 2; elide: Text.ElideRight
                                    text: modelData.nodes.map(id => "R" + id).join(" → ")
                                    color: "#5D6F89"; font.pixelSize: 11
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: window.highlightedFlowPath = window.highlightedFlowPath === index ? -1 : index
                            }
                        }
                    }
                }
                Text { visible: !window.showFlowResult && window.notice.length > 0; text: window.notice; color: "#C37D7F"; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }
                Item { visible: !window.showFlowResult; height: 6 }
                Text { visible: !window.showFlowResult; text: "提示：双击不是必需操作，单击即可编辑。"; color: "#A3AEC0"; font.pixelSize: 10; wrapMode: Text.Wrap; Layout.fillWidth: true }
            }
            }
        }
    }

    Rectangle {
        id: playback
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 118; color: "#FFFFFF"
        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: "#E7ECF4" }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: window.width < 1000 ? 18 : 30
            anchors.rightMargin: window.width < 1000 ? 18 : 30
            spacing: window.width < 1000 ? 7 : 14
            Rectangle {
                width: 42; height: 42; radius: 15; color: "#E7DFFD"
                Text { anchors.centerIn: parent; text: "↝"; color: "#9079D5"; font.pixelSize: 24 }
            }
            Column {
                spacing: 2
                Text { text: "算法回放"; color: "#344862"; font.pixelSize: 14; font.weight: Font.DemiBold }
                Text { text: window.activeAlgorithm === "flow" ? "最大流" : window.activeAlgorithm === "prim" ? "Prim" : window.activeAlgorithm === "kruskal" ? "Kruskal" : "选择算法后播放"; color: "#9EACC0"; font.pixelSize: 11 }
            }
            SoftButton { compact: true; symbol: "↞"; enabled: window.stepIndex > 0; onClicked: { window.playing = false; window.stepIndex-- } }
            SoftButton { compact: true; symbol: window.playing ? "Ⅱ" : "▶"; selected: true; enabled: window.traceSteps.length > 0; onClicked: window.playing = !window.playing }
            SoftButton { compact: true; symbol: "↠"; enabled: window.stepIndex >= 0 && window.stepIndex < window.traceSteps.length - 1; onClicked: { window.playing = false; window.stepIndex++ } }
            Slider {
                id: timeline
                Layout.fillWidth: true
                from: 0; to: Math.max(1, window.traceSteps.length - 1)
                value: Math.max(0, window.stepIndex)
                enabled: window.traceSteps.length > 0
                onMoved: { window.playing = false; window.stepIndex = Math.round(value) }
            }
            Text { text: window.traceSteps.length ? (window.stepIndex + 1) + " / " + window.traceSteps.length : "0 / 0"; color: "#8192A8"; font.pixelSize: 12 }
            ComboBox { id: speedBox; model: ["0.5×", "1×", "2×"]; currentIndex: 1; Layout.preferredWidth: window.width < 1000 ? 75 : 92 }
            SoftButton { text: "看结果"; enabled: window.traceSteps.length > 0; onClicked: { window.playing = false; window.stepIndex = window.traceSteps.length - 1 } }
            SoftButton {
                text: window.treeOnly ? "恢复全图" : "只看最小树"
                visible: window.treeReady
                compact: true; selected: window.treeOnly; accent: "#BDEAD9"
                onClicked: { window.playing = false; window.treeOnly = !window.treeOnly; window.selectedLinkId = -1 }
            }
        }
    }

    component ResizeGrip: MouseArea {
        property int edges: 0
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        visible: window.visibility !== Window.Maximized
        z: 100
        onPressed: window.startSystemResize(edges)
    }
    ResizeGrip { x: 0; y: 8; width: 6; height: window.height - 16; edges: Qt.LeftEdge; cursorShape: Qt.SizeHorCursor }
    ResizeGrip { x: window.width - 6; y: 8; width: 6; height: window.height - 16; edges: Qt.RightEdge; cursorShape: Qt.SizeHorCursor }
    ResizeGrip { x: 8; y: 0; width: window.width - 16; height: 6; edges: Qt.TopEdge; cursorShape: Qt.SizeVerCursor }
    ResizeGrip { x: 8; y: window.height - 6; width: window.width - 16; height: 6; edges: Qt.BottomEdge; cursorShape: Qt.SizeVerCursor }
    ResizeGrip { x: 0; y: 0; width: 9; height: 9; edges: Qt.LeftEdge | Qt.TopEdge; cursorShape: Qt.SizeFDiagCursor }
    ResizeGrip { x: window.width - 9; y: 0; width: 9; height: 9; edges: Qt.RightEdge | Qt.TopEdge; cursorShape: Qt.SizeBDiagCursor }
    ResizeGrip { x: 0; y: window.height - 9; width: 9; height: 9; edges: Qt.LeftEdge | Qt.BottomEdge; cursorShape: Qt.SizeBDiagCursor }
    ResizeGrip { x: window.width - 9; y: window.height - 9; width: 9; height: 9; edges: Qt.RightEdge | Qt.BottomEdge; cursorShape: Qt.SizeFDiagCursor }
}
