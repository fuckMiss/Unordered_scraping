#pragma once

#include <QString>

class QAction;
class QDialog;
class QLineEdit;
class QToolButton;
class QWidget;

QString AdminAuthMachineCode();
QString AdminAuthSecret(QString* error_message = nullptr);
QString BuildAdminAuthCode(const QString& machine_code, const QString& secret, const QString& purpose);
bool VerifyAdminAuthCode(const QString& machine_code,
                         const QString& provided_code,
                         const QString& secret,
                         const QString& purpose);
bool AdminAuthHasAccount();
QString AdminAuthUsername();
bool AdminAuthSetCredentials(const QString& username, const QString& password, QString* error_message);
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
