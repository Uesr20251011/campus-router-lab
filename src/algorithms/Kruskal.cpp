#include "AlgorithmSupport.h"
#include <QSet>
#include <QStringList>
#include <algorithm>
#include <limits>
#include <numeric>
#include <queue>

using namespace GraphAlgorithmDetail;

/* Kruskal：从全图最便宜的边开始，连接不同连通分量。
 * 核心不变量：selected 始终是一片无环森林，parent 表示森林的连通分量。
 * 与 Prim 相同，建设造价按无向连接处理，仅筛选 hasCost 边。
 * 排序 O(E log E)；并查集使用路径折半，没有按秩合并，不能直接宣称
 * 是“按秩 + 路径压缩”的 O(alpha(V)) 实现；visited 的线性查重也有额外开销。
 */
Trace runKruskal(const Graph &graph) {
    Trace result;
    if (graph.nodes.isEmpty()) { result.error = QStringLiteral("网络中没有路由器"); return result; }
    // 只排序边的下标，不改动原图；演示仍能通过原链路 id 定位边。
    QVector<int> order;
    for (int i = 0; i < graph.links.size(); ++i) if (graph.links[i].hasCost) order.append(i);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const auto &left = graph.links[a], &right = graph.links[b];
        if (left.cost != right.cost) return left.cost < right.cost;
        return left.id < right.id;
    });
    const auto indices = indicesOf(graph);
    // 初始时每个节点都是一个单独的分量：parent[i] = i。
    QVector<int> parent(graph.nodes.size()); std::iota(parent.begin(), parent.end(), 0);
    // 路径折半：沿父指针寻找根，同时把当前节点挂到祖父节点上，缩短后续查询。
    auto find = [&](int x) { while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; } return x; };
    QVector<int> selected, visited;
    qint64 total = 0;
    result.steps.append(makeStep(QStringLiteral("start"), QStringLiteral("按造价排列链路"),
                                 QStringLiteral("从最便宜的链路开始检查"), visited, selected, total));
    for (int index : order) {
        if (selected.size() == graph.nodes.size() - 1) break;
        const auto &link = graph.links[index];
        result.steps.append(makeStep(QStringLiteral("consider"), QStringLiteral("考察 R%1—R%2").arg(link.u).arg(link.v),
                                     QStringLiteral("建设造价为 %1").arg(link.cost), visited, selected, total, link.id));
        // 判断两端是否属于同一分量：同根代表已经连通，加边必成环。
        const int a = find(indices.value(link.u)), b = find(indices.value(link.v));
        if (a == b) {
            result.steps.append(makeStep(QStringLiteral("reject"), QStringLiteral("跳过这条边"),
                                         QStringLiteral("加入它会形成环"), visited, selected, total, -1, link.id));
            continue;
        }
        // 不同根才接受此边，并合并两分量；本步选的是剩余候选中最便宜的可用边。
        parent[a] = b; selected.append(link.id); total += link.cost;
        if (!visited.contains(link.u)) visited.append(link.u);
        if (!visited.contains(link.v)) visited.append(link.v);
        result.steps.append(makeStep(QStringLiteral("accept"), QStringLiteral("选用这条边"),
                                     QStringLiteral("累计造价为 %1").arg(total), visited, selected, total, link.id));
    }
    // V-1 条无环边才能覆盖全部 V 个节点；不足时报告不连通而非返回“生成树”。
    if (selected.size() != graph.nodes.size() - 1) {
        result.error = QStringLiteral("网络不连通，无法形成生成树");
        result.steps.append(makeStep(QStringLiteral("error"), QStringLiteral("无法完成建网"), result.error,
                                     visited, selected, total));
        return result;
    }
    result.success = true; result.value = total;
    result.steps.append(makeStep(QStringLiteral("finish"), QStringLiteral("最低造价：%1").arg(total),
                                 QStringLiteral("用 %1 条链路连接全部路由器").arg(selected.size()),
                                 visited, selected, total));
    return result;
}
