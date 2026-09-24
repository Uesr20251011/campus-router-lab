#pragma once

#include <QVector>
#include <QString>

struct RouterNode {
    int id;
    QString name;
    double x;
    double y;
};

struct RouterLink {
    int id;
    int u;
    int v;
    qint64 cost;
    qint64 capacity;
    bool directed;
    bool hasCost;
    bool hasCapacity;
};

struct Graph {
    QVector<RouterNode> nodes;
    QVector<RouterLink> links;
};

struct ResidualArcState {
    int from;
    int to;
    int linkId;
    qint64 remaining;
    bool reverse;
};

struct FlowPath {
    QVector<int> nodes;
    QVector<int> linkIds;
    qint64 amount = 0;
};

struct TraceStep {
    QString kind;
    QString title;
    QString detail;
    QVector<int> visitedNodes;
    QVector<int> selectedEdges;
    QVector<int> pathNodes;
    QVector<qint64> linkFlows;
    QVector<ResidualArcState> residualArcs;
    QVector<ResidualArcState> pathArcs;
    QVector<FlowPath> flowPaths;
    qint64 bottleneck = 0;
    int activeEdge = -1;
    int rejectedEdge = -1;
    qint64 value = 0;
};

struct Trace {
    bool success = false;
    QString error;
    qint64 value = 0;
    QVector<TraceStep> steps;
};

Graph sampleCostGraph();
Graph sampleFlowGraph();
Graph sampleCampusGraph(int count = 20);
Graph forceDirectedLayout(const Graph &graph, double width = 940, double height = 610);
Trace runPrim(const Graph &graph, int start);
Trace runKruskal(const Graph &graph);
Trace runMaxFlow(const Graph &graph, int source, int sink);
