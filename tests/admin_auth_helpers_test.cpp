#include "admin_auth_helpers.h"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QSettings>

#include <cassert>
#include <iostream>

namespace {

AdminHardwareFingerprint MakeFingerprint()
{
    AdminHardwareFingerprint fingerprint;
    fingerprint.machine_guid = QStringLiteral("machine-guid-a");
    fingerprint.bios_serial = QStringLiteral("bios-a");
    fingerprint.system_drive_serial = QStringLiteral("drive-a");
    fingerprint.qt_machine_id = QStringLiteral("qt-a");
    fingerprint.mac_addresses = QStringList({
        QStringLiteral("00:11:22:33:44:55"),
        QStringLiteral("66:77:88:99:AA:BB"),
    });
    return fingerprint;
}

QByteArray RequestJson(const AdminHardwareFingerprint& fingerprint)
{
    QJsonArray macs;
    for (const QString& mac : fingerprint.mac_addresses) {
        macs.append(mac);
    }
    QJsonObject request;
    request.insert(QStringLiteral("version"), QStringLiteral("1"));
    request.insert(QStringLiteral("machine_code"), AdminAuthMachineCode(fingerprint));
    request.insert(QStringLiteral("machine_guid"), fingerprint.machine_guid);
    request.insert(QStringLiteral("bios_serial"), fingerprint.bios_serial);
    request.insert(QStringLiteral("system_drive_serial"), fingerprint.system_drive_serial);
    request.insert(QStringLiteral("qt_machine_id"), fingerprint.qt_machine_id);
    request.insert(QStringLiteral("mac_addresses"), macs);
    return QJsonDocument(request).toJson(QJsonDocument::Compact);
}

void LicenseAllowsOneFingerprintChange()
{
    const QString secret = QStringLiteral("unit-test-secret");
    const AdminHardwareFingerprint fingerprint = MakeFingerprint();
    QString error;
    const QByteArray license = BuildAdminLicenseJson(RequestJson(fingerprint),
                                                     secret,
                                                     QStringLiteral("2026-08-12T00:00:00Z"),
                                                     &error);
    assert(!license.isEmpty());
    assert(error.isEmpty());

    AdminLicenseStatus status = VerifyAdminLicenseJson(license, fingerprint, secret);
    assert(status.valid);
    assert(status.matched_categories == 5);
    assert(status.machine_code == AdminAuthMachineCode(fingerprint));
    assert(status.message == QStringLiteral("已授权"));

    AdminHardwareFingerprint one_changed = fingerprint;
    one_changed.system_drive_serial = QStringLiteral("drive-b");
    status = VerifyAdminLicenseJson(license, one_changed, secret);
    assert(status.valid);
    assert(status.matched_categories == 4);

    AdminHardwareFingerprint mac_changed = fingerprint;
    mac_changed.mac_addresses = QStringList({
        QStringLiteral("66:77:88:99:AA:BB"),
        QStringLiteral("AA:BB:CC:DD:EE:FF"),
    });
    status = VerifyAdminLicenseJson(license, mac_changed, secret);
    assert(status.valid);
    assert(status.matched_categories == 5);

    AdminHardwareFingerprint two_changed = fingerprint;
    two_changed.system_drive_serial = QStringLiteral("drive-b");
    two_changed.qt_machine_id = QStringLiteral("qt-b");
    status = VerifyAdminLicenseJson(license, two_changed, secret);
    assert(!status.valid);
    assert(status.matched_categories == 3);

    AdminHardwareFingerprint missing_category = fingerprint;
    missing_category.bios_serial.clear();
    status = VerifyAdminLicenseJson(license, missing_category, secret);
    assert(status.valid);
    assert(status.matched_categories == 4);

    AdminHardwareFingerprint missing_two_categories = fingerprint;
    missing_two_categories.bios_serial.clear();
    missing_two_categories.qt_machine_id.clear();
    status = VerifyAdminLicenseJson(license, missing_two_categories, secret);
    assert(!status.valid);
    assert(status.matched_categories == 3);
    assert(status.message == QStringLiteral("授权文件不符"));

    QJsonDocument tampered_document = QJsonDocument::fromJson(license);
    QJsonObject tampered = tampered_document.object();
    tampered.insert(QStringLiteral("purpose"), QStringLiteral("OTHER"));
    status = VerifyAdminLicenseJson(QJsonDocument(tampered).toJson(QJsonDocument::Compact), fingerprint, secret);
    assert(!status.valid);
}

} // namespace

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
    assert(!AdminAuthSetCredentials(QString(), QStringLiteral("1234"), QStringLiteral("问题"), QStringLiteral("答案"), &error));
    assert(error.contains(QStringLiteral("不能为空")));
    assert(!AdminAuthSetCredentials(QStringLiteral("admin"), QStringLiteral("123"), QStringLiteral("问题"), QStringLiteral("答案"), &error));
    assert(error.contains(QStringLiteral("至少")));
    assert(!AdminAuthSetCredentials(QStringLiteral("admin"), QStringLiteral("1234"), QString(), QStringLiteral("答案"), &error));
    assert(error.contains(QStringLiteral("恢复问题")));
    assert(!AdminAuthSetCredentials(QStringLiteral("admin"), QStringLiteral("1234"), QStringLiteral("问题"), QString(), &error));
    assert(error.contains(QStringLiteral("恢复答案")));

    assert(AdminAuthSetCredentials(QStringLiteral(" admin "),
                                   QStringLiteral("pass123"),
                                   QStringLiteral("谁可以重置密码"),
                                   QStringLiteral("老板"),
                                   &error));
    assert(AdminAuthHasAccount());
    assert(AdminAuthUsername() == QStringLiteral("admin"));
    assert(AdminAuthHasRecoveryChallenge());
    assert(AdminAuthRecoveryQuestion() == QStringLiteral("谁可以重置密码"));
    assert(AdminAuthValidateRecoveryAnswer(QStringLiteral(" 老板 ")));
    assert(!AdminAuthValidateRecoveryAnswer(QStringLiteral("操作员")));
    assert(AdminAuthValidateCredentials(QStringLiteral("admin"), QStringLiteral("pass123")));
    assert(!AdminAuthValidateCredentials(QStringLiteral("admin"), QStringLiteral("bad")));

    assert(!AdminAuthChangeCredentials(QStringLiteral("bad"),
                                       QStringLiteral("ops"),
                                       QStringLiteral("newpass"),
                                       &error));
    assert(error.contains(QStringLiteral("当前密码")));
    assert(AdminAuthChangeCredentials(QStringLiteral("pass123"),
                                      QStringLiteral("ops"),
                                      QStringLiteral("newpass"),
                                      &error));
    assert(AdminAuthUsername() == QStringLiteral("ops"));
    assert(AdminAuthRecoveryQuestion() == QStringLiteral("谁可以重置密码"));
    assert(AdminAuthValidateRecoveryAnswer(QStringLiteral("老板")));
    assert(AdminAuthValidateCredentials(QStringLiteral("ops"), QStringLiteral("newpass")));

    assert(AdminAuthResetCredentials(QStringLiteral("boss"), QStringLiteral("resetpass"), QString(), QString(), &error));
    assert(AdminAuthUsername() == QStringLiteral("boss"));
    assert(AdminAuthValidateCredentials(QStringLiteral("boss"), QStringLiteral("resetpass")));
    assert(AdminAuthRecoveryQuestion() == QStringLiteral("谁可以重置密码"));
    assert(AdminAuthValidateRecoveryAnswer(QStringLiteral("老板")));

    assert(!AdminAuthResetCredentials(QStringLiteral("boss"),
                                      QStringLiteral("resetpass2"),
                                      QStringLiteral("新问题"),
                                      QString(),
                                      &error));
    assert(error.contains(QStringLiteral("必须同时")));
    assert(!AdminAuthResetCredentials(QStringLiteral("boss"),
                                      QStringLiteral("resetpass2"),
                                      QString(),
                                      QStringLiteral("新答案"),
                                      &error));
    assert(error.contains(QStringLiteral("必须同时")));

    assert(AdminAuthResetCredentials(QStringLiteral("boss"),
                                     QStringLiteral("resetpass2"),
                                     QStringLiteral("谁批准重置"),
                                     QStringLiteral("调试员"),
                                     &error));
    assert(AdminAuthRecoveryQuestion() == QStringLiteral("谁批准重置"));
    assert(AdminAuthValidateRecoveryAnswer(QStringLiteral("调试员")));
    assert(!AdminAuthValidateRecoveryAnswer(QStringLiteral("老板")));

    AdminAuthSaveRememberedPassword(true, QStringLiteral("newpass"));
    assert(AdminAuthRememberedPassword() == QStringLiteral("newpass"));
    AdminAuthSaveRememberedPassword(false, QString());
    assert(AdminAuthRememberedPassword().isEmpty());

    LicenseAllowsOneFingerprintChange();

    QDir(temp_root).removeRecursively();
    std::cout << "admin_auth_helpers_test passed" << std::endl;
    return 0;
}
