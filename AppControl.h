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

// Toggle preview mode for overlays
void app_set_preview_mode(bool on);

// Set or toggle an overlay by its config section key, e.g. "OverlayStandings"
void app_set_overlay(const char* sectionKey, bool on);
void app_toggle_overlay(const char* sectionKey);

// Set configuration values
void app_set_config_string(const char* component, const char* key, const char* value);
void app_set_config_int(const char* component, const char* key, int value);
void app_set_config_bool(const char* component, const char* key, bool value);
void app_set_config_float(const char* component, const char* key, float value);

// Build current state JSON for the GUI
std::string app_get_state_json();

// Called by the bridge after config changes so the main app re-applies enables
void app_handleConfigChange_external(); 