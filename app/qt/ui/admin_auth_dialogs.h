#pragma once

#include <QString>

#include <functional>

class QWidget;

using SaveAdminCredentialsCallback = std::function<bool(const QString& username,
                                                        const QString& password,
                                                        const QString& recovery_question,
                                                        const QString& recovery_answer,
                                                        QString* error_message)>;
using ValidateAdminCredentialsCallback = std::function<bool(const QString& username,
                                                            const QString& password)>;
using ValidateRecoveryAnswerCallback = std::function<bool(const QString& answer)>;
using SaveRememberedPasswordCallback = std::function<void(bool remember, const QString& password)>;
using ShowAdminResetCallback = std::function<bool()>;

void ShowAdminLicenseRequestDialog(QWidget* parent, double ui_scale, const QString& window_title);

bool ShowCreateAdminAccountDialog(QWidget* parent,
                                  double ui_scale,
                                  const SaveAdminCredentialsCallback& save_credentials,
                                  const SaveRememberedPasswordCallback& save_remembered_password);

bool ShowAdminLoginDialog(QWidget* parent,
                          double ui_scale,
                          const QString& username,
                          const QString& remembered_password,
                          const ValidateAdminCredentialsCallback& validate_credentials,
                          const SaveRememberedPasswordCallback& save_remembered_password,
                          const ShowAdminResetCallback& show_reset_dialog);

bool ShowAdminResetDialog(QWidget* parent,
                          double ui_scale,
                          const QString& username,
                          const QString& recovery_question,
                          const ValidateRecoveryAnswerCallback& validate_recovery_answer,
                          const SaveAdminCredentialsCallback& save_credentials,
                          const SaveRememberedPasswordCallback& save_remembered_password);
