#include "StoreWindow.h"
#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QPalette>
#include <QStyleFactory>

void setModernDarkTheme(QApplication &app) {
  app.setStyle(QStyleFactory::create("Fusion"));
  QPalette darkPalette;
  darkPalette.setColor(QPalette::Window, QColor(25, 25, 25));
  darkPalette.setColor(QPalette::WindowText, Qt::white);
  darkPalette.setColor(QPalette::Base, QColor(18, 18, 18));
  darkPalette.setColor(QPalette::AlternateBase, QColor(30, 30, 30));
  darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
  darkPalette.setColor(QPalette::ToolTipText, Qt::white);
  darkPalette.setColor(QPalette::Text, Qt::white);
  darkPalette.setColor(QPalette::Button, QColor(45, 45, 45));
  darkPalette.setColor(QPalette::ButtonText, Qt::white);
  darkPalette.setColor(QPalette::BrightText, Qt::red);
  darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
  darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
  darkPalette.setColor(QPalette::HighlightedText, Qt::black);
  app.setPalette(darkPalette);
}

int main(int argc, char *argv[]) {
  qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");
  qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
          "--ignore-certificate-errors --log-level=3");
  qputenv("GTK_MODULES", "");
  QApplication app(argc, argv);
  app.setWindowIcon(QIcon(":/icon.png"));

  setModernDarkTheme(app);

  QCoreApplication::setOrganizationName("Tiwut");
  QCoreApplication::setApplicationName("TitanStore");

  try {
    StoreWindow window;
    window.resize(1150, 800);
    window.show();
    return app.exec();
  } catch (...) {
    QMessageBox::critical(nullptr, "ERROR", "There is a problem!");
    return -1;
  }
}
