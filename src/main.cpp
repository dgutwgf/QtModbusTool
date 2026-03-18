#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 设置应用程序信息
    QApplication::setApplicationName("QtModbusTool");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("dgutwgf");
    
    MainWindow w;
    w.show();
    
    return QApplication::exec();
}
