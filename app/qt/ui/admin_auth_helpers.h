#pragma once

#include <QString>
#include <QStringList>

struct AdminHardwareFingerprint
{
    QString machine_guid;
    QString bios_serial;
    QString system_drive_serial;
    QString qt_machine_id;
    QStringList mac_addresses;
};

struct AdminLicenseStatus
{
    bool valid = false;
    QString message;
    QString machine_code;
    int matched_categories = 0;
};

class QAction;
class QDialog;
class QLineEdit;
class QToolButton;
class QWidget;

QString AdminAuthMachineCode();
QString AdminAuthLicenseRequestText();
AdminLicenseStatus AdminAuthLicenseStatus();
AdminHardwareFingerprint AdminAuthCurrentHardwareFingerprint();
QString AdminAuthMachineCode(const AdminHardwareFingerprint& fingerprint);
QByteArray BuildAdminLicenseJson(const QByteArray& request_json,
                                 const QString& secret,
                                 const QString& issued_at_utc,
                                 QString* error_message = nullptr);
AdminLicenseStatus VerifyAdminLicenseJson(const QByteArray& license_json,
                                          const AdminHardwareFingerprint& fingerprint,
                                          const QString& secret);
QString AdminAuthSecret(QString* error_message = nullptr);
bool AdminAuthHasAccount();
QString AdminAuthUsername();
bool AdminAuthHasRecoveryChallenge();
QString AdminAuthRecoveryQuestion();
bool AdminAuthValidateRecoveryAnswer(const QString& answer);
bool AdminAuthSetCredentials(const QString& username,
                             const QString& password,
                             const QString& recovery_question,
                             const QString& recovery_answer,
                             QString* error_message);
bool AdminAuthResetCredentials(const QString& username,
                               const QString& password,
                               const QString& recovery_question,
                               const QString& recovery_answer,
                               QString* error_message);
bool AdminAuthChangeCredentials(const QString& current_password,
                                const QString& username,
                                const QString& new_password,
                                QString* error_message);
bool AdminAuthValidateCredentials(const QString& username, const QString& password);
QString AdminAuthRememberedPassword();
void AdminAuthSaveRememberedPassword(bool remember, const QString& password);
QAction* AttachPasswordVisibilityAction(QLineEdit* edit, QWidget* parent);
QToolButton* CreatePasswordVisibilityButton(QLineEdit* edit, QWidget* parent);
QWidget* CreatePasswordFieldWithVisibilityButton(QLineEdit* edit, QWidget* parent);
