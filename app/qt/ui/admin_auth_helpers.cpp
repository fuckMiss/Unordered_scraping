#include "admin_auth_helpers.h"

#include <QAction>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QIODevice>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSizePolicy>
#include <QRegularExpression>
#include <QSysInfo>
#include <QToolButton>
#include <QPixmap>
#include <QWidget>

namespace {

constexpr int kAuthIconBaseSize = 18;
constexpr const char* kDefaultAdminSecret = "TankEye-Iris-Admin-Dev-Secret-v1";

QString NormalizeCode(QString code)
{
    code = code.toUpper().remove(QRegularExpression(QStringLiteral("[^A-Z0-9]")));
    return code;
}

QString FingerprintSeed()
{
    QString seed = QString::fromLatin1(QSysInfo::machineUniqueId().toHex());
    seed += QStringLiteral("|");
    seed += QSysInfo::machineHostName().trimmed();
    seed += QStringLiteral("|");
    seed += QSysInfo::prettyProductName().trimmed();
    if (seed.trimmed().isEmpty()) {
        seed = QStringLiteral("TankEye-Iris");
    }
    return seed;
}

QString ToGroupedCode(const QByteArray& digest, int length = 16)
{
    const QString hex = QString::fromLatin1(digest.toHex()).toUpper().left(length);
    QString grouped;
    for (int i = 0; i < hex.size(); ++i) {
        if (i > 0 && i % 4 == 0) {
            grouped += QLatin1Char('-');
        }
        grouped += hex.at(i);
    }
    return grouped;
}

QIcon CreateEyeIcon(bool visible)
{
    const QSize size(kAuthIconBaseSize, kAuthIconBaseSize);
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(visible ? QStringLiteral("#60a5fa") : QStringLiteral("#93c5fd")));
    pen.setWidthF(1.7);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const QRectF eye_rect(2.5, 5.0, 13.0, 8.0);
    QPainterPath eye_path;
    eye_path.moveTo(2.5, 9.0);
    eye_path.quadTo(8.0, 3.0, 15.5, 9.0);
    eye_path.quadTo(8.0, 15.0, 2.5, 9.0);
    painter.drawPath(eye_path);
    painter.drawEllipse(QPointF(8.0, 9.0), 2.3, 2.3);
    if (!visible) {
        painter.drawLine(QPointF(3.5, 14.0), QPointF(14.5, 4.0));
    }
    return QIcon(pixmap);
}

} // namespace

QString AdminAuthMachineCode()
{
    QByteArray seed = FingerprintSeed().toUtf8();
    const QByteArray digest = QCryptographicHash::hash(seed, QCryptographicHash::Sha256);
    return QStringLiteral("TK-") + ToGroupedCode(digest, 16);
}

QString AdminAuthSecret(QString* error_message)
{
    const QString env_secret = QString::fromLocal8Bit(qgetenv("TANKEYE_ADMIN_AUTH_SECRET")).trimmed();
    if (!env_secret.isEmpty()) {
        return env_secret;
    }

    const QString env_path = QString::fromLocal8Bit(qgetenv("TANKEYE_ADMIN_AUTH_KEY_FILE")).trimmed();
    const QStringList candidates = {
        env_path,
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config/admin_auth.key")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("admin_auth.key"))
    };

    for (const QString& candidate : candidates) {
        if (candidate.trimmed().isEmpty()) {
            continue;
        }
        QFile file(candidate);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const QString text = QString::fromUtf8(file.readAll()).trimmed();
        if (text.isEmpty()) {
            continue;
        }
        const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            const QString trimmed = line.trimmed();
            if (trimmed.startsWith(QStringLiteral("secret="), Qt::CaseInsensitive)) {
                return trimmed.mid(QStringLiteral("secret=").size()).trimmed();
            }
            if (trimmed.startsWith(QStringLiteral("private_key="), Qt::CaseInsensitive)) {
                return trimmed.mid(QStringLiteral("private_key=").size()).trimmed();
            }
        }
        return text;
    }

    if (error_message) {
        *error_message = QStringLiteral("未找到管理员认证密钥，已使用开发默认密钥。建议为正式部署创建 config/admin_auth.key。");
    }
    return QString::fromLatin1(kDefaultAdminSecret);
}

QString BuildAdminAuthCode(const QString& machine_code, const QString& secret, const QString& purpose)
{
    const QString normalized_machine = NormalizeCode(machine_code);
    const QString normalized_purpose = purpose.trimmed().toUpper();
    const QByteArray payload = (normalized_purpose + QStringLiteral("|") + normalized_machine + QStringLiteral("|") + secret.trimmed()).toUtf8();
    const QByteArray digest = QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
    return ToGroupedCode(digest, 16);
}

bool VerifyAdminAuthCode(const QString& machine_code,
                         const QString& provided_code,
                         const QString& secret,
                         const QString& purpose)
{
    return NormalizeCode(provided_code) == NormalizeCode(BuildAdminAuthCode(machine_code, secret, purpose));
}

QAction* AttachPasswordVisibilityAction(QLineEdit* edit, QWidget* parent)
{
    if (!edit) {
        return nullptr;
    }

    auto* action = new QAction(parent);
    action->setCheckable(true);
    action->setIcon(CreateEyeIcon(false));
    action->setToolTip(QStringLiteral("显示密码"));
    edit->setEchoMode(QLineEdit::Password);
    edit->addAction(action, QLineEdit::TrailingPosition);
    QObject::connect(action, &QAction::toggled, edit, [edit, action](bool checked) {
        edit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        action->setIcon(CreateEyeIcon(checked));
        action->setToolTip(checked ? QStringLiteral("隐藏密码") : QStringLiteral("显示密码"));
    });
    return action;
}

QToolButton* CreatePasswordVisibilityButton(QLineEdit* edit, QWidget* parent)
{
    if (!edit) {
        return nullptr;
    }
    edit->setEchoMode(QLineEdit::Password);
    edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* button = new QToolButton(parent);
    button->setObjectName("passwordEyeButton");
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setFixedSize(30, 30);
    button->setIconSize(QSize(kAuthIconBaseSize, kAuthIconBaseSize));
    button->setIcon(CreateEyeIcon(false));
    button->setToolTip(QStringLiteral("显示密码"));

    QObject::connect(button, &QToolButton::toggled, edit, [edit, button](bool checked) {
        edit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        button->setIcon(CreateEyeIcon(checked));
        button->setToolTip(checked ? QStringLiteral("隐藏密码") : QStringLiteral("显示密码"));
    });

    return button;
}

QWidget* CreatePasswordFieldWithVisibilityButton(QLineEdit* edit, QWidget* parent)
{
    auto* container = new QWidget(parent);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* button = CreatePasswordVisibilityButton(edit, container);
    layout->addWidget(edit, 1);
    layout->addWidget(button, 0);
    return container;
}
