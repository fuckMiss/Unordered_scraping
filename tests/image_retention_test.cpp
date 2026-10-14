#include "image_retention.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cassert>
#include <iostream>

namespace {

void CreateFile(const QString& path)
{
    QFile file(path);
    assert(file.open(QIODevice::WriteOnly));
    assert(file.write("test") == 4);
    file.close();
}

void RetainsNewestManagedImagesOnly()
{
    QTemporaryDir temporary_dir;
    assert(temporary_dir.isValid());
    const QDir directory(temporary_dir.path());
    CreateFile(directory.filePath(QStringLiteral("tankeye_plc_20260923_120000_000_none.png")));
    CreateFile(directory.filePath(QStringLiteral("tankeye_plc_20260923_120001_000_hidden.png")));
    CreateFile(directory.filePath(QStringLiteral("tankeye_plc_20260923_120002_000_all.png")));
    CreateFile(directory.filePath(QStringLiteral("unrelated.png")));

    QString warning;
    assert(EnforceImageRetention(temporary_dir.path(), 2, &warning));
    assert(warning.isEmpty());
    assert(!QFile::exists(directory.filePath(QStringLiteral("tankeye_plc_20260923_120000_000_none.png"))));
    assert(QFile::exists(directory.filePath(QStringLiteral("tankeye_plc_20260923_120001_000_hidden.png"))));
    assert(QFile::exists(directory.filePath(QStringLiteral("tankeye_plc_20260923_120002_000_all.png"))));
    assert(QFile::exists(directory.filePath(QStringLiteral("unrelated.png"))));
}

void ExistingOverflowIsCleanedOnNextSuccessfulSave()
{
    QTemporaryDir temporary_dir;
    assert(temporary_dir.isValid());
    const QDir directory(temporary_dir.path());
    CreateFile(directory.filePath(QStringLiteral("tankeye_plc_20260923_120000_000_none.png")));
    CreateFile(directory.filePath(QStringLiteral("tankeye_plc_20260923_120001_000_none.png")));
    CreateFile(directory.filePath(QStringLiteral("tankeye_plc_20260923_120002_000_none.png")));

    QString warning;
    assert(EnforceImageRetention(temporary_dir.path(), 1, &warning));
    assert(warning.isEmpty());
    assert(QDir(temporary_dir.path()).entryList(QStringList() << QStringLiteral("tankeye_plc_*.png"),
                                                QDir::Files).size() == 1);
    assert(ClampMaxSavedImages(0) == 1000);
    assert(ClampMaxSavedImages(-1) == 1000);
    assert(ClampMaxSavedImages(100001) == 100000);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    RetainsNewestManagedImagesOnly();
    ExistingOverflowIsCleanedOnNextSuccessfulSave();
    std::cout << "image_retention_test passed" << std::endl;
    return 0;
}
