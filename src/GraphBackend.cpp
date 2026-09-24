#include "GraphBackend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QVariantMap>
#include <algorithm>
#include <cmath>

namespace {
QVariantList integers(const QVector<int> &items) {
    QVariantList result;
    for (int item : items) result.append(item);
    return result;
}
QVariantList flows(const QVector<qint64> &items) {
    QVariantList result;
    for (qint64 item : items) result.append(item);
    return result;
}
QVariantList arcs(const QVector<ResidualArcState> &items) {
    QVariantList result;
    for (const auto &arc : items)
        result.append(QVariantMap{{"from", arc.from}, {"to", arc.to},
                                  {"linkId", arc.linkId}, {"remaining", arc.remaining},
                                  {"reverse", arc.reverse}});
    return result;
}
QVariantList paths(const QVector<FlowPath> &items) {
    QVariantList result;
    for (const auto &path : items)
        result.append(QVariantMap{{"nodes", integers(path.nodes)},
                                  {"linkIds", integers(path.linkIds)},
                                  {"amount", path.amount}});
    return result;
}
QVariantList nodeRecords(const Graph &graph) {
    QVariantList result;
    for (const auto &node : graph.nodes)
        result.append(QVariantMap{{"id", node.id}, {"name", node.name},
                                  {"x", node.x}, {"y", node.y}});
    return result;
}
QVariantList linkRecords(const Graph &graph) {
    QVariantList result;
    for (const auto &link : graph.links)
        result.append(QVariantMap{{"id", link.id}, {"u", link.u}, {"v", link.v},
                                  {"cost", link.cost}, {"capacity", link.capacity},
                                  {"directed", link.directed}, {"hasCost", link.hasCost},
                                  {"hasCapacity", link.hasCapacity}});
    return result;
}
bool parseGraph(const QVariantMap &record, Graph &graph) {
    const QVariantList nodes = record.value("nodes").toList();
    const QVariantList links = record.value("links").toList();
    if (nodes.size() > 200 || links.size() > 20000) return false;
    QSet<int> nodeIds, linkIds;
    for (const QVariant &item : nodes) {
        const QVariantMap data = item.toMap();
        bool idOk = false;
        const int id = data.value("id").toInt(&idOk);
        const QString name = data.value("name").toString();
        bool xOk = false, yOk = false;
        const double x = data.value("x").toDouble(&xOk);
        const double y = data.value("y").toDouble(&yOk);
        if (!idOk || id <= 0 || nodeIds.contains(id) || name.isEmpty() || name.size() > 24
            || !xOk || !yOk || !std::isfinite(x) || !std::isfinite(y)) return false;
        nodeIds.insert(id);
        graph.nodes.append({id, name, x, y});
    }
    for (const QVariant &item : links) {
        const QVariantMap data = item.toMap();
        bool idOk = false, uOk = false, vOk = false, costOk = false, capacityOk = false;
        const int id = data.value("id").toInt(&idOk);
        const int u = data.value("u").toInt(&uOk);
        const int v = data.value("v").toInt(&vOk);
        const qint64 cost = data.value("cost").toLongLong(&costOk);
        const qint64 capacity = data.value("capacity").toLongLong(&capacityOk);
        if (!idOk || !uOk || !vOk || !costOk || !capacityOk || id <= 0
            || linkIds.contains(id) || !nodeIds.contains(u) || !nodeIds.contains(v)
            || u == v || cost < 0 || capacity < 0) return false;
        linkIds.insert(id);
        graph.links.append({id, u, v, cost, capacity,
                            data.value("directed").toBool(), data.value("hasCost").toBool(),
                            data.value("hasCapacity").toBool()});
    }
    return true;
}
} // namespace

GraphBackend::GraphBackend(QObject *parent, const QString &presetFile)
    : QObject(parent), m_graph(forceDirectedLayout(sampleCampusGraph())),
      m_presetFile(presetFile.isEmpty()
                       ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                             + QStringLiteral("/presets.json")
                       : presetFile) {
    readPresets();
}

QVariantList GraphBackend::nodes() const { return nodeRecords(m_graph); }

QVariantList GraphBackend::links() const { return linkRecords(m_graph); }

QString GraphBackend::error() const { return m_error; }
QStringList GraphBackend::presetNames() const { return m_savedPresets.keys(); }

void GraphBackend::loadCostSample() {
    m_graph = forceDirectedLayout(sampleCostGraph());
    m_error.clear(); emit graphChanged(); emit topologyChanged(); emit errorChanged();
}
void GraphBackend::loadFlowSample() {
    m_graph = forceDirectedLayout(sampleFlowGraph());
    m_error.clear(); emit graphChanged(); emit topologyChanged(); emit errorChanged();
}
void GraphBackend::generateCampus(int count) {
    if (count < 2 || count > 200) {
        m_error = QStringLiteral("路由器数量应在 2 到 200 之间");
        emit errorChanged(); return;
    }
    m_graph = forceDirectedLayout(sampleCampusGraph(count));
    m_error.clear(); emit graphChanged(); emit topologyChanged(); emit errorChanged();
}
void GraphBackend::generateRandom(int count) {
    if (count < 2 || count > 200) {
        m_error = QStringLiteral("路由器数量应在 2 到 200 之间");
        emit errorChanged(); return;
    }
    Graph graph = sampleCampusGraph(count);
    graph.links.resize(count == 2 ? 1 : count);
    auto *random = QRandomGenerator::global();
    for (auto &link : graph.links) {
        link.cost = 4 + random->bounded(23);
        link.capacity = 8 + random->bounded(33);
    }
    const int targetChords = count / 2;
    int added = 0;
    for (int attempt = 0; attempt < count * 16 && added < targetChords && count >= 4; ++attempt) {
        const int u = 1 + random->bounded(count);
        const int offset = 2 + random->bounded(std::min(5, count - 2));
        const int v = (u - 1 + offset) % count + 1;
        bool duplicate = false;
        for (const auto &link : graph.links)
            if ((link.u == u && link.v == v) || (link.u == v && link.v == u)) {
                duplicate = true; break;
            }
        if (duplicate) continue;
        graph.links.append({int(graph.links.size()) + 1, u, v,
                            4 + random->bounded(23), 8 + random->bounded(33),
                            false, true, true});
        ++added;
    }
    m_graph = forceDirectedLayout(graph);
    m_error.clear(); emit graphChanged(); emit topologyChanged(); emit errorChanged();
}

void GraphBackend::readPresets() {
    QFile file(m_presetFile);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QVariantMap root = document.toVariant().toMap();
    if (root.value("version").toInt() != 1) return;
    for (const QVariant &item : root.value("presets").toList()) {
        const QVariantMap record = item.toMap();
        const QString name = record.value("name").toString();
        if (name.isEmpty() || name.size() > 32) continue;
        Graph graph;
        if (parseGraph(record, graph)) m_savedPresets.insert(name, graph);
    }
}

bool GraphBackend::writePresets() {
    if (!QDir().mkpath(QFileInfo(m_presetFile).absolutePath())) {
        m_error = QStringLiteral("无法创建预设保存目录"); return false;
    }
    QVariantList items;
    for (auto it = m_savedPresets.cbegin(); it != m_savedPresets.cend(); ++it)
        items.append(QVariantMap{{"name", it.key()}, {"nodes", nodeRecords(it.value())},
                                 {"links", linkRecords(it.value())}});
    const QByteArray bytes = QJsonDocument::fromVariant(
        QVariantMap{{"version", 1}, {"presets", items}}).toJson(QJsonDocument::Indented);
    QSaveFile file(m_presetFile);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        m_error = QStringLiteral("保存预设失败：%1").arg(file.errorString()); return false;
    }
    return true;
}

bool GraphBackend::savePreset(const QString &name) {
    const QString clean = name.trimmed();
    if (clean.isEmpty() || clean.size() > 32) {
        m_error = QStringLiteral("预设名称应为 1 到 32 个字符");
        emit errorChanged(); return false;
    }
    const bool existed = m_savedPresets.contains(clean);
    const Graph previous = m_savedPresets.value(clean);
    m_savedPresets.insert(clean, m_graph);
    if (!writePresets()) {
        if (existed) m_savedPresets.insert(clean, previous);
        else m_savedPresets.remove(clean);
        emit errorChanged(); return false;
    }
    m_error.clear(); emit presetsChanged(); emit errorChanged(); return true;
}

bool GraphBackend::loadPreset(const QString &name) {
    if (!m_savedPresets.contains(name)) {
        m_error = QStringLiteral("找不到这个预设网络");
        emit errorChanged(); return false;
    }
    m_graph = m_savedPresets.value(name);
    m_error.clear(); emit graphChanged(); emit topologyChanged(); emit errorChanged();
    return true;
}
void GraphBackend::autoLayout() {
    m_graph = forceDirectedLayout(m_graph);
    emit graphChanged();
}
void GraphBackend::setPositions(const QVariantList &positions) {
    for (const auto &entry : positions) {
        const QVariantMap point = entry.toMap();
        const int id = point.value("id").toInt();
        const double x = point.value("x").toDouble();
        const double y = point.value("y").toDouble();
        if (!std::isfinite(x) || !std::isfinite(y)) continue;
        for (auto &node : m_graph.nodes) if (node.id == id) {
            node.x = x; node.y = y; break;
        }
    }
    emit graphChanged();
}

bool GraphBackend::renameNode(int id, const QString &name) {
    const QString clean = name.trimmed();
    if (clean.isEmpty() || clean.size() > 24) {
        m_error = QStringLiteral("节点名称应为 1 到 24 个字符");
        emit errorChanged(); return false;
    }
    for (auto &node : m_graph.nodes) if (node.id == id) {
        node.name = clean; m_error.clear();
        emit graphChanged(); emit topologyChanged(); emit errorChanged(); return true;
    }
    m_error = QStringLiteral("找不到这个节点"); emit errorChanged(); return false;
}

bool GraphBackend::moveNode(int id, double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
    for (auto &node : m_graph.nodes) if (node.id == id) {
        node.x = x; node.y = y; emit graphChanged(); return true;
    }
    return false;
}

bool GraphBackend::removeNode(int id) {
    for (int i = 0; i < m_graph.nodes.size(); ++i) if (m_graph.nodes[i].id == id) {
        m_graph.nodes.removeAt(i);
        m_graph.links.erase(std::remove_if(m_graph.links.begin(), m_graph.links.end(),
                                           [=](const RouterLink &link) { return link.u == id || link.v == id; }),
                            m_graph.links.end());
        emit graphChanged(); emit topologyChanged(); return true;
    }
    return false;
}

int GraphBackend::addNode(double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return -1;
    int id = 0;
    for (const auto &node : m_graph.nodes) id = std::max(id, node.id);
    ++id;
    m_graph.nodes.append({id, QStringLiteral("R%1").arg(id), x, y});
    emit graphChanged(); emit topologyChanged(); return id;
}

int GraphBackend::addLink(int u, int v, qint64 cost, qint64 capacity, bool directed) {
    if (u == v || !hasNode(u) || !hasNode(v) || cost < 0 || capacity < 0) {
        m_error = QStringLiteral("请选择两个不同节点，并输入非负造价与容量");
        emit errorChanged(); return -1;
    }
    for (const auto &link : m_graph.links)
        if ((link.u == u && link.v == v) || (link.u == v && link.v == u)) {
            m_error = QStringLiteral("这两个节点之间已有链路，可直接修改");
            emit errorChanged(); return -1;
        }
    int id = 0;
    for (const auto &link : m_graph.links) id = std::max(id, link.id);
    ++id;
    m_graph.links.append({id, u, v, cost, capacity, directed, true, true});
    m_error.clear(); emit graphChanged(); emit topologyChanged(); emit errorChanged(); return id;
}

bool GraphBackend::updateLink(int id, qint64 cost, qint64 capacity, bool directed) {
    if (cost < 0 || capacity < 0) {
        m_error = QStringLiteral("造价和容量不能为负数"); emit errorChanged(); return false;
    }
    for (auto &link : m_graph.links) if (link.id == id) {
        link.cost = cost; link.capacity = capacity; link.directed = directed;
        link.hasCost = true; link.hasCapacity = true;
        m_error.clear(); emit graphChanged(); emit topologyChanged(); emit errorChanged(); return true;
    }
    m_error = QStringLiteral("找不到这条链路"); emit errorChanged(); return false;
}

bool GraphBackend::removeLink(int id) {
    for (int i = 0; i < m_graph.links.size(); ++i) if (m_graph.links[i].id == id) {
        m_graph.links.removeAt(i); emit graphChanged(); emit topologyChanged(); return true;
    }
    return false;
}

QString GraphBackend::nodeName(int id) const {
    for (const auto &node : m_graph.nodes) if (node.id == id) return node.name;
    return {};
}
qint64 GraphBackend::linkCost(int id) const {
    for (const auto &link : m_graph.links) if (link.id == id) return link.cost;
    return -1;
}
qint64 GraphBackend::linkCapacity(int id) const {
    for (const auto &link : m_graph.links) if (link.id == id) return link.capacity;
    return -1;
}
bool GraphBackend::hasNode(int id) const {
    for (const auto &node : m_graph.nodes) if (node.id == id) return true;
    return false;
}
bool GraphBackend::hasIncidentLink(int id) const {
    for (const auto &link : m_graph.links) if (link.u == id || link.v == id) return true;
    return false;
}

QVariantMap GraphBackend::run(const QString &algorithm, int source, int sink) const {
    Trace trace;
    if (algorithm == QStringLiteral("prim")) trace = runPrim(m_graph, source);
    else if (algorithm == QStringLiteral("kruskal")) trace = runKruskal(m_graph);
    else if (algorithm == QStringLiteral("flow")) trace = runMaxFlow(m_graph, source, sink);
    else trace.error = QStringLiteral("未知算法");
    QVariantList steps;
    for (const auto &item : trace.steps)
        steps.append(QVariantMap{{"kind", item.kind}, {"title", item.title}, {"detail", item.detail},
                                 {"visitedNodes", integers(item.visitedNodes)},
                                 {"selectedEdges", integers(item.selectedEdges)},
                                 {"pathNodes", integers(item.pathNodes)},
                                 {"linkFlows", flows(item.linkFlows)},
                                 {"residualArcs", arcs(item.residualArcs)},
                                 {"pathArcs", arcs(item.pathArcs)},
                                 {"flowPaths", paths(item.flowPaths)},
                                 {"bottleneck", item.bottleneck},
                                 {"activeEdge", item.activeEdge}, {"rejectedEdge", item.rejectedEdge},
                                 {"value", item.value}});
    return {{"success", trace.success}, {"error", trace.error},
            {"value", trace.value}, {"steps", steps}};
}
