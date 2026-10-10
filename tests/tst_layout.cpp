#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtTest>
#include <functional>
#include <QTemporaryDir>
#include "GraphBackend.h"
#include "SecureMessageService.h"

class LayoutTests : public QObject {
    Q_OBJECT

private slots:
    void edgeDirectionCanBeChangedAndSaved() {
        QQuickStyle::setStyle("Basic");
        QTemporaryDir directory;
        const QString presetFile = directory.filePath("presets.json");
        GraphBackend backend(nullptr, presetFile);
        backend.generateCampus(2);
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("graphBackend", &backend);
        engine.load(QUrl::fromLocalFile(QString::fromUtf8(QML_MAIN_PATH)));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QTRY_VERIFY(window->isExposed());
        window->setProperty("propertyPanePinned", true);
        QVERIFY(QMetaObject::invokeMethod(window, "chooseLink", Q_ARG(QVariant, 1)));
        auto *direction = window->findChild<QQuickItem *>("linkDirectionBox");
        auto *apply = window->findChild<QQuickItem *>("applyLinkButton");
        QVERIFY2(direction && apply, "Edge inspector needs both endpoint directions");
        QCOMPARE(direction->property("count").toInt(), 3);
        QCOMPARE(direction->property("currentIndex").toInt(), 0);
        direction->setProperty("currentIndex", 2);
        QVERIFY(backend.moveNode(1, 200, 200));
        QCOMPARE(direction->property("currentIndex").toInt(), 2);
        auto applyDirection = [&](int index) {
            direction->setProperty("currentIndex", index);
            QTRY_VERIFY(apply->mapToScene(QPointF(apply->width()/2, apply->height()/2)).x() < window->width());
            const QPoint center = apply->mapToScene(QPointF(apply->width()/2, apply->height()/2)).toPoint();
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);
        };
        applyDirection(2);
        QVariantMap link = backend.links().first().toMap();
        QCOMPARE(link.value("u").toInt(), 2);
        QCOMPARE(link.value("v").toInt(), 1);
        QVERIFY(link.value("directed").toBool());
        QVERIFY(backend.route(2, 1).value("success").toBool());
        QVERIFY(!backend.route(1, 2).value("success").toBool());
        QVERIFY(backend.run("dijkstra", 2, 1).value("success").toBool());
        QCOMPARE(backend.run("flow", 2, 1).value("value").toLongLong(), link.value("capacity").toLongLong());
        QCOMPARE(backend.run("flow", 1, 2).value("value").toLongLong(), qint64(0));
        // Reapplying the visible selection must not flip it back.
        applyDirection(direction->property("currentIndex").toInt());
        QCOMPARE(backend.links().first().toMap(), link);
        QVERIFY(backend.savePreset(QStringLiteral("reversed")));
        GraphBackend restored(nullptr, presetFile);
        QVERIFY(restored.loadPreset(QStringLiteral("reversed")));
        QCOMPARE(restored.links(), backend.links());
        applyDirection(0);
        QVERIFY(backend.route(1, 2).value("success").toBool());
        QVERIFY(backend.route(2, 1).value("success").toBool());
        applyDirection(2);
        QCOMPARE(backend.links().first().toMap().value("u").toInt(), 1);
        QCOMPARE(backend.links().first().toMap().value("v").toInt(), 2);
        QVERIFY(backend.route(1, 2).value("success").toBool());
        QVERIFY(!backend.route(2, 1).value("success").toBool());
    }
    void secureMessageUsesSelectedEndpoints();
    void securePacketCanBeInspectedAndTamperedThroughTheControls();
    void algorithmDiagramExpandsRealAesStepsAndExplainsRejection();
    void toolbarHintsWaitForHover() {
        GraphBackend backend;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("graphBackend", &backend);
        engine.load(QUrl::fromLocalFile(QString::fromUtf8(QML_MAIN_PATH)));
        QVERIFY2(!engine.rootObjects().isEmpty(), "Main.qml did not load");
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QTRY_VERIFY(window->isExposed());

        auto *prim = window->findChild<QQuickItem *>("primTool");
        auto *source = window->findChild<QQuickItem *>("sourceBox");
        auto *sink = window->findChild<QQuickItem *>("sinkBox");
        QVERIFY(prim && source && sink);
        for (auto *item : {prim, source, sink}) {
            QQmlExpression delay(QQmlEngine::contextForObject(item), item, QStringLiteral("ToolTip.delay"));
            const QVariant value = delay.evaluate();
            QVERIFY2(!delay.hasError(), qPrintable(delay.error().toString()));
            QCOMPARE(value.toInt(), 450);
        }

        const QPoint primCenter = prim->mapToScene(QPointF(prim->width() / 2, prim->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, primCenter);
        QVERIFY(prim->property("activeFocus").toBool());
        QTest::mouseMove(window, QPoint(window->width() - 30, window->height() - 30));
        QTRY_VERIFY(!prim->property("hovered").toBool());
        QQmlExpression visible(QQmlEngine::contextForObject(prim), prim, QStringLiteral("ToolTip.visible"));
        const QVariant value = visible.evaluate();
        QVERIFY2(!visible.hasError(), qPrintable(visible.error().toString()));
        QVERIFY2(!value.toBool(), "Toolbar hint remains active after the mouse leaves");
    }

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

void LayoutTests::secureMessageUsesSelectedEndpoints() {
    GraphBackend backend; backend.loadCostSample();
    SecureMessageService secure;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("graphBackend", &backend);
    engine.rootContext()->setContextProperty("secureService", &secure);
    engine.load(QUrl::fromLocalFile(QString::fromUtf8(QML_MAIN_PATH)));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    window->findChild<QObject *>("sourceBox")->setProperty("value", 4);
    window->findChild<QObject *>("sinkBox")->setProperty("value", 8);
    QMetaObject::invokeMethod(window, "toggleSecure");
    auto *message = window->findChild<QObject *>("secureMessageInput");
    message->setProperty("text", QStringLiteral("从 R4 发送到 R8"));
    QMetaObject::invokeMethod(window, "sendSecure");
    const auto snapshot = window->property("secureState").toMap();
    QCOMPARE(snapshot.value("source").toInt(), 4);
    QCOMPARE(snapshot.value("target").toInt(), 8);
    QCOMPARE(snapshot.value("plaintext").toString(), QStringLiteral("从 R4 发送到 R8"));
    QVERIFY(!window->property("secureRunning").toBool());
    const QString packet = snapshot.value("senderPacket").toString();
    message->setProperty("text", "new draft");
    QCOMPARE(window->property("securePacket").toString(), packet);
    QMetaObject::invokeMethod(window, "toggleSecure");
    QVERIFY(window->property("securePacket").toString().isEmpty());
}

void LayoutTests::securePacketCanBeInspectedAndTamperedThroughTheControls() {
    GraphBackend backend; backend.loadCostSample();
    SecureMessageService secure;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("graphBackend", &backend);
    engine.rootContext()->setContextProperty("secureService", &secure);
    engine.load(QUrl::fromLocalFile(QString::fromUtf8(QML_MAIN_PATH)));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    window->setWidth(1080); window->setHeight(680);
    window->findChild<QObject *>("sinkBox")->setProperty("value", 8);
    QMetaObject::invokeMethod(window, "toggleSecure");
    QTRY_VERIFY(window->isExposed());
    std::function<QQuickItem *(QQuickItem *, const QString &)> findVisual;
    findVisual = [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
        if (parent->objectName() == name) return parent;
        for (auto *child : parent->childItems())
            if (auto *found = findVisual(child, name)) return found;
        return nullptr;
    };
    auto click = [&](const char *name) {
        QTest::qWait(80);
        auto *item = findVisual(window->contentItem(), QString::fromLatin1(name));
        if (!item || !item->isVisible() || !item->isEnabled()) return false;
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
        return true;
    };
    const QString message = QStringLiteral("多节点篡改体验 #4821");
    window->findChild<QObject *>("secureMessageInput")->setProperty("text", message);
    QVERIFY(click("secureSendButton"));
    auto snapshot = window->property("secureState").toMap();
    QCOMPARE(snapshot.value("phase").toString(), QStringLiteral("decrypted"));
    const QString originalPacket = snapshot.value("senderPacket").toString();
    auto *flowPopup = window->findChild<QObject *>("secureWorkflowPopup");
    auto *panel = window->findChild<QObject *>("securePanel");
    QVERIFY(!flowPopup->property("visible").toBool());
    QVERIFY(click("securePacketPeekButton"));
    auto *detail = window->findChild<QObject *>("securePacketDetail");
    QTRY_VERIFY(detail->property("visible").toBool());
    QMetaObject::invokeMethod(detail, "close");
    const QVariantList route = snapshot.value("route").toList();
    QVERIFY(route.size() >= 4);
    auto select = [&](int index) {
        QMetaObject::invokeMethod(window, "chooseNode", Q_ARG(QVariant, route[index]));
        QCOMPARE(panel->property("selectedRouteNode").toInt(), route[index].toInt());
        QVERIFY(!flowPopup->property("visible").toBool());
    };
    select(1);
    QVERIFY(click("secureTamperButton"));
    auto *attackPopup = window->findChild<QObject *>("secureAttackPopup");
    QTRY_VERIFY(attackPopup->property("visible").toBool());
    auto *editor = window->findChild<QObject *>("secureAttackInput");
    QCOMPARE(editor->property("text").toString(), originalPacket);
    QByteArray bytes = QByteArray::fromBase64(originalPacket.toLatin1());
    bytes[28] = char(bytes[28] ^ 1);
    const QString firstPacket = QString::fromLatin1(bytes.toBase64());
    const QString oversized(65537, 'x');
    editor->setProperty("text", oversized);
    QVERIFY(click("secureApplyAttackButton"));
    QVERIFY(attackPopup->property("visible").toBool());
    QCOMPARE(editor->property("text").toString(), oversized);
    editor->setProperty("text", firstPacket);
    QVERIFY(click("secureApplyAttackButton"));
    QCOMPARE(window->property("secureState").toMap().value("phase").toString(), QStringLiteral("rejected"));
    select(2);
    QVERIFY(click("secureTamperButton"));
    QCOMPARE(editor->property("text").toString(), firstPacket);
    bytes[29] = char(bytes[29] ^ 2);
    editor->setProperty("text", QString::fromLatin1(bytes.toBase64()));
    QVERIFY(click("secureApplyAttackButton"));
    QCOMPARE(window->property("secureState").toMap().value("receivedPacket").toString(), QString::fromLatin1(bytes.toBase64()));
    select(1);
    QVERIFY(click("secureRestoreNodeButton"));
    select(2);
    const auto downstream = window->property("secureState").toMap().value("nodeStates").toList()[2].toMap();
    QCOMPARE(downstream.value("inputPacket").toString(), originalPacket);
    QVERIFY(downstream.value("attacked").toBool());
    QVERIFY(click("secureRestoreNodeButton"));
    select(route.size()-1);
    QCOMPARE(window->findChild<QObject *>("secureStageData")->property("text").toString(), message);
    QVERIFY(!flowPopup->property("visible").toBool());
    QVERIFY(click("secureFlowButton"));
    QTRY_VERIFY(flowPopup->property("visible").toBool());
    QCOMPARE(window->findChild<QObject *>("secureFlowView")->property("endpoint").toString(), QStringLiteral("receiver"));
    QMetaObject::invokeMethod(flowPopup, "close");
    const QString goodKey = window->property("secureReceiverKey").toString();
    window->setProperty("secureReceiverKey", QString(64, 'f'));
    QMetaObject::invokeMethod(window, "decryptSecure");
    QCOMPARE(window->property("secureState").toMap().value("phase").toString(), QStringLiteral("rejected"));
    QVERIFY(click("secureChangeKeyButton"));
    window->setProperty("secureReceiverKey", goodKey);
    QCOMPARE(window->property("secureDecrypted").toString(), message);
    QMetaObject::invokeMethod(window->findChild<QObject *>("secureKeyPopup"), "close");
    QVERIFY(click("secureRestartButton"));
    QCOMPARE(window->property("secureState").toMap().value("phase").toString(), QStringLiteral("idle"));
    window->setProperty("secureScenario", "plaintext");
    window->findChild<QObject *>("secureMessageInput")->setProperty("text", "hello");
    QVERIFY(click("secureSendButton"));
    select(1);
    QVERIFY(click("secureTamperButton"));
    editor->setProperty("text", "jello");
    QVERIFY(click("secureApplyAttackButton"));
    const auto history = window->property("secureHistory").toList().first().toMap();
    QVERIFY(!history.value("passed").toBool());
    QVERIFY(history.value("detail").toString().contains(QStringLiteral("不同")));
}

void LayoutTests::algorithmDiagramExpandsRealAesStepsAndExplainsRejection() {
    GraphBackend backend;
    backend.loadCostSample();
    SecureMessageService secure;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("graphBackend", &backend);
    engine.rootContext()->setContextProperty("secureService", &secure);
    engine.load(QUrl::fromLocalFile(QString::fromUtf8(QML_MAIN_PATH)));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    window->setWidth(1080); window->setHeight(680);
    window->findChild<QObject *>("sinkBox")->setProperty("value", 8);
    QMetaObject::invokeMethod(window, "toggleSecure");
    const QString message = QStringLiteral("结构图现场消息 #8217");
    window->findChild<QObject *>("secureMessageInput")->setProperty("text", message);
    QMetaObject::invokeMethod(window, "sendSecure");
    window->setProperty("secureRunning", false);
    QTRY_VERIFY(window->isExposed());
    std::function<QQuickItem *(QQuickItem *, const QString &)> findVisual;
    findVisual = [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
        if (parent->objectName() == name) return parent;
        for (auto *child : parent->childItems())
            if (auto *found = findVisual(child, name)) return found;
        return nullptr;
    };
    auto click = [&](const char *name) {
        QTest::qWait(80);
        auto *item = findVisual(window->contentItem(), QString::fromLatin1(name));
        if (!item || !item->isVisible() || !item->isEnabled()) return false;
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        return true;
    };
    QVERIFY2(click("secureFlowButton"), "A transmitted packet needs a clickable processing diagram");
    auto *popup = window->findChild<QObject *>("secureWorkflowPopup");
    QTRY_VERIFY(popup && popup->property("visible").toBool());
    auto *flow = window->findChild<QQuickItem *>("secureFlowView");
    QVERIFY(flow);
    const QVariantMap senderTrace = flow->property("trace").toMap();
    QCOMPARE(senderTrace.value("plaintext").toString(), message);
    QVERIFY(senderTrace.value("referenceMatches").toBool());
    auto hasEdge = [&](const QString &from, const QString &to) {
        for (const QVariant &edge : flow->property("edges").toList())
            if (edge.toMap().value("from") == from && edge.toMap().value("to") == to) return true;
        return false;
    };
    QVERIFY(hasEdge("key", "params"));
    QVERIFY(hasEdge("counter", "params"));
    QVERIFY(hasEdge("nonce", "packet"));
    QVERIFY(!hasEdge("expand", "params"));
    QVERIFY(click("flowNode_aes"));
    QVariant inputTone, outputTone, idleTone;
    QMetaObject::invokeMethod(flow, "edgeTone", Q_RETURN_ARG(QVariant, inputTone), Q_ARG(QVariant, "counter"), Q_ARG(QVariant, "aes"));
    QMetaObject::invokeMethod(flow, "edgeTone", Q_RETURN_ARG(QVariant, outputTone), Q_ARG(QVariant, "aes"), Q_ARG(QVariant, "xor"));
    QMetaObject::invokeMethod(flow, "edgeTone", Q_RETURN_ARG(QVariant, idleTone), Q_ARG(QVariant, "hash"), Q_ARG(QVariant, "tag"));
    QVERIFY(inputTone != outputTone && inputTone != idleTone && outputTone != idleTone);
    QVERIFY(click("flowExpandAesButton"));
    QTRY_VERIFY(flow->property("aesExpanded").toBool());
    QVERIFY(hasEdge("previousKey", "roundInput"));
    QVERIFY(hasEdge("expand", "roundKey"));
    QVERIFY(hasEdge("roundInput", "sub"));
    QVERIFY(click("flowAesInputSource"));
    QCOMPARE(flow->property("selectedStep").toString(), QStringLiteral("roundInput"));
    QVERIFY(flow->property("aesExpanded").toBool());
    QVERIFY(click("flowNode_sub"));
    QVERIFY(click("flowNextButton"));
    QVERIFY(flow->property("aesStep").toInt() > 0);
    auto *output = window->findChild<QQuickItem *>("flowOutputMatrix");
    QVERIFY(output && output->isVisible());
    const QVariantMap operation = senderTrace.value("aes").toMap().value("operations").toList()[flow->property("aesStep").toInt()].toMap();
    QCOMPARE(output->property("hexData").toString(), operation.value("output").toString());
    QVERIFY(click("flowOutputMatrixByte1"));
    QCOMPARE(flow->property("selectedByte").toInt(), 1);
    auto *input = window->findChild<QQuickItem *>("flowInputMatrix");
    QVERIFY(input);
    QCOMPARE(input->property("selected").toInt(), 5); // ShiftRows: output [1,0] comes from input [1,1].
    flow->setProperty("aesStep", 55);
    const QVariantList finalNodes = flow->property("nodes").toList();
    QVERIFY(!std::any_of(finalNodes.begin(), finalNodes.end(), [](const QVariant &row) { return row.toMap().value("id") == "mix"; }));
    QMetaObject::invokeMethod(popup, "close");
    // Neither a later sender key edit nor a new draft may rewrite the trace of an already sent packet.
    window->setProperty("secureKey", QString(64, 'f'));
    window->findChild<QObject *>("secureMessageInput")->setProperty("text", "new draft");
    QVERIFY(click("secureFlowButton"));
    QCOMPARE(flow->property("trace").toMap().value("plaintext").toString(), message);
    QMetaObject::invokeMethod(popup, "close");
    const QVariantList route = window->property("secureState").toMap().value("route").toList();
    QByteArray changed = QByteArray::fromBase64(window->property("securePacket").toString().toLatin1());
    changed[28] = char(changed[28] ^ 1);
    bool edited = false;
    QVERIFY(QMetaObject::invokeMethod(secure.channel(), "editNode", Q_RETURN_ARG(bool, edited), Q_ARG(int, route[1].toInt()), Q_ARG(QString, QString::fromLatin1(changed.toBase64()))));
    QVERIFY(edited);
    QMetaObject::invokeMethod(window, "chooseNode", Q_ARG(QVariant, route.last()));
    QVERIFY(click("secureFlowButton"));
    const QVariantMap receiverTrace = flow->property("trace").toMap();
    QCOMPARE(flow->property("endpoint").toString(), QStringLiteral("receiver"));
    QVERIFY(hasEdge("parse", "xor"));
    QVERIFY(hasEdge("key", "params"));
    QVERIFY(hasEdge("counter", "params"));
    QVERIFY(!receiverTrace.value("tagMatches").toBool());
    QVERIFY(receiverTrace.value("receivedTag") != receiverTrace.value("computedTag"));
    QVERIFY(receiverTrace.value("plaintext").toString().isEmpty());
    QVERIFY(click("flowNode_packet"));
    auto *raw = findVisual(window->contentItem(), QStringLiteral("flowRawPacket"));
    QVERIFY(raw && raw->isVisible());
    const QString receivedPacket = window->property("secureState").toMap().value("receivedPacket").toString();
    QCOMPARE(raw->property("text").toString(), receivedPacket);
    QVERIFY(receivedPacket != window->property("secureState").toMap().value("senderPacket").toString());
    QVERIFY(click("flowNode_parse"));
    QVERIFY(!findVisual(window->contentItem(), QStringLiteral("flowRawPacket")));
    QVERIFY(click("flowNode_counter"));
    auto *nonceInput=findVisual(window->contentItem(), QStringLiteral("flowValue_nonceInput"));
    QVERIFY(nonceInput);
    const QString nonceValue=nonceInput->property("rawValue").toString();
    QCOMPARE(nonceValue, receiverTrace.value("nonce").toString());
    auto *counterValue=findVisual(window->contentItem(), QStringLiteral("flowValue_counter"));
    QVERIFY(counterValue);
    QCOMPARE(counterValue->property("rawValue").toString().left(24), nonceInput->property("rawValue").toString());
    QCOMPARE(counterValue->property("rawValue").toString().mid(24), QStringLiteral("00000002"));
    QVERIFY(click("flowOrigin_nonceInput"));
    QCOMPARE(flow->property("selectedStep").toString(), QStringLiteral("parse"));
    auto *parsedNonce=findVisual(window->contentItem(), QStringLiteral("flowValue_nonce"));
    QTRY_VERIFY(parsedNonce && parsedNonce->property("highlighted").toBool());
    QCOMPARE(parsedNonce->property("rawValue").toString(), nonceValue);
    QVERIFY(click("flowNode_verify"));
    auto *comparison = window->findChild<QQuickItem *>("flowTagComparison");
    QVERIFY(comparison && comparison->isVisible());
    QCOMPARE(comparison->property("received").toString(), receiverTrace.value("receivedTag").toString());
    QCOMPARE(comparison->property("computed").toString(), receiverTrace.value("computedTag").toString());
    QVERIFY(click("flowTagOrigin_tag"));
    QCOMPARE(flow->property("selectedStep").toString(), QStringLiteral("tag"));
    window->findChild<QObject *>("flowDetailsScroll")->setProperty("contentY", 0);
    QVERIFY(click("flowOrigin_0"));
    QCOMPARE(flow->property("selectedStep").toString(), QStringLiteral("hash"));
    QCOMPARE(flow->property("hashIndex").toInt(), receiverTrace.value("hashSteps").toList().size()-1);
    QVERIFY(click("flowNode_output"));
    auto *result = window->findChild<QObject *>("flowPlaintextOutput");
    QVERIFY(result);
    QCOMPARE(result->property("text").toString(), QString{});
}

QTEST_MAIN(LayoutTests)
#include "tst_layout.moc"
