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
    QGuiApplication::setOrganizationName(QStringLiteral("CampusRouterLab"));
    QGuiApplication::setApplicationName(QStringLiteral("CampusRouter"));
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
                if (QObject *sinkBox = window->findChild<QObject *>(QStringLiteral("sinkBox")))
                    sinkBox->setProperty("value", 8);
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
            } else if (state == QStringLiteral("dijkstra") || state == QStringLiteral("dijkstra-finish")) {
                backend.loadCostSample();
                if (QObject *sinkBox = window->findChild<QObject *>(QStringLiteral("sinkBox")))
                    sinkBox->setProperty("value", 8);
                const QVariantMap trace = backend.run(QStringLiteral("dijkstra"), 1, 8);
                window->setProperty("activeAlgorithm", "dijkstra");
                window->setProperty("metric", "cost");
                window->setProperty("traceSteps", trace.value("steps"));
                const QVariantList steps = trace.value("steps").toList();
                int index = 0;
                for (int i = 0; i < steps.size(); ++i)
                    if (steps[i].toMap().value("kind") == QStringLiteral("relax")) {
                        index = i; break;
                    }
                window->setProperty("stepIndex", state == QStringLiteral("dijkstra-finish") ? steps.size() - 1 : index);
            } else if (state == QStringLiteral("cost") || state == QStringLiteral("cost-weights")) {
                backend.loadCostSample();
                if (state == QStringLiteral("cost-weights"))
                    if (QObject *canvas = window->findChild<QObject *>(QStringLiteral("graphCanvas")))
                        canvas->setProperty("showAllWeights", true);
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
            } else if (state == QStringLiteral("network")) {
                if (QObject *popup = window->findChild<QObject *>(QStringLiteral("networkPopup")))
                    QMetaObject::invokeMethod(popup, "open");
            } else if (state == QStringLiteral("selection")) {
                window->setProperty("selectedNodeId", 4);
            } else if (state == QStringLiteral("property")) {
                window->setProperty("propertyPanePinned", true);
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
