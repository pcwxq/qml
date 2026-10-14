#include "RefreshController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("Quick Refresh Comparison");
    QQuickStyle::setStyle("Basic");
    RefreshController controller("quick");
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &controller);
    engine.load(QUrl(QStringLiteral("qrc:/quick/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
