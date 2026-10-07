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
    readonly property int observer: candidates.length && observationChoice.currentIndex >= 0 && observationChoice.currentIndex < candidates.length
                                    ? candidates[observationChoice.currentIndex] : -1
    readonly property var route: phase === "idle" ? host.securePreviewRoute : snapshot.route || []
    readonly property int stage: phase === "idle" ? 0 : phase === "transit" || phase === "intercepted" ? 1 : 2
    readonly property color tone: phase === "rejected" ? "#B65360" : phase === "intercepted" ? "#AC7334" : phase === "decrypted" ? "#258470" : "#3778BE"
    property alias message: messageInput.text
    property alias testMessage1: testInput1.text
    property alias testMessage2: testInput2.text
    property string detailKind: "sender"
    property string verificationResult: ""
    property bool verificationPassed: false
    property bool showPacketParts: false
    property var flowTrace: ({})
    property string flowEndpoint: "sender"
    property int flowPacketId: 0
    readonly property string detailPacket: detailKind === "sender" ? snapshot.senderPacket || "" : detailKind === "observer" ? snapshot.capturedPacket ? snapshot.wirePacket || "" : "" : snapshot.receivedPacket || ""
    readonly property var packetParts: detailPacket && snapshot.encrypted && typeof secureService !== "undefined" ? secureService.inspectPacket(detailPacket) : ({})
    onCandidatesChanged: observationChoice.currentIndex = 0

    function observeNode(id) {
        const index = candidates.indexOf(id)
        if (index >= 0) observationChoice.currentIndex = index
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
    onSnapshotChanged: if (flowPopup.visible && snapshot.packetId !== flowPacketId) flowPopup.close()
    function packetLabel() { return snapshot.packetId ? "包 #" + String(snapshot.packetId).padStart(3, "0") : "本次通信路径" }
    function title() {
        if (phase === "idle") return "R" + panel.source + " · 输入发送消息"
        if (phase === "intercepted") return "R" + snapshot.currentNode + (snapshot.encrypted ? " · 只截获了密文" : " · 读到了原文")
        if (phase === "received") return "R" + snapshot.target + (snapshot.encrypted ? " · 收到密文" : " · 收到明文")
        if (phase === "decrypted") return "R" + snapshot.target + (snapshot.encrypted ? " · 解密成功" : " · 读取成功")
        if (phase === "rejected") return "R" + snapshot.target + " · 拒绝解密"
        return snapshot.encrypted ? "消息已加密，正在传输" : "消息正在以明文传输"
    }
    function explanation() {
        if (phase === "idle") return "现场输入任意内容，点击发送后再沿路径观察。"
        if (phase === "intercepted") return snapshot.encrypted ? "R" + snapshot.currentNode + " 没有通信密钥，无法从这份数据恢复消息。" : "R" + snapshot.currentNode + " 没有密钥，也能读到消息。"
        if (phase === "received") return "R" + snapshot.target + " 收到的是途中转发的数据包。"
        if (phase === "decrypted") return snapshot.matchesOriginal ? "✓ 解密结果与发送时的原文逐字一致。" : "解密结果与发送原文不同。"
        if (phase === "rejected") return snapshot.tampered ? "途中修改了密文，接收端的完整性校验未通过。" : "接收端密钥不匹配，校验未通过。"
        return "R" + snapshot.currentNode + " → R" + snapshot.route[snapshot.hop + 1] + "，到观察节点后会自动停下。"
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
                model: ["发送", "截获", "解密"]
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
                        Text { text: panel.phase === "rejected" ? "校验结果" : panel.phase === "decrypted" ? snapshot.encrypted ? "解密结果" : "收到的消息" : snapshot.encrypted ? "此节点的数据 · 密文节选" : "此节点的数据 · 明文"; color: "#8A99AE"; font.pixelSize: 11 }
                        TextArea {
                            objectName: "secureStageData"
                            Layout.fillWidth: true; Layout.fillHeight: true
                            readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere
                            text: panel.phase === "decrypted" ? snapshot.plaintext || "" : panel.phase === "rejected" ? "校验未通过" : snapshot.encrypted ? (snapshot.wirePacket || "").slice(0, 128) + ((snapshot.wirePacket || "").length > 128 ? "…" : "") : snapshot.wirePacket || ""
                            font.family: panel.phase === "decrypted" || snapshot.encrypted === false ? "Microsoft YaHei UI" : "Consolas"
                            font.pixelSize: panel.phase === "decrypted" ? 16 : 13; color: panel.phase === "rejected" ? "#B65360" : "#3C5878"
                            padding: 0; background: Item {}
                        }
                        Text { visible: !!snapshot.tampered; Layout.fillWidth: true; text: "已改 1 位：" + snapshot.beforeByte + " → " + snapshot.afterByte; color: "#B65360"; font.pixelSize: 12 }
                    }
                }
                Text { visible: host.secureError.length > 0; Layout.fillWidth: true; text: host.secureError; color: "#B65360"; font.pixelSize: 12; wrapMode: Text.Wrap }
                RowLayout {
                    visible: panel.phase !== "idle"
                    Layout.fillWidth: true; spacing: 6
                    SoftButton {
                        objectName: "securePacketPeekButton"
                        text: "完整数据"; compact: true
                        onClicked: panel.openDetail(panel.phase === "intercepted" ? "observer" : snapshot.receivedPacket ? "receiver" : "sender")
                    }
                    SoftButton {
                        objectName: "secureTamperButton"; visible: panel.phase === "intercepted" && snapshot.encrypted
                        text: snapshot.tampered ? "撤销篡改" : "尝试篡改"; compact: true
                        enabled: panel.phase === "intercepted" && snapshot.encrypted
                        onClicked: snapshot.tampered ? host.secureChannel.restore() : host.tamperSecure()
                    }
                    SoftButton { objectName: "securePlayButton"; visible: panel.phase === "transit"; text: host.secureRunning ? "暂停" : "继续"; compact: true; onClicked: host.secureRunning = !host.secureRunning }
                    SoftButton { objectName: "secureChangeKeyButton"; visible: panel.phase === "rejected"; text: "调整密钥重试"; compact: true; onClicked: keyPopup.open() }
                    Item { Layout.fillWidth: true }
                }
                SoftButton { objectName: "secureFlowButton"; Layout.fillWidth: true; visible: !!snapshot.packetId && snapshot.encrypted && panel.phase !== "intercepted"; text: snapshot.receivedPacket ? "查看接收校验结构图" : "查看发送加密结构图"; compact: true; onClicked: panel.openFlow(snapshot.receivedPacket ? "receiver" : "sender") }
                SoftButton { objectName: "secureStepButton"; Layout.fillWidth: true; visible: panel.phase === "transit"; text: "下一跳 →"; selected: true; accent: "#DDECFB"; onClicked: host.advanceSecure() }
                SoftButton { objectName: "secureSendButton"; Layout.fillWidth: true; visible: panel.phase === "idle"; text: host.secureScenario === "plaintext" ? "发送明文 →" : "加密并发送 →"; selected: true; accent: "#DDECFB"; enabled: !host.secureBusy && messageInput.text.length > 0; onClicked: host.sendSecure() }
                SoftButton { objectName: "secureForwardButton"; Layout.fillWidth: true; visible: panel.phase === "intercepted"; text: snapshot.tampered ? "转发篡改后的包 →" : "原样转发 →"; selected: true; accent: "#DDECFB"; enabled: panel.phase === "intercepted"; onClicked: host.forwardSecure() }
                SoftButton { objectName: "secureDecryptButton"; Layout.fillWidth: true; visible: panel.phase === "received"; text: snapshot.encrypted ? "用接收端密钥解密 →" : "读取收到的消息 →"; selected: true; accent: "#DDECFB"; enabled: !!snapshot.receivedPacket; onClicked: host.decryptSecure() }
                SoftButton { objectName: "secureRestartButton"; Layout.fillWidth: true; visible: panel.phase === "decrypted" || panel.phase === "rejected"; text: "换条消息再试 →"; selected: true; accent: "#DDECFB"; onClicked: { host.clearSecurePacket(); messageInput.forceActiveFocus(); messageInput.selectAll() } }
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
            RowLayout {
                Layout.fillWidth: true
                Caption { text: "暂停观察的节点" }
                SoftComboBox {
                    id: observationChoice; objectName: "secureObserverChoice"; Layout.preferredWidth: 240
                    model: panel.candidates.length ? panel.candidates.map(id => "R" + id).concat(["不暂停"]) : ["无中间节点"]
                    currentIndex: 0; enabled: !host.secureBusy && panel.candidates.length > 0
                }
            }
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
                    Caption { visible: panel.detailKind === "observer" && snapshot.tampered; text: "篡改前的截获包" }
                    DataBox { visible: panel.detailKind === "observer" && snapshot.tampered; Layout.preferredHeight: 76; text: snapshot.capturedPacket || "" }
                    SoftButton { visible: snapshot.encrypted; text: panel.showPacketParts ? "收起包结构 ▴" : "解析包结构 ▾"; compact: true; onClicked: panel.showPacketParts = !panel.showPacketParts }
                    Caption { visible: snapshot.encrypted && panel.showPacketParts; text: "包内随机数 · 12 字节" }
                    DataBox { visible: snapshot.encrypted && panel.showPacketParts; Layout.preferredHeight: 40; text: packetParts.nonce || "" }
                    Caption { visible: snapshot.encrypted && panel.showPacketParts; text: "包内认证标签 · 16 字节" }
                    DataBox { visible: snapshot.encrypted && panel.showPacketParts; Layout.preferredHeight: 52; text: packetParts.tag || "" }
                    Caption { visible: snapshot.encrypted && panel.showPacketParts; text: "从本包解析出的密文字节（十六进制）" }
                    DataBox { visible: snapshot.encrypted && panel.showPacketParts; Layout.preferredHeight: 80; text: packetParts.ciphertext || "" }
                    Caption { visible: !!snapshot.capturedPacket && snapshot.encrypted; text: "独立验证仅使用你提供的密文和密钥，不读取发送原文。" }
                    SoftButton {
                        visible: !!snapshot.capturedPacket && snapshot.encrypted
                        text: "提供密钥，独立验证这份包"; compact: true; selected: true; accent: "#DDECFB"
                        onClicked: {
                            verificationPacket.text = panel.detailKind === "receiver" ? snapshot.receivedPacket || "" : panel.detailKind === "sender" ? snapshot.senderPacket || "" : snapshot.wirePacket || ""
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
