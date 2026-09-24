#include <QQuickItem>
#include <QQuickView>
#include <QtTest>

class CanvasTests : public QObject {
    Q_OBJECT

private:
    static QVariantList nodes() {
        return {
            QVariantMap{{"id", 1}, {"name", "R1"}, {"x", 200}, {"y", 200}},
            QVariantMap{{"id", 2}, {"name", "R2"}, {"x", 400}, {"y", 200}}
        };
    }

    static QQuickItem *node(QQuickItem *root, int id) {
        for (auto *item : root->childItems()) {
            if (item->property("nodeId").isValid() && item->property("nodeId").toInt() == id)
                return item;
            if (auto *found = node(item, id)) return found;
        }
        return nullptr;
    }

    static void prepare(QQuickView &view) {
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.resize(976, 646);
        view.setSource(QUrl::fromLocalFile(QString::fromUtf8(QML_CANVAS_PATH)));
        QVERIFY2(view.status() == QQuickView::Ready, "GraphCanvas did not load");
        view.rootObject()->setProperty("nodes", nodes());
        view.rootObject()->setProperty("links", QVariantList{
            QVariantMap{{"id", 1}, {"u", 1}, {"v", 2}, {"cost", 7},
                        {"capacity", 10}, {"directed", false},
                        {"hasCost", true}, {"hasCapacity", true}}
        });
        view.show();
        QTRY_VERIFY(view.isExposed());
        QTRY_VERIFY(node(view.rootObject(), 1));
    }

private slots:
    void clickDoesNotStartForceLayout() {
        QQuickView view;
        prepare(view);
        auto *root = view.rootObject();
        const int beforeTick = root->property("layoutTick").toInt();
        const QPointF before = node(root, 1)->position();

        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, QPoint(218, 218));
        QTest::qWait(100);

        QCOMPARE(root->property("draggedNode").toInt(), -1);
        QCOMPARE(root->property("layoutTick").toInt(), beforeTick);
        QCOMPARE(node(root, 1)->position(), before);
    }

    void fixedModeMovesOnlyDraggedNode() {
        QQuickView view;
        prepare(view);
        auto *root = view.rootObject();
        root->setProperty("fixedNodes", true);
        const QPointF first = node(root, 1)->position();
        const QPointF second = node(root, 2)->position();
        QTRY_VERIFY(root->property("labelSlots").toMap().contains("1"));
        const int labelSlot = root->property("labelSlots").toMap().value("1").toInt();
        QSignalSpy saved(root, SIGNAL(positionsSettled(QVariant)));
        QVERIFY(saved.isValid());

        QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, QPoint(218, 218));
        QTest::mouseMove(&view, QPoint(240, 230));
        QCOMPARE(root->property("labelSlots").toMap().value("1").toInt(), labelSlot);
        QTest::mouseMove(&view, QPoint(270, 245));
        QCOMPARE(root->property("labelSlots").toMap().value("1").toInt(), labelSlot);
        QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, QPoint(270, 245));
        QTest::qWait(100);

        QVERIFY(node(root, 1)->position() != first);
        QCOMPARE(node(root, 2)->position(), second);
        QCOMPARE(root->property("draggedNode").toInt(), -1);
        QCOMPARE(saved.size(), 1);
    }

    void rightClickNodeRequestsContextAction() {
        QQuickView view;
        prepare(view);
        auto *root = view.rootObject();
        QSignalSpy context(root, SIGNAL(nodeContextRequested(int,double,double)));
        QVERIFY(context.isValid());

        QTest::mouseClick(&view, Qt::RightButton, Qt::NoModifier, QPoint(218, 218));

        QCOMPARE(context.size(), 1);
        QCOMPARE(context.first().at(0).toInt(), 1);
    }

    void linkToolSupportsRepeatedConnections() {
        QQuickView view;
        prepare(view);
        auto *root = view.rootObject();
        root->setProperty("tool", "link");
        QSignalSpy links(root, SIGNAL(linkRequested(int,int)));
        QVERIFY(links.isValid());

        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, QPoint(218, 218));
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, QPoint(418, 218));
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, QPoint(418, 218));
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, QPoint(218, 218));

        QCOMPARE(links.size(), 2);
        QCOMPARE(root->property("tool").toString(), QStringLiteral("link"));
        QCOMPARE(root->property("linkStart").toInt(), -1);
    }

    void numericInputAcceptsDirectTyping() {
        QQuickView view;
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.resize(180, 50);
        view.setSource(QUrl::fromLocalFile(QString::fromUtf8(QML_SPINBOX_PATH)));
        QVERIFY2(view.status() == QQuickView::Ready, "SoftSpinBox did not load");
        auto *input = view.rootObject();
        input->setProperty("from", 2);
        input->setProperty("to", 200);
        input->setProperty("value", 20);
        view.show();
        QTRY_VERIFY(view.isExposed());

        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, QPoint(90, 25));
        QTest::keyClick(&view, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(&view, Qt::Key_3);
        QTest::keyClick(&view, Qt::Key_7);
        QVERIFY(QMetaObject::invokeMethod(input, "commitInput"));

        QTRY_COMPARE(input->property("value").toInt(), 37);
    }
};

QTEST_MAIN(CanvasTests)
#include "tst_canvas.moc"
