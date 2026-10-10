#include "AlgorithmSupport.h"
#include <QSet>
#include <QStringList>
#include <algorithm>
#include <limits>
#include <numeric>
#include <queue>

using namespace GraphAlgorithmDetail;

/* Edmonds–Karp 最大流：在剩余网络中反复用 BFS 寻找边数最少的增广路。
 * 每轮按瓶颈容量增广，直到源点无法到达汇点；此时流量等于最小割容量。
 * 路由使用容量（hasCapacity），与建设造价无关；零容量边不进入剩余网络。
 * 标准增广主算法 O(VE^2)；本程序另外保存逐步快照及最终流量分解，
 * 因此完整演示的时间/内存还取决于快照数量，不能套用核心算法的空间开销。
 */
Trace runMaxFlow(const Graph &graph, int source, int sink) {
    Trace result;
    const auto indices = indicesOf(graph);
    if (!indices.contains(source) || !indices.contains(sink) || source == sink) {
        result.error = QStringLiteral("请选择两个不同且存在的路由器");
        return result;
    }
    // Arc 是剩余弧。capacity 为此刻剩余容量，而非原边总容量；
    // reverseIndex 指向对端邻接表中的配对弧；linkIndex 指回原始边；
    // sign 指出沿此弧增广时原边的“u→v 净流量”应增加(+1)还是减少(-1)。
    struct Arc { int to; int reverseIndex; qint64 capacity; int linkIndex; int sign; bool isReverse; };
    QVector<QVector<Arc>> residual(graph.nodes.size());
    // 正向弧初始可发送 capacity，配对反向弧初始为 0。
    // 反向弧允许撤销之前的流量并改走更优通路，不能省略。
    auto addArc = [&](int u, int v, qint64 capacity, int linkIndex, int sign) {
        const int forward = residual[u].size();
        const int backward = residual[v].size();
        residual[u].append({v, backward, capacity, linkIndex, sign, false});
        residual[v].append({u, forward, 0, linkIndex, -sign, true});
    };
    for (int i = 0; i < graph.links.size(); ++i) {
        const auto &link = graph.links[i];
        if (!link.hasCapacity || link.capacity <= 0) continue;
        const int u = indices.value(link.u), v = indices.value(link.v);
        addArc(u, v, link.capacity, i, 1);
        // 无向链路在两个方向各建一组剩余弧；flows 最终记录两方向抵消后的净流量。
        if (!link.directed) addArc(v, u, link.capacity, i, -1);
    }
    QVector<qint64> flows(graph.links.size(), 0);
    qint64 total = 0;
    // 将内部数组下标换成外部 id，供画布显示原向容量及紫色回退弧。
    auto snapshot = [&]() {
        QVector<ResidualArcState> state;
        for (int u = 0; u < residual.size(); ++u)
            for (const auto &arc : residual[u]) {
                if (arc.isReverse && arc.capacity <= 0) continue;
                state.append({graph.nodes[u].id, graph.nodes[arc.to].id,
                              graph.links[arc.linkIndex].id, arc.capacity, arc.isReverse});
            }
        return state;
    };
    auto decorate = [&](TraceStep &step) {
        step.linkFlows = flows;
        step.residualArcs = snapshot();
    };
    TraceStep initial = makeStep(QStringLiteral("start"), QStringLiteral("初始流量 0"),
                                 QStringLiteral("每条边显示 当前流量/容量；从 R%1 搜索到 R%2 的通路")
                                     .arg(source).arg(sink), {source}, {}, total);
    decorate(initial);
    result.steps.append(initial);
    const int start = indices.value(source), goal = indices.value(sink);
    int round = 0;
    while (true) {
        if (++round > 1) {
            TraceStep again = makeStep(QStringLiteral("start"), QStringLiteral("继续寻找增广路"),
                                       QStringLiteral("按更新后的剩余容量重新搜索"),
                                       {source}, {}, total);
            decorate(again);
            result.steps.append(again);
        }
        // 每轮重新 BFS。parent 存前驱节点下标，via 存该前驱邻接表中的弧下标；
        // 同一节点只入队一次，因此第一次到达汇点得到边数最少的增广路。
        QVector<int> parent(graph.nodes.size(), -1), via(graph.nodes.size(), -1);
        QVector<int> visited{source};
        std::queue<int> queue;
        queue.push(start);
        parent[start] = start;
        auto routeTo = [&](int target, TraceStep &step) {
            for (int v = target; v != start; v = parent[v]) {
                const int u = parent[v];
                const Arc &arc = residual[u][via[v]];
                step.pathNodes.prepend(graph.nodes[v].id);
                step.pathArcs.prepend({graph.nodes[u].id, graph.nodes[v].id,
                                       graph.links[arc.linkIndex].id, arc.capacity, arc.isReverse});
            }
            step.pathNodes.prepend(source);
        };
        while (!queue.empty() && parent[goal] == -1) {
            const int u = queue.front(); queue.pop();
            for (int i = 0; i < residual[u].size(); ++i) {
                const auto &arc = residual[u][i];
                if (arc.capacity <= 0 || parent[arc.to] != -1) continue;
                parent[arc.to] = u; via[arc.to] = i;
                queue.push(arc.to);
                visited.append(graph.nodes[arc.to].id);
                TraceStep search = makeStep(QStringLiteral("search"),
                                            QStringLiteral("搜索到 R%1").arg(graph.nodes[arc.to].id),
                                            arc.isReverse
                                                ? QStringLiteral("沿紫色反向边回退，尚可回退 %1 包/秒").arg(arc.capacity)
                                                : QStringLiteral("此方向剩余 %1 包/秒，继续找通路").arg(arc.capacity),
                                            visited, {}, total, graph.links[arc.linkIndex].id);
                routeTo(arc.to, search);
                decorate(search);
                result.steps.append(search);
                if (arc.to == goal) break;
            }
        }
        // 没有增广路：剩余网络中源点可达集合构成最小割的源侧，流量已最大。
        if (parent[goal] == -1) break;
        TraceStep found = makeStep(QStringLiteral("path"), QStringLiteral("找到增广路"),
                                   QString(),
                                   visited, {}, total);
        routeTo(goal, found);
        // 一条通路的增量不能超过任何一段剩余容量，因此取沿途最小值。
        qint64 bottleneck = std::numeric_limits<qint64>::max();
        for (const auto &arc : found.pathArcs)
            bottleneck = std::min(bottleneck, arc.remaining);
        found.bottleneck = bottleneck;
        found.detail = QStringLiteral("通路瓶颈为 %1 包/秒；下一步依次扣减剩余容量").arg(bottleneck);
        decorate(found);
        result.steps.append(found);
        for (int i = 1; i < found.pathNodes.size(); ++i) {
            const int u = indices.value(found.pathNodes[i - 1]);
            const int v = indices.value(found.pathNodes[i]);
            Arc &arc = residual[u][via[v]];
            Arc &reverse = residual[v][arc.reverseIndex];
            const qint64 before = arc.capacity;
            // 配对更新保持剩余网络一致：本方向扣除增量，回退方向增加增量。
            // 沿反向弧时 sign 为负，表示取消原边上先前分配的流量。
            arc.capacity -= bottleneck;
            reverse.capacity += bottleneck;
            flows[arc.linkIndex] += arc.sign * bottleneck;
            TraceStep update = makeStep(QStringLiteral("update"),
                                        QStringLiteral("R%1 → R%2：剩余 %3 → %4")
                                            .arg(graph.nodes[u].id).arg(graph.nodes[v].id)
                                            .arg(before).arg(arc.capacity),
                                        arc.isReverse
                                            ? QStringLiteral("沿反向边回退 %1；原方向流量减少").arg(bottleneck)
                                            : QStringLiteral("发送 %1；反向可回退量增至 %2")
                                                  .arg(bottleneck).arg(reverse.capacity),
                                        visited, {}, total, graph.links[arc.linkIndex].id);
            update.pathNodes = found.pathNodes;
            update.pathArcs = found.pathArcs;
            update.bottleneck = bottleneck;
            decorate(update);
            result.steps.append(update);
        }
        // 所有弧更新完才增加源汇总流量。逐边 update 快照是教学中间状态，
        // 本轮整体 augment 快照才代表一次完成的、满足流量守恒的增广结果。
        total += bottleneck;
        TraceStep pushed = makeStep(QStringLiteral("augment"),
                                    QStringLiteral("增加 %1 包/秒").arg(bottleneck),
                                    QStringLiteral("当前吞吐量为 %1 包/秒").arg(total),
                                    visited, {}, total);
        pushed.pathNodes = found.pathNodes;
        pushed.pathArcs = found.pathArcs;
        pushed.bottleneck = bottleneck;
        decorate(pushed);
        result.steps.append(pushed);
    }
    result.success = true; result.value = total;
    TraceStep finish = makeStep(QStringLiteral("finish"),
                                QStringLiteral("最大吞吐量：%1 包/秒").arg(total),
                                QStringLiteral("已找不到新的增广路"), {}, {}, total);
    decorate(finish);
    // 下面是“展示最终路径”的后处理，不是再次计算最大流。
    // 把最终每条原边的非零净流量按实际方向建图（负数表示 v→u）。
    struct UsedEdge { int to; int linkId; qint64 remaining; };
    QVector<UsedEdge> used;
    QVector<QVector<int>> outgoing(graph.nodes.size());
    for (int i = 0; i < graph.links.size(); ++i) {
        if (flows[i] == 0) continue;
        const auto &link = graph.links[i];
        const int from = indices.value(flows[i] > 0 ? link.u : link.v);
        const int to = indices.value(flows[i] > 0 ? link.v : link.u);
        outgoing[from].append(used.size());
        used.append({to, link.id, std::abs(flows[i])});
    }
    qint64 decomposed = 0;
    // 在载流图中用 BFS 找源汇路径，取路径最小流量并从各边扣掉，直到
    // 路径流量之和达到 total。与源汇输送无关的循环流不列成输送路径。
    while (decomposed < total) {
        QVector<int> parent(graph.nodes.size(), -1), via(graph.nodes.size(), -1);
        std::queue<int> queue;
        queue.push(start);
        parent[start] = start;
        while (!queue.empty() && parent[goal] < 0) {
            const int from = queue.front(); queue.pop();
            for (int edgeIndex : outgoing[from]) {
                const auto &edge = used[edgeIndex];
                if (edge.remaining <= 0 || parent[edge.to] >= 0) continue;
                parent[edge.to] = from;
                via[edge.to] = edgeIndex;
                queue.push(edge.to);
            }
        }
        if (parent[goal] < 0) break;
        QVector<int> route;
        qint64 amount = total - decomposed;
        for (int at = goal; at != start; at = parent[at]) {
            route.prepend(via[at]);
            amount = std::min(amount, used[via[at]].remaining);
        }
        FlowPath path;
        path.amount = amount;
        path.nodes.append(source);
        int at = start;
        for (int edgeIndex : route) {
            used[edgeIndex].remaining -= amount;
            at = used[edgeIndex].to;
            path.nodes.append(graph.nodes[at].id);
            path.linkIds.append(used[edgeIndex].linkId);
        }
        finish.flowPaths.append(path);
        decomposed += amount;
    }
    finish.detail = QStringLiteral("最终流量分解为 %1 条路径；点击路径可在图上突出显示")
                        .arg(finish.flowPaths.size());
    result.steps.append(finish);
    return result;
}
