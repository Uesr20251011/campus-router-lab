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
    property bool inspectorOpen: false
    property string inspectorTab: "demo"
    readonly property bool hasSelection: selectedNodeId >= 0 || selectedLinkId >= 0
    readonly property bool showingProperty: inspectorTab === "property" && hasSelection
    readonly property bool showingDemo: inspectorTab === "demo" && activeAlgorithm.length > 0
    readonly property var currentStep: stepIndex >= 0 && stepIndex < traceSteps.length ? traceSteps[stepIndex] : ({})
    readonly property bool treeReady: (activeAlgorithm === "prim" || activeAlgorithm === "kruskal")
                                      && currentStep.kind === "finish"
                                      && currentStep.selectedEdges
                                      && currentStep.selectedEdges.length === graphBackend.nodes.length - 1
    readonly property bool flowFinished: activeAlgorithm === "flow" && currentStep.kind === "finish"
    readonly property bool showFlowResult: flowFinished && showingDemo
    onHasSelectionChanged: updateInspector()
    onActiveAlgorithmChanged: updateInspector()
    onNoticeChanged: if (notice.length > 0) noticeTimer.restart()
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
    function updateInspector() {
        if (hasSelection) {
            inspectorCloseTimer.stop()
            inspectorOpen = true
            inspectorTab = "property"
        } else if (activeAlgorithm.length > 0) {
            inspectorCloseTimer.stop()
            inspectorOpen = true
            inspectorTab = "demo"
        } else {
            inspectorCloseTimer.restart()
        }
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
        if (id >= 0) {
            nodeNameField.text = graphBackend.nodeName(id)
            inspectorTab = "property"
        } else if (activeAlgorithm.length > 0) inspectorTab = "demo"
    }
    function chooseLink(id) {
        selectedLinkId = id
        selectedNodeId = -1
        if (id >= 0) inspectorTab = "property"
        else if (activeAlgorithm.length > 0) inspectorTab = "demo"
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
        inspectorTab = "demo"
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
        id: noticeTimer
        interval: 4500
        onTriggered: window.notice = ""
    }
    Timer {
        id: inspectorCloseTimer
        interval: 500
        onTriggered: if (!window.hasSelection && window.activeAlgorithm.length === 0) window.inspectorOpen = false
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
        objectName: "mainToolbar"
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: 96
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
        Row {
            id: editViewTools
            objectName: "editViewTools"
            anchors.left: parent.left; anchors.leftMargin: 18
            anchors.top: parent.top; anchors.topMargin: 7
            spacing: 4
            Text { text: "编辑"; color: "#8292A7"; font.pixelSize: 11; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
            ToolIconButton { symbol: "⌖"; hint: "选择与拖动 · Esc 返回此工具"; selected: window.activeTool === "select"; onClicked: window.activeTool = "select" }
            ToolIconButton { symbol: "＋"; hint: "添加节点 · 点击画布连续添加"; selected: window.activeTool === "node"; onClicked: window.activeTool = "node" }
            ToolIconButton { symbol: "↗"; hint: "连接节点 · 依次点击两个节点"; selected: window.activeTool === "link"; onClicked: window.activeTool = "link" }
            ToolIconButton { symbol: "▣"; hint: "固定节点模式 · 拖动只移动当前节点"; selected: window.fixedNodes; accent: "#E9E3FB"; onClicked: window.fixedNodes = !window.fixedNodes }
            ToolIconButton { symbol: "◎"; hint: "自动整理节点"; enabled: !window.fixedNodes; onClicked: { canvas.commitNow(); graphBackend.autoLayout(); canvas.fitView() } }
            Rectangle { width: 1; height: 25; color: "#DDE6F0"; anchors.verticalCenter: parent.verticalCenter }
            Text { text: "视图"; color: "#8292A7"; font.pixelSize: 11; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
            ToolIconButton { symbol: "¥"; hint: "显示建设造价"; selected: window.metric === "cost"; onClicked: window.metric = "cost" }
            ToolIconButton { symbol: "◫"; hint: "显示传输容量"; selected: window.metric === "capacity"; accent: "#E9E3FB"; onClicked: window.metric = "capacity" }
            ToolIconButton { symbol: "⊞"; hint: "显示或隐藏全部边权"; selected: canvas.showAllWeights; onClicked: canvas.showAllWeights = !canvas.showAllWeights }
            ToolIconButton { symbol: "⛶"; hint: "适应视图"; onClicked: canvas.fitView() }
        }
        Row {
            id: headerActions
            objectName: "headerActions"
            anchors.right: parent.right; anchors.rightMargin: 13
            anchors.top: parent.top; anchors.topMargin: 8
            spacing: 8
            SoftButton {
                id: networkButton
                objectName: "networkButton"
                text: "网络管理"; symbol: "◌"
                compact: true
                selected: networkPopup.visible
                onClicked: networkPopup.visible ? networkPopup.close() : networkPopup.open()
            }
            Rectangle { width: 1; height: 27; color: "#E8EDF4"; anchors.verticalCenter: parent.verticalCenter }
            Row {
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
        Rectangle { x: 18; y: 47; width: parent.width - 36; height: 1; color: "#F0F3F8" }
        Row {
            id: algorithmTools
            objectName: "algorithmTools"
            anchors.left: parent.left; anchors.leftMargin: 18
            anchors.top: parent.top; anchors.topMargin: 53
            spacing: 5
            Text { text: "算法"; color: "#8292A7"; font.pixelSize: 11; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
            ToolIconButton { objectName: "primTool"; symbol: "✳"; hint: "Prim 最小生成树 · 再次点击退出演示"; selected: window.activeAlgorithm === "prim"; onClicked: window.runAlgorithm("prim") }
            ToolIconButton { symbol: "◇"; hint: "Kruskal 最小生成树 · 再次点击退出演示"; selected: window.activeAlgorithm === "kruskal"; onClicked: window.runAlgorithm("kruskal") }
            ToolIconButton { symbol: "⇢"; hint: "最大流 · 再次点击退出演示"; selected: window.activeAlgorithm === "flow"; accent: "#E9E3FB"; onClicked: window.runAlgorithm("flow") }
            Rectangle { width: 1; height: 27; color: "#DDE6F0"; anchors.verticalCenter: parent.verticalCenter }
            Text { text: "起点"; color: "#8292A7"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
            SoftSpinBox { id: sourceBox; width: 88; height: 36; from: 1; to: 200; value: 1; ToolTip.visible: hovered; ToolTip.text: "算法起点 · 可直接输入" }
            Text { text: "终点"; color: "#8292A7"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
            SoftSpinBox { id: sinkBox; width: 88; height: 36; from: 1; to: 200; value: 20; ToolTip.visible: hovered; ToolTip.text: "最大流终点 · 可直接输入" }
        }
        Text {
            anchors.right: parent.right; anchors.rightMargin: 24
            anchors.verticalCenter: algorithmTools.verticalCenter
            text: graphBackend.nodes.length + " 个节点  ·  " + graphBackend.links.length + " 条链路"
            color: "#8A99AD"; font.pixelSize: 11
        }
    }

    Popup {
        id: networkPopup
        objectName: "networkPopup"
        parent: networkButton
        x: networkButton.width - width
        y: networkButton.mapFromItem(header, 0, header.height + 8).y
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
            id: canvasCard
            objectName: "canvasCard"
            x: 16; y: 16
            width: parent.width - 32 - (window.inspectorOpen ? rightPane.width + 12 : 0)
            height: parent.height - 32
            color: "#FFFFFF"; radius: 22; border.color: "#E7ECF4"
            Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
            GraphCanvas {
                id: canvas
                objectName: "graphCanvas"
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
            Rectangle {
                anchors.left: parent.left; anchors.bottom: parent.bottom
                anchors.leftMargin: 20; anchors.bottomMargin: 20
                width: legendRow.implicitWidth + 24; height: 36; radius: 12
                color: "#F5FFFFFF"; border.color: "#E7ECF5"
                Row {
                    id: legendRow; anchors.centerIn: parent; spacing: 13
                    Repeater {
                        model: window.flowFinished
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
            Rectangle {
                visible: window.notice.length > 0
                anchors.right: parent.right; anchors.bottom: parent.bottom
                anchors.rightMargin: 20; anchors.bottomMargin: 20
                width: Math.min(parent.width - 48, Math.max(170, noticeText.implicitWidth + 28))
                height: Math.max(38, noticeText.implicitHeight + 18)
                radius: 12; color: "#F4F8FF"; border.color: "#DCE7F2"
                Text {
                    id: noticeText
                    anchors.centerIn: parent
                    width: parent.width - 24
                    text: window.notice
                    color: "#54708C"; font.pixelSize: 11
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        Rectangle {
            id: rightPane
            objectName: "rightPane"
            width: window.width < 1200 ? 248 : 290
            height: parent.height - 32; y: 16
            x: window.inspectorOpen ? parent.width - width - 16 : parent.width + 8
            color: "#FFFFFF"; radius: 22; border.color: "#E7ECF4"
            Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
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
                Text {
                    text: window.showingProperty ? "属性编辑" : window.showFlowResult ? "最大流结果" : "算法演示"
                    color: "#344862"; font.pixelSize: 16; font.weight: Font.DemiBold
                }
                Text {
                    text: window.showingProperty ? "修改选中节点或链路" : window.showFlowResult
                          ? "总流量 " + window.currentStep.value + " 包/秒" : "跟随步骤查看算法过程"
                    color: window.showFlowResult ? "#45A996" : "#94A2B5"; font.pixelSize: 12
                }
                RowLayout {
                    visible: window.hasSelection && window.activeAlgorithm.length > 0
                    Layout.fillWidth: true; spacing: 7
                    SoftButton { text: "属性"; compact: true; Layout.fillWidth: true; selected: window.inspectorTab === "property"; onClicked: window.inspectorTab = "property" }
                    SoftButton { text: "演示"; compact: true; Layout.fillWidth: true; selected: window.inspectorTab === "demo"; onClicked: window.inspectorTab = "demo" }
                }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#EBEFF6"; Layout.topMargin: 5 }
                ColumnLayout {
                    visible: window.showingProperty && window.selectedNodeId >= 0
                    Layout.fillWidth: true; spacing: 8
                    Text { text: "节点  R" + window.selectedNodeId; color: "#7285A0"; font.pixelSize: 12 }
                    SoftTextField {
                        id: nodeNameField
                        Layout.fillWidth: true
                        placeholderText: "节点名称"
                        onAccepted: { canvas.commitNow(); graphBackend.renameNode(window.selectedNodeId, text) }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        SoftButton { text: "保存名称"; Layout.fillWidth: true; selected: true; onClicked: { canvas.commitNow(); if (!graphBackend.renameNode(window.selectedNodeId, nodeNameField.text)) window.notice = graphBackend.error } }
                        SoftButton { text: "删除"; accent: "#F4C9C9"; onClicked: window.removeSelection() }
                    }
                }
                ColumnLayout {
                    visible: window.showingProperty && window.selectedLinkId >= 0
                    Layout.fillWidth: true; spacing: 8
                    Text {
                        text: {
                            const link = window.selectedLinkData()
                            return link ? "链路  R" + link.u + (link.directed ? " → R" : " — R") + link.v : "链路"
                        }
                        color: "#7285A0"; font.pixelSize: 12
                    }
                    Text { text: "建设造价"; color: "#8191A8"; font.pixelSize: 11 }
                    SoftTextField { id: costField; Layout.fillWidth: true; placeholderText: "例如 10"; validator: IntValidator { bottom: 0 } }
                    Text { text: "最大传输容量（包/秒）"; color: "#8191A8"; font.pixelSize: 11 }
                    SoftTextField { id: capacityField; Layout.fillWidth: true; placeholderText: "例如 15"; validator: IntValidator { bottom: 0 } }
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
                    visible: !window.hasSelection && !window.showingDemo
                    Layout.fillWidth: true; height: 105; radius: 16; color: "#F6F8FD"
                    Column {
                        anchors.centerIn: parent; spacing: 7
                        Text { text: "○"; font.pixelSize: 28; color: "#B5C7E2"; anchors.horizontalCenter: parent.horizontalCenter }
                        Text { text: "选择一个节点或链路"; color: "#93A1B5"; font.pixelSize: 12; anchors.horizontalCenter: parent.horizontalCenter }
                    }
                }
                Rectangle { visible: window.showingDemo; Layout.fillWidth: true; height: 1; color: "#EBEFF6"; Layout.topMargin: 4 }
                Text { visible: window.showingDemo && !window.showFlowResult; text: "当前步骤"; color: "#344862"; font.pixelSize: 14; font.weight: Font.DemiBold }
                Rectangle {
                    visible: window.showingDemo && !window.showFlowResult
                    Layout.fillWidth: true; Layout.preferredHeight: window.activeAlgorithm === "flow" ? 166 : 146
                    radius: 17; color: "#F4F7FE"; border.color: "#E8ECF7"
                    Column {
                        anchors.fill: parent; anchors.margins: 15; spacing: 8
                        Text { text: window.stepIndex >= 0 ? "第 " + (window.stepIndex + 1) + " / " + window.traceSteps.length + " 步" : "等待运行"; color: "#8396B2"; font.pixelSize: 11 }
                        Text { width: parent.width; text: window.currentStep.title || "选择算法开始演示"; color: "#344964"; font.pixelSize: 16; font.weight: Font.DemiBold; wrapMode: Text.Wrap }
                        Text { width: parent.width; text: window.currentStep.detail || "算法经过的节点和链路会在画布上依次亮起。"; color: "#7D90A9"; font.pixelSize: 12; wrapMode: Text.Wrap }
                    }
                }
                Text { visible: window.showingDemo && !window.showFlowResult && window.stepIndex >= 0; text: (window.activeAlgorithm === "flow" ? "当前流量  " : "累计造价  ") + (window.currentStep.value || 0); color: "#57AF9C"; font.pixelSize: 17; font.weight: Font.Bold }
                Text {
                    visible: window.showingDemo && !window.showFlowResult && window.activeAlgorithm === "flow" && window.stepIndex >= 0
                    Layout.fillWidth: true; wrapMode: Text.Wrap
                    text: "边权 0/7 = 已用/总容量；灰边已耗尽，紫色箭头可反向回退。"
                    color: "#8494AA"; font.pixelSize: 11
                }
                ColumnLayout {
                    visible: window.showingDemo && window.showFlowResult
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
                Text { visible: window.notice.length > 0; text: window.notice; color: "#C37D7F"; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }
                Item { height: 6 }
            }
            }
        }
    }

    Rectangle {
        id: playback
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: window.activeAlgorithm.length > 0 ? 90 : 0
        clip: true
        color: "#FFFFFF"
        Behavior on height { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
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
            SoftComboBox { id: speedBox; model: ["0.5×", "1×", "2×"]; currentIndex: 1; Layout.preferredWidth: window.width < 1000 ? 75 : 92 }
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
