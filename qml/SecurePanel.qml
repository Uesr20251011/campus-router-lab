import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: panel
    objectName: "securePanel"
    property var host
    property int source: 1
    property int target: 2
    readonly property var snapshot: host.secureState
    readonly property string phase: snapshot.phase || "idle"
    readonly property bool compact: host.height < 660
    readonly property int panelWidth: host.width < 1100 ? 292 : 320
    readonly property var candidates: host.securePreviewRoute.slice(1, -1)
    readonly property int observer: -1
    readonly property var route: phase === "idle" ? host.securePreviewRoute : snapshot.route || []
    readonly property int stage: phase === "idle" || nodeKind === "sender" ? 0 : middleNode ? 1 : 2
    readonly property color tone: middleNode ? nodeState.attacked ? "#B65360" : "#258470" : nodeKind === "receiver" ? phase === "rejected" ? "#B65360" : "#258470" : "#3778BE"
    property alias message: messageInput.text
    property alias testMessage1: testInput1.text
    property alias testMessage2: testInput2.text
    property int selectedRouteNode: -1
    readonly property int nodeIndex: route.indexOf(selectedRouteNode)
    readonly property bool middleNode: nodeIndex > 0 && nodeIndex < route.length - 1
    readonly property string nodeKind: selectedRouteNode === snapshot.target ? "receiver" : middleNode ? "observer" : "sender"
    readonly property var nodeState: (snapshot.nodeStates || [])[nodeIndex] || ({})
    readonly property string nodePacket: nodeState.outputPacket !== undefined ? nodeState.outputPacket : snapshot.senderPacket || ""
    property bool attackHex: false
    property string attackError: ""
    property string detailKind: "sender"
    property string verificationResult: ""
    property bool verificationPassed: false
    property bool showPacketParts: false
    property var flowTrace: ({})
    property string flowEndpoint: "sender"
    property int flowPacketId: 0
    readonly property string detailPacket: detailKind === "sender" ? snapshot.senderPacket || "" : detailKind === "observer" ? nodePacket : snapshot.receivedPacket || ""
    readonly property var packetParts: detailPacket && snapshot.encrypted && typeof secureService !== "undefined" ? secureService.inspectPacket(detailPacket) : ({})


    function selectRouteNode(id) {
        if (route.indexOf(id) >= 0) selectedRouteNode = id
    }
    function observeNode(id) { selectRouteNode(id) }
    function packetHex(packet) {
        if (!packet || typeof secureService === "undefined") return ""
        const parts = secureService.inspectPacket(packet)
        return parts.success ? (parts.nonce || "") + (parts.tag || "") + (parts.ciphertext || "") : ""
    }
    function reloadAttack() {
        attackError = ""
        attackInput.text = attackHex ? packetHex(nodePacket) : nodePacket
    }
    function openAttack() {
        if (!middleNode || !snapshot.packetId) return
        attackHex = false; reloadAttack(); attackPopup.open()
    }
    function applyAttack() {
        let packet = attackInput.text
        if (attackHex) {
            const hex = packet.replace(/\s/g, "")
            if (hex.length % 2 || !/^[0-9a-fA-F]*$/.test(hex)) {
                attackError = "请输入偶数位的十六进制字节，可用空格或换行分隔。"; return
            }
            packet = secureService.packetFromHex(hex)
        }
        if (!host.secureChannel.editNode(selectedRouteNode, packet)) {
            attackError = snapshot.error || "修改失败"; return
        }
        host.secureError = ""; host.updateSecureHistory(); attackPopup.close()
    }
    function scenario(value) {
        if (host.secureBusy) return
        host.clearSecurePacket()
        host.secureScenario = value
    }
    function openDetail(kind) { detailKind = kind; showPacketParts = false; detailPopup.open() }
    function openFlow(endpoint) {
        if (!snapshot.encrypted || !snapshot.packetId) return
        if (endpoint === "receiver" && !snapshot.receivedPacket) return
        if (endpoint === "receiver" && phase === "received") host.decryptSecure()
        const computed = host.secureChannel.processTrace(endpoint, 0)
        if (!computed.success) { host.secureError = computed.error || "暂无处理过程"; return }
        host.secureRunning = false
        flowTrace = computed; flowEndpoint = endpoint; flowPacketId = snapshot.packetId
        flowView.aesExpanded = false; flowView.aesStep = 0; flowView.selectedByte = 0; flowView.hashIndex = 0
        flowView.resetDetailView()
        flowView.selectedStep = endpoint === "receiver" ? "verify" : "bytes"
        flowPopup.open()
    }
    onSnapshotChanged: {
        if (phase === "idle") selectedRouteNode = -1
        if (flowPopup.visible) {
            if (snapshot.packetId !== flowPacketId) flowPopup.close()
            else {
                const computed = host.secureChannel.processTrace(flowEndpoint, flowTrace.blockIndex || 0)
                if (computed.success) flowTrace = computed
                else flowPopup.close()
            }
        }
        if (phase === "idle") attackPopup.close()
    }
    function packetLabel() { return snapshot.packetId ? "包 #" + String(snapshot.packetId).padStart(3, "0") : "本次通信路径" }
    function title() {
        if (phase === "idle") return "R" + panel.source + " · 输入发送消息"
        if (nodeKind === "sender") return "R" + snapshot.source + " · 发送端"
        if (middleNode) return "R" + selectedRouteNode + (nodeState.attacked ? " · 已实施攻击" : " · 中间转发节点")
        return "R" + snapshot.target + (phase === "decrypted" ? " · 接收成功" : " · 接收校验失败")
    }
    function explanation() {
        if (phase === "idle") return "输入消息后展示完整路径。点击任一节点查看数据与说明。"
        if (nodeKind === "sender") return "此节点生成发送包。可进入具体步骤查看本次加密与认证计算。"
        if (middleNode) return (snapshot.encrypted ? "此节点只有公开包，无通信密钥。" : "明文传输，此节点可直接读取消息。") + (nodeState.changed ? "当前包已被修改。" : "当前包与发送包一致。") + "可编辑包内容实施攻击，所有后续节点会立即更新。"
        if (phase === "rejected") return snapshot.error || "接收端拒绝输出未经认证的原文。"
        return snapshot.matchesOriginal ? "接收结果与发送时的消息逐字一致。" : "接收消息与发送消息不同。"
    }
    ColumnLayout {
        id: story
        x: parent.width - panel.panelWidth - 16; y: 16
        width: panel.panelWidth; height: parent.height - 32
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Text { text: "安全通信"; color: "#263E59"; font.pixelSize: 20; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            SoftButton { objectName: "secureSettingsButton"; text: "设置"; compact: true; onClicked: settingsPopup.open() }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 5
            Repeater {
                model: ["发送", "途中", "接收"]
                delegate: RowLayout {
                    required property int index
                    required property string modelData
                    Layout.fillWidth: true; spacing: 5
                    Rectangle {
                        width: 20; height: 20; radius: 10
                        color: panel.stage === index ? "#3778BE" : panel.stage > index ? "#E1F2EC" : "#E8EDF4"
                        Text { anchors.centerIn: parent; text: panel.stage > index ? "✓" : index + 1; color: panel.stage === index ? "#FFFFFF" : panel.stage > index ? "#258470" : "#8A99AE"; font.pixelSize: 11 }
                    }
                    Text { text: modelData; color: panel.stage === index ? "#2A4D72" : "#8A99AE"; font.pixelSize: 12; font.weight: panel.stage === index ? Font.DemiBold : Font.Normal }
                    Text { visible: index < 2; text: "›"; color: "#B2C0D2"; font.pixelSize: 16; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
                }
            }
        }
        Rectangle {
            objectName: "secureFocusCard"
            Layout.fillWidth: true; Layout.fillHeight: true
            radius: 18; color: "#FFFFFF"; border.color: "#DFE7F0"
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 18
                spacing: panel.compact ? 8 : 12
                Text { text: panel.packetLabel(); color: "#8A99AE"; font.pixelSize: 11 }
                Text { Layout.fillWidth: true; text: panel.title(); color: panel.tone; font.pixelSize: 18; font.weight: Font.DemiBold; wrapMode: Text.Wrap }
                Text { Layout.fillWidth: true; text: panel.explanation(); color: "#73859B"; font.pixelSize: 12; wrapMode: Text.Wrap }
                TextArea {
                    id: messageInput; objectName: "secureMessageInput"
                    visible: panel.phase === "idle"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    placeholderText: "现场输入一条消息…"
                    wrapMode: TextEdit.Wrap; selectByMouse: true
                    font.family: "Microsoft YaHei UI"; font.pixelSize: 15; color: "#344862"
                    padding: 12
                    background: Rectangle { radius: 12; color: "#F5F8FC"; border.color: messageInput.activeFocus ? "#7DA8D6" : "#E3EAF3" }
                }
                Rectangle {
                    visible: panel.phase !== "idle"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    radius: 12; color: panel.phase === "rejected" ? "#FFF5F6" : "#F5F8FC"
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 12; spacing: 8
                        Text { text: panel.nodeKind === "receiver" ? panel.phase === "rejected" ? "校验结果" : "接收消息" : snapshot.encrypted ? "此节点的传输包 · Base64" : "此节点的明文包"; color: "#8A99AE"; font.pixelSize: 11 }
                        TextArea {
                            objectName: "secureStageData"
                            Layout.fillWidth: true; Layout.fillHeight: true
                            readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere
                            text: panel.nodeKind === "receiver" ? panel.phase === "rejected" ? "校验未通过，未输出明文" : snapshot.plaintext || "" : panel.nodePacket
                            font.family: (panel.nodeKind === "receiver" && panel.phase === "decrypted") || snapshot.encrypted === false ? "Microsoft YaHei UI" : "Consolas"
                            font.pixelSize: panel.nodeKind === "receiver" && panel.phase === "decrypted" ? 16 : 13; color: panel.nodeKind === "receiver" && panel.phase === "rejected" ? "#B65360" : "#3C5878"
                            padding: 0; background: Item {}
                        }
                        Text { visible: !!snapshot.tampered; Layout.fillWidth: true; text: "路径中已有 " + (snapshot.nodeStates || []).filter(n => n.attacked).length + " 个节点实施攻击"; color: "#B65360"; font.pixelSize: 12 }
                    }
                }
                Text { visible: host.secureError.length > 0; Layout.fillWidth: true; text: host.secureError; color: "#B65360"; font.pixelSize: 12; wrapMode: Text.Wrap }
                RowLayout {
                    visible: panel.phase !== "idle"
                    Layout.fillWidth: true; spacing: 6
                    SoftButton {
                        objectName: "securePacketPeekButton"
                        text: "完整数据"; compact: true
                        onClicked: panel.openDetail(panel.nodeKind)
                    }
                    SoftButton {
                        objectName: "secureTamperButton"; visible: panel.middleNode
                        text: "编辑包 / 攻击"; compact: true; onClicked: panel.openAttack()
                    }
                    SoftButton { objectName: "secureChangeKeyButton"; visible: panel.nodeKind === "receiver" && panel.phase === "rejected"; text: "调整密钥"; compact: true; onClicked: keyPopup.open() }
                    Item { Layout.fillWidth: true }
                }
                SoftButton {
                    objectName: "secureRestoreNodeButton"; Layout.fillWidth: true; visible: panel.middleNode && !!panel.nodeState.attacked
                    text: "撤销攻击"; compact: true
                    onClicked: { host.secureChannel.restoreNode(panel.selectedRouteNode); host.updateSecureHistory() }
                }
                SoftButton { objectName: "secureFlowButton"; Layout.fillWidth: true; visible: !!snapshot.packetId && snapshot.encrypted && !panel.middleNode; text: panel.nodeKind === "receiver" ? "查看接收端具体步骤 →" : "查看发送端具体步骤 →"; compact: true; onClicked: panel.openFlow(panel.nodeKind) }
                SoftButton { objectName: "secureSendButton"; Layout.fillWidth: true; visible: panel.phase === "idle"; text: host.secureScenario === "plaintext" ? "发送明文 →" : "加密并发送 →"; selected: true; accent: "#DDECFB"; enabled: !host.secureBusy && messageInput.text.length > 0; onClicked: host.sendSecure() }
                SoftButton { objectName: "secureRestartButton"; Layout.fillWidth: true; visible: !!snapshot.packetId; text: "换条消息再试 →"; selected: true; accent: "#DDECFB"; onClicked: { host.clearSecurePacket(); messageInput.forceActiveFocus(); messageInput.selectAll() } }

            }
        }
        Text { Layout.fillWidth: true; text: "真实 AES 加解密 · 本机模拟路径转发"; color: "#96A4B6"; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter }
    }
    component Caption: Text { color: "#7187A2"; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
    component DataBox: TextArea {
        readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere
        Layout.fillWidth: true; font.family: "Consolas"; font.pixelSize: 12; color: "#48617F"
        padding: 10
        background: Rectangle { radius: 10; color: "#F5F8FD"; border.color: "#E0E8F3" }
    }

    Popup {
        id: attackPopup; objectName: "secureAttackPopup"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(640, host.width - 48); height: Math.min(560, host.height - 48)
        modal: true; dim: true; focus: true; padding: 22
        background: Rectangle { radius: 20; color: "#FFFFFF"; border.color: "#DCE5F2" }
        Overlay.modal: Rectangle { color: "#550E2035" }
        ColumnLayout {
            anchors.fill: parent; spacing: 12
            Text { text: "R" + panel.selectedRouteNode + " · 修改传输包"; color: "#344862"; font.pixelSize: 20; font.weight: Font.DemiBold }
            Caption { text: "以下为此节点当前转发的完整包。可以修改、插入或删除内容；应用后立即更新后续节点及接收结果。后续攻击保留其修改的字节，其他字节继承新的上游包。" }
            RowLayout {
                Layout.fillWidth: true
                SoftButton { text: "原始包"; compact: true; selected: !panel.attackHex; onClicked: { panel.attackHex = false; panel.reloadAttack() } }
                SoftButton { text: "十六进制字节"; visible: snapshot.encrypted && panel.packetHex(panel.nodePacket).length > 0; compact: true; selected: panel.attackHex; onClicked: { panel.attackHex = true; panel.reloadAttack() } }
            }
            Caption { text: panel.attackHex ? "前 12 字节为随机数，随后 16 字节为标签，其余为密文，均可修改。切换展示方式会重新载入当前包。" : snapshot.encrypted ? "完整 raw 包（Base64）。也可输入无效或截断的包，观察接收端拒绝处理。" : "完整明文包。" }
            ScrollView {
                id: attackScroll
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                contentWidth: availableWidth
                TextArea {
                    id: attackInput; objectName: "secureAttackInput"
                    width: attackScroll.availableWidth
                    wrapMode: TextEdit.WrapAnywhere; selectByMouse: true
                    font.family: "Consolas"; font.pixelSize: 13; color: "#344862"
                    background: Rectangle { radius: 10; color: "#F5F8FD"; border.color: "#E0E8F3" }
                }
            }
            Caption { text: panel.attackError; color: "#B65360"; visible: panel.attackError.length > 0 }
            RowLayout {
                Layout.fillWidth: true
                SoftButton { objectName: "secureApplyAttackButton"; text: "实施攻击并更新路径"; compact: true; selected: true; onClicked: panel.applyAttack() }
                Item { Layout.fillWidth: true }
                SoftButton { text: "取消"; compact: true; onClicked: attackPopup.close() }
            }
        }
    }

    Popup {
        id: flowPopup; objectName: "secureWorkflowPopup"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(1180, host.width - 48); height: Math.min(770, host.height - 48)
        modal: true; dim: true; focus: true; padding: 22
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { radius: 22; color: "#FAFCFF"; border.color: "#DCE6F3" }
        Overlay.modal: Rectangle { color: "#550E2035" }
        ColumnLayout {
            anchors.fill: parent; spacing: 14
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    Text { text: "R" + (panel.flowEndpoint === "sender" ? snapshot.source : snapshot.target) + (panel.flowEndpoint === "sender" ? " · 发送端内部结构" : " · 接收端内部结构"); color: "#2D4C70"; font.pixelSize: 21; font.weight: Font.DemiBold }
                    Text { text: panel.packetLabel() + " · AES-256-GCM · 点击工序查看本次计算"; color: "#8A9CB3"; font.pixelSize: 11 }
                }
                Item { Layout.fillWidth: true }
                SoftButton { text: "返回网络"; compact: true; onClicked: flowPopup.close() }
            }
            CryptoWorkflow {
                id: flowView
                Layout.fillWidth: true; Layout.fillHeight: true
                trace: panel.flowTrace; endpoint: panel.flowEndpoint
                onBlockRequested: index => panel.flowTrace = host.secureChannel.processTrace(panel.flowEndpoint, index)
            }
            RowLayout {
                Layout.fillWidth: true
                Text { text: "查看数据块"; color: "#7A90AB"; font.pixelSize: 11 }
                SoftComboBox {
                    Layout.preferredWidth: 136
                    model: Array.from({length: panel.flowTrace.blockCount || 0}, (_,i) => "第 " + (i+1) + " 块 / " + panel.flowTrace.blockCount)
                    currentIndex: panel.flowTrace.blockIndex || 0
                    onActivated: index => panel.flowTrace = host.secureChannel.processTrace(panel.flowEndpoint, index)
                }
                Item { Layout.fillWidth: true }
                SoftButton { text: "上一工序"; compact: true; onClicked: flowView.moveStep(-1) }
                SoftButton { objectName: "flowNextButton"; text: "下一工序 →"; compact: true; selected: true; accent: "#DDECFB"; onClicked: flowView.moveStep(1) }
            }
        }
    }

    Popup {
        id: settingsPopup; objectName: "secureSettingsPopup"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(500, host.width - 48); height: 350
        modal: true; dim: true; focus: true; padding: 24
        background: Rectangle { radius: 20; color: "#FFFFFF"; border.color: "#DCE5F2" }
        Overlay.modal: Rectangle { color: "#550E2035" }
        ColumnLayout {
            anchors.fill: parent; spacing: 12
            Text { text: "实验设置"; font.pixelSize: 20; font.weight: Font.DemiBold; color: "#344862" }
            Caption { text: "默认进行加密通信。明文对照和验证工具可以在这里打开。" }
            RowLayout {
                Layout.fillWidth: true
                Caption { text: "传输方式" }
                SoftComboBox { Layout.preferredWidth: 240; model: ["加密传输（AES）", "明文传输（对照）"]; currentIndex: host.secureScenario === "plaintext" ? 1 : 0; enabled: !host.secureBusy; onActivated: index => panel.scenario(index === 1 ? "plaintext" : "encrypted") }
            }
            Caption { text: "路径上的每个中间节点均可攻击。点击节点后编辑包内容，后续路径与接收结果会立即更新。" }
            RowLayout {
                Layout.fillWidth: true
                SoftButton { text: "通信密钥"; compact: true; onClicked: { settingsPopup.close(); keyPopup.open() } }
                SoftButton { text: "测试与记录"; compact: true; onClicked: { settingsPopup.close(); recordsPopup.open() } }
            }
            Item { Layout.fillHeight: true }
            SoftButton { Layout.alignment: Qt.AlignRight; text: "返回实验"; compact: true; selected: true; onClicked: settingsPopup.close() }
        }
    }
    Popup {
        id: keyPopup; objectName: "secureKeyPopup"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(560, host.width - 48); height: 350
        modal: true; dim: true; focus: true; padding: 22
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { radius: 20; color: "#FFFFFF"; border.color: "#DCE5F2" }
        Overlay.modal: Rectangle { color: "#550E2035" }
        ColumnLayout {
            anchors.fill: parent; spacing: 10
            Text { text: "通信密钥"; font.pixelSize: 18; font.weight: Font.DemiBold; color: "#344862" }
            Caption { text: "双方预先共享 256 位密钥。这里可以独立修改接收端密钥，验证错误密钥无法解密。" }
            Caption { text: "发送端 · 64 个十六进制字符" }
            SoftTextField { objectName: "secureKeyField"; Layout.fillWidth: true; text: host.secureKey; font.family: "Consolas"; font.pixelSize: 11; onTextEdited: host.secureKey = text }
            Caption { text: "接收端 · 只用于解密收到的数据包" }
            SoftTextField { objectName: "secureReceiverKeyField"; Layout.fillWidth: true; text: host.secureReceiverKey; font.family: "Consolas"; font.pixelSize: 11; onTextEdited: host.secureReceiverKey = text }
            Caption { text: host.secureKey === host.secureReceiverKey ? "✓ 两端密钥相同" : "两端密钥不同 · 接收端可能无法解密"; color: host.secureKey === host.secureReceiverKey ? "#438E79" : "#B67754" }
            RowLayout {
                Layout.fillWidth: true
                SoftButton { text: "生成并同步新密钥"; compact: true; enabled: !host.secureBusy; onClicked: host.generateSecureKey() }
                SoftButton { text: "同步到接收端"; compact: true; onClicked: host.secureReceiverKey = host.secureKey }
                Item { Layout.fillWidth: true }
                SoftButton { objectName: "secureRetryButton"; visible: panel.phase === "rejected"; text: "重试解密"; compact: true; onClicked: { keyPopup.close(); host.decryptSecure() } }
                SoftButton { text: "完成"; compact: true; selected: true; onClicked: keyPopup.close() }
            }
        }
    }

    Popup {
        id: detailPopup; objectName: "securePacketDetail"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(650, host.width - 48); height: Math.min(620, host.height - 48)
        modal: true; dim: true; focus: true; padding: 22
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { radius: 20; color: "#FFFFFF"; border.color: "#DCE5F2" }
        Overlay.modal: Rectangle { color: "#550E2035" }
        ColumnLayout {
            anchors.fill: parent; spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Text { text: panel.packetLabel() + " · " + (panel.detailKind === "sender" ? "发送数据" : panel.detailKind === "observer" ? "截获数据" : "接收数据"); font.pixelSize: 17; font.weight: Font.DemiBold; color: "#344862" }
                Item { Layout.fillWidth: true }
                SoftButton { text: "关闭"; compact: true; onClicked: detailPopup.close() }
            }
            Flickable {
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true; contentHeight: packetColumn.implicitHeight; boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                ColumnLayout {
                    id: packetColumn; width: parent.width; spacing: 10
                    Caption { text: snapshot.encrypted ? "传输包（Base64 是展示编码，加密由 AES-GCM 完成）" : "传输包（未加密的明文）" }
                    DataBox { Layout.preferredHeight: 120; text: panel.detailPacket }
                    Caption { visible: panel.detailKind === "observer" && !!panel.nodeState.attacked; text: "篡改前的截获包" }
                    DataBox { visible: panel.detailKind === "observer" && !!panel.nodeState.attacked; Layout.preferredHeight: 76; text: panel.nodeState.inputPacket || "" }
                    SoftButton { visible: snapshot.encrypted; text: panel.showPacketParts ? "收起包结构 ▴" : "解析包结构 ▾"; compact: true; onClicked: panel.showPacketParts = !panel.showPacketParts }
                    Caption { visible: snapshot.encrypted && panel.showPacketParts; text: "包内随机数 · 12 字节" }
                    DataBox { visible: snapshot.encrypted && panel.showPacketParts; Layout.preferredHeight: 40; text: packetParts.nonce || "" }
                    Caption { visible: snapshot.encrypted && panel.showPacketParts; text: "包内认证标签 · 16 字节" }
                    DataBox { visible: snapshot.encrypted && panel.showPacketParts; Layout.preferredHeight: 52; text: packetParts.tag || "" }
                    Caption { visible: snapshot.encrypted && panel.showPacketParts; text: "从本包解析出的密文字节（十六进制）" }
                    DataBox { visible: snapshot.encrypted && panel.showPacketParts; Layout.preferredHeight: 80; text: packetParts.ciphertext || "" }
                    Caption { visible: !!snapshot.packetId && snapshot.encrypted; text: "独立验证仅使用你提供的密文和密钥，不读取发送原文。" }
                    SoftButton {
                        visible: !!snapshot.packetId && snapshot.encrypted
                        text: "提供密钥，独立验证这份包"; compact: true; selected: true; accent: "#DDECFB"
                        onClicked: {
                            verificationPacket.text = panel.detailKind === "receiver" ? snapshot.receivedPacket || "" : panel.detailKind === "sender" ? snapshot.senderPacket || "" : panel.nodePacket
                            verificationKey.text = ""
                            panel.verificationResult = ""
                            detailPopup.close(); verificationPopup.open()
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: verificationPopup; objectName: "secureVerificationPopup"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(600, host.width - 48); height: Math.min(500, host.height - 48)
        modal: true; dim: true; focus: true; padding: 22
        background: Rectangle { radius: 20; color: "#FFFFFF"; border.color: "#DCE5F2" }
        Overlay.modal: Rectangle { color: "#550E2035" }
        ColumnLayout {
            anchors.fill: parent; spacing: 10
            Text { text: "独立解密验证"; font.pixelSize: 18; font.weight: Font.DemiBold; color: "#344862" }
            Caption { text: "这里仅使用下方密文和密钥计算结果。可以粘贴另一份包，也可以尝试错误密钥。" }
            TextArea {
                id: verificationPacket; objectName: "secureVerificationPacket"
                Layout.fillWidth: true; Layout.preferredHeight: 108
                wrapMode: TextEdit.WrapAnywhere; selectByMouse: true; font.family: "Consolas"; font.pixelSize: 12
                background: Rectangle { radius: 10; color: "#F5F8FD"; border.color: "#E0E8F3" }
                onTextChanged: panel.verificationResult = ""
            }
            SoftTextField { id: verificationKey; objectName: "secureVerificationKey"; Layout.fillWidth: true; placeholderText: "输入 64 位十六进制密钥"; font.family: "Consolas"; onTextChanged: panel.verificationResult = "" }
            DataBox { objectName: "secureVerificationResult"; Layout.fillHeight: true; text: panel.verificationResult; placeholderText: "计算结果将在这里出现"; color: panel.verificationPassed ? "#3E8675" : "#B96972" }
            RowLayout {
                Layout.fillWidth: true
                SoftButton {
                    objectName: "secureVerificationRun"; text: "计算解密结果"; compact: true; selected: true
                    onClicked: {
                        const result = secureService.decrypt(verificationPacket.text, verificationKey.text)
                        panel.verificationPassed = result.success
                        panel.verificationResult = result.success ? result.plaintext : result.error
                    }
                }
                Item { Layout.fillWidth: true }
                SoftButton { text: "关闭"; compact: true; onClicked: verificationPopup.close() }
            }
        }
    }

    Popup {
        id: recordsPopup; objectName: "secureRecordsPopup"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(700, host.width - 48); height: Math.min(650, host.height - 48)
        modal: true; dim: true; focus: true; padding: 22
        background: Rectangle { radius: 20; color: "#FFFFFF"; border.color: "#DCE5F2" }
        Overlay.modal: Rectangle { color: "#550E2035" }
        ColumnLayout {
            anchors.fill: parent; spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Text { text: "验证与通信记录"; font.pixelSize: 18; font.weight: Font.DemiBold; color: "#344862" }
                Item { Layout.fillWidth: true }
                SoftButton { text: "关闭"; compact: true; onClicked: recordsPopup.close() }
            }
            Flickable {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                contentHeight: recordsColumn.implicitHeight; boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                ColumnLayout {
                    id: recordsColumn; width: parent.width; spacing: 10
                    Caption { text: "两条消息的算法正确性测试 · 可现场修改内容" }
                    SoftTextField { id: testInput1; objectName: "secureTestMessage1"; Layout.fillWidth: true; text: "校园网络状态：节点运行正常。" }
                    SoftTextField { id: testInput2; objectName: "secureTestMessage2"; Layout.fillWidth: true; text: "第二条数据 #42 · 速率 15 包/秒！" }
                    SoftButton { text: "运行两条消息测试"; compact: true; selected: true; enabled: testInput1.text.length > 0 && testInput2.text.length > 0; onClicked: host.runSecureTests() }
                    Repeater {
                        model: host.secureTests
                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true; spacing: 5
                            Caption { text: (modelData.passed ? "✓ " : "× ") + modelData.name + "：" + modelData.detail; color: modelData.passed ? "#3E927B" : "#BC727A" }
                            DataBox { visible: !!modelData.packet; Layout.preferredHeight: 55; text: modelData.packet || "" }
                            Caption { visible: !!modelData.packet; text: "解密结果：" + (modelData.plaintext || "未恢复明文") }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#E4EBF4" }
                    Caption { text: "本次通信过程 · " + panel.packetLabel() }
                    Repeater { model: snapshot.events || []; delegate: Caption { required property var modelData; text: "· " + modelData } }
                    Caption { text: "最近通信结果（最多保留 12 条）" }
                    Caption { visible: host.secureHistory.length === 0; text: "发送并在接收端解密后，这里将保存真实结果。" }
                    Repeater {
                        model: host.secureHistory
                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true; spacing: 5
                            Caption { text: (modelData.passed ? "✓ " : "× ") + "包 #" + modelData.packetId + " · " + (modelData.encrypted ? "AES 加密" : "明文对照") + " · " + modelData.detail; color: modelData.passed ? "#3E927B" : "#BC727A" }
                            Caption { text: "发送快照：" + modelData.original }
                            DataBox { Layout.preferredHeight: 55; text: modelData.packet }
                            Caption { text: "恢复原文：" + (modelData.plaintext || "未输出明文") }
                            Rectangle { Layout.fillWidth: true; height: 1; color: "#ECF0F6" }
                        }
                    }
                }
            }
        }
    }
}
