#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include "GraphBackend.h"
#include "SecureMessageService.h"
#include "TransmissionSession.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("CampusRouterLab"));
    QGuiApplication::setApplicationName(QStringLiteral("CampusRouter"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/app-icon.png")));
    QQuickStyle::setStyle("Basic");
    GraphBackend backend;
    SecureMessageService secureService;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("graphBackend", &backend);
    engine.rootContext()->setContextProperty("secureService", &secureService);
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
            } else if (state.startsWith(QStringLiteral("secure"))) {
                backend.loadCostSample();
                if (QObject *sinkBox = window->findChild<QObject *>(QStringLiteral("sinkBox")))
                    sinkBox->setProperty("value", 8);
                QMetaObject::invokeMethod(window, "toggleSecure");
                if (state == QStringLiteral("secure-plain")) window->setProperty("secureScenario", "plaintext");
                if (state == QStringLiteral("secure-tamper") || state == QStringLiteral("secure-flow-fail")) window->setProperty("secureScenario", "tamper");
                if (QObject *message = window->findChild<QObject *>(QStringLiteral("secureMessageInput")))
                    message->setProperty("text", QStringLiteral("今晚 20:00 开放实验室，通行口令 7391。"));
                if (state != QStringLiteral("secure-input")) QMetaObject::invokeMethod(window, "sendSecure");
                auto *channel = qobject_cast<TransmissionSession *>(secureService.channel());
                if (state == QStringLiteral("secure-tamper") || state == QStringLiteral("secure-flow-fail")) {
                    QByteArray bytes = QByteArray::fromBase64(channel->state().value("senderPacket").toString().toLatin1());
                    bytes[bytes.size()-1] = char(bytes.back() ^ 1);
                    channel->editNode(channel->state().value("route").toList()[1].toInt(), QString::fromLatin1(bytes.toBase64()));
                }
                if (state == QStringLiteral("secure-intercept") || state == QStringLiteral("secure-tamper"))
                    QMetaObject::invokeMethod(window, "chooseNode", Q_ARG(QVariant, channel->state().value("route").toList()[1]));
                else if (state == QStringLiteral("secure") || state == QStringLiteral("secure-plain"))
                    QMetaObject::invokeMethod(window, "chooseNode", Q_ARG(QVariant, channel->state().value("target")));
                window->setProperty("secureRunning", false);
                if (state.startsWith(QStringLiteral("secure-flow")) || state == QStringLiteral("secure-aes")) {
                    if (QObject *panel = window->findChild<QObject *>(QStringLiteral("securePanel")))
                        QMetaObject::invokeMethod(panel, "openFlow", Q_ARG(QVariant, state == QStringLiteral("secure-flow-fail") || state == QStringLiteral("secure-flow-raw") || state == QStringLiteral("secure-flow-hash") || state == QStringLiteral("secure-flow-counter") ? QStringLiteral("receiver") : QStringLiteral("sender")));
                    if (state == QStringLiteral("secure-flow-raw") || state == QStringLiteral("secure-flow-hash") || state == QStringLiteral("secure-flow-counter"))
                        if (QObject *flow = window->findChild<QObject *>(QStringLiteral("secureFlowView")))
                            flow->setProperty("selectedStep", state == QStringLiteral("secure-flow-raw") ? "packet" : state == QStringLiteral("secure-flow-counter") ? "counter" : "hash");
                    if (state == QStringLiteral("secure-aes"))
                        if (QObject *flow = window->findChild<QObject *>(QStringLiteral("secureFlowView"))) {
                            flow->setProperty("aesExpanded", true);
                            flow->setProperty("aesStep", 2);
                            flow->setProperty("selectedStep", "shift");
                            flow->setProperty("selectedByte", 1);
                        }
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
        const int heightOption = app.arguments().indexOf("--screenshot-height");
        if (heightOption >= 0 && heightOption + 1 < app.arguments().size()) {
            bool ok = false;
            const int height = app.arguments().at(heightOption + 1).toInt(&ok);
            if (ok && height >= 560)
                if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) window->setHeight(height);
        }
        QTimer::singleShot(1200, &app, [&app, &engine, output]() {
            if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()))
                window->grabWindow().save(output);
            app.quit();
        });
    }
    return app.exec();
}
