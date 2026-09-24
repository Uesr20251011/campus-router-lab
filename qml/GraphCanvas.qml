import QtQuick

Item {
    id: root
    clip: true
    property var nodes: []
    property var links: []
    property var step: ({})
    property string algorithm: ""
    property bool treeOnly: false
    property string metric: "cost"
    property string tool: "select"
    property bool fixedNodes: false
    property bool playing: false
    property int selectedNode: -1
    property int selectedLink: -1
    property int hoveredNode: -1
    property int hoveredLink: -1
    property int flowPathFocus: -1
    property bool showAllWeights: false
    property int linkStart: -1
    property int draggedNode: -1
    property int layoutTick: 0
    property int settleFrames: 0
    property real zoom: 1
    property real panX: 0
    property real panY: 0
    property var velocities: ({})
    property var restPositions: ({})
    property var labelSlots: ({})
    property string labelTopology: ""

    signal nodeSelected(int id)
    signal linkSelected(int id)
    signal blankClicked(real x, real y)
    signal linkRequested(int fromId, int toId)
    signal nodeContextRequested(int id, real x, real y)
    signal positionsSettled(var positions)

    readonly property real viewScale: Math.min((width - 36) / 940, (height - 36) / 610) * zoom

    function nodeItem(id) {
        for (let i = 0; i < nodeRepeater.count; ++i) {
            const item = nodeRepeater.itemAt(i)
            if (item && item.nodeId === id) return item
        }
        return null
    }
    function nodeCenter(id) {
        const observedTick = layoutTick
        const item = nodeItem(id)
        return item ? Qt.point(item.x + 31, item.y + 31) : Qt.point(0, 0)
    }
    function has(list, value) { return !!list && list.indexOf(value) >= 0 }
    function linkById(id) {
        for (let i = 0; i < links.length; ++i) if (links[i].id === id) return links[i]
        return null
    }
    function linkVisible(link) { return !treeOnly || has(step.selectedEdges, link.id) }
    function forwardResidual(link) {
        if (algorithm !== "flow" || !step.residualArcs) return link.capacity
        let available = 0
        for (const arc of step.residualArcs) {
            if (arc.reverse || arc.linkId !== link.id) continue
            if (link.directed && arc.from === link.u && arc.to === link.v)
                return arc.remaining
            if (!link.directed) available = Math.max(available, arc.remaining)
        }
        return available
    }
    function pathHas(link) {
        if (algorithm === "flow" && step.pathArcs) {
            for (const arc of step.pathArcs)
                if (arc.linkId === link.id && !arc.reverse) return true
            return false
        }
        if (!step.pathNodes) return false
        for (let i = 0; i < step.pathNodes.length - 1; ++i) {
            const a = step.pathNodes[i], b = step.pathNodes[i + 1]
            if ((a === link.u && b === link.v) || (a === link.v && b === link.u)) return true
        }
        return false
    }
    function activeRoute() {
        if (step.pathNodes && step.pathNodes.length > 1) return step.pathNodes
        const link = linkById(step.activeEdge)
        return link ? [link.u, link.v] : []
    }
    function routePoint(t) {
        const route = activeRoute()
        if (route.length < 2) return Qt.point(-100, -100)
        const part = Math.min(route.length - 2, Math.floor(t * (route.length - 1)))
        const fraction = t * (route.length - 1) - part
        const a = nodeCenter(route[part]), b = nodeCenter(route[part + 1])
        return Qt.point(a.x + (b.x - a.x) * fraction, a.y + (b.y - a.y) * fraction)
    }
    function pickLink(x, y) {
        let best = -1, nearest = 14
        for (let i = 0; i < links.length; ++i) {
            if (!linkVisible(links[i])) continue
            const a = nodeCenter(links[i].u), b = nodeCenter(links[i].v)
            const dx = b.x - a.x, dy = b.y - a.y
            const t = Math.max(0, Math.min(1, ((x - a.x) * dx + (y - a.y) * dy) / Math.max(1, dx * dx + dy * dy)))
            const distance = Math.hypot(x - a.x - t * dx, y - a.y - t * dy)
            if (distance < nearest) { nearest = distance; best = links[i].id }
        }
        return best
    }
    function fitView() { zoom = 1; panX = 0; panY = 0 }
    function flowColor(index) {
        const colors = ["#36AA99", "#E9A15D", "#8D75D9", "#5B9AD5", "#D97F9B", "#91A65B"]
        return colors[index % colors.length]
    }
    function relaxLayout() {
        if (fixedNodes) return
        const positions = {}
        for (let i = 0; i < nodeRepeater.count; ++i) {
            const item = nodeRepeater.itemAt(i)
            if (item) positions[item.nodeId] = {x: item.x, y: item.y}
        }
        restPositions = positions
        draggedNode = -1
        settleFrames = 100
        dynamics.start()
    }
    function savePositions() {
        const points = []
        for (let i = 0; i < nodeRepeater.count; ++i) {
            const item = nodeRepeater.itemAt(i)
            if (item) points.push({id: item.nodeId, x: item.x + 31, y: item.y + 31})
        }
        positionsSettled(points)
    }
    function commitNow() {
        if (!dynamics.running && draggedNode < 0) return
        dynamics.stop()
        savePositions()
    }

    onNodesChanged: { dynamics.stop(); velocities = ({}); restPositions = ({}); layoutTick++ }
    onFixedNodesChanged: { if (fixedNodes && dynamics.running) commitNow() }
    onLinksChanged: {
        const topology = links.map(link => link.id + ":" + link.u + ":" + link.v).join("|")
        if (topology !== labelTopology) { labelSlots = ({}); labelTopology = topology }
        edgeCanvas.requestPaint()
    }
    onLayoutTickChanged: edgeCanvas.requestPaint()
    onStepChanged: { edgeCanvas.requestPaint(); if (root.playing) beadMotion.restart() }
    onMetricChanged: edgeCanvas.requestPaint()
    onSelectedLinkChanged: edgeCanvas.requestPaint()
    onFlowPathFocusChanged: edgeCanvas.requestPaint()
    onTreeOnlyChanged: { edgeCanvas.requestPaint(); if (!fixedNodes && nodeRepeater.count > 1) relaxLayout() }
    onAlgorithmChanged: edgeCanvas.requestPaint()
    onHoveredNodeChanged: edgeCanvas.requestPaint()
    onHoveredLinkChanged: edgeCanvas.requestPaint()
    onShowAllWeightsChanged: edgeCanvas.requestPaint()

    Rectangle { anchors.fill: parent; color: "#F8FAFF" }
    Repeater {
        model: 28
        delegate: Rectangle {
            width: 3; height: 3; radius: 2; color: "#E1E9F4"
            x: 26 + (index % 7) * (root.width - 52) / 6
            y: 30 + Math.floor(index / 7) * (root.height - 60) / 3
        }
    }

    MouseArea {
        id: backgroundMouse
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        hoverEnabled: true
        property bool panning: false
        property real lastX: 0
        property real lastY: 0
        onPressed: mouse => {
            if (mouse.button === Qt.MiddleButton) {
                panning = true; lastX = mouse.x; lastY = mouse.y
            }
        }
        onPositionChanged: mouse => {
            if (panning) {
                root.panX += mouse.x - lastX; root.panY += mouse.y - lastY
                lastX = mouse.x; lastY = mouse.y
            } else if (root.tool === "select") {
                const point = graphLayer.mapFromItem(root, mouse.x, mouse.y)
                root.hoveredLink = root.pickLink(point.x, point.y)
            }
        }
        onExited: root.hoveredLink = -1
        onReleased: panning = false
        onWheel: wheel => { root.zoom = Math.max(0.55, Math.min(2.2, root.zoom * (wheel.angleDelta.y > 0 ? 1.1 : 0.9))) }
        onClicked: mouse => {
            if (mouse.button !== Qt.LeftButton) return
            const point = graphLayer.mapFromItem(root, mouse.x, mouse.y)
            if (root.tool === "node") root.blankClicked(point.x, point.y)
            else if (root.tool === "select") {
                const found = root.pickLink(point.x, point.y)
                if (found >= 0) root.linkSelected(found)
                else { root.nodeSelected(-1); root.linkSelected(-1) }
            }
        }
    }

    Item {
        id: graphLayer
        width: 940; height: 610
        x: (root.width - width * root.viewScale) / 2 + root.panX
        y: (root.height - height * root.viewScale) / 2 + root.panY
        scale: root.viewScale
        transformOrigin: Item.TopLeft

        Canvas {
            id: edgeCanvas
            anchors.fill: parent
            function pill(ctx, x, y, w, h, r) {
                ctx.beginPath()
                ctx.moveTo(x + r, y)
                ctx.lineTo(x + w - r, y)
                ctx.quadraticCurveTo(x + w, y, x + w, y + r)
                ctx.lineTo(x + w, y + h - r)
                ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h)
                ctx.lineTo(x + r, y + h)
                ctx.quadraticCurveTo(x, y + h, x, y + h - r)
                ctx.lineTo(x, y + r)
                ctx.quadraticCurveTo(x, y, x + r, y)
                ctx.closePath()
            }
            function arrow(ctx, x, y, ux, uy, size, color) {
                ctx.beginPath()
                ctx.moveTo(x + ux * size * 0.55, y + uy * size * 0.55)
                ctx.lineTo(x - ux * size * 0.55 - uy * size * 0.48,
                           y - uy * size * 0.55 + ux * size * 0.48)
                ctx.lineTo(x - ux * size * 0.55 + uy * size * 0.48,
                           y - uy * size * 0.55 - ux * size * 0.48)
                ctx.closePath()
                ctx.lineJoin = "round"
                ctx.strokeStyle = "#FFFFFF"; ctx.lineWidth = 3.5; ctx.stroke()
                ctx.fillStyle = color; ctx.fill()
            }
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                const placedLabels = []
                if (root.algorithm === "flow" && root.step.kind !== "finish" && root.step.residualArcs) {
                    for (const arc of root.step.residualArcs) {
                        if (!arc.reverse || arc.remaining <= 0) continue
                        const a = root.nodeCenter(arc.from), b = root.nodeCenter(arc.to)
                        const dx = b.x - a.x, dy = b.y - a.y
                        const length = Math.max(1, Math.hypot(dx, dy))
                        const ux = dx / length, uy = dy / length
                        const shift = 13
                        const sx = a.x + ux * 30 - uy * shift, sy = a.y + uy * 30 + ux * shift
                        const ex = b.x - ux * 30 - uy * shift, ey = b.y - uy * 30 + ux * shift
                        const onRoute = root.step.pathArcs && root.step.pathArcs.some(p =>
                            p.reverse && p.linkId === arc.linkId && p.from === arc.from && p.to === arc.to)
                        ctx.globalAlpha = onRoute ? 1 : 0.72
                        if (onRoute) {
                            ctx.strokeStyle = "#C7B9F8"; ctx.lineWidth = 13
                            ctx.beginPath(); ctx.moveTo(sx, sy); ctx.lineTo(ex, ey); ctx.stroke()
                        }
                        ctx.strokeStyle = "#876BDB"; ctx.lineWidth = onRoute ? 5 : 3.4
                        ctx.lineCap = "round"; ctx.setLineDash([8, 5])
                        ctx.beginPath(); ctx.moveTo(sx, sy); ctx.lineTo(ex, ey); ctx.stroke()
                        ctx.setLineDash([])
                        arrow(ctx, sx + (ex - sx) * 0.72, sy + (ey - sy) * 0.72,
                              ux, uy, 18, "#876BDB")
                        ctx.globalAlpha = 1
                        if (onRoute || root.selectedLink === arc.linkId || root.hoveredLink === arc.linkId) {
                            const label = "逆余 " + arc.remaining
                            ctx.font = "bold 11px sans-serif"
                            const labelWidth = ctx.measureText(label).width + 15
                            const mx = (sx + ex) / 2 - uy * 19, my = (sy + ey) / 2 + ux * 19
                            ctx.fillStyle = "#F1ECFF"; ctx.strokeStyle = "#D6C9FA"; ctx.lineWidth = 1
                            pill(ctx, mx - labelWidth / 2, my - 11, labelWidth, 22, 9)
                            ctx.fill(); ctx.stroke()
                            ctx.fillStyle = "#7356C5"; ctx.textAlign = "center"; ctx.textBaseline = "middle"
                            ctx.fillText(label, mx, my + 0.5)
                        }
                    }
                }
                for (let i = 0; i < root.links.length; ++i) {
                    const link = root.links[i]
                    if (!root.linkVisible(link)) continue
                    const a = root.nodeCenter(link.u), b = root.nodeCenter(link.v)
                    const dx = b.x - a.x, dy = b.y - a.y
                    const length = Math.max(1, Math.hypot(dx, dy))
                    const ux = dx / length, uy = dy / length
                    const sx = a.x + ux * 29, sy = a.y + uy * 29
                    const ex = b.x - ux * 29, ey = b.y - uy * 29
                    const chosen = root.has(root.step.selectedEdges, link.id)
                    const onPath = root.pathHas(link)
                    const reverseRoute = root.algorithm === "flow" && root.step.pathArcs
                        && root.step.pathArcs.some(arc => arc.linkId === link.id && arc.reverse)
                    const active = root.step.activeEdge === link.id && !reverseRoute
                    const rejected = root.step.rejectedEdge === link.id
                    const selected = root.selectedLink === link.id
                    const hovered = root.hoveredLink === link.id || root.hoveredNode === link.u || root.hoveredNode === link.v
                    const focused = root.hoveredNode >= 0 || root.hoveredLink >= 0 || root.selectedLink >= 0
                    const dimmed = focused && !selected && !hovered && !onPath && !active && !chosen
                    const flowMode = root.algorithm === "flow" && !!root.step.residualArcs
                    const flowFinal = flowMode && root.step.kind === "finish"
                    const flow = root.step.linkFlows && root.step.linkFlows.length > i ? root.step.linkFlows[i] : 0
                    const remaining = flowMode ? root.forwardResidual(link) : link.capacity
                    const exhausted = flowMode && remaining <= 0
                    const tone = exhausted ? "#AEB8C8" : selected ? "#7A66D8" : onPath
                        ? (root.step.kind === "search" ? "#EAA668" : "#50BDA7")
                        : active ? "#F0B477" : rejected ? "#E693A4" : chosen ? "#68C1AC"
                        : hovered ? "#6D9FC8" : link.directed ? "#7D9FC9" : "#BDCADC"
                    const thickness = selected || onPath ? 5.5 : active || chosen || hovered ? 4.5 : link.directed ? 3.3 : 2.5
                    const edgeAlpha = flowFinal ? (flow !== 0 ? 0.55 : 0.16)
                                    : exhausted ? 0.30 : dimmed ? 0.22 : 1
                    if (!exhausted && (selected || onPath || active || chosen)) {
                        ctx.strokeStyle = tone; ctx.globalAlpha = 0.16; ctx.lineWidth = thickness + 12
                        ctx.lineCap = "round"; ctx.beginPath(); ctx.moveTo(sx, sy); ctx.lineTo(ex, ey); ctx.stroke()
                    }
                    ctx.globalAlpha = edgeAlpha
                    ctx.strokeStyle = tone; ctx.lineWidth = thickness; ctx.lineCap = "round"
                    ctx.setLineDash(rejected ? [7, 6] : [])
                    ctx.beginPath(); ctx.moveTo(sx, sy); ctx.lineTo(ex, ey); ctx.stroke(); ctx.setLineDash([])
                    if (link.directed) {
                        arrow(ctx, ex - ux * 5, ey - uy * 5, ux, uy, 13, tone)
                    }
                    ctx.globalAlpha = 1
                    const routes = []
                    if (flowFinal && root.step.flowPaths) {
                        for (let pathIndex = 0; pathIndex < root.step.flowPaths.length; ++pathIndex) {
                            const route = root.step.flowPaths[pathIndex]
                            const segment = route.linkIds.indexOf(link.id)
                            if (segment >= 0) routes.push({index: pathIndex, forward: route.nodes[segment] === link.u})
                        }
                        for (let lane = 0; lane < routes.length; ++lane) {
                            const route = routes[lane]
                            const shift = (lane - (routes.length - 1) / 2) * 7
                            const color = root.flowColor(route.index)
                            const fromX = (route.forward ? sx : ex) - uy * shift
                            const fromY = (route.forward ? sy : ey) + ux * shift
                            const toX = (route.forward ? ex : sx) - uy * shift
                            const toY = (route.forward ? ey : sy) + ux * shift
                            ctx.globalAlpha = root.flowPathFocus < 0 || root.flowPathFocus === route.index ? 1 : 0.18
                            ctx.strokeStyle = color; ctx.lineWidth = 5; ctx.lineCap = "round"
                            ctx.beginPath(); ctx.moveTo(fromX, fromY); ctx.lineTo(toX, toY); ctx.stroke()
                            const direction = route.forward ? 1 : -1
                            arrow(ctx, toX - ux * direction * 5,
                                  toY - uy * direction * 5,
                                  ux * direction, uy * direction, 13, color)
                        }
                        ctx.globalAlpha = 1
                    }
                    const movingLargeGraph = root.draggedNode >= 0 && root.nodes.length > 60
                    const showLabel = flowFinal ? (flow !== 0 || selected || hovered)
                                    : movingLargeGraph ? (selected || active || onPath || reverseRoute)
                                                       : root.showAllWeights || root.links.length <= 16 || selected || hovered || active || chosen || onPath || reverseRoute
                    if (!showLabel) continue
                    const label = root.metric === "cost" ? (link.hasCost ? "¥ " + link.cost : "—")
                                  : (link.hasCapacity ? (flowMode
                                      ? (active && root.step.kind === "update" ? "余 " + remaining
                                         : Math.abs(flow) + "/" + link.capacity)
                                      : link.capacity) : "—")
                    ctx.font = "bold 12px sans-serif"
                    const labelWidth = ctx.measureText(label).width + 18
                    let mx = (a.x + b.x) / 2, my = (a.y + b.y) / 2, bestScore = -1e9
                    let bestSlot = -1, previousScore = -1e9, previousX = mx, previousY = my
                    const previousSlot = root.labelSlots[link.id]
                    const offsets = [17, -17, 30, -30]
                    const positions = [0.5, 0.38, 0.62, 0.28, 0.72]
                    for (let ti = 0; ti < positions.length; ++ti) {
                        for (let oi = 0; oi < offsets.length; ++oi) {
                            const t = positions[ti], offset = offsets[oi]
                            const slot = ti * offsets.length + oi
                            const cx = a.x + dx * t - uy * offset
                            const cy = a.y + dy * t + ux * offset
                            let score = 1000
                            for (let n = 0; n < nodeRepeater.count; ++n) {
                                const item = nodeRepeater.itemAt(n)
                                if (!item) continue
                                const clearance = Math.hypot(cx - item.x - 31, cy - item.y - 31)
                                score = Math.min(score, clearance - 38)
                            }
                            for (const prior of placedLabels) {
                                const separation = Math.hypot(cx - prior.x, cy - prior.y)
                                score = Math.min(score, separation - (labelWidth + prior.width) / 2 - 6)
                            }
                            if (slot === previousSlot) { previousScore = score; previousX = cx; previousY = cy }
                            if (score > bestScore) { bestScore = score; bestSlot = slot; mx = cx; my = cy }
                        }
                    }
                    const hysteresis = root.draggedNode >= 0 || dynamics.running ? 24 : 14
                    if (previousScore > -1e9 && previousScore >= bestScore - hysteresis) {
                        bestSlot = previousSlot; mx = previousX; my = previousY
                    }
                    root.labelSlots[link.id] = bestSlot
                    placedLabels.push({x: mx, y: my, width: labelWidth})
                    let labelFill = "#FFFFFF", labelStroke = "#E7EBF3", labelInk = "#60738D"
                    let labelBorder = 1
                    if (flowFinal && flow !== 0) {
                        const focusedRoute = routes.find(route => route.index === root.flowPathFocus) || routes[0]
                        const routeColor = focusedRoute ? root.flowColor(focusedRoute.index) : "#36AA99"
                        labelFill = "#F5F8FC"; labelStroke = routeColor; labelInk = routeColor; labelBorder = 2
                    } else if (exhausted) {
                        labelFill = "#F0F2F6"; labelStroke = "#D2D9E4"; labelInk = "#8C98AA"
                    }
                    if (chosen) {
                        labelFill = "#E7F8F2"; labelStroke = "#68C1AC"; labelInk = "#268B79"; labelBorder = 2
                    }
                    if (rejected) {
                        labelFill = "#FDECF0"; labelStroke = "#E693A4"; labelInk = "#A8546A"; labelBorder = 2
                    }
                    if (active) {
                        labelFill = "#FFF0DE"; labelStroke = "#F0B477"; labelInk = "#A36A30"; labelBorder = 2
                    }
                    if (onPath) {
                        const searching = root.step.kind === "search"
                        labelFill = searching ? "#FFF0DE" : "#E3F7F0"
                        labelStroke = searching ? "#EAA668" : "#50BDA7"
                        labelInk = searching ? "#A36A30" : "#268B79"
                        labelBorder = 2
                    }
                    if (reverseRoute) {
                        labelFill = "#F1ECFF"; labelStroke = "#9B84E3"; labelInk = "#7356C5"; labelBorder = 2
                    }
                    if (selected) {
                        labelFill = "#EFEAFF"; labelStroke = "#7A66D8"; labelInk = "#6650B6"; labelBorder = 2
                    } else if (hovered && !chosen && !onPath && !reverseRoute && !active && !rejected) {
                        labelFill = "#EAF3FC"; labelStroke = "#8CB1D6"; labelInk = "#507DAA"; labelBorder = 2
                    }
                    const routeInFocus = routes.some(route => route.index === root.flowPathFocus)
                    ctx.globalAlpha = flowFinal && root.flowPathFocus >= 0 && !routeInFocus && !selected && !hovered
                        ? 0.28 : exhausted && !flowFinal && !reverseRoute ? 0.56 : 1
                    ctx.fillStyle = labelFill; ctx.strokeStyle = labelStroke; ctx.lineWidth = labelBorder
                    pill(ctx, mx - labelWidth / 2, my - 13, labelWidth, 26, 10)
                    ctx.fill(); ctx.stroke()
                    ctx.fillStyle = labelInk; ctx.textAlign = "center"; ctx.textBaseline = "middle"
                    ctx.fillText(label, mx, my + 0.5)
                    ctx.globalAlpha = 1
                }
            }
        }

        Repeater {
            id: nodeRepeater
            model: root.nodes
            delegate: Item {
                id: nodeBody
                property int nodeId: modelData.id
                property bool inPath: root.has(root.step.pathNodes, nodeId)
                property bool visited: root.has(root.step.visitedNodes, nodeId)
                property bool searchingFlow: root.algorithm === "flow" && root.step.kind === "search"
                property bool edgeFocused: {
                    const activeLink = root.linkById(root.hoveredLink >= 0 ? root.hoveredLink : root.selectedLink)
                    return !!activeLink && (activeLink.u === nodeId || activeLink.v === nodeId)
                }
                width: 62; height: 62
                x: modelData.x - 31
                y: modelData.y - 31
                scale: inPath ? 1.14 : root.selectedNode === nodeId || root.linkStart === nodeId ? 1.1 : 1
                z: root.draggedNode === nodeId ? 5 : 1
                Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack } }
                onXChanged: root.layoutTick++
                onYChanged: root.layoutTick++

                Rectangle {
                    anchors.centerIn: parent
                    width: 75; height: 75; radius: 38
                    color: nodeBody.inPath ? (nodeBody.searchingFlow ? "#29EAA668" : root.algorithm === "flow" ? "#2950BDA7" : "#298D77E0")
                                           : root.selectedNode === nodeBody.nodeId || nodeBody.edgeFocused ? "#267BCBB9" : "transparent"
                }
                Rectangle {
                    anchors.fill: parent
                    radius: 31
                    color: nodeBody.inPath ? (nodeBody.searchingFlow ? "#FFF0DB" : root.algorithm === "flow" ? "#D8F7ED" : "#E7DFFF")
                                           : nodeBody.visited ? "#D8F5EB" : root.selectedNode === nodeBody.nodeId || nodeBody.edgeFocused ? "#DDF5ED" : "#FFFFFF"
                    border.width: root.selectedNode === nodeBody.nodeId || nodeBody.inPath || nodeBody.edgeFocused ? 3 : 2
                    border.color: nodeBody.inPath ? (nodeBody.searchingFlow ? "#EAA668" : root.algorithm === "flow" ? "#50BDA7" : "#9B84E3")
                                                  : root.selectedNode === nodeBody.nodeId || nodeBody.edgeFocused ? "#66BDA7" : "#CFD9E8"
                    Behavior on color { ColorAnimation { duration: 200 } }
                }
                Text {
                    anchors.centerIn: parent
                    width: parent.width - 8
                    text: modelData.name
                    color: "#31445F"
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: text.length > 5 ? 12 : 14
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true
                    cursorShape: root.tool === "select" ? Qt.OpenHandCursor : Qt.PointingHandCursor
                    onEntered: root.hoveredNode = nodeBody.nodeId
                    onExited: root.hoveredNode = -1
                    drag.target: root.tool === "select" && pressedButtons === Qt.LeftButton ? nodeBody : null
                    drag.minimumX: 8; drag.maximumX: 870
                    drag.minimumY: 8; drag.maximumY: 540
                    onPressed: {
                        if (mouse.button === Qt.LeftButton && root.tool === "select") {
                            root.nodeSelected(nodeBody.nodeId)
                        }
                    }
                    onPositionChanged: {
                        if (root.tool !== "select" || !drag.active || root.draggedNode === nodeBody.nodeId) return
                        root.draggedNode = nodeBody.nodeId
                        root.settleFrames = 0
                        if (root.fixedNodes) return
                        const positions = {}
                        for (let i = 0; i < nodeRepeater.count; ++i) {
                            const item = nodeRepeater.itemAt(i)
                            if (item) positions[item.nodeId] = {x: item.x, y: item.y}
                        }
                        root.restPositions = positions
                        dynamics.start()
                    }
                    onClicked: {
                        if (mouse.button === Qt.RightButton) {
                            const point = mapToItem(root, mouse.x, mouse.y)
                            root.nodeSelected(nodeBody.nodeId)
                            root.nodeContextRequested(nodeBody.nodeId, point.x, point.y)
                            return
                        }
                        if (mouse.button !== Qt.LeftButton) return
                        if (root.tool === "link") {
                            if (root.linkStart < 0) root.linkStart = nodeBody.nodeId
                            else if (root.linkStart !== nodeBody.nodeId) {
                                root.linkRequested(root.linkStart, nodeBody.nodeId)
                                root.linkStart = -1
                            }
                        }
                    }
                    onReleased: {
                        if (root.draggedNode === nodeBody.nodeId) {
                            root.draggedNode = -1
                            if (root.fixedNodes) root.savePositions()
                            else root.settleFrames = 80
                        }
                    }
                }
            }
        }

        Rectangle {
            id: routeBead
            property real progress: 0
            width: 17; height: 17; radius: 9
            color: "#FFFFFF"
            border.width: 4; border.color: root.algorithm === "flow"
                                             ? (root.step.kind === "search" ? "#EAA668" : "#50BDA7")
                                             : "#927AE4"
            visible: root.playing && root.activeRoute().length >= 2
            x: root.routePoint(progress).x - width / 2
            y: root.routePoint(progress).y - height / 2
            z: 7
            NumberAnimation on progress {
                id: beadMotion
                from: 0; to: 1
                duration: root.step.pathNodes ? Math.max(700, root.step.pathNodes.length * 280) : 1100
                loops: Animation.Infinite
                running: routeBead.visible
            }
        }
    }

    Rectangle {
        visible: root.hoveredLink >= 0
        anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.rightMargin: 16; anchors.bottomMargin: 16
        width: hoverDetails.implicitWidth + 24; height: 34
        radius: 12; color: "#F7FFFFFF"; border.color: "#DDE7F2"
        Text {
            id: hoverDetails
            anchors.centerIn: parent
            text: {
                const link = root.linkById(root.hoveredLink)
                return link ? "R" + link.u + (link.directed ? " → " : " — ") + "R" + link.v
                              + "   造价 " + (link.hasCost ? link.cost : "—")
                              + "   容量 " + (link.hasCapacity ? link.capacity : "—") : ""
            }
            color: "#526883"; font.pixelSize: 11; font.family: "Microsoft YaHei UI"
        }
    }

    Timer {
        id: dynamics
        interval: 16
        repeat: true
        running: false
        onTriggered: {
            const items = []
            for (let i = 0; i < nodeRepeater.count; ++i) {
                const item = nodeRepeater.itemAt(i)
                if (item) items.push(item)
            }
            const fx = new Array(items.length).fill(0), fy = new Array(items.length).fill(0)
            const lookup = {}
            const repulsionRange = items.length > 10 ? 130 : 175
            const springLength = items.length > 10 ? 112 : 148
            for (let i = 0; i < items.length; ++i) lookup[items[i].nodeId] = i
            for (let i = 0; i < items.length; ++i) {
                for (let j = i + 1; j < items.length; ++j) {
                    const dx = items[j].x - items[i].x, dy = items[j].y - items[i].y
                    const distance = Math.max(1, Math.hypot(dx, dy))
                    if (distance > repulsionRange) continue
                    const push = (repulsionRange - distance) * 0.018
                    fx[i] -= push * dx / distance; fy[i] -= push * dy / distance
                    fx[j] += push * dx / distance; fy[j] += push * dy / distance
                }
            }
            for (let e = 0; e < root.links.length; ++e) {
                const link = root.links[e]
                if (!root.linkVisible(link)) continue
                const u = lookup[link.u], v = lookup[link.v]
                if (u === undefined || v === undefined) continue
                const dx = items[v].x - items[u].x, dy = items[v].y - items[u].y
                const distance = Math.max(1, Math.hypot(dx, dy))
                const pull = (distance - springLength) * 0.006
                fx[u] += pull * dx / distance; fy[u] += pull * dy / distance
                fx[v] -= pull * dx / distance; fy[v] -= pull * dy / distance
                if (items.length <= 60) for (let k = 0; k < items.length; ++k) {
                    if (k === u || k === v) continue
                    const t = Math.max(0.15, Math.min(0.85, ((items[k].x - items[u].x) * dx + (items[k].y - items[u].y) * dy) / Math.max(1, dx * dx + dy * dy)))
                    const awayX = items[k].x - items[u].x - t * dx
                    const awayY = items[k].y - items[u].y - t * dy
                    const clearance = Math.max(1, Math.hypot(awayX, awayY))
                    if (clearance < 53) {
                        fx[k] += (53 - clearance) * 0.035 * awayX / clearance
                        fy[k] += (53 - clearance) * 0.035 * awayY / clearance
                    }
                }
            }
            for (let i = 0; i < items.length; ++i) {
                const item = items[i]
                if (item.nodeId === root.draggedNode) continue
                const velocity = root.velocities[item.nodeId] || {x: 0, y: 0}
                const rest = root.restPositions[item.nodeId] || {x: item.x, y: item.y}
                const anchoring = root.treeOnly ? 0.0015 : 0.009
                velocity.x = (velocity.x + fx[i] + (rest.x - item.x) * anchoring + (470 - item.x - 31) * 0.0007) * 0.79
                velocity.y = (velocity.y + fy[i] + (rest.y - item.y) * anchoring + (305 - item.y - 31) * 0.0007) * 0.79
                const limit = Math.min(1, 7 / Math.max(1, Math.hypot(velocity.x, velocity.y)))
                item.x = Math.max(8, Math.min(870, item.x + velocity.x * limit))
                item.y = Math.max(8, Math.min(540, item.y + velocity.y * limit))
                root.velocities[item.nodeId] = velocity
            }
            root.layoutTick++
            if (root.draggedNode < 0 && --root.settleFrames <= 0) root.commitNow()
        }
    }
}
