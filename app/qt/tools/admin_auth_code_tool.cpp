#include "admin_auth_helpers.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTextStream>

namespace {

QString ValueAfter(const QStringList& args, const QString& key)
{
    const int index = args.indexOf(key);
    if (index >= 0 && index + 1 < args.size()) {
        return args.at(index + 1).trimmed();
    }
    return QString();
}

void PrintUsage(QTextStream& out)
{
    out << "Usage: tankeye-admin-auth-code -RequestFile <request.json> -Output <admin_license.json>\n";
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QString request_file = ValueAfter(args, QStringLiteral("-RequestFile"));
    const QString output_file = ValueAfter(args, QStringLiteral("-Output"));

    if (request_file.isEmpty() || output_file.isEmpty()) {
        PrintUsage(err);
        return 2;
    }

    QString warning;
    const QString secret = AdminAuthSecret(&warning);
    if (!warning.isEmpty()) {
        err << warning << "\n";
        return 3;
    }

    QFile input(request_file);
    if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err << "Failed to read request file: " << request_file << "\n";
        return 4;
    }

    QString license_error;
    const QByteArray license = BuildAdminLicenseJson(input.readAll(),
                                                     secret,
                                                     QDateTime::currentDateTimeUtc().toString(Qt::ISODate),
                                                     &license_error);
    if (license.isEmpty()) {
        err << license_error << "\n";
        return 5;
    }

    QFile output(output_file);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        err << "Failed to write license file: " << output_file << "\n";
        return 6;
    }

    output.write(license);
    out << "License: " << output_file << "\n";
    return 0;
}
