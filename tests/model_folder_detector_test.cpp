#include "engineering_settings_dialog_helpers.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cassert>
#include <iostream>

namespace {

void WriteModelFile(const QString& directory, const QString& file_name)
{
    QFile file(QDir(directory).filePath(file_name));
    assert(file.open(QIODevice::WriteOnly));
    file.write("model");
}

void WriteCompleteModelFolder(const QString& directory)
{
    for (const QString& file_name : {
             QStringLiteral("best_obb.xml"),
             QStringLiteral("best_obb.bin"),
             QStringLiteral("best_seg.xml"),
             QStringLiteral("best_seg.bin"),
         }) {
        WriteModelFile(directory, file_name);
    }
}

} // namespace

int main()
{
    QTemporaryDir temp;
    assert(temp.isValid());

    const QString complete_dir = QDir(temp.path()).filePath(QStringLiteral("complete"));
    assert(QDir().mkpath(complete_dir));
    WriteCompleteModelFolder(complete_dir);

    const StandardModelFolderSelection complete = DetectStandardModelFolder(complete_dir);
    assert(complete.complete());
    assert(complete.obb_xml_path ==
           QFileInfo(QDir(complete_dir).filePath(QStringLiteral("best_obb.xml"))).absoluteFilePath());
    assert(complete.seg_xml_path ==
           QFileInfo(QDir(complete_dir).filePath(QStringLiteral("best_seg.xml"))).absoluteFilePath());

    QFile::remove(QDir(complete_dir).filePath(QStringLiteral("best_seg.bin")));
    const StandardModelFolderSelection missing_bin = DetectStandardModelFolder(complete_dir);
    assert(!missing_bin.complete());
    assert(missing_bin.missing_files == QStringList{ QStringLiteral("best_seg.bin") });
    assert(missing_bin.obb_xml_path.isEmpty());
    assert(missing_bin.seg_xml_path.isEmpty());

    const QString nonstandard_dir = QDir(temp.path()).filePath(QStringLiteral("nonstandard"));
    assert(QDir().mkpath(nonstandard_dir));
    WriteModelFile(nonstandard_dir, QStringLiteral("obb.xml"));
    WriteModelFile(nonstandard_dir, QStringLiteral("obb.bin"));
    WriteModelFile(nonstandard_dir, QStringLiteral("seg.xml"));
    WriteModelFile(nonstandard_dir, QStringLiteral("seg.bin"));
    const StandardModelFolderSelection nonstandard = DetectStandardModelFolder(nonstandard_dir);
    assert(!nonstandard.complete());
    assert(nonstandard.missing_files.size() == 4);

    const StandardModelFolderSelection missing_directory =
        DetectStandardModelFolder(QDir(temp.path()).filePath(QStringLiteral("missing")));
    assert(!missing_directory.complete());
    assert(missing_directory.missing_files.size() == 4);

    std::cout << "model_folder_detector_test passed" << std::endl;
    return 0;
}
