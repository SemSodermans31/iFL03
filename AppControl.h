/*
MIT License

Copyright (c) 2021-2025 L. E. Spalt & Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

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

void app_set_ui_edit(bool on);

// Toggle preview mode for overlays
void app_set_preview_mode(bool on);

void app_set_overlay(const char* sectionKey, bool on);
void app_toggle_overlay(const char* sectionKey);

// Set configuration values
void app_set_config_string(const char* component, const char* key, const char* value);
void app_set_config_int(const char* component, const char* key, int value);
void app_set_config_bool(const char* component, const char* key, bool value);
void app_set_config_float(const char* component, const char* key, float value);
void app_set_config_string_vec(const char* component, const char* key, const std::vector<std::string>& values);

// Build current state JSON for the GUI
std::string app_get_state_json();

// Called by the bridge after config changes so the main app re-applies enables
void app_handleConfigChange_external(); 