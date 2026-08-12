#pragma once

#include "engineering_settings_service.h"

#include <QString>

struct StartupShortcutSyncResult
{
    bool success = false;
    QString startup_entry_path;
    QString error_message;
};

int ClampStartupDelaySeconds(int delay_seconds);
QString BuildStartupLaunchArguments(int delay_seconds);
StartupShortcutSyncResult SyncStartupShortcut(const StartupLaunchSettings& settings,
                                              const QString& app_dir = QString(),
                                              const QString& startup_dir_override = QString());
