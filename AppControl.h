#pragma once

#include <string>
#include <vector>
#include "iracing.h"
#include "Overlay.h"

using HandleConfigChangeFn = void (*)(std::vector<Overlay*>, ConnectionStatus);

// Register pointers to the app internals so the bridge can act
void app_register_bridge(std::vector<Overlay*>* overlaysPtr,
                         bool* uiEditPtr,
                         ConnectionStatus* statusPtr,
                         HandleConfigChangeFn onConfigChangeFn);

// Toggle UI edit mode for all overlays
void app_set_ui_edit(bool on);

// Set or toggle an overlay by its config section key, e.g. "OverlayStandings"
void app_set_overlay(const char* sectionKey, bool on);
void app_toggle_overlay(const char* sectionKey);

// Build current state JSON for the GUI
std::string app_get_state_json();

// Called by the bridge after config changes so the main app re-applies enables
void app_handleConfigChange_external(); 