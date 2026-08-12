#include "project_profile_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

ProjectProfileSettings MakeProfile(const QString& name, const QString& obb, const QString& seg)
{
    ProjectProfileSettings profile;
    profile.name = name;
    profile.obb_model_path = obb;
    profile.seg_model_path = seg;
    profile.camera_exposure_us = 321.0;
    profile.model_thresholds.obb_conf_threshold = 0.61;
    profile.grab_limits.x.lower = 1.0;
    profile.coordinate_transform.profile_name = name + QStringLiteral("_calibration");
    profile.coordinate_transform.points.resize(9);
    return profile;
}

void WriteFile(const QString& path, const QByteArray& data)
{
    QFile file(path);
    assert(file.open(QIODevice::WriteOnly));
    assert(file.write(data) == data.size());
}

void ProfilesRoundTrip()
{
    QTemporaryDir temp;
    assert(temp.isValid());
    qputenv("TANKEYE_SETTINGS_INI_PATH", (temp.path() + QStringLiteral("/engineering.ini")).toLocal8Bit());

    const QString model_dir = QDir(temp.path()).filePath(QStringLiteral("models"));
    assert(QDir().mkpath(model_dir));
    const QString obb = QDir(model_dir).filePath(QStringLiteral("obb.xml"));
    const QString seg = QDir(model_dir).filePath(QStringLiteral("seg.xml"));
    const QString obb_bin = QDir(model_dir).filePath(QStringLiteral("best_obb.bin"));
    const QString seg_bin = QDir(model_dir).filePath(QStringLiteral("best_seg.bin"));
    WriteFile(obb, QByteArray("obb"));
    WriteFile(seg, QByteArray("seg"));
    WriteFile(obb_bin, QByteArray("obb-bin"));
    WriteFile(seg_bin, QByteArray("seg-bin"));

    ProjectProfileSettings dg8 = MakeProfile(QStringLiteral("DG_8"),
                                              QDir(model_dir).filePath(QStringLiteral("best_obb.xml")),
                                              QDir(model_dir).filePath(QStringLiteral("best_seg.xml")));
    dg8.angle_calibration.offset_deg = -292.45;
    dg8.obb_postprocess.show_head_type_adjusted_geometry = false;
    WriteFile(dg8.obb_model_path, QByteArray("obb"));
    WriteFile(dg8.seg_model_path, QByteArray("seg"));
    QString saved_name;
    QString error;
    assert(SaveProjectProfile(dg8, &saved_name, &error));
    assert(saved_name == QStringLiteral("DG_8"));
    SaveActiveProjectProfileName(saved_name);

    ProjectProfileSettings dg10 = MakeProfile(QStringLiteral("DG_10"), seg, obb);
    assert(SaveProjectProfile(dg10, &saved_name, &error));
    assert(ListProjectProfileNames().contains(QStringLiteral("DG_8")));
    assert(ListProjectProfileNames().contains(QStringLiteral("DG_10")));

    ProjectProfileSettings loaded;
    assert(LoadProjectProfile(QStringLiteral("DG_8"), &loaded, &error));
    assert(loaded.name == QStringLiteral("DG_8"));
    assert(loaded.obb_model_path == QFileInfo(obb).absoluteFilePath());
    assert(loaded.camera_exposure_us == 321.0);
    assert(loaded.model_thresholds.obb_conf_threshold == 0.61);
    assert(std::fabs(loaded.angle_calibration.offset_deg - (-292.45)) < 0.001);
    assert(!loaded.obb_postprocess.show_head_type_adjusted_geometry);
    assert(LoadActiveProjectProfileName() == QStringLiteral("DG_8"));

    const QString json_path = QDir(temp.path()).filePath(QStringLiteral("DG_10.json"));
    assert(ExportProjectProfile(QStringLiteral("DG_10"), json_path, false, &error));
    bool replaced = false;
    QString imported_name;
    assert(ImportProjectProfile(json_path, &replaced, &imported_name, &error));
    assert(imported_name == QStringLiteral("DG_10"));
    assert(replaced);

    const QString zip_path = QDir(temp.path()).filePath(QStringLiteral("DG_8.zip"));
    assert(ExportProjectProfile(QStringLiteral("DG_8"), zip_path, true, &error));
    assert(ImportProjectProfile(zip_path, &replaced, &imported_name, &error));
    assert(imported_name == QStringLiteral("DG_8"));
    ProjectProfileSettings imported;
    assert(LoadProjectProfile(QStringLiteral("DG_8"), &imported, &error));
    assert(QFileInfo::exists(imported.obb_model_path));
    assert(QFileInfo::exists(imported.seg_model_path));
    assert(QFileInfo::exists(QDir(QFileInfo(imported.obb_model_path).absolutePath()).filePath(QStringLiteral("best_obb.bin"))));
    assert(QFileInfo::exists(QDir(QFileInfo(imported.seg_model_path).absolutePath()).filePath(QStringLiteral("best_seg.bin"))));
}

void BlankProfileDefaults()
{
    const ProjectProfileSettings blank = CreateBlankProjectProfile(QStringLiteral("DG_10"));
    assert(blank.name == QStringLiteral("DG_10"));
    assert(blank.obb_model_path.isEmpty());
    assert(blank.seg_model_path.isEmpty());
    assert(blank.camera_exposure_us == 0.0);
    assert(!blank.coordinate_transform.enabled);
    assert(blank.coordinate_transform.points.size() == 9);
    assert(!blank.grab_limits.enabled);
    assert(blank.obb_postprocess.show_head_type_adjusted_geometry);
    assert(blank.ui_overlay.mechanical_gripper_length == 0.0);
    assert(blank.ui_overlay.mechanical_gripper_width == 0.0);
    assert(blank.angle_calibration.offset_deg == 0.0);
    for (int code = 1; code < static_cast<int>(blank.head_type_compensation.types.size()); ++code) {
        assert(blank.head_type_compensation.types[code].angle_offset_deg == 0.0F);
        assert(blank.head_type_compensation.types[code].ac_ray_offset_mm == 0.0F);
    }
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ProfilesRoundTrip();
    BlankProfileDefaults();
    std::cout << "project_profile_store_test passed" << std::endl;
    return 0;
}
