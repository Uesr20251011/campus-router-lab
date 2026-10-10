#pragma once
#include "GraphAlgorithms.h"
#include <QHash>

namespace GraphAlgorithmDetail {
// 节点 id 不要求连续。用哈希表把界面的 id 映射到 QVector 的连续下标，
// 避免删除节点后把 id 当成数组下标使用。
inline QHash<int, int> indicesOf(const Graph &graph) {
    QHash<int, int> indices;
    for (int i = 0; i < graph.nodes.size(); ++i) indices.insert(graph.nodes[i].id, i);
    return indices;
}

// 构造独立的演示快照：这里仅组织显示数据，不参与贪心选择或流量计算。
// visited/selected 保存节点/链路的外部 id；active/rejected 指出本步高亮边。
inline TraceStep makeStep(const QString &kind, const QString &title, const QString &detail,
                   const QVector<int> &visited, const QVector<int> &selected,
                   qint64 total, int active = -1, int rejected = -1) {
    TraceStep state;
    state.kind = kind;
    state.title = title;
    state.detail = detail;
    state.visitedNodes = visited;
    state.selectedEdges = selected;
    state.value = total;
    state.activeEdge = active;
    state.rejectedEdge = rejected;
    return state;
}

} // namespace GraphAlgorithmDetail
