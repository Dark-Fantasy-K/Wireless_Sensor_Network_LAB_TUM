#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    // Q_INIT_RESOURCE(gui_resources);      //in case of using pictures or other resources

    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowTitle("Parking Lot Monitor");
    // w.setFixedSize(1850,1080);           //expand to full screen
    w.show();

    return a.exec();
}
