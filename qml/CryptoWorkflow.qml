import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: flow
    objectName: "secureFlowView"
    property var trace: ({})
    property string endpoint: "sender"
    property string selectedStep: "bytes"
    property bool aesExpanded: false
    property int aesStep: 0
    property int selectedByte: 0
    property int hashIndex: 0
    property string focusedField: ""
    signal blockRequested(int index)
    readonly property var aes: trace.aes || ({operations: [], roundKeys: []})
    readonly property var operations: aes.operations || []
    readonly property var operation: operations[aesStep] || ({kind:"key",round:0,input:"",output:""})
    readonly property var hashes: trace.hashSteps || []
    readonly property var hash: hashes[Math.min(hashIndex, hashes.length - 1)] || ({})
    readonly property bool accepted: !!trace.tagMatches
    readonly property var order: endpoint === "sender" ? ["bytes","counter","expand","aes","xor","cipher","params","hash","tag","packet"]
                                                      : ["packet","parse","counter","expand","params","hash","tag","verify","aes","xor","output"]
    readonly property var nodes: aesExpanded ? roundNodes() : endpoint === "sender" ? [
        {id:"plain",label:"现场消息",note:"UTF-8 文本",x:.02,y:.02},
        {id:"nonce",label:"随机数",note:"12 字节 · 公开传输",x:.36,y:.02},
        {id:"key",label:"通信密钥",note:"32 字节 · 本端持有",x:.70,y:.02},
        {id:"bytes",label:"编码与分块",note:"每块最多 16 字节",x:.02,y:.22},
        {id:"counter",label:"构造计数器",note:"随机数 + 块序号",x:.36,y:.22},
        {id:"expand",label:"密钥扩展",note:"生成 K₀ … K₁₄",x:.70,y:.22},
        {id:"xor",label:"⊕  异或",note:"明文 ⊕ 密钥流",x:.02,y:.42},
        {id:"aes",label:"AES-256",note:"14 轮 · 点击可展开",x:.36,y:.42},
        {id:"params",label:"认证辅助值",note:"AES 计算 H 与 M",x:.70,y:.42},
        {id:"cipher",label:"密文",note:"从此不再携带原文",x:.02,y:.62},
        {id:"hash",label:"GHASH",note:"处理密文与长度",x:.36,y:.62},
        {id:"tag",label:"认证标签",note:"GHASH ⊕ M",x:.70,y:.62},
        {id:"packet",label:"封装并发送",note:"随机数 + 标签 + 密文",x:.36,y:.82}
    ] : [
        {id:"packet",label:"实际收到的包",note:"只使用接收数据",x:.02,y:.02},
        {id:"key",label:"接收端密钥",note:"本次实际使用的密钥",x:.70,y:.02},
        {id:"parse",label:"解析包结构",note:"分离随机数 / 标签 / 密文",x:.02,y:.22},
        {id:"counter",label:"构造计数器",note:"使用收到的随机数",x:.36,y:.22},
        {id:"expand",label:"密钥扩展",note:"生成 K₀ … K₁₄",x:.70,y:.22},
        {id:"hash",label:"GHASH",note:"收到的密文与长度",x:.02,y:.42},
        {id:"aes",label:"AES-256",note:"同样生成密钥流",x:.36,y:.42},
        {id:"params",label:"认证辅助值",note:"重新计算 H 与 M",x:.70,y:.42},
        {id:"tag",label:"重算认证标签",note:"GHASH ⊕ M",x:.02,y:.62},
        {id:"verify",label:accepted ? "✓ 标签一致" : "× 标签不一致",note:accepted ? "校验闸门放行" : "停止 · 不输出原文",x:.36,y:.62},
        {id:"xor",label:"⊕  恢复字节",note:accepted ? "密文 ⊕ 密钥流" : "校验失败 · 不放行",x:.70,y:.62},
        {id:"output",label:accepted ? "输出原文" : "拒绝输出",note:accepted ? "恢复 UTF-8 消息" : "保持原文为空",x:.70,y:.82}
    ]

    function roundNodes() {
        const round = operation.round
        const ids = round === 0 ? ["key"] : round === 14 ? ["sub","shift","key"] : ["sub","shift","mix","key"]
        let rows = [{id:"roundInput",label:"本轮输入",note:"16 字节状态矩阵",x:.36,y:.02}]
        for (let i = 0; i < ids.length; ++i)
            rows.push({id:ids[i],label:operationName(ids[i]),note:operationEnglish(ids[i]),x:.36,y:.22 + i * .20})
        rows.push({id:"roundKey",label:"轮密钥 K" + round,note:"来自密钥扩展",x:.70,y:.22 + (ids.length - 1) * .20})
        return rows
    }
    function operationName(kind) { return ({sub:"字节替换",shift:"行移位",mix:"列混合",key:"加轮密钥"})[kind] || kind }
    function operationEnglish(kind) { return ({sub:"SubBytes",shift:"ShiftRows",mix:"MixColumns",key:"AddRoundKey"})[kind] || "" }
    function selectNode(id) {
        focusedField = ""
        if (aesExpanded && ["sub","shift","mix","key"].indexOf(id) >= 0) {
            const index = operations.findIndex(op => op.round === operation.round && op.kind === id)
            if (index >= 0) { aesStep = index; selectedStep = id }
        } else selectedStep = id
    }
    function followSource(row) {
        if (row.blockIndex !== undefined) blockRequested(row.blockIndex)
        if (row.aesIndex !== undefined) {
            if (row.aesIndex < 0) { aesExpanded=false; selectedStep="counter" }
            else { aesStep=row.aesIndex; selectedStep=operation.kind }
        } else {
            aesExpanded=false; selectedStep=row.sourceStep
            if (row.hashIndex !== undefined) hashIndex=row.hashIndex
        }
        focusedField=row.sourceField || ""
    }
    function counterMarkup(value) {
        return '<span style="color:#397FBB">' + spaced(value.slice(0,24)) + '</span> <span style="color:#AC742C">' + spaced(value.slice(24)) + '</span>'
    }
    function resetDetailView() { focusedField=""; detailScroll.contentY=0 }
    function moveStep(delta) {
        if (aesExpanded) { aesStep = Math.max(0, Math.min(operations.length - 1, aesStep + delta)); selectedStep = operation.kind; return }
        let index = order.indexOf(selectedStep)
        if (index < 0) index = 0
        selectedStep = order[Math.max(0, Math.min(order.length - 1, index + delta))]
    }
    function title() {
        if (aesExpanded) return "第 " + operation.round + " 轮 · " + (selectedStep === "roundKey" ? "轮密钥" : selectedStep === "roundInput" ? "本轮输入" : operationName(operation.kind))
        const node = nodes.find(item => item.id === selectedStep)
        return node ? node.label : "选择一道工序"
    }
    function explanation() {
        if (aesExpanded) {
            if (selectedStep === "roundInput") return "做什么：取本轮开始时的 16 个字节，按列放进 4 × 4 矩阵：前 4 个字节是第一列，随后依次填入其他列。\n\n数据从哪来：初始轮接收本块计数器；第 1 至 14 轮接收上一轮的输出。后续四道工序会继续改变这个矩阵。"
            if (selectedStep === "roundKey") return "做什么：从密钥扩展结果中取出第 " + operation.round + " 轮使用的 16 字节密钥 K" + operation.round + "。\n\n为什么：原始密钥是 32 字节，而状态矩阵是 16 字节。扩展算法为各轮准备不同的 16 字节材料。\n\n接下来：这一份轮密钥与状态矩阵逐字节异或；它由通信密钥推导，不随包传输。"
            return ({sub:"做什么：把矩阵中的每个字节送入固定的 S-box 替换表，查出一个新字节。例如十六进制 ab 用 a 定位行、b 定位列。\n\n为什么：替换关系不是简单的线性运算，使输入和输出之间更难推算。字节位置保持不变，变化的是数值。\n\n怎么看：点击输出格，查看该格的查表输入与结果；输出矩阵接着交给行移位。",
                shift:"做什么：把第 0、1、2、3 行分别循环左移 0、1、2、3 格。移出左边的字节会回到右边。\n\n为什么：字节的数值保持不变，但会换到其他列；下一步列混合就能让原来不同列的字节互相影响。\n\n怎么看：点击输出格，输入矩阵会标出它原来的位置。下方坐标按 [行,列] 表示，从 0 开始。",
                mix:"做什么：同一列的 4 个字节一起参与运算，生成新的 4 个字节。每个输出都是这 4 个输入按指定系数相乘后再异或的结果。\n\n为什么：使一个字节的改动扩散到同列的多个字节，再配合行移位传到其他列。\n\n怎么看：点击输出格，蓝色标出参与计算的输入列，下面列出公式。乘法采用 GF(2⁸) 的字节规则，不是普通整数乘法。第 14 轮省略这一步。",
                key:"做什么：把状态矩阵和本轮的 16 字节密钥逐字节异或。⊕ 表示逐位比较：相同得 0，不同得 1。\n\n为什么：这一步把通信密钥带入状态；同样的计数器配不同密钥，会生成不同密钥流。\n\n顺序：初始轮先执行一次加轮密钥，之后再进入 14 轮运算。点击格子可核对本次“输入 ⊕ 轮密钥 = 输出”。"})[operation.kind] || ""
        }
        return ({plain:"这就是点击发送时的消息内容。先把文字编码成字节，才可以进入加密计算。\n\n这份内容对应已发送的包；后来修改输入框，不会改变本次消息。",
            bytes:"做什么：把文字编码为 UTF-8 字节，再按最多 16 字节划分消息块。一个汉字通常会占多个字节，所以“字数”与“字节数”不同。\n\n为什么：AES 每次处理 16 字节计数器，产生 16 字节密钥流，再与同一块消息异或。GCM 的消息末块可以不足 16 字节，取等长的密钥流即可，不填充明文。\n\n下方显示当前选中块，底部可切换其他块。",
            nonce:"做什么：发送时生成 12 字节随机数，接收端从包里取出同一个值。\n\n为什么：它参与生成本次计数器和认证辅助值，让同一密钥下重复发送相同消息时得到不同的密文。同一密钥下不能重复使用同一个随机数。\n\n随机数公开随包传输；它本身不能代替密钥解密。",
            counter:"做什么：把 12 字节随机数与 4 字节计数值拼成 16 字节输入块。J₀ 的计数值为 1，用于生成认证遮蔽值；消息块从 2 开始逐块递增。\n\n本次：第 " + ((trace.blockIndex || 0)+1) + " 块使用计数值 " + ((trace.blockIndex || 0)+2) + "。\n\n为什么：不同消息块需要不同密钥流。将本块计数器送入 AES，发送与接收就能生成对应的同一份密钥流。",
            key:"做什么：使用本端持有的 32 字节（256 位）通信密钥。发送端用它生成密文，接收端用自己的密钥校验并恢复消息。\n\n双方预先共享相同密钥。密钥不放进传输包，中间节点只能看到公开包数据。\n\n这里展示的是本次实际计算时使用的密钥；修改后续设置不会改写这次计算。",
            expand:"做什么：对 32 字节密钥按 AES-256 的扩展规则进行字节替换、移位和异或，推导出 K₀ 至 K₁₄ 共 15 份轮密钥，每份 16 字节。\n\n为什么：每一轮都需要与 16 字节状态矩阵异或的密钥材料。K₀ 用于初始轮，K₁ 至 K₁₄ 用于后续 14 轮。\n\n点击 AES 工位展开后，可在“轮密钥”节点查看各轮实际使用的值。",
            aes:"做什么：把本块 16 字节计数器放进 4 × 4 状态矩阵，先加初始轮密钥，再进行 14 轮字节替换、行移位、列混合和加轮密钥；最后一轮没有列混合。\n\n得到什么：输出 16 字节密钥流。它再与消息字节异或，才形成密文。\n\n接收端对同一计数器也执行正向 AES，得到同一密钥流。点击下面的按钮可逐轮查看实际矩阵变化。",
            xor:accepted ? endpoint === "sender" ? "做什么：将本块明文字节与对应密钥流逐字节异或。⊕ 的逐位规则是相同得 0、不同得 1。\n\n得到什么：与明文等长的密文。末块不足 16 字节时，只取等长部分的密钥流。\n\n为什么可以恢复：A ⊕ B ⊕ B = A。接收端拿同一密钥流再异或一次，就能还原这些明文字节。" : "做什么：校验通过后，把收到的本块密文与对应密钥流逐字节异或。\n\n为什么能恢复：发送时“明文 ⊕ 密钥流 = 密文”，因此“密文 ⊕ 同一密钥流 = 明文”。末块只使用与密文等长的密钥流。\n\n得到什么：原始消息字节，之后按 UTF-8 解码成文字。" : "停止原因：包内标签与接收端重算标签不一致，数据没有通过认证。\n\n此时即使能进行异或运算，得到的字节也不可信；应用保持这道工序关闭，不向用户交付未经认证的原文。\n\n可以返回标签校验工序查看差异，检查途中是否篡改或两端密钥是否一致。",
            cipher:"这些是本块实际产生的密文字节，来自“明文 ⊕ 密钥流”。\n\n接下来：密文参与 GHASH 认证计算，并与随机数、标签一起封装发送。整份包沿网络路径逐跳转发。\n\n中间节点可以截获这些字节；恢复消息还需要通信密钥。",
            params:"做什么：使用本端密钥 K，额外进行两次 AES 计算：H = AES(K, 全零块)，M = AES(K, J₀)。\n\n分别用在哪：H 是 GHASH 的乘法参数，让认证结果依赖密钥；M 与 GHASH 最终值异或，生成包里的标签。\n\nH 和 M 在两端各自计算，不随包传输。接收端使用包中随机数构造 J₀，密钥相同就能算出相同辅助值。",
            hash:"做什么：累计值 Y 从全零开始。逐块计算“新 Y = (旧 Y ⊕ 本块) × H”，这里的乘法采用 GF(2¹²⁸) 规则，把 128 位当作二进制多项式运算。\n\n末尾怎么处理：不足 16 字节的密文块仅在这一步补零，再加入长度块。本实验没有额外认证数据（AAD），长度块前 8 字节为零，后 8 字节记录密文的位数。\n\n为什么：让所有密文块及长度共同影响认证结果。改变密文会影响累计值，进而影响标签。下方可以逐块查看累计过程。",
            tag:"做什么：将 GHASH 的 16 字节最终值与遮蔽值 M 逐字节异或：标签 T = GHASH ⊕ M。\n\n发送端：把这个标签随密文发送。接收端：用实际收到的密文、本端密钥和包内随机数独立重算。\n\n为什么：下一步比较“包内标签”与“重算标签”，判断数据是否通过认证；只有通过后才输出原文。",
            packet:endpoint === "sender" ? "做什么：把本次随机数、认证标签和完整密文封装为一个包，再沿选定路径发送。\n\n下方显示封装后的完整 raw 数据。Base64 只是把二进制写成可复制的文字；加密已经由 AES-GCM 完成。\n\n包内没有通信密钥或明文。" : "这里显示接收端实际拿到的完整 raw 数据，保持收到时的内容，可直接选中复制。\n\nBase64 是二进制包的文字展示形式。当前步骤先保留整份包，下一步“解析包结构”才将它拆成各字段。\n\n后续校验和恢复消息都以这份接收数据与接收端密钥为输入。",
            parse:"做什么：先将收到的 Base64 文字还原为二进制，再按本实验的包格式拆分：前 12 字节是随机数，接着 16 字节是标签，其余全部是密文。\n\n为什么：随机数送入计数器工序，密文送入 GHASH 和恢复工序，包内标签留给校验工序进行比较。\n\n这一操作只识别字段，无需密钥。下面的密文来自完整收到的包，不是发送端草稿。",
            verify:accepted ? "比较什么：包里携带的标签，与接收端根据收到的密文、随机数和本端密钥重新计算的标签。\n\n本次结果：16 个字节全部相同，认证通过，绿色格子表示一致。\n\n接下来：允许用密钥流恢复消息字节，再交付原文。认证关注的是包和密钥是否匹配，不依赖发送端的原文。" : "比较什么：包里携带的标签，与接收端根据收到的密文、随机数和本端密钥重新计算的标签。\n\n本次结果：至少一个字节不同，红色格子标出差异。密文被篡改、随机数或标签被修改、接收密钥错误，都可能造成不一致；单靠标签差异不能判断是哪一种。\n\n接下来：关闭原文输出。篡改实验中，只翻转密文 1 位也会使重算标签无法匹配原标签。",
            output:accepted ? "做什么：将通过认证后恢复的各块字节按原顺序拼接，再用 UTF-8 解码成文字。\n\n得到什么：下面就是接收端根据实际收到的包和本端密钥恢复的整条消息。\n\n至此才能把消息交给用户；更改发送端草稿不会改变已收到的内容。" : "本次没有原文可交付：标签校验失败，接收端拒绝输出未经认证的内容。\n\n输出框保持为空。请返回校验工序查看标签差异，或调整接收端密钥后对同一份收到的包重试。"})[selectedStep] || "点击结构图中的工序查看本次数据。"
    }
    function fields() {
        const nonceSource = endpoint==="sender" ? "nonce" : "parse"
        const cipherSource = endpoint==="sender" ? "cipher" : "parse"
        if (aesExpanded) {
            const first=operations.findIndex(op=>op.round===operation.round)
            return selectedStep === "roundKey" ? [{name:"轮密钥 K" + operation.round,value:(aes.roundKeys || [])[operation.round] || "",origin:"来自「密钥扩展」· 第 " + operation.round + " 轮",sourceStep:"expand",sourceField:"expandedKeys"}]
                : selectedStep === "roundInput" ? [{name:"本轮初始状态",value:(operations[first] || {}).input || "",origin:first===0?"来自「构造计数器」· 本块 AES 输入":"来自上一轮最后一道「加轮密钥」的输出",aesIndex:first-1}] : []
        }
        switch (selectedStep) {
        case "plain": return [{id:"plain",name:"本次发送消息",value:trace.plaintext || "",text:true,origin:"来自发送时的消息输入快照"}]
        case "bytes": return [{id:"plainBlock",name:"本块明文字节",value:trace.selectedPlain || "",origin:"来自「现场消息」· UTF-8 编码后第 " + ((trace.blockIndex || 0)+1) + " 块",sourceStep:"plain",sourceField:"plain"},{name:"UTF-8 总字节数",value:String(trace.messageBytes || 0),text:true,origin:"本地统计编码后的字节数"}]
        case "nonce": return [{id:"nonce",name:"随机数 · 12 字节",value:trace.nonce || "",origin:"发送时由随机数生成器产生"}]
        case "key": return [{id:"key",name:"本端密钥 · 32 字节",value:trace.key || "",origin:"来自本端通信密钥设置 · 本次计算快照"}]
        case "expand": return [{id:"key",name:"原始密钥",value:trace.key || "",origin:"来自「"+(endpoint==="sender"?"通信密钥":"接收端密钥")+"」",sourceStep:"key",sourceField:"key"},{id:"expandedKeys",name:"当前 AES 轮密钥 K"+operation.round,value:(aes.roundKeys || [])[operation.round] || "",origin:"本工序扩展结果 · 与当前 AES 第 "+operation.round+" 轮对应"},{name:"最终轮 K₁₄",value:(aes.roundKeys || [])[14] || "",origin:"本工序扩展结果"}]
        case "counter": return [{id:"nonceInput",name:"输入 · 随机数（12 字节）",value:trace.nonce || "",origin:endpoint==="sender"?"来自「随机数」· 本次生成的 12 字节":"来自「解析包结构」· 还原包的第 1–12 字节",sourceStep:nonceSource,sourceField:"nonce"},{id:"j0",name:"拼接结果 · J₀（认证计数器）",value:trace.j0 || "",parts:true,origin:"本地追加 4 字节整数 1：00 00 00 01"},{id:"counter",name:"拼接结果 · 本块 AES 输入",value:trace.counter || "",parts:true,origin:"本地追加块序号 + 1：第 " + ((trace.blockIndex || 0)+1) + " 块 → " + ((trace.blockIndex || 0)+2)}]
        case "aes": return [{id:"counterInput",name:"输入 · 本块计数器",value:aes.input || "",origin:"来自「构造计数器」· 本块 AES 输入",sourceStep:"counter",sourceField:"counter"},{name:"使用的轮密钥",value:"K₀ … K₁₄（共 15 份）",text:true,origin:"来自「密钥扩展」",sourceStep:"expand",sourceField:"expandedKeys"},{id:"stream",name:"输出 · 密钥流",value:aes.output || "",origin:"本工序 14 轮运算的最终输出 → 异或工序"}]
        case "xor": return accepted ? [{name:endpoint === "sender" ? "输入 · 明文字节" : "输入 · 密文字节",value:endpoint === "sender" ? trace.selectedPlain || "" : trace.selectedCipher || "",origin:endpoint==="sender"?"来自「编码与分块」· 当前块":"来自「解析包结构」· 密文第 " + ((trace.blockIndex || 0)+1) + " 块",sourceStep:endpoint==="sender"?"bytes":"parse",sourceField:endpoint==="sender"?"plainBlock":"cipher"},{name:"输入 · 密钥流（取等长部分）",value:(aes.output || "").slice(0,(trace.selectedCipher || "").length),origin:"来自「AES-256」· 最终输出",sourceStep:"aes",sourceField:"stream"},{id:"xorOutput",name:endpoint === "sender" ? "异或结果 · 密文" : "异或结果 · 明文",value:endpoint === "sender" ? trace.selectedCipher || "" : trace.selectedPlain || "",origin:"上述两个输入逐字节异或的结果"}] : []
        case "cipher": return [{id:"cipher",name:"本块实际密文",value:trace.selectedCipher || "",origin:"来自「异或」· 本块输出",sourceStep:"xor",sourceField:"xorOutput"}]
        case "params": return [{name:"输入 · 本端密钥 K",value:trace.key || "",origin:"来自「"+(endpoint==="sender"?"通信密钥":"接收端密钥")+"」",sourceStep:"key",sourceField:"key"},{id:"h",name:"输出 · H = AES(K, 0¹²⁸)",value:trace.h || "",origin:"本地规定输入：16 字节全零块 → 用于 GHASH"},{name:"输入 · J₀",value:trace.j0 || "",origin:"来自「构造计数器」· 计数值为 1",sourceStep:"counter",sourceField:"j0"},{id:"mask",name:"输出 · M = AES(K, J₀)",value:trace.mask || "",origin:"对上述 J₀ 执行 AES → 用于生成标签"}]
        case "hash": return [{name:"累计值 Y（输入）",value:hash.before || "",origin:hashIndex===0?"本地初始化：累计值为全零":"来自 GHASH 上一工序的「新 Y」",sourceStep:hashIndex>0?"hash":"",hashIndex:Math.max(0,hashIndex-1),sourceField:"hashOutput"},{name:hash.kind === "length" ? "长度块（AAD 长度为 0）" : "本步密文块（末块仅在 GHASH 补零）",value:hash.input || "",origin:hash.kind==="length"?"本地按完整密文的位数构造长度块":"来自「" + (endpoint==="sender"?"密文":"解析包结构") + "」· 密文第 " + (hashIndex+1) + " 块",sourceStep:hash.kind==="length"?"":cipherSource,blockIndex:hash.kind==="cipher"?hashIndex:undefined,sourceField:"cipher"},{name:"乘法参数 H",value:trace.h || "",origin:"来自「认证辅助值」· AES(K, 全零块)",sourceStep:"params",sourceField:"h"},{name:"异或后 · Y ⊕ 输入块",value:hash.xor || "",origin:"本工序：上述累计值与输入块异或"},{id:"hashOutput",name:"有限域乘 H 后 · 新 Y",value:hash.output || "",origin:"本工序输出 → 下一 GHASH 工序或最终标签"}]
        case "tag": return [{name:"GHASH 最终值",value:trace.ghash || "",origin:"来自「GHASH」· 最后一个长度块工序",sourceStep:"hash",hashIndex:hashes.length-1,sourceField:"hashOutput"},{name:"遮蔽值 M",value:trace.mask || "",origin:"来自「认证辅助值」· AES(K, J₀)",sourceStep:"params",sourceField:"mask"},{id:"computedTag",name:"两者异或 · 本端计算的标签",value:trace.computedTag || "",origin:"上述两个输入异或 → 标签校验 / 封装"}]
        case "packet": return [{name:endpoint === "sender" ? "完整发送数据 · raw（Base64）" : "完整接收数据 · raw（Base64）",value:trace.rawPacket || "",raw:true,origin:endpoint==="sender"?"由随机数、认证标签、完整密文封装而成":"来自网络沿路径逐跳转发的实际数据"}]
        case "parse": return [{id:"nonce",name:"随机数 · 12 字节",value:trace.nonce || "",origin:"来自「实际收到的包」· Base64 还原后第 1–12 字节",sourceStep:"packet"},{id:"receivedTag",name:"包里的标签 · 16 字节",value:trace.receivedTag || "",origin:"来自「实际收到的包」· 还原后第 13–28 字节",sourceStep:"packet"},{id:"cipher",name:"完整密文 · " + (trace.messageBytes || 0) + " 字节",value:trace.ciphertext || "",origin:"来自「实际收到的包」· 还原后第 29 字节起",sourceStep:"packet"}]
        default: return []
        }
    }
    function byteExplanation() {
        const i = selectedByte, row = i % 4, col = Math.floor(i / 4)
        const input = (operation.input || "").slice(i * 2,i * 2 + 2), output = (operation.output || "").slice(i * 2,i * 2 + 2)
        if (operation.kind === "sub") return "S-box 查表：第 " + input[0] + " 行、第 " + input[1] + " 列 → " + output
        if (operation.kind === "shift") return "输出 [" + row + "," + col + "] 来自输入 [" + row + "," + ((col + row) % 4) + "]"
        if (operation.kind === "key") return input + " ⊕ " + (operation.roundKey || "").slice(i * 2,i * 2 + 2) + " = " + output
        const inputColumn = (operation.input || "").slice(col * 8,col * 8 + 8).match(/.{2}/g) || []
        const coefficients = [[2,3,1,1],[1,2,3,1],[1,1,2,3],[3,1,1,2]][row]
        const terms = inputColumn.map((value,index) => coefficients[index] + "·" + value)
        const products = inputColumn.map((value,index) => multiplyByte(parseInt(value,16),coefficients[index]).toString(16).padStart(2,"0"))
        return "输入第 " + col + " 列的四个字节共同影响本格。\n" + terms.join(" ⊕ ") + "\n= " + products.join(" ⊕ ") + " = " + output + "\n乘法在 GF(2⁸) 中进行。"
    }
    function multiplyByte(a,b) {
        let result = 0
        for (let bit=0; bit<8; ++bit) { if (b&1) result ^= a; a=((a<<1)^((a&128)?0x11b:0))&255; b>>=1 }
        return result
    }
    function inputByteIndex() {
        const row=selectedByte%4, col=Math.floor(selectedByte/4)
        return operation.kind==="shift" ? ((col+row)%4)*4+row : selectedByte
    }
    function spaced(value) { return (value || "").match(/.{1,2}/g)?.join(" ") || "" }

    RowLayout {
        anchors.fill: parent; spacing: 18
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 8
            RowLayout {
                Layout.fillWidth: true
                SoftButton { visible: aesExpanded; text: "← 返回总结构"; compact: true; onClicked: { aesExpanded = false; selectedStep = "aes" } }
                Text { visible: !aesExpanded; text: endpoint === "sender" ? "加密与认证结构" : "接收与校验结构"; color:"#5A728F"; font.pixelSize:13 }
                Item { Layout.fillWidth: true }
                SoftComboBox {
                    visible: aesExpanded; Layout.preferredWidth: 132
                    model: Array.from({length:15},(_,i) => i === 0 ? "初始轮 0" : "第 " + i + " 轮")
                    currentIndex: operation.round
                    onActivated: index => { aesStep = index === 0 ? 0 : 1 + (index - 1) * 4; selectedStep = operations[aesStep].kind }
                }
            }
            Rectangle {
                id: chart
                objectName: "flowStructureChart"
                Layout.fillWidth: true; Layout.fillHeight: true
                radius: 16; color: "#F2F6FC"; border.color: "#E1E9F4"
                Canvas {
                    id: arrows; anchors.fill: parent
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    Connections {
                        target: flow
                        function onNodesChanged() { arrows.requestPaint() }
                        function onSelectedStepChanged() { arrows.requestPaint() }
                    }
                    onPaint: {
                        const ctx = getContext("2d"); ctx.reset()
                        const w = width, h = height
                        function path(points) {
                            ctx.strokeStyle = "#A3B5CD"; ctx.lineWidth = 1.7; ctx.lineJoin = "round"
                            ctx.beginPath(); ctx.moveTo(points[0][0]*w, points[0][1]*h)
                            for (let i=1;i<points.length;i++) ctx.lineTo(points[i][0]*w,points[i][1]*h)
                            ctx.stroke()
                            const end=points[points.length-1], prev=points[points.length-2]
                            const angle=Math.atan2((end[1]-prev[1])*h,(end[0]-prev[0])*w)
                            ctx.fillStyle=ctx.strokeStyle;ctx.beginPath();ctx.moveTo(end[0]*w,end[1]*h)
                            ctx.lineTo(end[0]*w-6*Math.cos(angle-.5),end[1]*h-6*Math.sin(angle-.5))
                            ctx.lineTo(end[0]*w-6*Math.cos(angle+.5),end[1]*h-6*Math.sin(angle+.5));ctx.closePath();ctx.fill()
                        }
                        if (flow.aesExpanded) {
                            const count=flow.operation.round===0?1:flow.operation.round===14?3:4
                            for(let i=0;i<count;i++) path([[.50,.16+i*.20],[.50,.22+i*.20]])
                            path([[.70,.29+(count-1)*.20],[.64,.29+(count-1)*.20]])
                        } else if(flow.endpoint==="sender") {
                            for(const x of [.16,.50,.84]) path([[x,.16],[x,.22]])
                            for(const x of [.16,.50,.84]) path([[x,.36],[x,.42]])
                            path([[.70,.29],[.67,.29],[.67,.49],[.64,.49]])
                            path([[.36,.49],[.30,.49]])
                            path([[.16,.56],[.16,.62]])
                            path([[.30,.69],[.36,.69]])
                            path([[.70,.49],[.67,.49],[.67,.59],[.50,.59],[.50,.62]])
                            path([[.64,.69],[.70,.69]])
                            path([[.84,.56],[.84,.62]])
                            path([[.16,.76],[.16,.89],[.36,.89]])
                            path([[.84,.76],[.84,.89],[.64,.89]])
                        } else {
                            path([[.16,.16],[.16,.22]]);path([[.84,.16],[.84,.22]])
                            path([[.30,.29],[.36,.29]]);path([[.50,.36],[.50,.42]])
                            path([[.70,.29],[.67,.29],[.67,.49],[.64,.49]])
                            path([[.84,.36],[.84,.42]]);path([[.16,.36],[.16,.42]])
                            path([[.70,.49],[.67,.49],[.67,.39],[.32,.39],[.32,.49],[.30,.49]])
                            path([[.16,.56],[.16,.62]])
                            path([[.84,.56],[.84,.59],[.32,.59],[.32,.69],[.30,.69]])
                            path([[.30,.69],[.36,.69]])
                            path([[.30,.29],[.32,.29],[.32,.57],[.50,.57],[.50,.62]])
                            path([[.64,.49],[.67,.49],[.67,.69],[.70,.69]])
                            path([[.64,.69],[.70,.69]])
                            path([[.84,.76],[.84,.82]])
                        }
                    }
                }
                Repeater {
                    model: flow.nodes
                    delegate: Rectangle {
                        required property var modelData
                        objectName: "flowNode_" + modelData.id
                        x: modelData.x * chart.width; y: modelData.y * chart.height
                        width: chart.width * .28; height: chart.height * .14
                        radius: 11
                        readonly property bool active: modelData.id === flow.selectedStep
                        readonly property bool gate: !flow.aesExpanded && modelData.id === "verify"
                        color: gate ? flow.accepted ? "#E5F5EE" : "#FFF0F1" : active ? "#E1EEFC" : nodeMouse.containsMouse ? "#F9FBFF" : "#FFFFFF"
                        border.width: active ? 2 : 1
                        border.color: gate ? flow.accepted ? "#6DB59D" : "#CD7C88" : active ? "#6B9ED3" : "#D5E0EF"
                        Column {
                            anchors.centerIn: parent; width: parent.width - 8; spacing: chart.height < 420 ? 2 : 5
                            Text { width: parent.width; text: modelData.label; horizontalAlignment: Text.AlignHCenter; color: gate && !flow.accepted ? "#B65360" : active ? "#2A629A" : "#425E7F"; font.pixelSize: chart.width < 550 ? 12 : 14; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { width: parent.width; text: modelData.note; horizontalAlignment: Text.AlignHCenter; color:"#8B9DB3"; font.pixelSize: chart.width < 550 ? 9 : 10; elide: Text.ElideRight }
                        }
                        MouseArea { id: nodeMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: flow.selectNode(modelData.id) }
                    }
                }
            }
            Text { Layout.fillWidth: true; text: aesExpanded ? "矩阵按列装入字节 · 点击方格查看计算 · 第 14 轮没有列混合" : "点击任一工序查看本次数据；AES 工位还可以展开轮运算。"; color:"#8B9CB1"; font.pixelSize:11; wrapMode: Text.Wrap }
        }
        Rectangle {
            Layout.preferredWidth: Math.max(290, Math.min(360, flow.width * .36)); Layout.fillHeight: true
            radius:16; color:"#FFFFFF"; border.color:"#DFE8F3"
            Flickable {
                id: detailScroll
                objectName:"flowDetailsScroll"
                anchors.fill: parent; anchors.margins:18; clip:true
                contentHeight: details.implicitHeight; boundsBehavior:Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                Connections {
                    target:flow
                    function onSelectedStepChanged() { detailScroll.contentY=0 }
                    function onAesStepChanged() { detailScroll.contentY=0 }
                    function onHashIndexChanged() { detailScroll.contentY=0 }
                    function onFocusedFieldChanged() {
                        Qt.callLater(() => {
                            for (let i=0;i<fieldRepeater.count;++i) {
                                const item=fieldRepeater.itemAt(i)
                                if (item && item.modelData.id===flow.focusedField && flow.focusedField)
                                    detailScroll.contentY=Math.max(0,Math.min(item.y-12,details.implicitHeight-detailScroll.height))
                            }
                        })
                    }
                }
                ColumnLayout {
                    id: details; width: parent.width; spacing:12
                    Text { Layout.fillWidth:true; text:flow.title(); color:"#2B4E74"; font.pixelSize:18; font.weight:Font.DemiBold; wrapMode:Text.Wrap }
                    SourceLink {
                        objectName:"flowAesInputSource"; visible:flow.aesExpanded && flow.selectedStep!=="roundKey" && flow.selectedStep!=="roundInput"; Layout.fillWidth:true
                        descriptor:({origin:flow.aesStep===0?"输入来自「构造计数器」· 本块 AES 输入":"输入来自第 " + ((flow.operations[flow.aesStep-1] || {}).round || 0) + " 轮「" + flow.operationName((flow.operations[flow.aesStep-1] || {}).kind) + "」的输出",aesIndex:flow.aesStep-1,sourceField:"counter"})
                        onFollowed:row=>flow.followSource(row)
                    }
                    Text { objectName:"flowStepExplanation"; visible:flow.aesExpanded; Layout.fillWidth:true; text:flow.explanation(); color:"#5F748E"; font.pixelSize:12; lineHeight:1.3; wrapMode:Text.Wrap }
                    Text { visible:!flow.aesExpanded && flow.selectedStep==="counter"; Layout.fillWidth:true; text:'<font color="#397FBB">蓝色：上一步的 12 字节随机数</font><br><font color="#AC742C">黄色：本地追加的 4 字节计数值</font>'; font.pixelSize:11; lineHeight:1.3; textFormat:Text.RichText }
                    SoftButton { objectName:"flowExpandAesButton"; visible:!flow.aesExpanded && flow.selectedStep==="aes"; text:"展开 AES 轮运算 →"; compact:true; selected:true; accent:"#DDECFB"; onClicked:{ flow.aesExpanded=true; flow.aesStep=1; flow.selectedStep="sub" } }
                    RowLayout {
                        visible:flow.aesExpanded && flow.selectedStep!=="roundKey" && flow.selectedStep!=="roundInput"
                        Layout.fillWidth:true; spacing:10
                        ColumnLayout {
                            Layout.fillWidth:true
                            Text { text:"输入矩阵"; color:"#8A9AAF"; font.pixelSize:11 }
                            ByteMatrix {
                                objectName:"flowInputMatrix"; Layout.fillWidth:true; hexData:flow.operation.input || ""
                                selected:flow.inputByteIndex()
                                dependencies:flow.operation.kind==="mix" ? [0,1,2,3].map(i=>Math.floor(flow.selectedByte/4)*4+i) : []
                                onPicked:index=> { const row=index%4,col=Math.floor(index/4); flow.selectedByte=flow.operation.kind==="shift" ? ((col-row+4)%4)*4+row : index }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth:true
                            Text { text:"输出矩阵"; color:"#8A9AAF"; font.pixelSize:11 }
                            ByteMatrix { objectName:"flowOutputMatrix"; Layout.fillWidth:true; hexData:flow.operation.output || ""; compareHex:flow.operation.input || ""; selected:flow.selectedByte; onPicked:index=>flow.selectedByte=index }
                        }
                    }
                    Text { visible:flow.aesExpanded && flow.selectedStep!=="roundKey" && flow.selectedStep!=="roundInput"; Layout.fillWidth:true; text:flow.byteExplanation(); color:"#557CA8"; font.pixelSize:12; wrapMode:Text.Wrap; font.family:"Consolas" }
                    Text { visible:flow.aesExpanded && flow.operation.kind==="key" && flow.selectedStep!=="roundKey" && flow.selectedStep!=="roundInput"; Layout.fillWidth:true; text:"本轮密钥：\n"+flow.spaced(flow.operation.roundKey); color:"#8295AD"; font.pixelSize:11; wrapMode:Text.WrapAnywhere; font.family:"Consolas" }
                    RowLayout {
                        visible:!flow.aesExpanded && flow.selectedStep==="hash"
                        Layout.fillWidth:true
                        Text { text:"GHASH 工序"; color:"#7E91A9"; font.pixelSize:11 }
                        SoftComboBox { Layout.fillWidth:true; model:flow.hashes.map((row,index)=>row.kind==="length"?"最后：长度块":"密文块 "+(index+1)); currentIndex:flow.hashIndex; onActivated:index=>flow.hashIndex=index }
                    }
                    Repeater {
                        id: fieldRepeater
                        model:flow.fields()
                        delegate: ColumnLayout {
                            required property var modelData
                            required property int index
                            Layout.fillWidth:true; spacing:5
                            Text { Layout.fillWidth:true; text:modelData.name; color:"#8B9DB2"; font.pixelSize:11; wrapMode:Text.Wrap }
                            SourceLink { objectName:"flowOrigin_"+(modelData.id || index); Layout.fillWidth:true; visible:!!modelData.origin; descriptor:modelData; onFollowed:row=>flow.followSource(row) }
                            TextArea {
                                objectName:modelData.raw?"flowRawPacket":"flowValue_"+(modelData.id || index)
                                property string rawValue:modelData.value
                                property bool highlighted:!!modelData.id && modelData.id===flow.focusedField
                                Layout.fillWidth:true; Layout.minimumHeight:modelData.raw?140:0; readOnly:true; selectByMouse:true; wrapMode:TextEdit.WrapAnywhere
                                textFormat:modelData.parts?TextEdit.RichText:TextEdit.PlainText
                                text:modelData.parts?flow.counterMarkup(modelData.value):modelData.text || modelData.raw?modelData.value:flow.spaced(modelData.value)
                                font.family:modelData.text?"Microsoft YaHei UI":"Consolas"; font.pixelSize:12; color:modelData.id==="nonceInput"?"#397FBB":"#47688E"; padding:10
                                background:Rectangle {radius:9;color:parent.highlighted?"#E6F0FC":"#F3F7FC";border.color:parent.highlighted?"#75A5D4":"transparent"}
                            }
                        }
                    }
                    RowLayout {
                        visible:!flow.aesExpanded && flow.selectedStep==="packet" && flow.endpoint==="sender"; Layout.fillWidth:true;spacing:10
                        SourceLink { descriptor:({origin:"随机数",sourceStep:"nonce",sourceField:"nonce"});onFollowed:row=>flow.followSource(row) }
                        SourceLink { descriptor:({origin:"认证标签",sourceStep:"tag",sourceField:"computedTag"});onFollowed:row=>flow.followSource(row) }
                        SourceLink { descriptor:({origin:"全部密文块",sourceStep:"cipher",sourceField:"cipher"});onFollowed:row=>flow.followSource(row) }
                        Item { Layout.fillWidth:true }
                    }
                    SourceLink { visible:!flow.aesExpanded && flow.selectedStep==="xor" && flow.endpoint==="receiver";Layout.fillWidth:true;descriptor:({origin:flow.accepted?"放行依据：来自「标签校验」· 16 字节全部一致":"停止依据：来自「标签校验」· 标签不一致",sourceStep:"verify"});onFollowed:row=>flow.followSource(row) }
                    TagComparison {
                        objectName:"flowTagComparison"; visible:!flow.aesExpanded && flow.selectedStep==="verify"
                        Layout.fillWidth:true
                        received:flow.trace.receivedTag || ""; computed:flow.trace.computedTag || ""
                    }
                    SourceLink { visible:!flow.aesExpanded && flow.selectedStep==="output" && flow.accepted; Layout.fillWidth:true; descriptor:({origin:"来自「恢复字节」· 各块明文字节拼接后解码",sourceStep:"xor",sourceField:"xorOutput"});onFollowed:row=>flow.followSource(row) }
                    TextArea {
                        objectName:"flowPlaintextOutput"; visible:!flow.aesExpanded && flow.selectedStep==="output"
                        Layout.fillWidth:true; Layout.preferredHeight:120
                        text:flow.accepted ? flow.trace.plaintext || "" : ""; placeholderText:"校验闸门关闭 · 不输出原文"
                        readOnly:true; selectByMouse:true; wrapMode:TextEdit.Wrap
                        font.pixelSize:15; color:"#3A8872"; padding:12
                        background:Rectangle {radius:10;color:flow.accepted?"#EEF8F3":"#FFF4F5"}
                    }
                    Text { visible:!flow.accepted && flow.selectedStep==="xor"; Layout.fillWidth:true; text:"校验失败，原文恢复工序已停止。"; color:"#B65360"; font.pixelSize:13; wrapMode:Text.Wrap }
                    Text { visible:!flow.aesExpanded; Layout.fillWidth:true; text:"工序说明"; color:"#577795"; font.pixelSize:12; font.weight:Font.DemiBold }
                    Text { visible:!flow.aesExpanded; Layout.fillWidth:true; text:flow.explanation(); color:"#5F748E"; font.pixelSize:12; lineHeight:1.3; wrapMode:Text.Wrap }
                    Text { Layout.fillWidth:true; text:trace.referenceMatches ? "✓ 本次计算与实际 AES-GCM 结果核对一致" : trace.error || ""; color:trace.referenceMatches?"#71A491":"#B65360"; font.pixelSize:10; wrapMode:Text.Wrap }
                }
            }
        }
    }
    component SourceLink: Text {
        id: originLink
        property var descriptor: ({})
        readonly property bool linked: !!descriptor.sourceStep || descriptor.aesIndex!==undefined
        signal followed(var row)
        text:(linked?"↖ ":"")+(descriptor.origin || "")
        color:linked?"#387BB1":"#8A99AC";font.pixelSize:11;wrapMode:Text.Wrap;lineHeight:1.2
        font.underline:linked && linkMouse.containsMouse
        MouseArea { id:linkMouse;anchors.fill:parent;enabled:originLink.linked;hoverEnabled:true;cursorShape:Qt.PointingHandCursor;onClicked:originLink.followed(originLink.descriptor) }
    }
    component ByteMatrix: Item {
        id: matrix
        property string hexData: ""
        property string compareHex: ""
        property int selected: 0
        property var dependencies: []
        signal picked(int index)
        implicitHeight: width
        Grid {
            anchors.fill:parent; columns:4; spacing:3
            Repeater {
                model:16
                delegate:Rectangle {
                    required property int index
                    readonly property int byteIndex: (index % 4) * 4 + Math.floor(index / 4)
                    objectName: matrix.objectName + "Byte" + byteIndex
                    readonly property string value: hexData.slice(byteIndex*2,byteIndex*2+2)
                    width:(parent.width-9)/4; height:width; radius:5
                    color: selected===byteIndex || dependencies.indexOf(byteIndex)>=0 ?"#DCEBFC":compareHex.length && value!==compareHex.slice(byteIndex*2,byteIndex*2+2)?"#FFF0D9":"#EDF2F9"
                    border.color:selected===byteIndex?"#70A0D2":"transparent"
                    Text { anchors.centerIn:parent;text:value;color:"#42658D";font.family:"Consolas";font.pixelSize:12 }
                    MouseArea { anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:picked(byteIndex) }
                }
            }
        }
    }
    component TagComparison: ColumnLayout {
        property string received: ""
        property string computed: ""
        spacing:8
        Repeater {
            model:[{title:"包中携带的标签",value:received,step:"parse",field:"receivedTag",origin:"来自「解析包结构」· 包内第 13–28 字节"},{title:"接收端重新计算的标签",value:computed,step:"tag",field:"computedTag",origin:"来自「重算认证标签」· GHASH ⊕ M"}]
            delegate:ColumnLayout {
                id: tagRow
                required property var modelData
                Layout.fillWidth:true;spacing:5
                Text { text:modelData.title;color:"#8A9DB4";font.pixelSize:11 }
                SourceLink { objectName:"flowTagOrigin_"+modelData.step;Layout.fillWidth:true;descriptor:({origin:modelData.origin,sourceStep:modelData.step,sourceField:modelData.field});onFollowed:row=>flow.followSource(row) }
                Grid {
                    Layout.fillWidth:true;columns:8;spacing:4
                    Repeater {
                        model:16
                        delegate:Rectangle {
                            required property int index
                            readonly property bool same:received.slice(index*2,index*2+2)===computed.slice(index*2,index*2+2)
                            width:(parent.width-28)/8;height:28;radius:5;color:same?"#E5F4EC":"#FCE6EA"
                            Text { anchors.centerIn:parent;text:tagRow.modelData.value.slice(index*2,index*2+2);color:same?"#458B73":"#B34D62";font.family:"Consolas";font.pixelSize:11 }
                        }
                    }
                }
            }
        }
        Text { Layout.fillWidth:true;text:received===computed?"✓ 每个字节一致 · 校验通过":"× 红色字节不同 · 拒绝输出原文";color:received===computed?"#458B73":"#B34D62";font.pixelSize:12;wrapMode:Text.Wrap }
    }
}
