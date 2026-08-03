#pragma once

#include <QString>

#include <functional>

class QWidget;

using SaveAdminCredentialsCallback = std::function<bool(const QString& username,
                                                        const QString& password,
                                                        QString* error_message)>;
using ValidateAdminCredentialsCallback = std::function<bool(const QString& username,
                                                            const QString& password)>;
using SaveRememberedPasswordCallback = std::function<void(bool remember, const QString& password)>;
using ShowAdminResetCallback = std::function<bool()>;

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
                          const SaveAdminCredentialsCallback& save_credentials,
                          const SaveRememberedPasswordCallback& save_remembered_password);
