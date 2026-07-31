#include "admin_auth_helpers.h"

#include <QCoreApplication>
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
    out << "Usage: tankeye-admin-auth-code -MachineCode <code> [-Purpose INIT|RESET]\n";
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QString machine_code = ValueAfter(args, QStringLiteral("-MachineCode"));
    QString purpose = ValueAfter(args, QStringLiteral("-Purpose")).toUpper();
    if (purpose.isEmpty()) {
        purpose = QStringLiteral("INIT");
    }

    if (machine_code.isEmpty() || (purpose != QStringLiteral("INIT") && purpose != QStringLiteral("RESET"))) {
        PrintUsage(err);
        return 2;
    }

    QString warning;
    const QString secret = AdminAuthSecret(&warning);
    if (!warning.isEmpty()) {
        err << warning << "\n";
    }

    out << "Purpose: " << purpose << "\n";
    out << "Machine: " << machine_code << "\n";
    out << "Code: " << BuildAdminAuthCode(machine_code, secret, purpose) << "\n";
    return 0;
}

