#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    MainWindow w;

    QIcon windowIcon("D:/code/qt_project/OpenCV_Face/stu1.png"); //图标
    w.setWindowIcon(windowIcon);

    w.show();
    return a.exec();
}
