#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include "GraphBackend.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/app-icon.png")));
    QQuickStyle::setStyle("Basic");
    GraphBackend backend;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("graphBackend", &backend);
    engine.loadFromModule("CampusRouter", "Main");
    if (engine.rootObjects().isEmpty()) return -1;
    const int screenshotOption = app.arguments().indexOf("--screenshot");
    if (screenshotOption >= 0 && screenshotOption + 1 < app.arguments().size()) {
        const int stateOption = app.arguments().indexOf("--screenshot-state");
        if (stateOption >= 0 && stateOption + 1 < app.arguments().size()) {
            const QString state = app.arguments().at(stateOption + 1);
            QObject *window = engine.rootObjects().first();
            if (state == QStringLiteral("flow") || state == QStringLiteral("flow-finish")) {
                backend.loadFlowSample();
                const QVariantMap trace = backend.run(QStringLiteral("flow"), 1, 8);
                window->setProperty("activeAlgorithm", "flow");
                window->setProperty("metric", "capacity");
                window->setProperty("traceSteps", trace.value("steps"));
                const QVariantList steps = trace.value("steps").toList();
                int index = 0;
                for (int i = 0; i < steps.size(); ++i)
                    if (steps[i].toMap().value("kind") == QStringLiteral("update")) {
                        index = i; break;
                    }
                window->setProperty("stepIndex", state == QStringLiteral("flow-finish") ? steps.size() - 1 : index);
            } else if (state == QStringLiteral("tree")) {
                backend.loadCostSample();
                const QVariantMap trace = backend.run(QStringLiteral("prim"), 1, 8);
                window->setProperty("activeAlgorithm", "prim");
                window->setProperty("traceSteps", trace.value("steps"));
                window->setProperty("stepIndex", trace.value("steps").toList().size() - 1);
                window->setProperty("treeOnly", true);
            } else if (state == QStringLiteral("context")) {
                if (QObject *menu = window->findChild<QObject *>(QStringLiteral("nodeMenu"))) {
                    menu->setProperty("nodeId", 4);
                    menu->setProperty("x", 600);
                    menu->setProperty("y", 300);
                    QMetaObject::invokeMethod(menu, "open");
                }
            }
        }
        const int widthOption = app.arguments().indexOf("--screenshot-width");
        if (widthOption >= 0 && widthOption + 1 < app.arguments().size()) {
            bool ok = false;
            const int width = app.arguments().at(widthOption + 1).toInt(&ok);
            if (ok && width >= 880)
                if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()))
                    window->setWidth(width);
        }
        const QString output = app.arguments().at(screenshotOption + 1);
        QTimer::singleShot(1200, &app, [&app, &engine, output]() {
            if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()))
                window->grabWindow().save(output);
            app.quit();
        });
    }
    return app.exec();
}
