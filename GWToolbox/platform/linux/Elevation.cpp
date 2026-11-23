#include <string>

#include "Elevation.h"

bool CanRestartLauncherAsAdmin()
{
    return false;
}

bool RestartLauncherAsAdmin(std::string& error)
{
    error = "GWToolbox can't restart itself with elevated rights on Linux.";
    return false;
}
