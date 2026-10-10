#include "AlgorithmSupport.h"
#include <QSet>
#include <QStringList>
#include <algorithm>
#include <limits>
#include <numeric>
#include <queue>

using namespace GraphAlgorithmDetail;

/* Prim：从指定起点逐步扩展最小生成树。
 * 核心不变量：seen 中的节点已经由 selected 中的边连成一棵树。
 * 每次从跨越“树内/树外”的候选边中取最低造价边（割性质保证安全）。
 * 本项目将建设链路按无向连接处理，即 directed 不限制 MST；仅用 hasCost 边。
 * 这里用最小堆，但每次扩展仍扫描全部 E 条边：不含演示快照的主计算
 * 约为 O(VE + E log E)，不是邻接表版 Prim 的 O(E log V)。
 */
Trace runPrim(const Graph &graph, int start) {
    Trace result;
    const auto indices = indicesOf(graph);
    if (graph.nodes.isEmpty() || !indices.contains(start)) {
        result.error = QStringLiteral("请选择存在的起点"); return result;
    }
    // index 是原始 links 数组下标，to 是即将接入的节点 id。
    struct Candidate { qint64 cost; int index; int to; };
    auto compare = [&](const Candidate &a, const Candidate &b) {
        // priority_queue 默认最大堆；反向比较使最小造价位于堆顶。
        // 相同造价按链路 id 打破平局，使每次演示的选择顺序可复现。
        if (a.cost != b.cost) return a.cost > b.cost;
        return graph.links[a.index].id > graph.links[b.index].id;
    };
    std::priority_queue<Candidate, std::vector<Candidate>, decltype(compare)> queue(compare);
    QSet<int> seen{start};
    QVector<int> visited{start}, selected;
    qint64 total = 0;
    result.steps.append(makeStep(QStringLiteral("start"), QStringLiteral("从 R%1 出发").arg(start),
                                 QStringLiteral("向外寻找造价最低的连接"), visited, selected, total));
    // 新节点加入树后，把它连接树外节点的边放入堆。
    // 无须主动删除旧候选：取出时再用 seen 判断是否已失效（惰性删除）。
    auto addFrontier = [&](int node) {
        for (int i = 0; i < graph.links.size(); ++i) {
            const auto &link = graph.links[i];
            if (!link.hasCost) continue;
            if (link.u == node && !seen.contains(link.v)) queue.push({link.cost, i, link.v});
            else if (link.v == node && !seen.contains(link.u)) queue.push({link.cost, i, link.u});
        }
    };
    addFrontier(start);
    while (!queue.empty() && selected.size() < graph.nodes.size() - 1) {
        const Candidate next = queue.top(); queue.pop();
        const auto &link = graph.links[next.index];
        result.steps.append(makeStep(QStringLiteral("consider"), QStringLiteral("考察 R%1—R%2").arg(link.u).arg(link.v),
                                     QStringLiteral("建设造价为 %1").arg(link.cost), visited, selected, total, link.id));
        // 另一条便宜边可能已经接入 to。再接入会形成环，因此跳过。
        if (seen.contains(next.to)) {
            result.steps.append(makeStep(QStringLiteral("reject"), QStringLiteral("跳过这条边"),
                                         QStringLiteral("两端已经连通"), visited, selected, total, -1, link.id));
            continue;
        }
        // 只有确认目标在树外才真正接入；每次恰好增加一节点和一边。
        seen.insert(next.to); visited.append(next.to); selected.append(link.id); total += link.cost;
        result.steps.append(makeStep(QStringLiteral("accept"), QStringLiteral("接入 R%1").arg(next.to),
                                     QStringLiteral("累计造价为 %1").arg(total), visited, selected, total, link.id));
        addFrontier(next.to);
    }
    // 连通的 V 节点生成树恰有 V-1 条边。队列提前耗尽说明剩余节点不可接入。
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
