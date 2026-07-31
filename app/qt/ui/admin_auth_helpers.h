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
QAction* AttachPasswordVisibilityAction(QLineEdit* edit, QWidget* parent);
QToolButton* CreatePasswordVisibilityButton(QLineEdit* edit, QWidget* parent);
QWidget* CreatePasswordFieldWithVisibilityButton(QLineEdit* edit, QWidget* parent);
