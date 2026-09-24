#pragma once

#include "GraphAlgorithms.h"
#include <QMap>
#include <QObject>
#include <QVariantList>

class GraphBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList nodes READ nodes NOTIFY graphChanged)
    Q_PROPERTY(QVariantList links READ links NOTIFY graphChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QStringList presetNames READ presetNames NOTIFY presetsChanged)

public:
    explicit GraphBackend(QObject *parent = nullptr, const QString &presetFile = {});
    QVariantList nodes() const;
    QVariantList links() const;
    QString error() const;
    QStringList presetNames() const;
    Q_INVOKABLE void loadCostSample();
    Q_INVOKABLE void loadFlowSample();
    Q_INVOKABLE void generateCampus(int count);
    Q_INVOKABLE void generateRandom(int count);
    Q_INVOKABLE bool savePreset(const QString &name);
    Q_INVOKABLE bool loadPreset(const QString &name);
    Q_INVOKABLE void autoLayout();
    Q_INVOKABLE void setPositions(const QVariantList &positions);
    Q_INVOKABLE bool renameNode(int id, const QString &name);
    Q_INVOKABLE bool moveNode(int id, double x, double y);
    Q_INVOKABLE bool removeNode(int id);
    Q_INVOKABLE int addNode(double x, double y);
    Q_INVOKABLE int addLink(int u, int v, qint64 cost, qint64 capacity, bool directed);
    Q_INVOKABLE bool updateLink(int id, qint64 cost, qint64 capacity, bool directed);
    Q_INVOKABLE bool removeLink(int id);
    Q_INVOKABLE QString nodeName(int id) const;
    Q_INVOKABLE qint64 linkCost(int id) const;
    Q_INVOKABLE qint64 linkCapacity(int id) const;
    Q_INVOKABLE bool hasNode(int id) const;
    Q_INVOKABLE bool hasIncidentLink(int id) const;
    Q_INVOKABLE QVariantMap run(const QString &algorithm, int source = 1, int sink = 8) const;

signals:
    void graphChanged();
    void topologyChanged();
    void errorChanged();
    void presetsChanged();

private:
    void readPresets();
    bool writePresets();
    Graph m_graph;
    QString m_error;
    QString m_presetFile;
    QMap<QString, Graph> m_savedPresets;
};
