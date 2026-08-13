#include "admin_auth_helpers.h"

#include <QAction>
#include <QByteArray>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QIODevice>
#include <QNetworkInterface>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScreen>
#include <QSettings>
#include <QSizePolicy>
#include <QRegularExpression>
#include <QSysInfo>
#include <QToolButton>
#include <QPixmap>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

constexpr int kAuthIconBaseSize = 18;
constexpr int kAdminSaltBytes = 16;
constexpr const char* kSettingsOrganization = "TankEye";
constexpr const char* kSettingsApplication = "TankEye-Iris";
constexpr const char* kAdminAuthGroup = "admin_auth";
constexpr const char* kLicenseVersion = "1";
constexpr int kRequiredLicenseMatches = 5;

QString NormalizeCode(QString code)
{
    code = code.toUpper().remove(QRegularExpression(QStringLiteral("[^A-Z0-9]")));
    return code;
}

QString NormalizeFingerprintValue(QString value)
{
    value = value.trimmed().toUpper();
    value.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return value;
}

QString Sha256Hex(const QByteArray& payload)
{
    return QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

QString HashFingerprintValue(const QString& label, const QString& value)
{
    const QString normalized = NormalizeFingerprintValue(value);
    if (normalized.isEmpty()) {
        return QString();
    }
    return Sha256Hex((label + QStringLiteral("|") + normalized).toUtf8());
}

QString RegistryValue(const QString& key, const QString& name)
{
    QSettings settings(key, QSettings::NativeFormat);
    return settings.value(name).toString().trimmed();
}

QString WmicValue(const QStringList& args)
{
    QProcess process;
    process.start(QStringLiteral("wmic"), args);
    if (!process.waitForFinished(2500) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return QString();
    }
    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    for (const QString& line : output.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty() &&
            trimmed.compare(args.last(), Qt::CaseInsensitive) != 0 &&
            trimmed.compare(QStringLiteral("SerialNumber"), Qt::CaseInsensitive) != 0) {
            return trimmed;
        }
    }
    return QString();
}

QString SystemDriveRoot()
{
    QString root = QString::fromLocal8Bit(qgetenv("SystemDrive")).trimmed();
    if (root.isEmpty()) {
        root = QDir::rootPath();
    }
    if (!root.endsWith(QStringLiteral("\\")) && !root.endsWith(QStringLiteral("/"))) {
        root += QStringLiteral("\\");
    }
    return root;
}

QString SystemDriveSerial()
{
#ifdef Q_OS_WIN
    DWORD serial = 0;
    if (GetVolumeInformationW(reinterpret_cast<LPCWSTR>(SystemDriveRoot().utf16()),
                              nullptr,
                              0,
                              &serial,
                              nullptr,
                              nullptr,
                              nullptr,
                              0)) {
        return QString::number(serial, 16).toUpper();
    }
#endif
    return QString();
}

QStringList PhysicalMacAddresses()
{
    QStringList addresses;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            !(iface.flags() & QNetworkInterface::IsRunning) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const QString mac = NormalizeFingerprintValue(iface.hardwareAddress());
        if (mac.isEmpty() || mac == QStringLiteral("00:00:00:00:00:00")) {
            continue;
        }
        if (!addresses.contains(mac)) {
            addresses.push_back(mac);
        }
    }
    addresses.sort();
    return addresses;
}

QJsonArray StringListToJsonArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : values) {
        array.append(value);
    }
    return array;
}

QStringList StringListFromJsonArray(const QJsonArray& array)
{
    QStringList values;
    for (const QJsonValue& value : array) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty() && !values.contains(text)) {
            values.push_back(text);
        }
    }
    values.sort();
    return values;
}

bool HashFieldMatches(const QJsonObject& licensed, const QJsonObject& current, const QString& name)
{
    const QString licensed_value = licensed.value(name).toString().trimmed();
    const QString current_value = current.value(name).toString().trimmed();
    return !licensed_value.isEmpty() && !current_value.isEmpty() && licensed_value == current_value;
}

QJsonObject FingerprintHashesToJson(const AdminHardwareFingerprint& fingerprint)
{
    QJsonObject hashes;
    hashes.insert(QStringLiteral("machine_guid"), HashFingerprintValue(QStringLiteral("machine_guid"), fingerprint.machine_guid));
    hashes.insert(QStringLiteral("bios_serial"), HashFingerprintValue(QStringLiteral("bios_serial"), fingerprint.bios_serial));
    hashes.insert(QStringLiteral("system_drive_serial"), HashFingerprintValue(QStringLiteral("system_drive_serial"), fingerprint.system_drive_serial));
    hashes.insert(QStringLiteral("qt_machine_id"), HashFingerprintValue(QStringLiteral("qt_machine_id"), fingerprint.qt_machine_id));
    QStringList mac_hashes;
    for (const QString& mac : fingerprint.mac_addresses) {
        const QString hash = HashFingerprintValue(QStringLiteral("mac"), mac);
        if (!hash.isEmpty()) {
            mac_hashes.push_back(hash);
        }
    }
    mac_hashes.sort();
    hashes.insert(QStringLiteral("mac_addresses"), StringListToJsonArray(mac_hashes));
    return hashes;
}

QJsonObject CanonicalLicensePayload(const QJsonObject& license)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("version"), license.value(QStringLiteral("version")).toString());
    payload.insert(QStringLiteral("machine_code"), license.value(QStringLiteral("machine_code")).toString());
    payload.insert(QStringLiteral("issued_at_utc"), license.value(QStringLiteral("issued_at_utc")).toString());
    payload.insert(QStringLiteral("purpose"), license.value(QStringLiteral("purpose")).toString());
    payload.insert(QStringLiteral("fingerprints"), license.value(QStringLiteral("fingerprints")).toObject());
    return payload;
}

QString LicenseSignature(const QJsonObject& license, const QString& secret)
{
    const QByteArray payload = QJsonDocument(CanonicalLicensePayload(license)).toJson(QJsonDocument::Compact);
    return Sha256Hex(payload + QByteArray("|") + secret.trimmed().toUtf8());
}

QString LicensePath()
{
    const QDir app_dir(QCoreApplication::applicationDirPath());
    const QString app_license_path = app_dir.filePath(QStringLiteral("config/admin_license.json"));
    if (QFile::exists(app_license_path)) {
        return app_license_path;
    }

    const QString source_license_path = app_dir.filePath(QStringLiteral("../../config/admin_license.json"));
    if (QFile::exists(source_license_path)) {
        return source_license_path;
    }

    return app_license_path;
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

QByteArray RandomSalt()
{
    QByteArray salt;
    salt.resize(kAdminSaltBytes);
    for (int i = 0; i < salt.size(); ++i) {
        salt[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return salt;
}

QString HashAdminPassword(const QString& password, const QByteArray& salt)
{
    QByteArray payload = salt;
    payload.append(password.toUtf8());
    return QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

QString HashRecoveryAnswer(const QString& answer, const QByteArray& salt)
{
    QByteArray payload = salt;
    payload.append(answer.trimmed().toUtf8());
    return QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

QString EncodeRememberedPassword(const QString& password)
{
    return QString::fromLatin1(password.toUtf8().toBase64());
}

QString DecodeRememberedPassword(const QString& encoded)
{
    return QString::fromUtf8(QByteArray::fromBase64(encoded.toLatin1()));
}

QSettings AdminAuthSettings()
{
    return QSettings(QString::fromLatin1(kSettingsOrganization), QString::fromLatin1(kSettingsApplication));
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
    return AdminAuthMachineCode(AdminAuthCurrentHardwareFingerprint());
}

AdminHardwareFingerprint AdminAuthCurrentHardwareFingerprint()
{
    AdminHardwareFingerprint fingerprint;
    fingerprint.machine_guid = RegistryValue(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography"),
                                             QStringLiteral("MachineGuid"));
    fingerprint.bios_serial = WmicValue({ QStringLiteral("bios"), QStringLiteral("get"), QStringLiteral("serialnumber") });
    fingerprint.system_drive_serial = SystemDriveSerial();
    fingerprint.qt_machine_id = QString::fromLatin1(QSysInfo::machineUniqueId().toHex());
    fingerprint.mac_addresses = PhysicalMacAddresses();
    return fingerprint;
}

QString AdminAuthMachineCode(const AdminHardwareFingerprint& fingerprint)
{
    QJsonObject hashes = FingerprintHashesToJson(fingerprint);
    const QByteArray seed = QJsonDocument(hashes).toJson(QJsonDocument::Compact);
    return QStringLiteral("TK-") + ToGroupedCode(QCryptographicHash::hash(seed, QCryptographicHash::Sha256), 16);
}

QString AdminAuthLicenseRequestText()
{
    const AdminHardwareFingerprint fingerprint = AdminAuthCurrentHardwareFingerprint();
    QJsonObject request;
    request.insert(QStringLiteral("version"), QString::fromLatin1(kLicenseVersion));
    request.insert(QStringLiteral("machine_code"), AdminAuthMachineCode(fingerprint));
    request.insert(QStringLiteral("machine_guid"), NormalizeFingerprintValue(fingerprint.machine_guid));
    request.insert(QStringLiteral("bios_serial"), NormalizeFingerprintValue(fingerprint.bios_serial));
    request.insert(QStringLiteral("system_drive_serial"), NormalizeFingerprintValue(fingerprint.system_drive_serial));
    request.insert(QStringLiteral("qt_machine_id"), NormalizeFingerprintValue(fingerprint.qt_machine_id));
    request.insert(QStringLiteral("mac_addresses"), StringListToJsonArray(fingerprint.mac_addresses));
    return QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact));
}

QString AdminAuthSecret(QString* error_message)
{
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
        *error_message = QStringLiteral("未找到管理员认证密钥 config/admin_auth.key，管理员授权校验不可用。");
    }
    return QString();
}

QByteArray BuildAdminLicenseJson(const QByteArray& request_json,
                                 const QString& secret,
                                 const QString& issued_at_utc,
                                 QString* error_message)
{
    QJsonParseError parse_error;
    const QJsonDocument request_document = QJsonDocument::fromJson(request_json, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !request_document.isObject()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("授权申请信息不是有效 JSON。");
        }
        return {};
    }

    const QJsonObject request = request_document.object();
    AdminHardwareFingerprint fingerprint;
    fingerprint.machine_guid = request.value(QStringLiteral("machine_guid")).toString();
    fingerprint.bios_serial = request.value(QStringLiteral("bios_serial")).toString();
    fingerprint.system_drive_serial = request.value(QStringLiteral("system_drive_serial")).toString();
    fingerprint.qt_machine_id = request.value(QStringLiteral("qt_machine_id")).toString();
    fingerprint.mac_addresses = StringListFromJsonArray(request.value(QStringLiteral("mac_addresses")).toArray());

    const QString machine_code = AdminAuthMachineCode(fingerprint);
    if (NormalizeCode(machine_code) != NormalizeCode(request.value(QStringLiteral("machine_code")).toString())) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("授权申请信息中的机器码与指纹不一致。");
        }
        return {};
    }

    QJsonObject license;
    license.insert(QStringLiteral("version"), QString::fromLatin1(kLicenseVersion));
    license.insert(QStringLiteral("machine_code"), machine_code);
    license.insert(QStringLiteral("issued_at_utc"), issued_at_utc.trimmed());
    license.insert(QStringLiteral("purpose"), QStringLiteral("ADMIN_SETTINGS"));
    license.insert(QStringLiteral("fingerprints"), FingerprintHashesToJson(fingerprint));
    license.insert(QStringLiteral("signature"), LicenseSignature(license, secret));
    return QJsonDocument(license).toJson(QJsonDocument::Indented);
}

AdminLicenseStatus VerifyAdminLicenseJson(const QByteArray& license_json,
                                          const AdminHardwareFingerprint& fingerprint,
                                          const QString& secret)
{
    AdminLicenseStatus status;
    status.machine_code = AdminAuthMachineCode(fingerprint);

    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(license_json, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        status.message = QStringLiteral("授权文件不是有效 JSON。");
        return status;
    }

    const QJsonObject license = document.object();
    if (license.value(QStringLiteral("version")).toString() != QString::fromLatin1(kLicenseVersion)) {
        status.message = QStringLiteral("授权文件版本不支持。");
        return status;
    }
    if (license.value(QStringLiteral("purpose")).toString() != QStringLiteral("ADMIN_SETTINGS")) {
        status.message = QStringLiteral("授权文件用途不匹配。");
        return status;
    }
    if (license.value(QStringLiteral("signature")).toString() != LicenseSignature(license, secret)) {
        status.message = QStringLiteral("授权文件签名无效。");
        return status;
    }

    const QJsonObject licensed = license.value(QStringLiteral("fingerprints")).toObject();
    const QJsonObject current = FingerprintHashesToJson(fingerprint);
    int matches = 0;
    matches += HashFieldMatches(licensed, current, QStringLiteral("machine_guid")) ? 1 : 0;
    matches += HashFieldMatches(licensed, current, QStringLiteral("bios_serial")) ? 1 : 0;
    matches += HashFieldMatches(licensed, current, QStringLiteral("system_drive_serial")) ? 1 : 0;
    matches += HashFieldMatches(licensed, current, QStringLiteral("qt_machine_id")) ? 1 : 0;

    const QStringList licensed_macs = StringListFromJsonArray(licensed.value(QStringLiteral("mac_addresses")).toArray());
    const QStringList current_macs = StringListFromJsonArray(current.value(QStringLiteral("mac_addresses")).toArray());
    bool mac_matches = !licensed_macs.isEmpty() && licensed_macs.size() == current_macs.size();
    for (int i = 0; mac_matches && i < licensed_macs.size(); ++i) {
        mac_matches = licensed_macs.at(i) == current_macs.at(i);
    }
    matches += mac_matches ? 1 : 0;

    status.matched_categories = matches;
    status.valid = matches >= kRequiredLicenseMatches;
    status.message = status.valid
        ? QStringLiteral("已授权")
        : QStringLiteral("授权文件不符");
    return status;
}

AdminLicenseStatus AdminAuthLicenseStatus()
{
    const AdminHardwareFingerprint fingerprint = AdminAuthCurrentHardwareFingerprint();
    AdminLicenseStatus status;
    status.machine_code = AdminAuthMachineCode(fingerprint);

    QFile file(LicensePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        status.message = QStringLiteral("无授权文件");
        return status;
    }

    QString warning;
    const QString secret = AdminAuthSecret(&warning);
    if (!warning.isEmpty()) {
        status.message = warning;
        return status;
    }
    return VerifyAdminLicenseJson(file.readAll(), fingerprint, secret);
}

bool AdminAuthHasAccount()
{
    QSettings settings = AdminAuthSettings();
    settings.beginGroup(QString::fromLatin1(kAdminAuthGroup));
    const bool result = !settings.value(QStringLiteral("username")).toString().trimmed().isEmpty() &&
                        !settings.value(QStringLiteral("password_hash")).toString().trimmed().isEmpty() &&
                        !settings.value(QStringLiteral("salt")).toString().trimmed().isEmpty();
    settings.endGroup();
    return result;
}

QString AdminAuthUsername()
{
    QSettings settings = AdminAuthSettings();
    settings.beginGroup(QString::fromLatin1(kAdminAuthGroup));
    const QString username = settings.value(QStringLiteral("username")).toString().trimmed();
    settings.endGroup();
    return username;
}

bool AdminAuthHasRecoveryChallenge()
{
    QSettings settings = AdminAuthSettings();
    settings.beginGroup(QString::fromLatin1(kAdminAuthGroup));
    const bool result = !settings.value(QStringLiteral("recovery_question")).toString().trimmed().isEmpty() &&
                        !settings.value(QStringLiteral("recovery_answer_hash")).toString().trimmed().isEmpty() &&
                        !settings.value(QStringLiteral("recovery_salt")).toString().trimmed().isEmpty();
    settings.endGroup();
    return result;
}

QString AdminAuthRecoveryQuestion()
{
    QSettings settings = AdminAuthSettings();
    settings.beginGroup(QString::fromLatin1(kAdminAuthGroup));
    const QString question = settings.value(QStringLiteral("recovery_question")).toString().trimmed();
    settings.endGroup();
    return question;
}

bool AdminAuthValidateRecoveryAnswer(const QString& answer)
{
    QSettings settings = AdminAuthSettings();
    settings.beginGroup(QString::fromLatin1(kAdminAuthGroup));
    const QByteArray salt = QByteArray::fromBase64(settings.value(QStringLiteral("recovery_salt")).toString().toLatin1());
    const QString saved_hash = settings.value(QStringLiteral("recovery_answer_hash")).toString();
    settings.endGroup();
    if (saved_hash.isEmpty() || salt.isEmpty()) {
        return false;
    }
    return HashRecoveryAnswer(answer, salt) == saved_hash;
}

bool AdminAuthSetCredentials(const QString& username,
                             const QString& password,
                             const QString& recovery_question,
                             const QString& recovery_answer,
                             QString* error_message)
{
    const QString trimmed_username = username.trimmed();
    const QString trimmed_question = recovery_question.trimmed();
    const QString trimmed_answer = recovery_answer.trimmed();
    if (trimmed_username.isEmpty()) {
        if (error_message) {
            *error_message = QStringLiteral("管理员账号不能为空。");
        }
        return false;
    }
    if (password.size() < 4) {
        if (error_message) {
            *error_message = QStringLiteral("管理员密码至少需要 4 位。");
        }
        return false;
    }
    if (trimmed_question.isEmpty()) {
        if (error_message) {
            *error_message = QStringLiteral("恢复问题不能为空。");
        }
        return false;
    }
    if (trimmed_answer.isEmpty()) {
        if (error_message) {
            *error_message = QStringLiteral("恢复答案不能为空。");
        }
        return false;
    }

    const QByteArray salt = RandomSalt();
    const QByteArray recovery_salt = RandomSalt();
    QSettings settings = AdminAuthSettings();
    settings.beginGroup(QString::fromLatin1(kAdminAuthGroup));
    settings.setValue(QStringLiteral("username"), trimmed_username);
    settings.setValue(QStringLiteral("salt"), QString::fromLatin1(salt.toBase64()));
    settings.setValue(QStringLiteral("password_hash"), HashAdminPassword(password, salt));
    settings.setValue(QStringLiteral("recovery_question"), trimmed_question);
    settings.setValue(QStringLiteral("recovery_salt"), QString::fromLatin1(recovery_salt.toBase64()));
    settings.setValue(QStringLiteral("recovery_answer_hash"), HashRecoveryAnswer(trimmed_answer, recovery_salt));
    settings.endGroup();
    settings.sync();
    return true;
}

bool AdminAuthResetCredentials(const QString& username,
                               const QString& password,
                               const QString& recovery_question,
                               const QString& recovery_answer,
                               QString* error_message)
{
    const QString trimmed_username = username.trimmed();
    const QString trimmed_question = recovery_question.trimmed();
    const QString trimmed_answer = recovery_answer.trimmed();
    if (trimmed_username.isEmpty()) {
        if (error_message) {
            *error_message = QStringLiteral("管理员账号不能为空。");
        }
        return false;
    }
    if (password.size() < 4) {
        if (error_message) {
            *error_message = QStringLiteral("管理员密码至少需要 4 位。");
        }
        return false;
    }
    if (trimmed_question.isEmpty() != trimmed_answer.isEmpty()) {
        if (error_message) {
            *error_message = QStringLiteral("新恢复问题和新恢复答案必须同时填写。");
        }
        return false;
    }

    const QByteArray salt = RandomSalt();
    QSettings settings = AdminAuthSettings();
    settings.beginGroup(QString::fromLatin1(kAdminAuthGroup));
    settings.setValue(QStringLiteral("username"), trimmed_username);
    settings.setValue(QStringLiteral("salt"), QString::fromLatin1(salt.toBase64()));
    settings.setValue(QStringLiteral("password_hash"), HashAdminPassword(password, salt));
    if (!trimmed_question.isEmpty()) {
        const QByteArray recovery_salt = RandomSalt();
        settings.setValue(QStringLiteral("recovery_question"), trimmed_question);
        settings.setValue(QStringLiteral("recovery_salt"), QString::fromLatin1(recovery_salt.toBase64()));
        settings.setValue(QStringLiteral("recovery_answer_hash"), HashRecoveryAnswer(trimmed_answer, recovery_salt));
    }
    settings.endGroup();
    settings.sync();
    return true;
}

bool AdminAuthChangeCredentials(const QString& current_password,
                                const QString& username,
                                const QString& new_password,
                                QString* error_message)
{
    if (AdminAuthHasAccount() && !AdminAuthValidateCredentials(AdminAuthUsername(), current_password)) {
        if (error_message) {
            *error_message = QStringLiteral("当前密码不正确。");
        }
        return false;
    }

    const QString trimmed_username = username.trimmed();
    if (trimmed_username.isEmpty()) {
        if (error_message) {
            *error_message = QStringLiteral("管理员账号不能为空。");
        }
        return false;
    }
    if (new_password.size() < 4) {
        if (error_message) {
            *error_message = QStringLiteral("管理员密码至少需要 4 位。");
        }
        return false;
    }

    const QByteArray salt = RandomSalt();
    QSettings settings = AdminAuthSettings();
    settings.beginGroup(QString::fromLatin1(kAdminAuthGroup));
    settings.setValue(QStringLiteral("username"), trimmed_username);
    settings.setValue(QStringLiteral("salt"), QString::fromLatin1(salt.toBase64()));
    settings.setValue(QStringLiteral("password_hash"), HashAdminPassword(new_password, salt));
    settings.endGroup();
    settings.sync();
    return true;
}

bool AdminAuthValidateCredentials(const QString& username, const QString& password)
{
    QSettings settings = AdminAuthSettings();
    settings.beginGroup(QString::fromLatin1(kAdminAuthGroup));
    const QString saved_username = settings.value(QStringLiteral("username")).toString().trimmed();
    const QByteArray salt = QByteArray::fromBase64(settings.value(QStringLiteral("salt")).toString().toLatin1());
    const QString saved_hash = settings.value(QStringLiteral("password_hash")).toString();
    settings.endGroup();
    if (saved_username.isEmpty() || saved_hash.isEmpty() || salt.isEmpty()) {
        return false;
    }
    return username.trimmed() == saved_username && HashAdminPassword(password, salt) == saved_hash;
}

QString AdminAuthRememberedPassword()
{
    QSettings settings = AdminAuthSettings();
    settings.beginGroup(QString::fromLatin1(kAdminAuthGroup));
    const bool remember = settings.value(QStringLiteral("remember_password"), false).toBool();
    const QString encoded = settings.value(QStringLiteral("remembered_password")).toString();
    settings.endGroup();
    return remember ? DecodeRememberedPassword(encoded) : QString();
}

void AdminAuthSaveRememberedPassword(bool remember, const QString& password)
{
    QSettings settings = AdminAuthSettings();
    settings.beginGroup(QString::fromLatin1(kAdminAuthGroup));
    settings.setValue(QStringLiteral("remember_password"), remember);
    if (remember) {
        settings.setValue(QStringLiteral("remembered_password"), EncodeRememberedPassword(password));
    } else {
        settings.remove(QStringLiteral("remembered_password"));
    }
    settings.endGroup();
    settings.sync();
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
    button->setFocusPolicy(Qt::NoFocus);
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
