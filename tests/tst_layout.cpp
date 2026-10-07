#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtTest>
#include <functional>
#include "GraphBackend.h"
#include "SecureMessageService.h"

class LayoutTests : public QObject {
    Q_OBJECT

private slots:
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

void LayoutTests::secureMessageUsesSelectedEndpoints() {
#ifndef Q_OS_WIN
    QSKIP("Secure message encryption uses Windows CNG in this build");
#endif
    GraphBackend backend;
    SecureMessageService secure;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("graphBackend", &backend);
    engine.rootContext()->setContextProperty("secureService", &secure);
    engine.load(QUrl::fromLocalFile(QString::fromUtf8(QML_MAIN_PATH)));
    QVERIFY2(!engine.rootObjects().isEmpty(), "Main.qml did not load");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY(window);
    window->setWidth(880);
    QTRY_VERIFY(window->isExposed());
    auto *tool = window->findChild<QQuickItem *>("secureTool");
    auto *source = window->findChild<QQuickItem *>("sourceBox");
    auto *sink = window->findChild<QQuickItem *>("sinkBox");
    auto *message = window->findChild<QQuickItem *>("secureMessageInput");
    QVERIFY(tool && source && sink && message);
    source->setProperty("value", 4);
    sink->setProperty("value", 8);
    const QPoint center = tool->mapToScene(QPointF(tool->width() / 2, tool->height() / 2)).toPoint();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);
    QTRY_VERIFY(window->property("secureMode").toBool());
    QCOMPARE(window->property("inspectorTab").toString(), QStringLiteral("secure"));
    QCOMPARE(window->property("secureKey").toString().size(), 64);
    message->setProperty("text", QStringLiteral("从 R4 发送到 R8"));
    QVERIFY(QMetaObject::invokeMethod(window, "sendSecure"));
    QVERIFY(!window->property("securePacket").toString().isEmpty());
    const QVariantList route = window->property("securePathStep").toMap().value("pathNodes").toList();
    QCOMPARE(route.first().toInt(), 4);
    QCOMPARE(route.last().toInt(), 8);
    QVERIFY(QMetaObject::invokeMethod(window, "decryptSecure"));
    QVERIFY(window->property("secureDecrypted").toString().isEmpty());
    const QString packet = window->property("securePacket").toString();
    message->setProperty("text", QStringLiteral("修改后的新消息"));
    QCOMPARE(window->property("securePacket").toString(), packet);
    for (int hop = 1; hop < route.size(); ++hop) {
        QVERIFY(QMetaObject::invokeMethod(window, "advanceSecure"));
        if (window->property("secureState").toMap().value("phase") == "intercepted")
            QVERIFY(QMetaObject::invokeMethod(window, "forwardSecure"));
    }
    QVERIFY(QMetaObject::invokeMethod(window, "decryptSecure"));
    QCOMPARE(window->property("secureDecrypted").toString(), QStringLiteral("从 R4 发送到 R8"));
    // The new draft never replaces the already transmitted message.
    message->setProperty("text", QStringLiteral("从 R4 发送到 R8"));
    QVERIFY(QMetaObject::invokeMethod(window, "sendSecure"));
    QVERIFY(!window->property("securePacket").toString().isEmpty());
    sink->setProperty("value", 7);
    QTRY_VERIFY(window->property("securePacket").toString().isEmpty());
    backend.loadFlowSample();
    source->setProperty("value", 1);
    sink->setProperty("value", 8);
    QVERIFY(QMetaObject::invokeMethod(window, "sendSecure"));
    QVERIFY(!window->property("securePacket").toString().isEmpty());
}

void LayoutTests::securePacketCanBeInspectedAndTamperedThroughTheControls() {
#ifndef Q_OS_WIN
    QSKIP("Windows CNG required");
#endif
    GraphBackend backend;
    backend.loadCostSample();
    SecureMessageService secure;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("graphBackend", &backend);
    engine.rootContext()->setContextProperty("secureService", &secure);
    engine.load(QUrl::fromLocalFile(QString::fromUtf8(QML_MAIN_PATH)));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    window->setWidth(880);
    window->setHeight(560);
    window->findChild<QObject *>("sinkBox")->setProperty("value", 8);
    QVERIFY(QMetaObject::invokeMethod(window, "toggleSecure"));
    QTRY_VERIFY(window->isExposed());
    QTest::qWait(300);
    auto *networkCard = window->findChild<QQuickItem *>("canvasCard");
    auto *networkCanvas = window->findChild<QQuickItem *>("graphCanvas");
    QVERIFY(networkCard && networkCard->isVisible());
    QVERIFY(networkCanvas && networkCanvas->isVisible());
    QVERIFY(networkCard->width() > window->findChild<QQuickItem *>("secureFocusCard")->width());
    auto click = [&](const char *name) {
        // The current step replaces the visible controls; allow its layout to settle.
        QTest::qWait(80);
        auto *button = window->findChild<QQuickItem *>(name);
        if (!button || !button->isVisible() || !button->isEnabled()) return false;
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint());
        return true;
    };
    const QString message = QStringLiteral("现场挑战 #8642 / 中文与符号 !?");
    window->findChild<QObject *>("secureMessageInput")->setProperty("text", message);
    QVERIFY(click("secureSendButton"));
    window->setProperty("secureRunning", false);
    QCOMPARE(window->property("secureState").toMap().value("phase").toString(), QStringLiteral("transit"));
    QVERIFY(!window->findChild<QQuickItem *>("secureDecryptButton")->isEnabled());
    QVERIFY(click("secureMovingPacket"));
    auto *detail = window->findChild<QObject *>("securePacketDetail");
    QVERIFY(detail);
    QTRY_VERIFY(detail->property("visible").toBool());
    QMetaObject::invokeMethod(detail, "close");
    QTest::qWait(200);
    QVERIFY(click("secureStepButton"));
    auto snapshot = window->property("secureState").toMap();
    QCOMPARE(snapshot.value("phase").toString(), QStringLiteral("intercepted"));
    const QString originalPacket = snapshot.value("senderPacket").toString();
    QCOMPARE(snapshot.value("capturedPacket").toString(), originalPacket);
    QVERIFY(!window->findChild<QQuickItem *>("secureMessageInput")->isVisible());
    QVERIFY(!window->findChild<QQuickItem *>("secureDecryptButton")->isVisible());
    QCOMPARE(window->findChild<QObject *>("secureStageData")->property("text").toString(),
             originalPacket.left(128) + (originalPacket.size() > 128 ? QStringLiteral("…") : QString{}));
    QVERIFY(click("secureTamperButton"));
    const QString changedPacket = window->property("secureState").toMap().value("wirePacket").toString();
    QVERIFY(changedPacket != originalPacket);
    QVERIFY(click("secureForwardButton"));
    window->setProperty("secureRunning", false);
    QVERIFY(click("secureStepButton"));
    QVERIFY(click("secureStepButton"));
    QCOMPARE(window->property("secureState").toMap().value("receivedPacket").toString(), changedPacket);
    QVERIFY(click("secureDecryptButton"));
    QCOMPARE(window->property("secureState").toMap().value("phase").toString(), QStringLiteral("rejected"));
    QVERIFY(window->property("secureDecrypted").toString().isEmpty());
    for (const char *name : {"secureFocusCard"}) {
        auto *card = window->findChild<QQuickItem *>(name);
        QVERIFY(card);
        const QPointF corner = card->mapToScene(QPointF(card->width(), card->height()));
        QVERIFY(corner.x() <= 880);
        QVERIFY(corner.y() <= 560);
    }
    QVERIFY(!window->findChild<QQuickItem *>("secureMessageInput")->isVisible());
    QVERIFY(!window->findChild<QQuickItem *>("secureTamperButton")->isVisible());
    QVERIFY(!window->findChild<QQuickItem *>("secureDecryptButton")->isVisible());
    QVERIFY(click("secureRestartButton"));
    QCOMPARE(window->property("secureState").toMap().value("phase").toString(), QStringLiteral("idle"));
    QVERIFY(window->findChild<QQuickItem *>("secureMessageInput")->isVisible());
    QVERIFY(!window->findChild<QQuickItem *>("secureStageData")->isVisible());
    QCOMPARE(window->findChild<QObject *>("secureMessageInput")->property("text").toString(), message);
    // Repeat with a new live message; a wrong key must fail before the same packet can recover.
    const QString nextMessage = QStringLiteral("重新输入的消息 #1927");
    window->findChild<QObject *>("secureMessageInput")->setProperty("text", nextMessage);
    QVERIFY(click("secureSendButton"));
    window->setProperty("secureRunning", false);
    QVERIFY(click("secureStepButton"));
    QVERIFY(click("secureForwardButton"));
    window->setProperty("secureRunning", false);
    QVERIFY(click("secureStepButton"));
    QVERIFY(click("secureStepButton"));
    const QString arrivedPacket = window->property("secureState").toMap().value("receivedPacket").toString();
    const QString goodKey = window->property("secureKey").toString();
    QString wrongKey = goodKey;
    wrongKey[0] = goodKey[0] == QLatin1Char('0') ? QLatin1Char('1') : QLatin1Char('0');
    window->setProperty("secureReceiverKey", wrongKey);
    QVERIFY(click("secureDecryptButton"));
    QCOMPARE(window->property("secureState").toMap().value("phase").toString(), QStringLiteral("rejected"));
    QVERIFY(window->property("secureDecrypted").toString().isEmpty());
    QVERIFY(click("secureChangeKeyButton"));
    window->setProperty("secureReceiverKey", goodKey);
    QVERIFY(click("secureRetryButton"));
    QCOMPARE(window->property("secureState").toMap().value("receivedPacket").toString(), arrivedPacket);
    QCOMPARE(window->findChild<QObject *>("secureStageData")->property("text").toString(), nextMessage);
    QVERIFY(!window->findChild<QQuickItem *>("secureDecryptButton")->isVisible());
    // Independently decode the original captured packet, without reading the sender's draft.
    auto *verification = window->findChild<QObject *>("secureVerificationPopup");
    QVERIFY(verification);
    window->findChild<QObject *>("secureVerificationPacket")->setProperty("text", originalPacket);
    window->findChild<QObject *>("secureVerificationKey")->setProperty("text", window->property("secureKey"));
    QMetaObject::invokeMethod(verification, "open");
    QTest::qWait(200);
    QVERIFY(click("secureVerificationRun"));
    auto *result = window->findChild<QObject *>("secureVerificationResult");
    QVERIFY(result);
    QCOMPARE(result->property("text").toString(), message);
    window->findChild<QObject *>("secureVerificationPacket")->setProperty("text", changedPacket);
    QCOMPARE(result->property("text").toString(), QString{});
    QVERIFY(click("secureVerificationRun"));
    QVERIFY(result->property("text").toString().contains(QStringLiteral("解密失败")));
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
    QVERIFY(click("flowNode_aes"));
    QVERIFY(click("flowExpandAesButton"));
    QTRY_VERIFY(flow->property("aesExpanded").toBool());
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
    QMetaObject::invokeMethod(window, "advanceSecure");
    QMetaObject::invokeMethod(window, "tamperSecure");
    QMetaObject::invokeMethod(window, "forwardSecure");
    window->setProperty("secureRunning", false);
    QMetaObject::invokeMethod(window, "advanceSecure");
    QMetaObject::invokeMethod(window, "advanceSecure");
    QMetaObject::invokeMethod(window, "decryptSecure");
    QVERIFY(click("secureFlowButton"));
    const QVariantMap receiverTrace = flow->property("trace").toMap();
    QCOMPARE(flow->property("endpoint").toString(), QStringLiteral("receiver"));
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
