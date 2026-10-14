#include "engineering_settings_dialog_helpers.h"

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QSpinBox>
#include <QWheelEvent>

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

void SendWheel(QWidget* widget)
{
    QWheelEvent event(QPointF(5.0, 5.0),
                      QPointF(5.0, 5.0),
                      QPoint(0, 0),
                      QPoint(0, 120),
                      Qt::NoButton,
                      Qt::NoModifier,
                      Qt::NoScrollPhase,
                      false);
    QApplication::sendEvent(widget, &event);
}

void WheelDoesNotChangeDoubleSpinBoxWithoutFocus()
{
    QDoubleSpinBox* spin_box = CreateLimitSpinBox(nullptr, 10.0);
    spin_box->clearFocus();

    SendWheel(spin_box);

    assert(std::abs(spin_box->value() - 10.0) < 0.0001);
    delete spin_box;
}

void WheelChangesDoubleSpinBoxWithFocus()
{
    QDoubleSpinBox* spin_box = CreateLimitSpinBox(nullptr, 10.0);
    spin_box->setFocus();

    SendWheel(spin_box);

    assert(std::abs(spin_box->value() - 10.0) > 0.0001);
    delete spin_box;
}

void WheelDoesNotChangeSpinBoxWithoutFocus()
{
    QSpinBox* spin_box = CreateStartupDelaySpinBox(nullptr, 10);
    spin_box->clearFocus();

    SendWheel(spin_box);

    assert(spin_box->value() == 10);
    delete spin_box;
}

void WheelChangesSpinBoxWithFocus()
{
    QSpinBox* spin_box = CreateStartupDelaySpinBox(nullptr, 10);
    spin_box->setFocus();

    SendWheel(spin_box);

    assert(spin_box->value() != 10);
    delete spin_box;
}

void WheelDoesNotChangeComboBoxWithoutFocus()
{
    QComboBox* combo_box = CreateClickFocusedComboBox(nullptr);
    combo_box->addItem(QStringLiteral("正向"));
    combo_box->addItem(QStringLiteral("反向"));
    combo_box->setCurrentIndex(0);
    combo_box->clearFocus();

    SendWheel(combo_box);

    assert(combo_box->currentIndex() == 0);
    delete combo_box;
}

void WheelChangesComboBoxWithFocus()
{
    QComboBox* combo_box = CreateClickFocusedComboBox(nullptr);
    combo_box->addItem(QStringLiteral("正向"));
    combo_box->addItem(QStringLiteral("反向"));
    combo_box->setCurrentIndex(0);
    combo_box->setFocus();

    SendWheel(combo_box);

    assert(combo_box->currentIndex() != 0);
    delete combo_box;
}

void ReturnDoesNotChangeComboBoxSelection()
{
    QComboBox* combo_box = CreateClickFocusedComboBox(nullptr);
    combo_box->addItem(QStringLiteral("正向"));
    combo_box->addItem(QStringLiteral("反向"));
    combo_box->setCurrentIndex(0);
    combo_box->setFocus();

    QKeyEvent return_event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(combo_box, &return_event);
    assert(combo_box->currentIndex() == 0);
    assert(return_event.isAccepted());

    QKeyEvent enter_event(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
    QApplication::sendEvent(combo_box, &enter_event);
    assert(combo_box->currentIndex() == 0);
    assert(enter_event.isAccepted());
    delete combo_box;
}

} // namespace

int main(int argc, char** argv)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QApplication app(argc, argv);

    WheelDoesNotChangeDoubleSpinBoxWithoutFocus();
    WheelChangesDoubleSpinBoxWithFocus();
    WheelDoesNotChangeSpinBoxWithoutFocus();
    WheelChangesSpinBoxWithFocus();
    WheelDoesNotChangeComboBoxWithoutFocus();
    WheelChangesComboBoxWithFocus();
    ReturnDoesNotChangeComboBoxSelection();

    std::cout << "engineering_settings_spinbox_wheel_test passed" << std::endl;
    return 0;
}
