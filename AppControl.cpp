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

#include "AppControl.h"
#include "Config.h"
#include "preview_mode.h"
#include "iracing.h"
#include "stub_data.h"
#include <cassert>
#include <cctype>

extern Config g_cfg;

static std::vector<Overlay*>* s_overlays = nullptr;
static bool* s_uiEdit = nullptr;
static ConnectionStatus* s_status = nullptr;
static HandleConfigChangeFn s_onConfigChange = nullptr;

void app_register_bridge(std::vector<Overlay*>* overlaysPtr,
                         bool* uiEditPtr,
                         ConnectionStatus* statusPtr,
                         HandleConfigChangeFn onConfigChangeFn)
{
	assert(overlaysPtr && uiEditPtr && statusPtr);
	s_overlays = overlaysPtr;
	s_uiEdit = uiEditPtr;
	s_status = statusPtr;
	s_onConfigChange = onConfigChangeFn;
}

void app_set_ui_edit(bool on)
{
	if (!s_overlays || !s_uiEdit) return;
	if ((*s_uiEdit) == on) return;
	*s_uiEdit = on;
	for (Overlay* o : *s_overlays)
		o->enableUiEdit(on);
}

void app_set_preview_mode(bool on)
{
	preview_mode_set(on);
}

static void setOverlayEnabled(const char* sectionKey, bool on)
{
	g_cfg.setBool(sectionKey, "enabled", on);
	g_cfg.save();
	if (s_overlays && s_status && s_onConfigChange)
		s_onConfigChange(*s_overlays, *s_status);
}

void app_set_overlay(const char* sectionKey, bool on)
{
	if (!sectionKey) return;
	setOverlayEnabled(sectionKey, on);
}

void app_toggle_overlay(const char* sectionKey)
{
	if (!sectionKey) return;
	bool cur = g_cfg.getBool(sectionKey, "enabled", true);
	setOverlayEnabled(sectionKey, !cur);
}

void app_set_config_string(const char* component, const char* key, const char* value)
{
	if (!component || !key || !value) return;
	g_cfg.setString(component, key, value);
	g_cfg.save();
	if (s_overlays && s_status && s_onConfigChange)
		s_onConfigChange(*s_overlays, *s_status);
}

void app_set_config_int(const char* component, const char* key, int value)
{
	if (!component || !key) return;
	g_cfg.setInt(component, key, value);
	g_cfg.save();
	if (s_overlays && s_status && s_onConfigChange)
		s_onConfigChange(*s_overlays, *s_status);
}

void app_set_config_bool(const char* component, const char* key, bool value)
{
	if (!component || !key) return;
	g_cfg.setBool(component, key, value);
	g_cfg.save();
	if (s_overlays && s_status && s_onConfigChange)
		s_onConfigChange(*s_overlays, *s_status);
}

void app_set_config_float(const char* component, const char* key, float value)
{
	if (!component || !key) return;
	g_cfg.setFloat(component, key, value);
	g_cfg.save();
	if (s_overlays && s_status && s_onConfigChange)
		s_onConfigChange(*s_overlays, *s_status);
}

void app_set_config_string_vec(const char* component, const char* key, const std::vector<std::string>& values)
{
    if (!component || !key) return;
    g_cfg.setStringVec(component, key, values);
    g_cfg.save();
    if (s_overlays && s_status && s_onConfigChange)
        s_onConfigChange(*s_overlays, *s_status);
}

std::string app_get_state_json()
{
	// Build a tiny JSON string without extra deps
	auto boolStr = [](bool v){ return v ? "true" : "false"; };
	int st = s_status ? (int)(*s_status) : 0;
	
	// Get current car information
	std::string currentCarName = "";
	if (StubDataManager::shouldUseStubData()) {
		// Use stub data for preview mode
		StubDataManager::populateSessionCars();
		if (ir_session.driverCarIdx >= 0 && ir_session.driverCarIdx < IR_MAX_CARS) {
			currentCarName = ir_session.cars[ir_session.driverCarIdx].carName;
		}
	} else if (ir_session.driverCarIdx >= 0 && ir_session.driverCarIdx < IR_MAX_CARS) {
		// Use real iRacing data
		currentCarName = ir_session.cars[ir_session.driverCarIdx].carName;
	}
	
	// Get available car configs
	std::vector<std::string> availableCarConfigs = g_cfg.getAvailableCarConfigs();
	
	char buf[16384];
	
	// Helper to escape string values for JSON
	auto escapeJson = [](const std::string& str) -> std::string {
		std::string escaped = str;
		size_t pos = 0;
		while ((pos = escaped.find("\"", pos)) != std::string::npos) {
			escaped.replace(pos, 1, "\\\"");
			pos += 2;
		}
		return escaped;
	};
	
	// Build car configs JSON array
	std::string carConfigsJson = "[";
	for (size_t i = 0; i < availableCarConfigs.size(); ++i) {
		if (i > 0) carConfigsJson += ",";
		carConfigsJson += "\"" + escapeJson(availableCarConfigs[i]) + "\"";
	}
	carConfigsJson += "]";
	
	// Build General string arrays (buddies/flagged) with proper escaping and without empty entries
	auto buildStringArrayJson = [&](const char* component, const char* key) -> std::string {
		std::vector<std::string> v = g_cfg.getStringVec(component, key, {});
		std::string out;
		out.reserve(256);
		bool first = true;
		for (const std::string& sRaw : v) {
			bool allWs = true; for (char c : sRaw) { if (!isspace((unsigned char)c)) { allWs = false; break; } }
			if (sRaw.empty() || allWs) continue;
			if (!first) out += ","; else first = false;
			out += "\"" + escapeJson(sRaw) + "\"";
		}
		return out;
	};

	const std::string buddiesJson = buildStringArrayJson("General", "buddies");
	const std::string flaggedJson = buildStringArrayJson("General", "flagged");

    snprintf(buf, sizeof(buf),
		"{\"uiEdit\":%s,\"previewMode\":%s,\"connectionStatus\":\"%s\"," 
		"\"currentCar\":\"%s\",\"currentCarConfig\":\"%s\",\"availableCarConfigs\":%s,"
        "\"overlays\":{"
        "\"OverlayStandings\":%s,\"OverlayDDU\":%s,\"OverlayFuel\":%s,\"OverlayInputs\":%s,\"OverlayRelative\":%s,\"OverlayCover\":%s,\"OverlayWeather\":%s,\"OverlayFlags\":%s,\"OverlayDelta\":%s,\"OverlayRadar\":%s,\"OverlayTrack\":%s,\"OverlayTire\":%s,\"OverlayPit\":%s},"
		"\"config\":{\"General\":{\"units\":\"%s\",\"performance_mode_30hz\":%s,\"buddies\":[%s],\"flagged\":[%s]},"
		"\"OverlayStandings\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"target_fps\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"show_all_classes\":%s,\"show_pit\":%s,\"show_license\":%s,\"show_irating\":%s,\"show_car_brand\":%s,\"show_positions_gained\":%s,\"show_gap\":%s,\"show_best\":%s,\"show_lap_time\":%s,\"show_delta\":%s,\"show_L5\":%s,\"show_SoF\":%s,\"show_laps\":%s,\"show_session_end\":%s,\"show_track_temp\":%s,\"show_tire_compound\":%s,\"font\":\"%s\",\"font_size\":%.2f,\"font_spacing\":%.2f,\"font_style\":\"%s\",\"font_weight\":%d},"
		"\"OverlayDDU\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"target_fps\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"font\":\"%s\",\"font_size\":%.2f,\"font_spacing\":%.2f,\"font_style\":\"%s\",\"font_weight\":%d},"
		"\"OverlayFuel\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"target_fps\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"fuel_estimate_factor\":%.2f,\"fuel_reserve_margin\":%.2f,\"fuel_target_lap\":%d,\"fuel_decimal_places\":%d,\"fuel_estimate_avg_green_laps\":%d,\"font\":\"%s\",\"font_size\":%.2f,\"font_spacing\":%.2f,\"font_style\":\"%s\",\"font_weight\":%d},"
        "\"OverlayInputs\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"target_fps\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"steering_wheel\":\"%s\",\"left_side\":%s,\"show_steering_line\":%s,\"show_steering_wheel\":%s,\"font\":\"%s\",\"font_size\":%.2f,\"font_spacing\":%.2f,\"font_style\":\"%s\",\"font_weight\":%d},"
		"\"OverlayRelative\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"target_fps\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"minimap_enabled\":%s,\"minimap_is_relative\":%s,\"show_ir_pred\":%s,\"show_irating\":%s,\"show_last\":%s,\"show_license\":%s,\"show_pit_age\":%s,\"show_sr\":%s,\"show_tire_compound\":%s,\"font\":\"%s\",\"font_size\":%.2f,\"font_spacing\":%.2f,\"font_style\":\"%s\",\"font_weight\":%d},"
		"\"OverlayCover\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"target_fps\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"font\":\"%s\",\"font_size\":%.2f,\"font_spacing\":%.2f,\"font_style\":\"%s\",\"font_weight\":%d},"
		"\"OverlayWeather\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"target_fps\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"preview_weather_type\":%d,\"font\":\"%s\",\"font_size\":%.2f,\"font_spacing\":%.2f,\"font_style\":\"%s\",\"font_weight\":%d},"
		"\"OverlayFlags\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"target_fps\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"preview_flag\":\"%s\",\"font\":\"%s\",\"font_size\":%.2f,\"font_spacing\":%.2f,\"font_style\":\"%s\",\"font_weight\":%d},"
		"\"OverlayDelta\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"target_fps\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"reference_mode\":%d,\"font\":\"%s\",\"font_size\":%.2f,\"font_spacing\":%.2f,\"font_style\":\"%s\",\"font_weight\":%d},"
		"\"OverlayRadar\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"target_fps\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"show_background\":%s,\"font\":\"%s\",\"font_size\":%.2f,\"font_spacing\":%.2f,\"font_style\":\"%s\",\"font_weight\":%d},"
        "\"OverlayTrack\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"target_fps\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"show_other_cars\":%s,\"reverse_direction\":%s,\"track_width\":%.1f,\"font\":\"%s\",\"font_size\":%.2f,\"font_spacing\":%.2f,\"font_style\":\"%s\",\"font_weight\":%d},"
        "\"OverlayTire\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"target_fps\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"show_only_in_pitlane\":%s,\"advanced_mode\":%s,\"pressure_use_psi\":%s,\"temp_cool_c\":%.1f,\"temp_opt_c\":%.1f,\"temp_hot_c\":%.1f,\"font\":\"%s\",\"font_size\":%.2f,\"font_spacing\":%.2f,\"font_style\":\"%s\",\"font_weight\":%d},"
        "\"OverlayPit\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"target_fps\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"font\":\"%s\",\"font_size\":%.2f,\"font_spacing\":%.2f,\"font_style\":\"%s\",\"font_weight\":%d}"
        "}}",
		(s_uiEdit && *s_uiEdit) ? "true":"false",
		boolStr(preview_mode_get()),
		ConnectionStatusStr[st],
		escapeJson(currentCarName).c_str(),
		escapeJson(g_cfg.getCurrentCarName()).c_str(),
		carConfigsJson.c_str(),
        boolStr(g_cfg.getBool("OverlayStandings","enabled",true)),
        boolStr(g_cfg.getBool("OverlayDDU","enabled",true)),
        boolStr(g_cfg.getBool("OverlayFuel","enabled",true)),
        boolStr(g_cfg.getBool("OverlayInputs","enabled",true)),
		boolStr(g_cfg.getBool("OverlayRelative","enabled",true)),
		boolStr(g_cfg.getBool("OverlayCover","enabled",true)),
		boolStr(g_cfg.getBool("OverlayWeather","enabled",true)),
		boolStr(g_cfg.getBool("OverlayFlags","enabled",true)),
		boolStr(g_cfg.getBool("OverlayDelta","enabled",true)),
		boolStr(g_cfg.getBool("OverlayRadar","enabled",true)),
        boolStr(g_cfg.getBool("OverlayTrack","enabled",true)),
        boolStr(g_cfg.getBool("OverlayTire","enabled",true)),
        boolStr(g_cfg.getBool("OverlayPit","enabled",true)),
		escapeJson(g_cfg.getString("General","units","metric")).c_str(),
		boolStr(g_cfg.getBool("General","performance_mode_30hz",false)),
		buddiesJson.c_str(),
		flaggedJson.c_str(),
		// OverlayStandings config
		boolStr(g_cfg.getBool("OverlayStandings","enabled",true)),
        escapeJson(g_cfg.getString("OverlayStandings","toggle_hotkey","ctrl+1")).c_str(),
		escapeJson(g_cfg.getString("OverlayStandings","position","custom")).c_str(),
		g_cfg.getInt("OverlayStandings","opacity",100),
		g_cfg.getInt("OverlayStandings","target_fps",10),
		boolStr(g_cfg.getBool("OverlayStandings","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_in_race",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_all_classes",false)),
		boolStr(g_cfg.getBool("OverlayStandings","show_pit",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_license",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_irating",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_car_brand",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_positions_gained",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_gap",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_best",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_lap_time",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_delta",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_L5",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_SoF",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_laps",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_session_end",true)),
        boolStr(g_cfg.getBool("OverlayStandings","show_track_temp",true)),
        boolStr(g_cfg.getBool("OverlayStandings","show_tire_compound",false)),
        // Typography (per-overlay with built-in defaults)
        escapeJson(g_cfg.getString("OverlayStandings","font","Poppins")).c_str(),
        g_cfg.getFloat("OverlayStandings","font_size", 16.0f),
        g_cfg.getFloat("OverlayStandings","font_spacing", 0.30f),
        escapeJson(g_cfg.getString("OverlayStandings","font_style","normal")).c_str(),
        g_cfg.getInt("OverlayStandings","font_weight", 500),
		// OverlayDDU config
		boolStr(g_cfg.getBool("OverlayDDU","enabled",true)),
		escapeJson(g_cfg.getString("OverlayDDU","toggle_hotkey","ctrl+2")).c_str(),
		escapeJson(g_cfg.getString("OverlayDDU","position","custom")).c_str(),
		g_cfg.getInt("OverlayDDU","opacity",100),
		g_cfg.getInt("OverlayDDU","target_fps",10),
		boolStr(g_cfg.getBool("OverlayDDU","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayDDU","show_in_race",true)),
		escapeJson(g_cfg.getString("OverlayDDU","font","Poppins")).c_str(),
		g_cfg.getFloat("OverlayDDU","font_size", 16.0f),
		g_cfg.getFloat("OverlayDDU","font_spacing", 0.30f),
		escapeJson(g_cfg.getString("OverlayDDU","font_style","normal")).c_str(),
		g_cfg.getInt("OverlayDDU","font_weight", 500),
		// OverlayFuel config
		boolStr(g_cfg.getBool("OverlayFuel","enabled",true)),
        escapeJson(g_cfg.getString("OverlayFuel","toggle_hotkey","ctrl+shift+2")).c_str(),
        escapeJson(g_cfg.getString("OverlayFuel","position","custom")).c_str(),
		g_cfg.getInt("OverlayFuel","opacity",100),
		g_cfg.getInt("OverlayFuel","target_fps",10),
        boolStr(g_cfg.getBool("OverlayFuel","show_in_menu",true)),
        boolStr(g_cfg.getBool("OverlayFuel","show_in_race",true)),
        g_cfg.getFloat("OverlayFuel","fuel_estimate_factor",1.1f),
        g_cfg.getFloat("OverlayFuel","fuel_reserve_margin",0.25f),
        g_cfg.getInt("OverlayFuel","fuel_target_lap",0),
        g_cfg.getInt("OverlayFuel","fuel_decimal_places",2),
        g_cfg.getInt("OverlayFuel","fuel_estimate_avg_green_laps",4),
        // Typography (per-overlay with built-in defaults)
        escapeJson(g_cfg.getString("OverlayFuel","font","Poppins")).c_str(),
        g_cfg.getFloat("OverlayFuel","font_size", 16.0f),
        g_cfg.getFloat("OverlayFuel","font_spacing", 0.30f),
        escapeJson(g_cfg.getString("OverlayFuel","font_style","normal")).c_str(),
        g_cfg.getInt("OverlayFuel","font_weight", 500),
		// OverlayInputs config
		boolStr(g_cfg.getBool("OverlayInputs","enabled",true)),
		escapeJson(g_cfg.getString("OverlayInputs","toggle_hotkey","ctrl+3")).c_str(),
		escapeJson(g_cfg.getString("OverlayInputs","position","custom")).c_str(),
		g_cfg.getInt("OverlayInputs","opacity",100),
		g_cfg.getInt("OverlayInputs","target_fps",30),
		boolStr(g_cfg.getBool("OverlayInputs","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayInputs","show_in_race",true)),
		escapeJson(g_cfg.getString("OverlayInputs","steering_wheel","builtin")).c_str(),
		boolStr(g_cfg.getBool("OverlayInputs","left_side",false)),
		boolStr(g_cfg.getBool("OverlayInputs","show_steering_line",false)),
        boolStr(g_cfg.getBool("OverlayInputs","show_steering_wheel",true)),
		escapeJson(g_cfg.getString("OverlayInputs","font","Poppins")).c_str(),
		g_cfg.getFloat("OverlayInputs","font_size", 16.0f),
		g_cfg.getFloat("OverlayInputs","font_spacing", 0.30f),
		escapeJson(g_cfg.getString("OverlayInputs","font_style","normal")).c_str(),
		g_cfg.getInt("OverlayInputs","font_weight", 500),
		// OverlayRelative config
		boolStr(g_cfg.getBool("OverlayRelative","enabled",true)),
        escapeJson(g_cfg.getString("OverlayRelative","toggle_hotkey","ctrl+4")).c_str(),
		escapeJson(g_cfg.getString("OverlayRelative","position","custom")).c_str(),
		g_cfg.getInt("OverlayRelative","opacity",100),
		g_cfg.getInt("OverlayRelative","target_fps",10),
		boolStr(g_cfg.getBool("OverlayRelative","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayRelative","show_in_race",true)),
		boolStr(g_cfg.getBool("OverlayRelative","minimap_enabled",true)),
		boolStr(g_cfg.getBool("OverlayRelative","minimap_is_relative",true)),
		boolStr(g_cfg.getBool("OverlayRelative","show_ir_pred",false)),
		boolStr(g_cfg.getBool("OverlayRelative","show_irating",true)),
		boolStr(g_cfg.getBool("OverlayRelative","show_last",true)),
		boolStr(g_cfg.getBool("OverlayRelative","show_license",true)),
		boolStr(g_cfg.getBool("OverlayRelative","show_pit_age",true)),
        boolStr(g_cfg.getBool("OverlayRelative","show_sr",false)),
        boolStr(g_cfg.getBool("OverlayRelative","show_tire_compound",false)),
        // Typography (per-overlay with built-in defaults)
        escapeJson(g_cfg.getString("OverlayRelative","font","Poppins")).c_str(),
        g_cfg.getFloat("OverlayRelative","font_size", 16.0f),
        g_cfg.getFloat("OverlayRelative","font_spacing", 0.30f),
        escapeJson(g_cfg.getString("OverlayRelative","font_style","normal")).c_str(),
        g_cfg.getInt("OverlayRelative","font_weight", 500),
		// OverlayCover config
		boolStr(g_cfg.getBool("OverlayCover","enabled",true)),
		escapeJson(g_cfg.getString("OverlayCover","toggle_hotkey","ctrl+5")).c_str(),
		escapeJson(g_cfg.getString("OverlayCover","position","custom")).c_str(),
		g_cfg.getInt("OverlayCover","opacity",100),
		g_cfg.getInt("OverlayCover","target_fps",10),
		boolStr(g_cfg.getBool("OverlayCover","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayCover","show_in_race",true)),
		escapeJson(g_cfg.getString("OverlayCover","font","Poppins")).c_str(),
		g_cfg.getFloat("OverlayCover","font_size", 16.0f),
		g_cfg.getFloat("OverlayCover","font_spacing", 0.30f),
		escapeJson(g_cfg.getString("OverlayCover","font_style","normal")).c_str(),
		g_cfg.getInt("OverlayCover","font_weight", 500),
		// OverlayWeather config  
		boolStr(g_cfg.getBool("OverlayWeather","enabled",true)),
		escapeJson(g_cfg.getString("OverlayWeather","toggle_hotkey","ctrl+6")).c_str(),
		escapeJson(g_cfg.getString("OverlayWeather","position","custom")).c_str(),
		g_cfg.getInt("OverlayWeather","opacity",100),
		g_cfg.getInt("OverlayWeather","target_fps",10),
		boolStr(g_cfg.getBool("OverlayWeather","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayWeather","show_in_race",true)),
		g_cfg.getInt("OverlayWeather","preview_weather_type",1),
		escapeJson(g_cfg.getString("OverlayWeather","font","Poppins")).c_str(),
		g_cfg.getFloat("OverlayWeather","font_size", 16.0f),
		g_cfg.getFloat("OverlayWeather","font_spacing", 0.30f),
		escapeJson(g_cfg.getString("OverlayWeather","font_style","normal")).c_str(),
		g_cfg.getInt("OverlayWeather","font_weight", 500),
		// OverlayFlags config
		boolStr(g_cfg.getBool("OverlayFlags","enabled",true)),
		escapeJson(g_cfg.getString("OverlayFlags","toggle_hotkey","ctrl+7")).c_str(),
		escapeJson(g_cfg.getString("OverlayFlags","position","custom")).c_str(),
		g_cfg.getInt("OverlayFlags","opacity",100),
		g_cfg.getInt("OverlayFlags","target_fps",10),
		boolStr(g_cfg.getBool("OverlayFlags","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayFlags","show_in_race",true)),
		escapeJson(g_cfg.getString("OverlayFlags","preview_flag","none")).c_str(),
		escapeJson(g_cfg.getString("OverlayFlags","font","Poppins")).c_str(),
		g_cfg.getFloat("OverlayFlags","font_size", 16.0f),
		g_cfg.getFloat("OverlayFlags","font_spacing", 0.30f),
		escapeJson(g_cfg.getString("OverlayFlags","font_style","normal")).c_str(),
		g_cfg.getInt("OverlayFlags","font_weight", 500),
		// OverlayDelta config
		boolStr(g_cfg.getBool("OverlayDelta","enabled",true)),
		escapeJson(g_cfg.getString("OverlayDelta","toggle_hotkey","ctrl+8")).c_str(),
		escapeJson(g_cfg.getString("OverlayDelta","position","custom")).c_str(),
		g_cfg.getInt("OverlayDelta","opacity",100),
		g_cfg.getInt("OverlayDelta","target_fps",15),
		boolStr(g_cfg.getBool("OverlayDelta","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayDelta","show_in_race",true)),
		g_cfg.getInt("OverlayDelta","reference_mode",1),
		escapeJson(g_cfg.getString("OverlayDelta","font","Poppins")).c_str(),
		g_cfg.getFloat("OverlayDelta","font_size", 16.0f),
		g_cfg.getFloat("OverlayDelta","font_spacing", 0.30f),
		escapeJson(g_cfg.getString("OverlayDelta","font_style","normal")).c_str(),
		g_cfg.getInt("OverlayDelta","font_weight", 500),
		// OverlayRadar config
		boolStr(g_cfg.getBool("OverlayRadar","enabled",true)),
		escapeJson(g_cfg.getString("OverlayRadar","toggle_hotkey","ctrl+9")).c_str(),
		escapeJson(g_cfg.getString("OverlayRadar","position","custom")).c_str(),
		g_cfg.getInt("OverlayRadar","opacity",100),
		g_cfg.getInt("OverlayRadar","target_fps",10),
		boolStr(g_cfg.getBool("OverlayRadar","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayRadar","show_in_race",true)),
		boolStr(g_cfg.getBool("OverlayRadar","show_background",true)),
		escapeJson(g_cfg.getString("OverlayRadar","font","Poppins")).c_str(),
		g_cfg.getFloat("OverlayRadar","font_size", 16.0f),
		g_cfg.getFloat("OverlayRadar","font_spacing", 0.30f),
		escapeJson(g_cfg.getString("OverlayRadar","font_style","normal")).c_str(),
		g_cfg.getInt("OverlayRadar","font_weight", 500),
		// OverlayTrack config
		boolStr(g_cfg.getBool("OverlayTrack","enabled",true)),
		escapeJson(g_cfg.getString("OverlayTrack","toggle_hotkey","ctrl+0")).c_str(),
		escapeJson(g_cfg.getString("OverlayTrack","position","custom")).c_str(),
		g_cfg.getInt("OverlayTrack","opacity",100),
		g_cfg.getInt("OverlayTrack","target_fps",15),
		boolStr(g_cfg.getBool("OverlayTrack","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayTrack","show_in_race",true)),
		boolStr(g_cfg.getBool("OverlayTrack","show_other_cars",false)),
		boolStr(g_cfg.getBool("OverlayTrack","reverse_direction",false)),
		g_cfg.getFloat("OverlayTrack","track_width",6.0f),
		escapeJson(g_cfg.getString("OverlayTrack","font","Poppins")).c_str(),
		g_cfg.getFloat("OverlayTrack","font_size", 16.0f),
		g_cfg.getFloat("OverlayTrack","font_spacing", 0.30f),
		escapeJson(g_cfg.getString("OverlayTrack","font_style","normal")).c_str(),
		g_cfg.getInt("OverlayTrack","font_weight", 500),
		// OverlayTire config
		boolStr(g_cfg.getBool("OverlayTire","enabled",true)),
		escapeJson(g_cfg.getString("OverlayTire","toggle_hotkey","ctrl+shift+3")).c_str(),
		escapeJson(g_cfg.getString("OverlayTire","position","custom")).c_str(),
		g_cfg.getInt("OverlayTire","opacity",100),
		g_cfg.getInt("OverlayTire","target_fps",10),
		boolStr(g_cfg.getBool("OverlayTire","show_in_menu",false)),
		boolStr(g_cfg.getBool("OverlayTire","show_in_race",true)),
		boolStr(g_cfg.getBool("OverlayTire","show_only_in_pitlane",false)),
		boolStr(g_cfg.getBool("OverlayTire","advanced_mode",true)),
		boolStr(g_cfg.getBool("OverlayTire","pressure_use_psi",true)),
		g_cfg.getFloat("OverlayTire","temp_cool_c",60.0f),
		g_cfg.getFloat("OverlayTire","temp_opt_c",85.0f),
		g_cfg.getFloat("OverlayTire","temp_hot_c",105.0f),
		escapeJson(g_cfg.getString("OverlayTire","font","Poppins")).c_str(),
		g_cfg.getFloat("OverlayTire","font_size", 16.0f),
		g_cfg.getFloat("OverlayTire","font_spacing", 0.30f),
		escapeJson(g_cfg.getString("OverlayTire","font_style","normal")).c_str(),
		g_cfg.getInt("OverlayTire","font_weight", 500),
		// OverlayPit config
		boolStr(g_cfg.getBool("OverlayPit","enabled",true)),
        escapeJson(g_cfg.getString("OverlayPit","toggle_hotkey","ctrl+shift+0")).c_str(),
        escapeJson(g_cfg.getString("OverlayPit","position","custom")).c_str(),
		g_cfg.getInt("OverlayPit","opacity",100),
		g_cfg.getInt("OverlayPit","target_fps",30),
        boolStr(g_cfg.getBool("OverlayPit","show_in_menu",true)),
        boolStr(g_cfg.getBool("OverlayPit","show_in_race",true)),
		escapeJson(g_cfg.getString("OverlayPit","font","Poppins")).c_str(),
		g_cfg.getFloat("OverlayPit","font_size", 16.0f),
		g_cfg.getFloat("OverlayPit","font_spacing", 0.30f),
		escapeJson(g_cfg.getString("OverlayPit","font_style","normal")).c_str(),
		g_cfg.getInt("OverlayPit","font_weight", 500)
	);
	return std::string(buf);
}

void app_handleConfigChange_external()
{
	if (s_overlays && s_status && s_onConfigChange)
		s_onConfigChange(*s_overlays, *s_status);
} 