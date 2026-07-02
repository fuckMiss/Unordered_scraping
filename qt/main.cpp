#include "grasp_main_window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QScreen>
#include <QString>

#include <cstdio>
#include <iostream>

#include <fcntl.h>
#include <io.h>

namespace {

QString ConfigureFileLog()
{
    QString log_path = QString::fromLocal8Bit(qgetenv("TANKEYE_LOG_FILE"));
    if (log_path.trimmed().isEmpty()) {
        QDir log_dir(QCoreApplication::applicationDirPath());
        if (!log_dir.exists("logs")) {
            log_dir.mkpath("logs");
        }
        const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        log_path = log_dir.filePath(QStringLiteral("logs/tankeye_%1.log").arg(stamp));
    } else {
        const QFileInfo info(log_path);
        if (!info.absoluteDir().exists()) {
            QDir().mkpath(info.absolutePath());
        }
    }

    const std::wstring wide_log_path = log_path.toStdWString();
    FILE* stdout_file = nullptr;
    FILE* stderr_file = nullptr;
    _wfreopen_s(&stdout_file, wide_log_path.c_str(), L"a", stdout);
    _wfreopen_s(&stderr_file, wide_log_path.c_str(), L"a", stderr);

    std::cout << "============================================================" << std::endl;
    std::cout << "TankEye Qt app started at "
              << QDateTime::currentDateTime().toString(Qt::ISODate).toStdString()
              << std::endl;
    std::cout << "Log file: " << log_path.toStdString() << std::endl;
    std::cout.flush();
    return log_path;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/app/icon.ico")));
    const QString log_path = ConfigureFileLog();

    GraspMainWindow window;
    const QString obb_model_path = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    const QString seg_model_path = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QString();
    std::cout << "OBB model argument: " << obb_model_path.toStdString() << std::endl;
    std::cout << "SEG model argument: " << seg_model_path.toStdString() << std::endl;
    std::cout << "QT_SCALE_FACTOR: " << qgetenv("QT_SCALE_FACTOR").constData() << std::endl;
    std::cout << "TANKEYE_UI_SCALE: " << qgetenv("TANKEYE_UI_SCALE").constData() << std::endl;
    std::cout << "Runtime log is enabled." << std::endl;
    std::cout.flush();
    window.setInitialModelPaths(obb_model_path, seg_model_path);

    const QString window_mode = QString::fromLocal8Bit(qgetenv("TANKEYE_WINDOW_MODE")).trimmed().toLower();
    const int requested_width = QString::fromLocal8Bit(qgetenv("TANKEYE_WINDOW_WIDTH")).toInt();
    const int requested_height = QString::fromLocal8Bit(qgetenv("TANKEYE_WINDOW_HEIGHT")).toInt();
    const int window_width = requested_width > 0 ? requested_width : 1280;
    const int window_height = requested_height > 0 ? requested_height : 720;
    window.resize(window_width, window_height);

    if (window_mode == QStringLiteral("maximized")) {
        window.showMaximized();
    } else if (window_mode == QStringLiteral("fullscreen")) {
        window.showFullScreen();
    } else {
        window.show();
        if (QScreen* screen = window.screen()) {
            const QRect available = screen->availableGeometry();
            window.move(available.center() - window.rect().center());
        }
    }

    const int exit_code = app.exec();
    std::cout << "TankEye Qt app exited with code " << exit_code << std::endl;
    std::cout.flush();
    return exit_code;
}


