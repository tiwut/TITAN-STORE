#include <QApplication>
#include "StoreWindow.h"

int main(int argc, char *argv[]) {
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");

    QApplication app(argc, argv);
    
    QCoreApplication::setOrganizationName("TitanDevs");
    QCoreApplication::setApplicationName("100% TITAN STORE");

    StoreWindow window;
    window.resize(1200, 800);
    window.show();

    return app.exec();
}
