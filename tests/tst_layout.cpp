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
        auto *headerActions = window->findChild<QQuickItem *>("headerActions");
        QVERIFY(card && canvas && toolbar && editTools && algorithmTools && headerActions);
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
        QTRY_VERIFY(window->property("inspectorOpen").toBool());
        QTRY_VERIFY(card->width() < fullWidth - 200);
        QCOMPARE(canvas->property("zoom").toDouble(), 1.4);
        QCOMPARE(canvas->property("panX").toDouble(), 23.0);
        QCOMPARE(canvas->property("layoutTick").toInt(), layoutTick);

        window->setProperty("selectedNodeId", -1);
        window->setProperty("selectedNodeId", 5);
        QTest::qWait(600);
        QVERIFY(window->property("inspectorOpen").toBool());

        window->setProperty("activeAlgorithm", "prim");
        window->setProperty("selectedNodeId", -1);
        QCOMPARE(window->property("inspectorTab").toString(), QStringLiteral("demo"));
        QTest::qWait(600);
        QVERIFY(window->property("inspectorOpen").toBool());

        window->setProperty("activeAlgorithm", "");
        QTRY_VERIFY_WITH_TIMEOUT(!window->property("inspectorOpen").toBool(), 1500);
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
    }
};

QTEST_MAIN(LayoutTests)
#include "tst_layout.moc"
