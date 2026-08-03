#include "admin_auth_dialogs.h"

#include "admin_auth_helpers.h"
#include "ui_scale_utils.h"

#include <QAbstractAnimation>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QTimer>
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
               "QLabel#authCopyToast { color: #9be7b0; font-size: %2px; font-weight: 600; }"
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

void ShowCopyToast(QLabel* toast)
{
    if (!toast) {
        return;
    }

    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(toast->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(toast);
        toast->setGraphicsEffect(effect);
    }
    effect->setOpacity(1.0);
    toast->setVisible(true);

    auto* animation = new QPropertyAnimation(effect, "opacity", toast);
    animation->setDuration(1200);
    animation->setStartValue(1.0);
    animation->setEndValue(0.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(animation, &QPropertyAnimation::finished, toast, [toast]() {
        toast->setVisible(false);
    });
    QTimer::singleShot(650, toast, [animation]() {
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    });
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
    QString code_label;
    QString code_placeholder;
    QString purpose;
    QString empty_code_message;
    QString password_mismatch_message;
    QString invalid_code_message;
    QString submit_text;
    QString initial_username;
};

bool ShowAccountAuthCodeDialog(QWidget* parent,
                               double ui_scale,
                               const AccountAuthDialogOptions& options,
                               const SaveAdminCredentialsCallback& save_credentials,
                               const SaveRememberedPasswordCallback& save_remembered_password)
{
    const QString machine_code = AdminAuthMachineCode();
    QString auth_secret_warning;
    const QString auth_secret = AdminAuthSecret(&auth_secret_warning);

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
    auto* copy_machine_button = new QPushButton(QStringLiteral("复制"), &dialog);
    copy_machine_button->setObjectName("secondaryAuthButton");
    grid->addWidget(copy_machine_button, 0, 2);

    auto* code_edit = AddTextRow(grid, 1, options.code_label, QString(), &dialog, ui_scale);
    code_edit->setPlaceholderText(options.code_placeholder);
    auto* username_edit = AddTextRow(grid, 2, QStringLiteral("账号："), options.initial_username, &dialog, ui_scale);
    auto* password_edit = AddPasswordRow(grid, 3, QStringLiteral("密码："), QStringLiteral("设置管理员密码"), &dialog, ui_scale);
    auto* confirm_edit = AddPasswordRow(grid, 4, QStringLiteral("确认密码："), QStringLiteral("再次输入密码"), &dialog, ui_scale);
    layout->addLayout(grid);

    auto* copy_toast = new QLabel(QStringLiteral("机器码已复制"), &dialog);
    copy_toast->setObjectName("authCopyToast");
    copy_toast->setVisible(false);
    layout->addWidget(copy_toast, 0, Qt::AlignRight);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(options.submit_text);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);

    QObject::connect(copy_machine_button, &QPushButton::clicked, &dialog, [machine_code_edit, copy_toast]() {
        QApplication::clipboard()->setText(machine_code_edit->text());
        ShowCopyToast(copy_toast);
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (code_edit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, options.window_title, options.empty_code_message);
            return;
        }
        if (password_edit->text() != confirm_edit->text()) {
            QMessageBox::warning(&dialog, options.window_title, options.password_mismatch_message);
            return;
        }
        if (!VerifyAdminAuthCode(machine_code_edit->text(), code_edit->text(), auth_secret, options.purpose)) {
            QString message = options.invalid_code_message;
            if (!auth_secret_warning.isEmpty()) {
                message += QStringLiteral("\n\n当前为开发默认密钥，测试码应为：%1")
                               .arg(BuildAdminAuthCode(machine_code_edit->text(), auth_secret, options.purpose));
            }
            QMessageBox::warning(&dialog, options.window_title, message);
            return;
        }
        QString error;
        if (!save_credentials(username_edit->text(), password_edit->text(), &error)) {
            QMessageBox::warning(&dialog, options.window_title, error);
            return;
        }
        save_remembered_password(false, QString());
        dialog.accept();
    });

    return dialog.exec() == QDialog::Accepted;
}

} // namespace

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
            QStringLiteral("首次授权：把机器码发给工程师，拿到授权码后才能创建管理员。"),
            QStringLiteral("授权码："),
            QStringLiteral("请输入工程师提供的授权码"),
            QStringLiteral("INIT"),
            QStringLiteral("请输入授权码。"),
            QStringLiteral("两次输入的密码不一致。"),
            QStringLiteral("授权码无效。"),
            QStringLiteral("创建并登录"),
            QString(),
        },
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
                          const SaveAdminCredentialsCallback& save_credentials,
                          const SaveRememberedPasswordCallback& save_remembered_password)
{
    return ShowAccountAuthCodeDialog(
        parent,
        ui_scale,
        {
            QStringLiteral("重置管理员"),
            QStringLiteral("忘记密码时，使用机器码和重置码重新设置管理员账号。"),
            QStringLiteral("重置码："),
            QStringLiteral("请输入维护重置码"),
            QStringLiteral("RESET"),
            QStringLiteral("请输入重置码。"),
            QStringLiteral("两次输入的新密码不一致。"),
            QStringLiteral("重置码无效。"),
            QStringLiteral("重置并登录"),
            username,
        },
        save_credentials,
        save_remembered_password);
}
