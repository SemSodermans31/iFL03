/*
 Helper for global units selection
 - Uses General.units from config if set: "imperial" or "metric"
 - Falls back to iRacing's ir_DisplayUnits (0=english/imperial, 1=metric)
*/

#pragma once

#include "Config.h"
#include "iracing.h"

inline bool isImperialUnits()
{
    const std::string u = g_cfg.getString("General", "units", "");
    if (u == "imperial") return true;
    if (u == "metric") return false;
    // Fallback to iRacing default UI units
    return ir_DisplayUnits.getInt() == 0;
}


