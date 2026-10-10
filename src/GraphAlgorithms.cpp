#include "GraphAlgorithms.h"
#include "algorithms/AlgorithmSupport.h"

using GraphAlgorithmDetail::indicesOf;

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
    // Corrected saved throughput example, including its original node positions.
    Graph graph;
    graph.nodes = {
        {1, QStringLiteral("R1"), 251.0742585596222, 313.5831219520687},
        {2, QStringLiteral("R2"), 355.85741032888177, 468.4225796112485},
        {3, QStringLiteral("R3"), 337.3550200076425, 156.06336284819673},
        {4, QStringLiteral("R4"), 419.33123582794536, 311.58155027447873},
        {5, QStringLiteral("R5"), 545.9536772643086, 445.8295983403202},
        {6, QStringLiteral("R6"), 585.3112177581326, 301.83459824733},
        {7, QStringLiteral("R7"), 522.6885327938718, 167.56752247016072},
        {8, QStringLiteral("R8"), 725.1578742181406, 296.33156557081224}
    };
    graph.links = {
        {1, 1, 3, 0, 7, true, false, true},
        {2, 1, 4, 0, 4, true, false, true},
        {3, 1, 2, 0, 5, true, false, true},
        {5, 2, 5, 0, 8, true, false, true},
        {6, 3, 7, 0, 4, true, false, true},
        {7, 4, 7, 0, 6, true, false, true},
        {8, 4, 5, 0, 5, true, false, true},
        {9, 5, 6, 0, 12, true, true, true},
        {10, 5, 8, 0, 6, true, false, true},
        {11, 7, 6, 0, 4, true, false, true},
        {12, 6, 8, 0, 7, true, false, true},
        {13, 7, 8, 0, 5, true, false, true},
        {14, 4, 3, 0, 2, true, true, true}
    };
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
    // 受力排布仅改变节点坐标，不修改边、方向、造价或容量，不参与四个图算法结果。
    // 每轮先叠加节点间斥力、链路弹簧力和节点远离边线的推力，再加阻尼更新位置。
    // 320 次迭代是显示用固定预算，不是“求解最优布局”的数学保证。
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
                // 距离越小斥力越大；极小距离加扰动避免除零，同位置节点也能分开。
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
            // 链路像弹簧：过长则拉近，过短则推远，使相邻节点保持适当距离。
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
        // 降温限制单步位移，0.72 阻尼降低震荡；clamp 把节点保留在画布边界内。
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
