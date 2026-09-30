#include "stdafx.h"

#include "SplitsProfile.h"

SplitsProfile MakeManualProfile()
{
    SplitsProfile p;
    p.name                   = "Manual";
    return p;
}

SplitsProfile MakeRunningProfile()
{
    SplitsProfile p;
    p.name                   = "Running";
    p.sequential_route       = true;
    return p;
}

SplitsProfile MakeSCProfile()
{
    SplitsProfile p;
    p.name                   = "SC";
    p.dynamic_by_default     = true;
    p.auto_reset_on_complete = true;
    return p;
}
