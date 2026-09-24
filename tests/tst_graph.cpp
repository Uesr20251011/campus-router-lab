#include <QtTest>
#include <QTemporaryDir>

#include "GraphAlgorithms.h"
#include "GraphBackend.h"

class GraphTests : public QObject {
    Q_OBJECT

private slots:
    void figure2HasMinimumCost41();
    void figure3HasMaximumFlow13();
    void disconnectedGraphCannotHaveSpanningTree();
    void editingNodeAndLinkChangesGraph();
    void automaticLayoutAvoidsOverlapAndIsRepeatable();
    void flowTraceShowsSearchUpdatesAndReverseResidual();
    void finalFlowIsDecomposedIntoVisibleRoutes();
    void savedPresetRestoresEditedNetwork();
    void randomNetworkStaysConnected();
};

void GraphTests::figure2HasMinimumCost41() {
    const Graph graph = sampleCostGraph();
    const Trace prim = runPrim(graph, 1);
    const Trace kruskal = runKruskal(graph);
    QVERIFY(prim.success);
    QVERIFY(kruskal.success);
    QCOMPARE(prim.value, qint64(41));
    QCOMPARE(kruskal.value, qint64(41));
    QCOMPARE(prim.steps.last().selectedEdges.size(), 7);
    QCOMPARE(kruskal.steps.last().selectedEdges.size(), 7);
    QCOMPARE(prim.steps.last().kind, QStringLiteral("finish"));
    QCOMPARE(kruskal.steps.last().kind, QStringLiteral("finish"));
    QVERIFY(prim.steps.size() > 8);
    QVERIFY(kruskal.steps.size() > 8);
}

void GraphTests::figure3HasMaximumFlow13() {
    const Trace flow = runMaxFlow(sampleFlowGraph(), 1, 8);
    QVERIFY(flow.success);
    QCOMPARE(flow.value, qint64(13));
    QCOMPARE(flow.steps.last().value, qint64(13));
    QVERIFY(flow.steps.size() > 3);
    bool hasPath = false;
    for (const auto &step : flow.steps)
        hasPath |= step.pathNodes.size() >= 2;
    QVERIFY(hasPath);
}

void GraphTests::disconnectedGraphCannotHaveSpanningTree() {
    Graph graph;
    graph.nodes = {{1, "A", 0, 0}, {2, "B", 100, 0}, {3, "C", 200, 0}};
    graph.links = {{1, 1, 2, 4, 10, false, true, true}};
    QVERIFY(!runPrim(graph, 1).success);
    QVERIFY(!runKruskal(graph).success);
}

void GraphTests::editingNodeAndLinkChangesGraph() {
    GraphBackend backend;
    backend.loadCostSample();
    QVERIFY(backend.renameNode(1, "主教学楼"));
    QCOMPARE(backend.nodeName(1), QStringLiteral("主教学楼"));
    QVERIFY(backend.updateLink(1, 19, 27, false));
    QCOMPARE(backend.linkCost(1), qint64(19));
    QCOMPARE(backend.linkCapacity(1), qint64(27));
    QVERIFY(backend.removeNode(1));
    QVERIFY(!backend.hasNode(1));
    QVERIFY(!backend.hasIncidentLink(1));
}

void GraphTests::savedPresetRestoresEditedNetwork() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString file = directory.filePath(QStringLiteral("presets.json"));
    GraphBackend first(nullptr, file);
    first.loadFlowSample();
    QVERIFY(first.renameNode(1, QStringLiteral("教学楼入口")));
    QVERIFY(first.moveNode(1, 123.5, 210.25));
    QVERIFY(first.updateLink(1, 19, 27, true));
    QVERIFY(first.savePreset(QStringLiteral("我的流量图")));

    GraphBackend restored(nullptr, file);
    QVERIFY(restored.presetNames().contains(QStringLiteral("我的流量图")));
    QVERIFY(restored.loadPreset(QStringLiteral("我的流量图")));
    QCOMPARE(restored.nodeName(1), QStringLiteral("教学楼入口"));
    QCOMPARE(restored.linkCost(1), qint64(19));
    QCOMPARE(restored.linkCapacity(1), qint64(27));
    QCOMPARE(restored.links().first().toMap().value("directed").toBool(), true);
    QCOMPARE(restored.nodes().first().toMap().value("x").toDouble(), 123.5);
    QCOMPARE(restored.nodes().first().toMap().value("y").toDouble(), 210.25);
    QCOMPARE(restored.run(QStringLiteral("flow"), 1, 8).value("success").toBool(), true);
}

void GraphTests::randomNetworkStaysConnected() {
    GraphBackend backend;
    backend.generateRandom(20);
    QCOMPARE(backend.nodes().size(), 20);
    QVERIFY(backend.links().size() >= 19);
    QVERIFY(backend.run(QStringLiteral("kruskal")).value("success").toBool());
    const QVariantList first = backend.links();
    backend.generateRandom(20);
    QVERIFY(backend.links() != first);
    QVERIFY(backend.run(QStringLiteral("kruskal")).value("success").toBool());
}

void GraphTests::automaticLayoutAvoidsOverlapAndIsRepeatable() {
    const Graph source = sampleCampusGraph(20);
    for (const auto &link : source.links) {
        const int aroundRing = std::abs(link.u - link.v);
        QVERIFY(std::min(aroundRing, 20 - aroundRing) <= 2);
    }
    const Graph first = forceDirectedLayout(source, 940, 610);
    const Graph second = forceDirectedLayout(source, 940, 610);
    QCOMPARE(first.nodes.size(), 20);
    for (int i = 0; i < first.nodes.size(); ++i) {
        QCOMPARE(first.nodes[i].x, second.nodes[i].x);
        QCOMPARE(first.nodes[i].y, second.nodes[i].y);
        QVERIFY(first.nodes[i].x >= 30 && first.nodes[i].x <= 910);
        QVERIFY(first.nodes[i].y >= 30 && first.nodes[i].y <= 580);
        for (int j = i + 1; j < first.nodes.size(); ++j) {
            const double dx = first.nodes[i].x - first.nodes[j].x;
            const double dy = first.nodes[i].y - first.nodes[j].y;
            QVERIFY2(dx * dx + dy * dy >= 85 * 85, "Nodes need readable separation");
        }
    }
    QVERIFY(first.nodes[0].x != source.nodes[0].x || first.nodes[0].y != source.nodes[0].y);
}

void GraphTests::flowTraceShowsSearchUpdatesAndReverseResidual() {
    Graph graph;
    for (int id = 1; id <= 6; ++id)
        graph.nodes.append({id, QStringLiteral("R%1").arg(id), double(id * 100), 200});
    const int arcs[][2] = {{1, 2}, {1, 3}, {2, 4}, {2, 5},
                           {3, 4}, {4, 6}, {5, 6}};
    int id = 1;
    for (const auto &arc : arcs)
        graph.links.append({id++, arc[0], arc[1], 0, 1, true, false, true});
    const Trace trace = runMaxFlow(graph, 1, 6);
    QVERIFY(trace.success);
    QCOMPARE(trace.value, qint64(2));
    QCOMPARE(trace.steps.first().linkFlows.size(), graph.links.size());
    for (qint64 flow : trace.steps.first().linkFlows) QCOMPARE(flow, qint64(0));
    bool searchedRoute = false, sawReverseRoute = false, sawUpdate = false;
    bool sawExhaustedArc = false, sawReverseCapacity = false;
    bool firstUpdateChecked = false;
    for (const auto &step : trace.steps) {
        QVERIFY(!step.residualArcs.isEmpty());
        if (step.kind == QStringLiteral("search"))
            searchedRoute |= step.pathNodes.size() >= 2 &&
                             step.pathArcs.size() == step.pathNodes.size() - 1;
        if (step.kind == QStringLiteral("update")) {
            sawUpdate = true;
            if (!firstUpdateChecked) {
                bool forwardEmpty = false, reverseReady = false;
                for (const auto &arc : step.residualArcs) {
                    forwardEmpty |= arc.linkId == 1 && !arc.reverse && arc.remaining == 0;
                    reverseReady |= arc.linkId == 1 && arc.reverse && arc.remaining == 1;
                }
                QVERIFY(forwardEmpty);
                QVERIFY(reverseReady);
                firstUpdateChecked = true;
            }
        }
        for (const auto &arc : step.pathArcs) sawReverseRoute |= arc.reverse;
        for (const auto &arc : step.residualArcs) {
            sawExhaustedArc |= !arc.reverse && arc.remaining == 0;
            sawReverseCapacity |= arc.reverse && arc.remaining > 0;
        }
    }
    QVERIFY(searchedRoute);
    QVERIFY(sawUpdate);
    QVERIFY(sawExhaustedArc);
    QVERIFY(sawReverseCapacity);
    QVERIFY(sawReverseRoute);
    GraphBackend backend;
    backend.loadFlowSample();
    const QVariantList serialized = backend.run("flow", 1, 8).value("steps").toList();
    QVERIFY(!serialized.isEmpty());
    QVERIFY(!serialized.first().toMap().value("residualArcs").toList().isEmpty());
    bool serializedUpdate = false;
    for (const auto &entry : serialized) {
        const QVariantMap state = entry.toMap();
        serializedUpdate |= state.value("kind") == QStringLiteral("update") &&
                            !state.value("pathArcs").toList().isEmpty();
    }
    QVERIFY(serializedUpdate);
}

void GraphTests::finalFlowIsDecomposedIntoVisibleRoutes() {
    auto verify = [](const Graph &graph, int source, int sink) {
        const Trace trace = runMaxFlow(graph, source, sink);
        QVERIFY(trace.success);
        const auto &finish = trace.steps.last();
        QCOMPARE(finish.kind, QStringLiteral("finish"));
        QVector<qint64> rebuilt(graph.links.size(), 0);
        qint64 sum = 0;
        for (const auto &path : finish.flowPaths) {
            QVERIFY(path.amount > 0);
            QCOMPARE(path.nodes.first(), source);
            QCOMPARE(path.nodes.last(), sink);
            QCOMPARE(path.linkIds.size(), path.nodes.size() - 1);
            sum += path.amount;
            for (int i = 0; i < path.linkIds.size(); ++i) {
                int index = -1;
                for (int j = 0; j < graph.links.size(); ++j)
                    if (graph.links[j].id == path.linkIds[i]) { index = j; break; }
                QVERIFY(index >= 0);
                const auto &link = graph.links[index];
                if (link.u == path.nodes[i] && link.v == path.nodes[i + 1])
                    rebuilt[index] += path.amount;
                else {
                    QVERIFY(!link.directed);
                    QCOMPARE(link.v, path.nodes[i]);
                    QCOMPARE(link.u, path.nodes[i + 1]);
                    rebuilt[index] -= path.amount;
                }
            }
        }
        QCOMPARE(sum, trace.value);
        QCOMPARE(rebuilt, finish.linkFlows);
    };
    verify(sampleFlowGraph(), 1, 8);

    Graph reversedUndirected;
    reversedUndirected.nodes = {{1, "A", 0, 0}, {2, "B", 100, 0}, {3, "C", 200, 0}};
    reversedUndirected.links = {{1, 2, 1, 0, 5, false, false, true},
                                {2, 2, 3, 0, 5, true, false, true}};
    verify(reversedUndirected, 1, 3);
}

QTEST_GUILESS_MAIN(GraphTests)
#include "tst_graph.moc"
