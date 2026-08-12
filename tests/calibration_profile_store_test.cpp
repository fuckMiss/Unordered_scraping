#include "calibration_profile_store.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <cassert>
#include <iostream>

namespace {

QString WriteFile(QTemporaryDir& dir, const QString& name, const QByteArray& data)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    assert(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    assert(file.write(data) == data.size());
    return path;
}

QByteArray ValidJsonProfile()
{
    return R"({
  "version": 1,
  "name": "json_profile",
  "enabled": true,
  "points": [
    { "image_x": 0, "image_y": 0, "machine_x": 100, "machine_y": 200 },
    { "image_x": 10, "image_y": 0, "machine_x": 110, "machine_y": 200 },
    { "image_x": 20, "image_y": 0, "machine_x": 120, "machine_y": 200 },
    { "image_x": 0, "image_y": 10, "machine_x": 100, "machine_y": 210 },
    { "image_x": 10, "image_y": 10, "machine_x": 110, "machine_y": 210 },
    { "image_x": 20, "image_y": 10, "machine_x": 120, "machine_y": 210 },
    { "image_x": 0, "image_y": 20, "machine_x": 100, "machine_y": 220 },
    { "image_x": 10, "image_y": 20, "machine_x": 110, "machine_y": 220 },
    { "image_x": 20, "image_y": 20, "machine_x": 120, "machine_y": 220 }
  ]
})";
}

QByteArray HeaderTxtProfile()
{
    QByteArray text("image_x image_y machine_x machine_y\n");
    for (int i = 0; i < 9; ++i) {
        text += QByteArray::number(i * 10);
        text += " ";
        text += QByteArray::number(i * 10 + 1);
        text += " ";
        text += QByteArray::number(i * 10 + 100);
        text += " ";
        text += QByteArray::number(i * 10 + 200);
        text += "\n";
    }
    return text;
}

QByteArray VisionMasterFiveColumnProfile()
{
    return QByteArray(
        "916.866    366.526    916.866    366.526    0.000\n"
        "1324.134   361.221    1324.134   361.221    0.000\n"
        "1663.025   359.835    1663.025   359.835    0.000\n"
        "1673.823   735.471    1673.823   735.471    0.000\n"
        "1320.797   733.178    1320.797   733.178    0.000\n"
        "943.378    735.315    943.378    735.315    0.000\n"
        "951.980    1121.911   951.980    1121.911   0.000\n"
        "1333.030   1118.676   1333.030   1118.676   0.000\n"
        "1667.278   1120.060   1667.278   1120.060   0.000\n"
        "0.000      0.000      0.000      0.000      0.000\n"
        "0.000      0.000      0.000      0.000      0.000\n"
        "0.000      0.000      0.000      0.000      0.000\n");
}

void JsonImportRequiresCompleteNinePoints(QTemporaryDir& dir)
{
    CoordinateTransformConfig config;
    QString error;
    assert(LoadCalibrationProfileFromFile(WriteFile(dir, QStringLiteral("valid.json"), ValidJsonProfile()), &config, &error));
    assert(config.enabled);
    assert(config.profile_name == QStringLiteral("json_profile"));
    assert(config.points.size() == 9);
    assert(config.points[8].machine_y == 220.0);

    const QByteArray missing_field = R"({
  "name": "bad",
  "enabled": true,
  "points": [
    { "image_x": 0, "image_y": 0, "machine_x": 100, "machine_y": 200 },
    { "image_x": 1, "image_y": 1, "machine_x": 101 },
    { "image_x": 2, "image_y": 2, "machine_x": 102, "machine_y": 202 },
    { "image_x": 3, "image_y": 3, "machine_x": 103, "machine_y": 203 },
    { "image_x": 4, "image_y": 4, "machine_x": 104, "machine_y": 204 },
    { "image_x": 5, "image_y": 5, "machine_x": 105, "machine_y": 205 },
    { "image_x": 6, "image_y": 6, "machine_x": 106, "machine_y": 206 },
    { "image_x": 7, "image_y": 7, "machine_x": 107, "machine_y": 207 },
    { "image_x": 8, "image_y": 8, "machine_x": 108, "machine_y": 208 }
  ]
})";
    assert(!LoadCalibrationProfileFromFile(WriteFile(dir, QStringLiteral("missing.json"), missing_field), &config, &error));

    const QByteArray wrong_count = R"({
  "name": "bad_count",
  "points": [
    { "image_x": 0, "image_y": 0, "machine_x": 100, "machine_y": 200 }
  ]
})";
    assert(!LoadCalibrationProfileFromFile(WriteFile(dir, QStringLiteral("wrong_count.json"), wrong_count), &config, &error));
}

void TxtImportRequiresExplicitColumns(QTemporaryDir& dir)
{
    CoordinateTransformConfig config;
    QString error;
    assert(LoadCalibrationProfileFromFile(WriteFile(dir, QStringLiteral("visionmaster.txt"), HeaderTxtProfile()), &config, &error));
    assert(config.enabled);
    assert(config.points.size() == 9);
    assert(config.points[3].image_y == 31.0);
    assert(config.points[3].machine_y == 230.0);

    const QByteArray ambiguous_numbers =
        "0 0 100 200\n"
        "1 1 101 201\n"
        "2 2 102 202\n"
        "3 3 103 203\n"
        "4 4 104 204\n"
        "5 5 105 205\n"
        "6 6 106 206\n"
        "7 7 107 207\n"
        "8 8 108 208\n";
    assert(!LoadCalibrationProfileFromFile(WriteFile(dir, QStringLiteral("ambiguous.txt"), ambiguous_numbers), &config, &error));

    assert(LoadCalibrationProfileFromFile(WriteFile(dir, QStringLiteral("vm_five_column.txt"), VisionMasterFiveColumnProfile()), &config, &error));
    assert(config.enabled);
    assert(config.points.size() == 9);
    assert(config.points[0].image_x == 916.866);
    assert(config.points[8].machine_y == 1120.060);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    assert(dir.isValid());

    JsonImportRequiresCompleteNinePoints(dir);
    TxtImportRequiresExplicitColumns(dir);

    std::cout << "calibration_profile_store_test passed" << std::endl;
    return 0;
}
