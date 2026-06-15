#include "grasp_main_window.h"

#include <QApplication>

int main(int argc, char** argv)
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);

    GraspMainWindow window;
    const QString obb_engine_path = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    const QString seg_engine_path = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QString();
    window.setInitialEnginePaths(obb_engine_path, seg_engine_path);
    window.showMaximized();

    return app.exec();
}
