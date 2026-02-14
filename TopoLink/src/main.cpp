#include "gui/MainWindow.h"
#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <exception>

int main(int argc, char *argv[]) {
  try {
    QApplication app(argc, argv);

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
