#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtTest>
#include "GraphBackend.h"

class LayoutTests : public QObject {
    Q_OBJECT

private slots:
    void draggingNodeKeepsPropertyPaneClosed() {
        GraphBackend backend;
        backend.loadCostSample();
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("graphBackend", &backend);
        engine.load(QUrl::fromLocalFile(QString::fromUtf8(QML_MAIN_PATH)));
        QVERIFY2(!engine.rootObjects().isEmpty(), "Main.qml did not load");
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->setWidth(1200);
        QTRY_VERIFY(window->isExposed());
        auto *card = window->findChild<QQuickItem *>("canvasCard");
        auto *canvas = window->findChild<QQuickItem *>("graphCanvas");
        QVERIFY(card && canvas);
        QTRY_VERIFY(qAbs(card->width() - (window->width() - 32)) < 0.1);
        QQuickItem *node = nullptr;
        QList<QQuickItem *> pending{canvas};
        while (!pending.isEmpty()) {
            QQuickItem *item = pending.takeLast();
            if (item->property("nodeId").isValid() && item->property("nodeId").toInt() == 1) {
                node = item;
                break;
            }
            for (QQuickItem *child : item->childItems()) pending.append(child);
        }
        QVERIFY(node);
        const qreal initialWidth = card->width();
        const QPoint start = node->mapToScene(QPointF(31, 31)).toPoint();
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(window, start + QPoint(42, 12));
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, start + QPoint(42, 12));
        QTest::qWait(300);
        QCOMPARE(window->property("selectedNodeId").toInt(), 1);
        QVERIFY(!window->property("inspectorOpen").toBool());
        QVERIFY(qAbs(card->width() - initialWidth) < 0.1);
    }

    void inspectorAndToolbarKeepCanvasState() {
        QQuickStyle::setStyle("Basic");
        GraphBackend backend;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("graphBackend", &backend);
        engine.load(QUrl::fromLocalFile(QString::fromUtf8(QML_MAIN_PATH)));
        QVERIFY2(!engine.rootObjects().isEmpty(), "Main.qml did not load");
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->setWidth(880);
        window->setHeight(680);
        QTRY_VERIFY(window->isExposed());

        auto *card = window->findChild<QQuickItem *>("canvasCard");
        auto *canvas = window->findChild<QQuickItem *>("graphCanvas");
        auto *toolbar = window->findChild<QQuickItem *>("mainToolbar");
        auto *editTools = window->findChild<QQuickItem *>("editViewTools");
        auto *algorithmTools = window->findChild<QQuickItem *>("algorithmTools");
        auto *layoutTools = window->findChild<QQuickItem *>("layoutTools");
        auto *propertyTool = window->findChild<QQuickItem *>("propertyPanelTool");
        auto *headerActions = window->findChild<QQuickItem *>("headerActions");
        QVERIFY(card && canvas && toolbar && editTools && algorithmTools && layoutTools && propertyTool && headerActions);
        QCOMPARE(toolbar->height(), 96.0);
        QVERIFY(editTools->x() + editTools->width() < headerActions->x());
        QVERIFY(algorithmTools->x() + algorithmTools->width() < window->width());
        QVERIFY(!window->findChild<QObject *>("toolbarShell"));
        auto *networkButton = window->findChild<QQuickItem *>("networkButton");
        auto *networkPopup = window->findChild<QObject *>("networkPopup");
        QVERIFY(networkButton && networkPopup);
        QVERIFY(QMetaObject::invokeMethod(networkPopup, "open"));
        QTRY_VERIFY(networkPopup->property("visible").toBool());
        const qreal popupRight = networkButton->mapToScene(QPointF(networkPopup->property("x").toDouble(), 0)).x()
                                 + networkPopup->property("width").toDouble();
        QVERIFY(popupRight <= window->width());
        QVERIFY(QMetaObject::invokeMethod(networkPopup, "close"));
        QTRY_VERIFY(qAbs(card->width() - 848.0) < 0.1);
        const qreal fullWidth = card->width();
        const int layoutTick = canvas->property("layoutTick").toInt();
        canvas->setProperty("zoom", 1.4);
        canvas->setProperty("panX", 23);

        QVERIFY(qAbs(card->width() - fullWidth) < 0.1);
        QCOMPARE(canvas->property("zoom").toDouble(), 1.4);

        window->setProperty("selectedNodeId", 4);
        QTest::qWait(600);
        QVERIFY(!window->property("inspectorOpen").toBool());
        QVERIFY(qAbs(card->width() - fullWidth) < 0.1);
        QCOMPARE(canvas->property("zoom").toDouble(), 1.4);
        QCOMPARE(canvas->property("panX").toDouble(), 23.0);
        QCOMPARE(canvas->property("layoutTick").toInt(), layoutTick);

        window->setProperty("selectedNodeId", -1);
        window->setProperty("selectedNodeId", 5);
        QTest::qWait(600);
        QVERIFY(!window->property("inspectorOpen").toBool());
        auto clickPropertyTool = [&]() {
            const QPointF scene = propertyTool->mapToScene(QPointF(propertyTool->width() / 2, propertyTool->height() / 2));
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, scene.toPoint());
        };
        clickPropertyTool();
        QTRY_VERIFY(window->property("inspectorOpen").toBool());
        QTRY_VERIFY(card->width() < fullWidth - 200);
        QCOMPARE(window->property("inspectorTab").toString(), QStringLiteral("property"));
        window->setProperty("selectedNodeId", -1);
        QTest::qWait(600);
        QVERIFY(window->property("inspectorOpen").toBool());
        clickPropertyTool();
        QTRY_VERIFY(!window->property("inspectorOpen").toBool());
        QTRY_VERIFY(qAbs(card->width() - fullWidth) < 0.1);
        window->setProperty("selectedNodeId", 5);
        QTest::qWait(200);
        QVERIFY(!window->property("inspectorOpen").toBool());

        window->setProperty("activeAlgorithm", "prim");
        QCOMPARE(window->property("inspectorTab").toString(), QStringLiteral("demo"));
        QTRY_VERIFY(window->property("inspectorOpen").toBool());
        clickPropertyTool();
        QCOMPARE(window->property("inspectorTab").toString(), QStringLiteral("property"));
        clickPropertyTool();
        QCOMPARE(window->property("inspectorTab").toString(), QStringLiteral("demo"));
        QVERIFY(window->property("inspectorOpen").toBool());

        window->setProperty("activeAlgorithm", "");
        QTRY_VERIFY(!window->property("inspectorOpen").toBool());
        QTRY_VERIFY(qAbs(card->width() - fullWidth) < 0.1);
        QCOMPARE(canvas->property("layoutTick").toInt(), layoutTick);
        QCOMPARE(canvas->property("zoom").toDouble(), 1.4);

        auto *prim = window->findChild<QQuickItem *>("primTool");
        QVERIFY(prim);
        auto clickPrim = [&]() {
            const QPointF scene = prim->mapToScene(QPointF(prim->width() / 2, prim->height() / 2));
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, scene.toPoint());
        };
        clickPrim();
        QTRY_COMPARE(window->property("activeAlgorithm").toString(), QStringLiteral("prim"));
        QCOMPARE(toolbar->height(), 96.0);
        clickPrim();
        QTRY_COMPARE(window->property("activeAlgorithm").toString(), QString());
        auto *sourceBox = window->findChild<QQuickItem *>("sourceBox");
        auto *sinkBox = window->findChild<QQuickItem *>("sinkBox");
        auto *dijkstra = window->findChild<QQuickItem *>("dijkstraTool");
        QVERIFY(sourceBox && sinkBox && dijkstra);
        sourceBox->setProperty("value", 4);
        sinkBox->setProperty("value", 8);
        clickPrim();
        QTRY_COMPARE(window->property("activeAlgorithm").toString(), QStringLiteral("prim"));
        QVERIFY(window->property("traceSteps").toList().first().toMap().value("title").toString()
                    .contains(QStringLiteral("R4")));
        clickPrim();
        QTRY_COMPARE(window->property("activeAlgorithm").toString(), QString());
        const QPointF scene = dijkstra->mapToScene(QPointF(dijkstra->width() / 2, dijkstra->height() / 2));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, scene.toPoint());
        QTRY_COMPARE(window->property("activeAlgorithm").toString(), QStringLiteral("dijkstra"));
        const QVariantMap finish = window->property("traceSteps").toList().last().toMap();
        QCOMPARE(finish.value("kind").toString(), QStringLiteral("finish"));
        QCOMPARE(finish.value("pathNodes").toList().first().toInt(), 4);
        QCOMPARE(finish.value("pathNodes").toList().last().toInt(), 8);
        QCOMPARE(finish.value("distances").toList().size(), 20);
    }
};

QTEST_MAIN(LayoutTests)
#include "tst_layout.moc"
