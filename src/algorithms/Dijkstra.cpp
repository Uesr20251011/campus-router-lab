#include "AlgorithmSupport.h"
#include <QSet>
#include <QStringList>
#include <algorithm>
#include <limits>
#include <numeric>
#include <queue>

using namespace GraphAlgorithmDetail;

/* Dijkstra：按建设造价求源点到终点的最短路径。
 * 前提：造价非负（编辑与预设读取入口负责校验）。只使用 hasCost 边。
 * 不变量：settled 节点的 distance 已是最终最短距离；未确定节点存暂定距离。
 * 此实现用线性扫描找最近节点，并扫描全图边寻找邻居；没有优先队列。
 * 不计路径重建及演示快照，主循环 O(V^2 + VE)，不能写成堆版 O(E log V)。
 */
Trace runDijkstra(const Graph &graph, int source, int sink) {
    Trace result;
    const auto indices = indicesOf(graph);
    if (!indices.contains(source) || !indices.contains(sink)) {
        result.error = QStringLiteral("请选择存在的起点和终点");
        return result;
    }

    const int count = graph.nodes.size();
    const int start = indices.value(source), goal = indices.value(sink);
    // 保留加法安全余量，用 infinity 表示未知距离；显示时转换成 -1（界面画 ∞）。
    const qint64 infinity = std::numeric_limits<qint64>::max() / 4;
    QVector<qint64> distance(count, infinity);
    // 前驱信息保存当前最佳路线，只有松弛成功时才修改，用于终点路径回溯。
    QVector<int> previousNode(count, -1), previousEdge(count, -1);
    QVector<char> settled(count, false);
    QVector<int> settledOrder;
    distance[start] = 0;

    // 从目标沿前驱反向走回源点，再反转成正向路线。depth 是防御性循环上限。
    auto routeTo = [&](int target) -> std::pair<QVector<int>, QVector<int>> {
        QVector<int> nodes, edges;
        for (int at = target, depth = 0; depth <= count; ++depth) {
            nodes.append(graph.nodes[at].id);
            if (at == start) {
                std::reverse(nodes.begin(), nodes.end());
                std::reverse(edges.begin(), edges.end());
                return {nodes, edges};
            }
            if (previousNode[at] < 0) break;
            edges.append(previousEdge[at]);
            at = previousNode[at];
        }
        return {};
    };
    // 演示快照同时包含全部距离、已确定节点和当前路线；与求最短路的状态分开。
    auto record = [&](const QString &kind, const QString &title, const QString &detail,
                      int focus, int active = -1, int rejected = -1) {
        QVector<int> chosen;
        for (int i = 0; i < count; ++i)
            if (settled[i] && previousEdge[i] >= 0) chosen.append(previousEdge[i]);
        TraceStep step = makeStep(kind, title, detail, settledOrder, chosen,
                                  focus >= 0 && distance[focus] < infinity ? distance[focus] : -1,
                                  active, rejected);
        for (qint64 value : distance) step.distances.append(value < infinity ? value : -1);
        if (focus >= 0 && distance[focus] < infinity) {
            const auto route = routeTo(focus);
            step.pathNodes = route.first;
            step.pathEdges = route.second;
        }
        return step;
    };

    result.steps.append(record(QStringLiteral("start"),
                               QStringLiteral("从 R%1 出发").arg(source),
                               QStringLiteral("起点距离为 0，目标是 R%1；∞ 表示尚未找到路径").arg(sink), start));
    while (true) {
        // 贪心选择：未确定且可达的节点中选距离最小者；平局按节点 id 决定。
        int current = -1;
        for (int i = 0; i < count; ++i)
            if (!settled[i] && distance[i] < infinity &&
                (current < 0 || distance[i] < distance[current] ||
                 (distance[i] == distance[current] && graph.nodes[i].id < graph.nodes[current].id)))
                current = i;
        if (current < 0) break;

        settled[current] = true;
        settledOrder.append(graph.nodes[current].id);
        result.steps.append(record(QStringLiteral("settle"),
                                   QStringLiteral("确定 R%1 的最短距离").arg(graph.nodes[current].id),
                                   QStringLiteral("从未确定节点中选距离最小者：%1").arg(distance[current]), current));
        // 非负权前提下，终点一旦被确定就可以结束，无须计算其余节点的最终距离。
        if (current == goal) break;

        for (const auto &link : graph.links) {
            if (!link.hasCost) continue;
            int neighbor = -1;
            // 有向边只允许 u→v；无向边在 current==v 时也允许回到 u。
            if (link.u == graph.nodes[current].id) neighbor = indices.value(link.v, -1);
            else if (!link.directed && link.v == graph.nodes[current].id)
                neighbor = indices.value(link.u, -1);
            if (neighbor < 0 || settled[neighbor]) continue;
            // 候选距离 = 当前最短距离 + 边造价；先检查余量，避免有符号加法溢出。
            const qint64 candidate = link.cost <= infinity - distance[current]
                                         ? distance[current] + link.cost : infinity;
            TraceStep considering = record(QStringLiteral("consider"),
                                           QStringLiteral("考察 R%1 → R%2")
                                               .arg(graph.nodes[current].id).arg(graph.nodes[neighbor].id),
                                           QStringLiteral("距离 %1 + 边权 %2 = 候选 %3")
                                               .arg(distance[current]).arg(link.cost).arg(candidate),
                                           current, link.id);
            considering.pathNodes.append(graph.nodes[neighbor].id);
            considering.pathEdges.append(link.id);
            result.steps.append(considering);

            // 松弛：发现严格更短的路线就同时更新距离、前驱节点和前驱边。
            if (candidate < distance[neighbor]) {
                distance[neighbor] = candidate;
                previousNode[neighbor] = current;
                previousEdge[neighbor] = link.id;
                result.steps.append(record(QStringLiteral("relax"),
                                           QStringLiteral("更新 R%1：%2").arg(graph.nodes[neighbor].id).arg(candidate),
                                           QStringLiteral("找到更短路线，暂定距离改为 %1").arg(candidate),
                                           neighbor, link.id));
            } else {
                result.steps.append(record(QStringLiteral("reject"),
                                           QStringLiteral("保留 R%1 的原距离").arg(graph.nodes[neighbor].id),
                                           QStringLiteral("候选路线没有更短，当前距离仍为 %1")
                                               .arg(distance[neighbor]), current, -1, link.id));
            }
        }
    }

    // 所有可达节点已处理仍未到终点，说明有向连通性或边权缺失导致不可达。
    if (distance[goal] >= infinity) {
        result.error = QStringLiteral("R%1 无法到达 R%2").arg(source).arg(sink);
        TraceStep failed = record(QStringLiteral("error"), QStringLiteral("终点不可达"),
                                  result.error, -1);
        failed.value = -1;
        result.steps.append(failed);
        return result;
    }
    result.success = true;
    result.value = distance[goal];
    TraceStep finish = record(QStringLiteral("finish"),
                              QStringLiteral("最短距离：%1").arg(result.value),
                              QString(), goal);
    finish.selectedEdges = finish.pathEdges;
    QStringList labels;
    for (int node : finish.pathNodes) labels.append(QStringLiteral("R%1").arg(node));
    finish.detail = labels.join(QStringLiteral(" → "));
    result.steps.append(finish);
    return result;
}
