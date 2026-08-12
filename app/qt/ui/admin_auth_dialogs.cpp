#include "admin_auth_dialogs.h"

#include "admin_auth_helpers.h"
#include "ui_scale_utils.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

namespace {

QString AdminAuthDialogStyle(double ui_scale)
{
    return QString(
               "QDialog { background: #101720; }"
               "QLabel { color: #edf4fb; font-size: %1px; }"
               "QLabel#authHintLabel { color: #a7b9c9; font-size: %2px; }"
               "QLineEdit { background: #f8fafc; color: #10202d; border: 1px solid #6e86a0; border-radius: %3px; padding: %4px %5px; font-size: %1px; min-height: %6px; selection-background-color: #2c7be5; }"
               "QLineEdit[readOnly=\"true\"] { background: #dce6ef; color: #243444; }"
               "QPushButton { background: #1f79db; color: #ffffff; border: 1px solid #5b97e2; border-radius: %3px; padding: %4px %5px; font-size: %1px; font-weight: 600; min-height: %6px; }"
               "QPushButton:hover { background: #2f87e5; }"
               "QPushButton#secondaryAuthButton { background: #2a3440; color: #eef4fb; border: 1px solid #556679; }"
               "QPushButton#secondaryAuthButton:hover { background: #344150; }"
               "QCheckBox { color: #edf4fb; font-size: %2px; spacing: %7px; }"
               "QToolButton#passwordEyeButton { background: transparent; border: none; padding: 0px; }"
               "QToolButton#passwordEyeButton:hover { background: transparent; }")
        .arg(ScalePx(15, ui_scale))
        .arg(ScalePx(13, ui_scale))
        .arg(ScalePx(8, ui_scale))
        .arg(ScalePx(8, ui_scale))
        .arg(ScalePx(10, ui_scale))
        .arg(ScalePx(32, ui_scale))
        .arg(ScalePx(8, ui_scale));
}

QWidget* CreateAuthFormLabel(QString text, QWidget* parent, double ui_scale)
{
    text.remove(QLatin1Char(' '));
    text.remove(QChar(0x3000));
    text.remove(QStringLiteral("："));
    text.remove(QLatin1Char(':'));

    auto* container = new QWidget(parent);
    container->setMinimumWidth(ScalePx(112, ui_scale));
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    for (int i = 0; i < text.size(); ++i) {
        auto* char_label = new QLabel(QString(text.at(i)), container);
        char_label->setAlignment(Qt::AlignCenter);
        layout->addWidget(char_label, 0);
        if (i + 1 < text.size()) {
            layout->addStretch(1);
        }
    }

    auto* colon_label = new QLabel(QStringLiteral("："), container);
    colon_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(colon_label, 0);
    return container;
}

void SaveAdminLicenseRequestFile(QWidget* parent, const QString& window_title, const QString& machine_code)
{
    const QString default_file_name = QStringLiteral("TankEye_admin_license_request_%1.json").arg(machine_code);
    const QString path = QFileDialog::getSaveFileName(parent,
                                                      window_title,
                                                      default_file_name,
                                                      QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(parent,
                             window_title,
                             QStringLiteral("授权申请文件保存失败：%1").arg(file.errorString()));
        return;
    }
    const QByteArray request_json = AdminAuthLicenseRequestText().toUtf8();
    if (file.write(request_json) != request_json.size()) {
        QMessageBox::warning(parent,
                             window_title,
                             QStringLiteral("授权申请文件保存失败：%1").arg(file.errorString()));
        return;
    }

    QMessageBox::information(parent,
                             window_title,
                             QStringLiteral("授权申请文件已保存"));
}

void ConfigureAuthGrid(QGridLayout* grid, double ui_scale)
{
    grid->setHorizontalSpacing(ScalePx(8, ui_scale));
    grid->setVerticalSpacing(ScalePx(10, ui_scale));
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 0);
}

QLineEdit* AddTextRow(QGridLayout* grid,
                      int row,
                      const QString& label,
                      const QString& text,
                      QWidget* parent,
                      double ui_scale)
{
    auto* edit = new QLineEdit(text, parent);
    grid->addWidget(CreateAuthFormLabel(label, parent, ui_scale), row, 0);
    grid->addWidget(edit, row, 1);
    return edit;
}

QLineEdit* AddPasswordRow(QGridLayout* grid,
                          int row,
                          const QString& label,
                          const QString& placeholder,
                          QWidget* parent,
                          double ui_scale)
{
    auto* edit = AddTextRow(grid, row, label, QString(), parent, ui_scale);
    edit->setPlaceholderText(placeholder);
    grid->addWidget(CreatePasswordVisibilityButton(edit, parent), row, 2);
    return edit;
}

struct AccountAuthDialogOptions {
    QString window_title;
    QString hint;
    QString password_mismatch_message;
    QString submit_text;
    QString initial_username;
    bool require_existing_recovery_answer = false;
    QString recovery_question;
};

bool ShowAccountAuthCodeDialog(QWidget* parent,
                               double ui_scale,
                               const AccountAuthDialogOptions& options,
                               const ValidateRecoveryAnswerCallback& validate_recovery_answer,
                               const SaveAdminCredentialsCallback& save_credentials,
                               const SaveRememberedPasswordCallback& save_remembered_password)
{
    const QString machine_code = AdminAuthMachineCode();
    const AdminLicenseStatus license_status = AdminAuthLicenseStatus();

    QDialog dialog(parent);
    dialog.setWindowTitle(options.window_title);
    dialog.setWindowFlags(Qt::Window | Qt::Dialog);
    dialog.setStyleSheet(AdminAuthDialogStyle(ui_scale));

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(ScaleMargins(18, 18, 18, 18, ui_scale));
    layout->setSpacing(ScalePx(12, ui_scale));

    auto* hint_label = new QLabel(options.hint, &dialog);
    hint_label->setObjectName("authHintLabel");
    hint_label->setWordWrap(true);
    layout->addWidget(hint_label);

    auto* grid = new QGridLayout();
    ConfigureAuthGrid(grid, ui_scale);

    auto* machine_code_edit = AddTextRow(grid, 0, QStringLiteral("机器码："), machine_code, &dialog, ui_scale);
    machine_code_edit->setReadOnly(true);
    machine_code_edit->setCursorPosition(0);
    auto* save_request_button = new QPushButton(QStringLiteral("保存授权申请"), &dialog);
    save_request_button->setObjectName("secondaryAuthButton");
    grid->addWidget(save_request_button, 0, 2);

    auto* license_status_edit =
        AddTextRow(grid, 1, QStringLiteral("授权状态："), license_status.message, &dialog, ui_scale);
    license_status_edit->setReadOnly(true);
    int row = 2;
    QLineEdit* recovery_answer_check_edit = nullptr;
    if (options.require_existing_recovery_answer) {
        auto* recovery_question_edit =
            AddTextRow(grid, row++, QStringLiteral("恢复问题："), options.recovery_question, &dialog, ui_scale);
        recovery_question_edit->setReadOnly(true);
        recovery_answer_check_edit =
            AddPasswordRow(grid, row++, QStringLiteral("恢复答案："), QStringLiteral("输入恢复答案"), &dialog, ui_scale);
    }
    auto* username_edit = AddTextRow(grid, row++, QStringLiteral("账号："), options.initial_username, &dialog, ui_scale);
    auto* password_edit = AddPasswordRow(grid, row++, QStringLiteral("密码："), QStringLiteral("设置管理员密码"), &dialog, ui_scale);
    auto* confirm_edit = AddPasswordRow(grid, row++, QStringLiteral("确认密码："), QStringLiteral("再次输入密码"), &dialog, ui_scale);
    auto* recovery_question_edit =
        AddTextRow(grid,
                   row++,
                   options.require_existing_recovery_answer ? QStringLiteral("新恢复问题：") : QStringLiteral("恢复问题："),
                   QString(),
                   &dialog,
                   ui_scale);
    auto* recovery_answer_edit =
        AddPasswordRow(grid,
                       row++,
                       options.require_existing_recovery_answer ? QStringLiteral("新恢复答案：") : QStringLiteral("恢复答案："),
                       QStringLiteral("设置恢复答案"),
                       &dialog,
                       ui_scale);
    layout->addLayout(grid);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(options.submit_text);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);

    QObject::connect(save_request_button, &QPushButton::clicked, &dialog, [&dialog, window_title = options.window_title, machine_code]() {
        SaveAdminLicenseRequestFile(&dialog, window_title, machine_code);
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (!license_status.valid) {
            QMessageBox::warning(&dialog,
                                 options.window_title,
                                 QStringLiteral("管理员授权文件无效，不能创建或重置管理员。\n\n请保存授权申请文件，发给工程师生成 admin_license.json 后放入 config 目录。"));
            return;
        }
        if (password_edit->text() != confirm_edit->text()) {
            QMessageBox::warning(&dialog, options.window_title, options.password_mismatch_message);
            return;
        }
        if (recovery_answer_check_edit && !validate_recovery_answer(recovery_answer_check_edit->text())) {
            QMessageBox::warning(&dialog, options.window_title, QStringLiteral("恢复答案错误。"));
            return;
        }
        QString error;
        if (!save_credentials(username_edit->text(),
                              password_edit->text(),
                              recovery_question_edit->text(),
                              recovery_answer_edit->text(),
                              &error)) {
            QMessageBox::warning(&dialog, options.window_title, error);
            return;
        }
        save_remembered_password(false, QString());
        dialog.accept();
    });

    return dialog.exec() == QDialog::Accepted;
}

} // namespace

void ShowAdminLicenseRequestDialog(QWidget* parent, double ui_scale, const QString& window_title)
{
    const QString machine_code = AdminAuthMachineCode();
    const AdminLicenseStatus license_status = AdminAuthLicenseStatus();

    QDialog dialog(parent);
    dialog.setWindowTitle(window_title);
    dialog.setWindowFlags(Qt::Window | Qt::Dialog);
    dialog.setStyleSheet(AdminAuthDialogStyle(ui_scale));

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(ScaleMargins(18, 18, 18, 18, ui_scale));
    layout->setSpacing(ScalePx(12, ui_scale));

    auto* hint_label = new QLabel(
        QStringLiteral("普通检测可继续使用；管理员和工程设置需要本机有效授权文件。保存授权申请文件发给工程师，拿到 admin_license.json 后放入 config 目录。"),
        &dialog);
    hint_label->setObjectName("authHintLabel");
    hint_label->setWordWrap(true);
    layout->addWidget(hint_label);

    auto* grid = new QGridLayout();
    ConfigureAuthGrid(grid, ui_scale);

    auto* machine_code_edit = AddTextRow(grid, 0, QStringLiteral("机器码："), machine_code, &dialog, ui_scale);
    machine_code_edit->setReadOnly(true);
    machine_code_edit->setCursorPosition(0);
    auto* save_request_button = new QPushButton(QStringLiteral("保存授权申请"), &dialog);
    save_request_button->setObjectName("secondaryAuthButton");
    grid->addWidget(save_request_button, 0, 2);

    auto* license_status_edit =
        AddTextRow(grid, 1, QStringLiteral("授权状态："), license_status.message, &dialog, ui_scale);
    license_status_edit->setReadOnly(true);
    layout->addLayout(grid);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("关闭"));
    layout->addWidget(buttons);

    QObject::connect(save_request_button, &QPushButton::clicked, &dialog, [&dialog, window_title, machine_code]() {
        SaveAdminLicenseRequestFile(&dialog, window_title, machine_code);
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    dialog.exec();
}

bool ShowCreateAdminAccountDialog(QWidget* parent,
                                  double ui_scale,
                                  const SaveAdminCredentialsCallback& save_credentials,
                                  const SaveRememberedPasswordCallback& save_remembered_password)
{
    return ShowAccountAuthCodeDialog(
        parent,
        ui_scale,
        {
            QStringLiteral("创建管理员账号"),
            QStringLiteral("首次授权：保存授权申请文件发给工程师，拿到 admin_license.json 后放入 config 目录，才能创建管理员。"),
            QStringLiteral("两次输入的密码不一致。"),
            QStringLiteral("创建并登录"),
            QString(),
            false,
            QString(),
        },
        nullptr,
        save_credentials,
        save_remembered_password);
}

bool ShowAdminLoginDialog(QWidget* parent,
                          double ui_scale,
                          const QString& username,
                          const QString& remembered_password,
                          const ValidateAdminCredentialsCallback& validate_credentials,
                          const SaveRememberedPasswordCallback& save_remembered_password,
                          const ShowAdminResetCallback& show_reset_dialog)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("管理员登录"));
    dialog.setWindowFlags(Qt::Window | Qt::Dialog);
    dialog.setStyleSheet(AdminAuthDialogStyle(ui_scale));

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(ScaleMargins(18, 18, 18, 18, ui_scale));
    layout->setSpacing(ScalePx(12, ui_scale));

    auto* hint_label = new QLabel(QStringLiteral("管理员模式用于调试入口；普通模式下只保留目标列表。"), &dialog);
    hint_label->setObjectName("authHintLabel");
    hint_label->setWordWrap(true);
    layout->addWidget(hint_label);

    auto* form = new QGridLayout();
    ConfigureAuthGrid(form, ui_scale);
    auto* username_edit = AddTextRow(form, 0, QStringLiteral("账号："), username, &dialog, ui_scale);
    auto* password_edit = AddPasswordRow(form, 1, QStringLiteral("密码："), QString(), &dialog, ui_scale);
    password_edit->setText(remembered_password);
    auto* remember_check = new QCheckBox(QStringLiteral("记住密码"), &dialog);
    remember_check->setChecked(!password_edit->text().isEmpty());
    form->addWidget(remember_check, 2, 1);
    layout->addLayout(form);

    auto* button_row = new QHBoxLayout();
    button_row->setSpacing(ScalePx(8, ui_scale));
    auto* reset_button = new QPushButton(QStringLiteral("忘记密码"), &dialog);
    reset_button->setObjectName("secondaryAuthButton");
    auto* login_button = new QPushButton(QStringLiteral("登录"), &dialog);
    auto* cancel_button = new QPushButton(QStringLiteral("取消"), &dialog);
    cancel_button->setObjectName("secondaryAuthButton");
    button_row->addWidget(reset_button, 0, Qt::AlignLeft);
    button_row->addStretch(1);
    button_row->addWidget(login_button);
    button_row->addWidget(cancel_button);
    layout->addLayout(button_row);

    QObject::connect(reset_button, &QPushButton::clicked, &dialog, [&dialog, show_reset_dialog]() {
        if (show_reset_dialog()) {
            dialog.accept();
        }
    });
    QObject::connect(cancel_button, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(login_button, &QPushButton::clicked, &dialog, [&]() {
        if (!validate_credentials(username_edit->text(), password_edit->text())) {
            QMessageBox::warning(&dialog, QStringLiteral("登录失败"), QStringLiteral("管理员账号或密码错误。"));
            return;
        }
        save_remembered_password(remember_check->isChecked(), password_edit->text());
        dialog.accept();
    });

    return dialog.exec() == QDialog::Accepted;
}

bool ShowAdminResetDialog(QWidget* parent,
                          double ui_scale,
                          const QString& username,
                          const QString& recovery_question,
                          const ValidateRecoveryAnswerCallback& validate_recovery_answer,
                          const SaveAdminCredentialsCallback& save_credentials,
                          const SaveRememberedPasswordCallback& save_remembered_password)
{
    return ShowAccountAuthCodeDialog(
        parent,
        ui_scale,
        {
            QStringLiteral("重置管理员"),
            QStringLiteral("忘记密码时，需要本机有效 admin_license.json，并正确回答恢复问题后才能重置管理员账号。"),
            QStringLiteral("两次输入的新密码不一致。"),
            QStringLiteral("重置并登录"),
            username,
            true,
            recovery_question,
        },
        validate_recovery_answer,
        save_credentials,
        save_remembered_password);
}
