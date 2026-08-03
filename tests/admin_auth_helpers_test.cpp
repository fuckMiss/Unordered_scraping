#include "admin_auth_helpers.h"

#include <QDir>
#include <QSettings>

#include <cassert>
#include <iostream>

int main()
{
    const QString temp_root = QDir::temp().absoluteFilePath(QStringLiteral("tankeye_admin_auth_helpers_test"));
    QDir(temp_root).removeRecursively();
    QDir().mkpath(temp_root);
    qputenv("XDG_CONFIG_HOME", temp_root.toLocal8Bit());

    QSettings settings(QStringLiteral("TankEye"), QStringLiteral("TankEye-Iris"));
    settings.clear();
    settings.sync();

    assert(!AdminAuthHasAccount());
    QString error;
    assert(!AdminAuthSetCredentials(QString(), QStringLiteral("1234"), &error));
    assert(error.contains(QStringLiteral("不能为空")));
    assert(!AdminAuthSetCredentials(QStringLiteral("admin"), QStringLiteral("123"), &error));
    assert(error.contains(QStringLiteral("至少")));

    assert(AdminAuthSetCredentials(QStringLiteral(" admin "), QStringLiteral("pass123"), &error));
    assert(AdminAuthHasAccount());
    assert(AdminAuthUsername() == QStringLiteral("admin"));
    assert(AdminAuthValidateCredentials(QStringLiteral("admin"), QStringLiteral("pass123")));
    assert(!AdminAuthValidateCredentials(QStringLiteral("admin"), QStringLiteral("bad")));

    assert(!AdminAuthChangeCredentials(QStringLiteral("bad"), QStringLiteral("ops"), QStringLiteral("newpass"), &error));
    assert(error.contains(QStringLiteral("当前密码")));
    assert(AdminAuthChangeCredentials(QStringLiteral("pass123"), QStringLiteral("ops"), QStringLiteral("newpass"), &error));
    assert(AdminAuthUsername() == QStringLiteral("ops"));
    assert(AdminAuthValidateCredentials(QStringLiteral("ops"), QStringLiteral("newpass")));

    AdminAuthSaveRememberedPassword(true, QStringLiteral("newpass"));
    assert(AdminAuthRememberedPassword() == QStringLiteral("newpass"));
    AdminAuthSaveRememberedPassword(false, QString());
    assert(AdminAuthRememberedPassword().isEmpty());

    QDir(temp_root).removeRecursively();
    std::cout << "admin_auth_helpers_test passed" << std::endl;
    return 0;
}
