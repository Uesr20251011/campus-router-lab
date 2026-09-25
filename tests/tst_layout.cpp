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
        auto *toolbar = window->findChild<QQuickItem *>("toolbarShell");
        QVERIFY(card && canvas && toolbar);
        QTRY_VERIFY(qAbs(card->width() - 848.0) < 0.1);
        const qreal fullWidth = card->width();
        const int layoutTick = canvas->property("layoutTick").toInt();
        canvas->setProperty("zoom", 1.4);
        canvas->setProperty("panX", 23);

        window->setProperty("toolbarPinned", true);
        QTRY_VERIFY(toolbar->width() > 470);
        QVERIFY(qAbs(card->width() - fullWidth) < 0.1);
        QCOMPARE(canvas->property("zoom").toDouble(), 1.4);

        window->setProperty("toolbarPinned", false);
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

        window->setProperty("toolbarPinned", true);
        QTRY_VERIFY(toolbar->width() > 470);
        auto *prim = window->findChild<QQuickItem *>("primTool");
        auto *rightPane = window->findChild<QQuickItem *>("rightPane");
        QVERIFY(prim && rightPane);
        auto clickPrim = [&]() {
            const QPointF scene = prim->mapToScene(QPointF(prim->width() / 2, prim->height() / 2));
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, scene.toPoint());
        };
        clickPrim();
        QTRY_COMPARE(window->property("activeAlgorithm").toString(), QStringLiteral("prim"));
        QTRY_VERIFY(toolbar->x() + toolbar->width() < rightPane->x());
        clickPrim();
        QTRY_COMPARE(window->property("activeAlgorithm").toString(), QString());
        window->setProperty("toolbarPinned", false);
        QTRY_VERIFY(toolbar->width() < 80);
    }
};

QTEST_MAIN(LayoutTests)
#include "tst_layout.moc"
