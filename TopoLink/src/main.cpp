#include "gui/MainWindow.h"
#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <exception>

int main(int argc, char *argv[]) {
  try {
    QApplication app(argc, argv);

    // Increase default menu width and add padding
    app.setStyleSheet(
        "QMenu { min-width: 200px; padding: 5px; background-color: #3E3E42; "
        "color: #F0F0F0; border: 1px solid #555; } "
        "QMenu::item { padding: 5px 25px 5px 20px; } "
        "QMenu::item:selected { background-color: #007ACC; }");

    qDebug() << "Creating MainWindow...";
    MainWindow window;
    window.setWindowTitle("Bijective Meshing Tool");
    window.setWindowIcon(QIcon(":/resources/MeshingApp.png"));

    qDebug() << "Showing MainWindow...";
    window.show();

    qDebug() << "Starting Event Loop...";
    return app.exec();
  } catch (const std::exception &e) {
    qCritical() << "Exception caught in main:" << e.what();
    return 1;
  } catch (...) {
    qCritical() << "Unknown exception caught in main";
    return 1;
  }
}
