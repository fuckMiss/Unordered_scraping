#pragma once

class GraspMainWindow;

class EngineeringSettingsDialogController
{
public:
    explicit EngineeringSettingsDialogController(GraspMainWindow& window);

    void show();

private:
    GraspMainWindow& window_;
};
