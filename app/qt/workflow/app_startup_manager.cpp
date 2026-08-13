#include "app_startup_manager.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QTemporaryFile>
#include <QStringList>
#include <QtGlobal>

namespace {

QString EscapeVbsString(QString value)
{
    return value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
}

QString DefaultStartupDir()
{
    const QString app_data = QProcessEnvironment::systemEnvironment().value(QStringLiteral("APPDATA")).trimmed();
    if (app_data.isEmpty()) {
        return QString();
    }
    return QDir(app_data).absoluteFilePath(QStringLiteral("Microsoft/Windows/Start Menu/Programs/Startup"));
}

QString ResolveAppDir(const QString& app_dir)
{
    if (!app_dir.trimmed().isEmpty()) {
        return QDir(app_dir).absolutePath();
    }
    return QCoreApplication::applicationDirPath();
}

QString FindLaunchScript(const QString& app_dir)
{
    const QDir dir(app_dir);
    const QStringList candidates = {
        dir.absoluteFilePath(QStringLiteral("launch_tankeye.ps1")),
        dir.absoluteFilePath(QStringLiteral("../launch_tankeye.ps1")),
        dir.absoluteFilePath(QStringLiteral("../../launch_tankeye.ps1")),
    };
    for (const QString& candidate : candidates) {
        if (QFile::exists(candidate)) {
            return QDir::cleanPath(candidate);
        }
    }
    return QDir::cleanPath(candidates.front());
}

void RemoveStartupEntries(const QDir& startup_dir)
{
    QFile::remove(startup_dir.absoluteFilePath(QStringLiteral("TankEye-Iris.vbs")));
    QFile::remove(startup_dir.absoluteFilePath(QStringLiteral("TankEye-Iris.lnk")));
}

void RemoveStartupTempEntries(const QDir& startup_dir)
{
    const QStringList temp_entries = startup_dir.entryList(QDir::Files);
    for (const QString& entry : temp_entries) {
        if (entry.startsWith(QStringLiteral("TankEye-Iris.vbs."))) {
            QFile::remove(startup_dir.absoluteFilePath(entry));
        }
    }
}

QString BuildStartupVbs(const QString& app_dir, const QString& launch_script, int delay_seconds)
{
    const QString launch_arguments = BuildStartupLaunchArguments(delay_seconds);
    return QStringLiteral(
        "Option Explicit\r\n"
        "\r\n"
        "Dim shell, appDir, launchScript, command\r\n"
        "\r\n"
        "Set shell = CreateObject(\"WScript.Shell\")\r\n"
        "\r\n"
        "appDir = \"%1\"\r\n"
        "launchScript = \"%2\"\r\n"
        "command = \"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \" & _\r\n"
        "          \"\"\"\" & launchScript & \"\"\"\" & \" %3\"\r\n"
        "\r\n"
        "shell.CurrentDirectory = appDir\r\n"
        "shell.Run command, 0, False\r\n")
        .arg(EscapeVbsString(QDir::toNativeSeparators(app_dir)))
        .arg(EscapeVbsString(QDir::toNativeSeparators(launch_script)))
        .arg(launch_arguments);
}

bool WriteStartupEntryFile(const QString& startup_entry_path,
                           const QByteArray& content,
                           QString* error_message)
{
    QTemporaryFile temp_file(QDir::tempPath() + QStringLiteral("/TankEye-Iris-Startup-XXXXXX.vbs"));
    temp_file.setAutoRemove(false);
    if (!temp_file.open()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("无法创建临时启动入口：%1").arg(temp_file.errorString());
        }
        return false;
    }
    if (temp_file.write(content) != content.size()) {
        temp_file.close();
        QFile::remove(temp_file.fileName());
        if (error_message != nullptr) {
            *error_message = QStringLiteral("写入临时启动入口失败：%1").arg(temp_file.errorString());
        }
        return false;
    }
    if (!temp_file.flush()) {
        const QString error = temp_file.errorString();
        temp_file.close();
        QFile::remove(temp_file.fileName());
        if (error_message != nullptr) {
            *error_message = QStringLiteral("刷新临时启动入口失败：%1").arg(error);
        }
        return false;
    }
    temp_file.close();

    QFile::remove(startup_entry_path);
    if (QFile::rename(temp_file.fileName(), startup_entry_path)) {
        return true;
    }

    if (QFile::copy(temp_file.fileName(), startup_entry_path)) {
        QFile::remove(temp_file.fileName());
        return true;
    }

    const QString error = QStringLiteral("无法替换开机启动入口：%1 -> %2")
                              .arg(temp_file.fileName(), startup_entry_path);
    QFile::remove(temp_file.fileName());
    if (error_message != nullptr) {
        *error_message = error;
    }
    return false;
}

} // namespace

int ClampStartupDelaySeconds(int delay_seconds)
{
    return qBound(0, delay_seconds, 600);
}

QString BuildStartupLaunchArguments(int delay_seconds)
{
    return QStringLiteral("-StartupProfile AutoStart -StartupDelaySeconds %1")
        .arg(ClampStartupDelaySeconds(delay_seconds));
}

StartupShortcutSyncResult SyncStartupShortcut(const StartupLaunchSettings& settings,
                                              const QString& app_dir,
                                              const QString& startup_dir_override)
{
    StartupShortcutSyncResult result;
    const QString startup_dir_path = startup_dir_override.trimmed().isEmpty()
        ? DefaultStartupDir()
        : startup_dir_override;
    if (startup_dir_path.trimmed().isEmpty()) {
        result.error_message = QStringLiteral("无法定位当前用户 Startup 文件夹。");
        return result;
    }

    const QDir startup_dir(startup_dir_path);
    result.startup_entry_path = startup_dir.absoluteFilePath(QStringLiteral("TankEye-Iris.vbs"));
    if (!QDir().mkpath(startup_dir.absolutePath())) {
        result.error_message = QStringLiteral("无法创建 Startup 文件夹：%1").arg(startup_dir.absolutePath());
        return result;
    }
    RemoveStartupTempEntries(startup_dir);

    if (!settings.auto_start_enabled) {
        RemoveStartupEntries(startup_dir);
        RemoveStartupTempEntries(startup_dir);
        result.success = true;
        return result;
    }

    const QString resolved_app_dir = ResolveAppDir(app_dir);
    const QString launch_script = FindLaunchScript(resolved_app_dir);
    if (!QFile::exists(launch_script)) {
        result.error_message = QStringLiteral("启动脚本不存在：%1").arg(launch_script);
        return result;
    }

    const QByteArray content =
        BuildStartupVbs(resolved_app_dir, launch_script, ClampStartupDelaySeconds(settings.delay_seconds)).toUtf8();

    QFile existing_file(result.startup_entry_path);
    if (existing_file.open(QIODevice::ReadOnly)) {
        const bool unchanged = existing_file.readAll() == content;
        existing_file.close();
        if (unchanged) {
            QFile::remove(startup_dir.absoluteFilePath(QStringLiteral("TankEye-Iris.lnk")));
            RemoveStartupTempEntries(startup_dir);
            result.success = true;
            return result;
        }
    }

    RemoveStartupEntries(startup_dir);
    if (!WriteStartupEntryFile(result.startup_entry_path, content, &result.error_message)) {
        return result;
    }
    RemoveStartupTempEntries(startup_dir);

    result.success = true;
    return result;
}
