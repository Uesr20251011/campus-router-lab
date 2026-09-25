#include "GraphAlgorithms.h"

#include <QHash>
#include <QSet>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <random>

namespace {
QHash<int, int> indicesOf(const Graph &graph) {
    QHash<int, int> indices;
    for (int i = 0; i < graph.nodes.size(); ++i) indices.insert(graph.nodes[i].id, i);
    return indices;
}

TraceStep makeStep(const QString &kind, const QString &title, const QString &detail,
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

Graph figureNodes() {
    Graph graph;
    graph.nodes = {{1, QStringLiteral("R1"), 90, 270},
                   {2, QStringLiteral("R2"), 260, 450},
                   {3, QStringLiteral("R3"), 260, 90},
                   {4, QStringLiteral("R4"), 395, 270},
                   {5, QStringLiteral("R5"), 585, 450},
                   {6, QStringLiteral("R6"), 650, 270},
                   {7, QStringLiteral("R7"), 650, 90},
                   {8, QStringLiteral("R8"), 850, 270}};
    return graph;
}
} // namespace

Graph sampleCostGraph() {
    Graph graph = figureNodes();
    const int data[][3] = {{1, 3, 8}, {1, 4, 12}, {1, 2, 6}, {2, 4, 7}, {2, 5, 8},
                           {2, 6, 13}, {3, 7, 11}, {3, 6, 12}, {4, 7, 6}, {4, 5, 5},
                           {5, 6, 12}, {5, 8, 15}, {6, 7, 4}, {6, 8, 7}, {7, 8, 5}};
    int id = 1;
    for (const auto &row : data)
        graph.links.append({id++, row[0], row[1], row[2], 0, false, true, false});
    return graph;
}

Graph sampleFlowGraph() {
    Graph graph = figureNodes();
    const int data[][3] = {{1, 3, 7}, {1, 4, 4}, {1, 2, 5}, {2, 4, 2}, {2, 5, 8},
                           {3, 7, 4}, {4, 7, 6}, {4, 5, 5}, {5, 6, 7}, {5, 8, 6},
                           {7, 6, 4}, {6, 8, 7}, {7, 8, 5}};
    int id = 1;
    for (const auto &row : data)
        graph.links.append({id++, row[0], row[1], 0, row[2], true, false, true});
    return graph;
}

Graph sampleCampusGraph(int count) {
    Graph graph;
    count = std::max(2, count);
    constexpr double pi = 3.141592653589793;
    for (int id = 1; id <= count; ++id) {
        const double angle = 2.0 * pi * (id - 1) / count;
        const double radiusX = 337.0 + 15.0 * std::sin(id * 2.17);
        const double radiusY = 196.0 + 12.0 * std::cos(id * 1.83);
        graph.nodes.append({id, QStringLiteral("R%1").arg(id),
                            470.0 + radiusX * std::cos(angle),
                            305.0 + radiusY * std::sin(angle)});
    }
    auto addLink = [&](int u, int v) {
        if (u == v) return;
        for (const auto &link : graph.links)
            if ((link.u == u && link.v == v) || (link.u == v && link.v == u)) return;
        const int id = graph.links.size() + 1;
        const qint64 cost = 4 + ((u * 11 + v * 7) % 23);
        const qint64 capacity = 8 + ((u * 5 + v * 13) % 33);
        graph.links.append({id, u, v, cost, capacity, false, true, true});
    };
    for (int id = 1; id <= count; ++id) addLink(id, id == count ? 1 : id + 1);
    // Short chords keep the example readable while still providing alternate routes.
    for (int id = 1; id <= count; id += 2)
        addLink(id, (id + 1) % count + 1);
    return graph;
}

Graph forceDirectedLayout(const Graph &source, double width, double height) {
    Graph graph = source;
    const int count = graph.nodes.size();
    if (count == 0 || width < 120 || height < 120) return graph;
    const auto indices = indicesOf(graph);
    const double desired = std::clamp(std::sqrt(width * height / count) * 0.67, 82.0, 150.0);
    const double centerX = width / 2.0;
    const double centerY = height / 2.0;
    const Graph starting = graph;
    const double marginX = count > 10 ? 75.0 : 36.0;
    const double marginY = count > 10 ? 82.0 : 36.0;
    QVector<double> vx(count, 0.0), vy(count, 0.0);
    for (int iteration = 0; iteration < 320; ++iteration) {
        QVector<double> fx(count, 0.0), fy(count, 0.0);
        for (int i = 0; i < count; ++i) {
            for (int j = i + 1; j < count; ++j) {
                double dx = graph.nodes[i].x - graph.nodes[j].x;
                double dy = graph.nodes[i].y - graph.nodes[j].y;
                if (std::abs(dx) + std::abs(dy) < 0.01) {
                    dx = 0.1 * (i + 1); dy = 0.1 * (j + 1);
                }
                const double distance = std::max(1.0, std::hypot(dx, dy));
                const double force = 2.0 * desired * desired / (distance * distance);
                fx[i] += force * dx / distance; fy[i] += force * dy / distance;
                fx[j] -= force * dx / distance; fy[j] -= force * dy / distance;
                if (distance < 96.0) {
                    const double collision = (96.0 - distance) * 0.20;
                    fx[i] += collision * dx / distance; fy[i] += collision * dy / distance;
                    fx[j] -= collision * dx / distance; fy[j] -= collision * dy / distance;
                }
            }
        }
        for (const auto &link : graph.links) {
            const int u = indices.value(link.u, -1), v = indices.value(link.v, -1);
            if (u < 0 || v < 0) continue;
            const double dx = graph.nodes[v].x - graph.nodes[u].x;
            const double dy = graph.nodes[v].y - graph.nodes[u].y;
            const double distance = std::max(1.0, std::hypot(dx, dy));
            const double pull = (distance - desired * 1.12) * 0.006;
            fx[u] += pull * dx / distance; fy[u] += pull * dy / distance;
            fx[v] -= pull * dx / distance; fy[v] -= pull * dy / distance;
            for (int k = 0; k < count; ++k) {
                if (k == u || k == v) continue;
                const double lineX = graph.nodes[v].x - graph.nodes[u].x;
                const double lineY = graph.nodes[v].y - graph.nodes[u].y;
                const double t = std::clamp(((graph.nodes[k].x - graph.nodes[u].x) * lineX +
                                              (graph.nodes[k].y - graph.nodes[u].y) * lineY) /
                                                 std::max(1.0, lineX * lineX + lineY * lineY), 0.15, 0.85);
                const double awayX = graph.nodes[k].x - graph.nodes[u].x - t * lineX;
                const double awayY = graph.nodes[k].y - graph.nodes[u].y - t * lineY;
                const double clearance = std::max(1.0, std::hypot(awayX, awayY));
                if (clearance >= 54.0) continue;
                const double push = (54.0 - clearance) * 0.035;
                fx[k] += push * awayX / clearance;
                fy[k] += push * awayY / clearance;
            }
        }
        const double temperature = 8.0 * (1.0 - iteration / 360.0);
        for (int i = 0; i < count; ++i) {
            fx[i] += (centerX - graph.nodes[i].x) * 0.001;
            fy[i] += (centerY - graph.nodes[i].y) * 0.001;
            if (count > 10) {
                fx[i] += (starting.nodes[i].x - graph.nodes[i].x) * 0.018;
                fy[i] += (starting.nodes[i].y - graph.nodes[i].y) * 0.018;
            }
            vx[i] = (vx[i] + fx[i]) * 0.72;
            vy[i] = (vy[i] + fy[i]) * 0.72;
            const double speed = std::max(1.0, std::hypot(vx[i], vy[i]));
            graph.nodes[i].x = std::clamp(graph.nodes[i].x + vx[i] * std::min(1.0, temperature / speed), marginX, width - marginX);
            graph.nodes[i].y = std::clamp(graph.nodes[i].y + vy[i] * std::min(1.0, temperature / speed), marginY, height - marginY);
        }
    }
    return graph;
}

Trace runPrim(const Graph &graph, int start) {
    Trace result;
    const auto indices = indicesOf(graph);
    if (graph.nodes.isEmpty() || !indices.contains(start)) {
        result.error = QStringLiteral("请选择存在的起点"); return result;
    }
    struct Candidate { qint64 cost; int index; int to; };
    auto compare = [&](const Candidate &a, const Candidate &b) {
        if (a.cost != b.cost) return a.cost > b.cost;
        return graph.links[a.index].id > graph.links[b.index].id;
    };
    std::priority_queue<Candidate, std::vector<Candidate>, decltype(compare)> queue(compare);
    QSet<int> seen{start};
    QVector<int> visited{start}, selected;
    qint64 total = 0;
    result.steps.append(makeStep(QStringLiteral("start"), QStringLiteral("从 R%1 出发").arg(start),
                                 QStringLiteral("向外寻找造价最低的连接"), visited, selected, total));
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
        if (seen.contains(next.to)) {
            result.steps.append(makeStep(QStringLiteral("reject"), QStringLiteral("跳过这条边"),
                                         QStringLiteral("两端已经连通"), visited, selected, total, -1, link.id));
            continue;
        }
        seen.insert(next.to); visited.append(next.to); selected.append(link.id); total += link.cost;
        result.steps.append(makeStep(QStringLiteral("accept"), QStringLiteral("接入 R%1").arg(next.to),
                                     QStringLiteral("累计造价为 %1").arg(total), visited, selected, total, link.id));
        addFrontier(next.to);
    }
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

Trace runKruskal(const Graph &graph) {
    Trace result;
    if (graph.nodes.isEmpty()) { result.error = QStringLiteral("网络中没有路由器"); return result; }
    QVector<int> order;
    for (int i = 0; i < graph.links.size(); ++i) if (graph.links[i].hasCost) order.append(i);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const auto &left = graph.links[a], &right = graph.links[b];
        if (left.cost != right.cost) return left.cost < right.cost;
        return left.id < right.id;
    });
    const auto indices = indicesOf(graph);
    QVector<int> parent(graph.nodes.size()); std::iota(parent.begin(), parent.end(), 0);
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
        const int a = find(indices.value(link.u)), b = find(indices.value(link.v));
        if (a == b) {
            result.steps.append(makeStep(QStringLiteral("reject"), QStringLiteral("跳过这条边"),
                                         QStringLiteral("加入它会形成环"), visited, selected, total, -1, link.id));
            continue;
        }
        parent[a] = b; selected.append(link.id); total += link.cost;
        if (!visited.contains(link.u)) visited.append(link.u);
        if (!visited.contains(link.v)) visited.append(link.v);
        result.steps.append(makeStep(QStringLiteral("accept"), QStringLiteral("选用这条边"),
                                     QStringLiteral("累计造价为 %1").arg(total), visited, selected, total, link.id));
    }
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

Trace runDijkstra(const Graph &graph, int source, int sink) {
    Trace result;
    const auto indices = indicesOf(graph);
    if (!indices.contains(source) || !indices.contains(sink)) {
        result.error = QStringLiteral("请选择存在的起点和终点");
        return result;
    }

    const int count = graph.nodes.size();
    const int start = indices.value(source), goal = indices.value(sink);
    const qint64 infinity = std::numeric_limits<qint64>::max() / 4;
    QVector<qint64> distance(count, infinity);
    QVector<int> previousNode(count, -1), previousEdge(count, -1);
    QVector<char> settled(count, false);
    QVector<int> settledOrder;
    distance[start] = 0;

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
        if (current == goal) break;

        for (const auto &link : graph.links) {
            if (!link.hasCost) continue;
            int neighbor = -1;
            if (link.u == graph.nodes[current].id) neighbor = indices.value(link.v, -1);
            else if (!link.directed && link.v == graph.nodes[current].id)
                neighbor = indices.value(link.u, -1);
            if (neighbor < 0 || settled[neighbor]) continue;
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

Trace runMaxFlow(const Graph &graph, int source, int sink) {
    Trace result;
    const auto indices = indicesOf(graph);
    if (!indices.contains(source) || !indices.contains(sink) || source == sink) {
        result.error = QStringLiteral("请选择两个不同且存在的路由器");
        return result;
    }
    struct Arc { int to; int reverseIndex; qint64 capacity; int linkIndex; int sign; bool isReverse; };
    QVector<QVector<Arc>> residual(graph.nodes.size());
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
        if (!link.directed) addArc(v, u, link.capacity, i, -1);
    }
    QVector<qint64> flows(graph.links.size(), 0);
    qint64 total = 0;
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
        if (parent[goal] == -1) break;
        TraceStep found = makeStep(QStringLiteral("path"), QStringLiteral("找到增广路"),
                                   QString(),
                                   visited, {}, total);
        routeTo(goal, found);
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
