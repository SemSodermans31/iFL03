#include "AppControl.h"
#include "Config.h"
#include "preview_mode.h"
#include "iracing.h"
#include "stub_data.h"
#include <cassert>

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
	
	char buf[8192]; // Increased buffer size for full config with car info
	
	// Helper to escape string values for JSON
	auto escapeJson = [](const std::string& str) -> std::string {
		std::string escaped = str;
		// Basic JSON escaping - replace quotes with escaped quotes
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
	
	snprintf(buf, sizeof(buf),
		"{\"uiEdit\":%s,\"previewMode\":%s,\"connectionStatus\":\"%s\"," 
		"\"currentCar\":\"%s\",\"currentCarConfig\":\"%s\",\"availableCarConfigs\":%s,"
		"\"overlays\":{"
		"\"OverlayStandings\":%s,\"OverlayDDU\":%s,\"OverlayInputs\":%s,\"OverlayRelative\":%s,\"OverlayCover\":%s,\"OverlayWeather\":%s,\"OverlayFlags\":%s,\"OverlayDelta\":%s,\"OverlayRadar\":%s,\"OverlayTrack\":%s},"
		"\"config\":{\"General\":{\"units\":\"%s\",\"performance_mode_30hz\":%s},"
		"\"OverlayStandings\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"show_pit\":%s,\"show_license\":%s,\"show_irating\":%s,\"show_car_brand\":%s,\"show_positions_gained\":%s,\"show_gap\":%s,\"show_best\":%s,\"show_lap_time\":%s,\"show_delta\":%s,\"show_L5\":%s},"
		"\"OverlayDDU\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"show_in_menu\":%s,\"show_in_race\":%s},"
		"\"OverlayInputs\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"steering_wheel\":\"%s\"},"
		"\"OverlayRelative\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"show_in_menu\":%s,\"show_in_race\":%s},"
		"\"OverlayCover\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"show_in_menu\":%s,\"show_in_race\":%s},"
		"\"OverlayWeather\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"preview_weather_type\":%d},"
		"\"OverlayFlags\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"preview_flag\":\"%s\"},"
		"\"OverlayDelta\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"reference_mode\":%d},"
		"\"OverlayRadar\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"show_in_menu\":%s,\"show_in_race\":%s},"
		"\"OverlayTrack\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d,\"show_in_menu\":%s,\"show_in_race\":%s,\"show_other_cars\":%s,\"track_width\":%.1f}"
		"}}",
		(s_uiEdit && *s_uiEdit) ? "true":"false",
		boolStr(preview_mode_get()),
		ConnectionStatusStr[st],
		escapeJson(currentCarName).c_str(),
		escapeJson(g_cfg.getCurrentCarName()).c_str(),
		carConfigsJson.c_str(),
		boolStr(g_cfg.getBool("OverlayStandings","enabled",true)),
		boolStr(g_cfg.getBool("OverlayDDU","enabled",true)),
		boolStr(g_cfg.getBool("OverlayInputs","enabled",true)),
		boolStr(g_cfg.getBool("OverlayRelative","enabled",true)),
		boolStr(g_cfg.getBool("OverlayCover","enabled",true)),
		boolStr(g_cfg.getBool("OverlayWeather","enabled",true)),
		boolStr(g_cfg.getBool("OverlayFlags","enabled",true)),
		boolStr(g_cfg.getBool("OverlayDelta","enabled",true)),
		boolStr(g_cfg.getBool("OverlayRadar","enabled",true)),
		boolStr(g_cfg.getBool("OverlayTrack","enabled",true)),
		escapeJson(g_cfg.getString("General","units","metric")).c_str(),
		boolStr(g_cfg.getBool("General","performance_mode_30hz",false)),
		// OverlayStandings config
		boolStr(g_cfg.getBool("OverlayStandings","enabled",true)),
		escapeJson(g_cfg.getString("OverlayStandings","toggle_hotkey","ctrl+1")).c_str(),
		escapeJson(g_cfg.getString("OverlayStandings","position","custom")).c_str(),
		g_cfg.getInt("OverlayStandings","opacity",100),
		boolStr(g_cfg.getBool("OverlayStandings","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayStandings","show_in_race",true)),
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
		// OverlayDDU config
		boolStr(g_cfg.getBool("OverlayDDU","enabled",true)),
		escapeJson(g_cfg.getString("OverlayDDU","toggle_hotkey","ctrl+2")).c_str(),
		escapeJson(g_cfg.getString("OverlayDDU","position","custom")).c_str(),
		g_cfg.getInt("OverlayDDU","opacity",100),
		boolStr(g_cfg.getBool("OverlayDDU","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayDDU","show_in_race",true)),
		// OverlayInputs config
		boolStr(g_cfg.getBool("OverlayInputs","enabled",true)),
		escapeJson(g_cfg.getString("OverlayInputs","toggle_hotkey","ctrl+3")).c_str(),
		escapeJson(g_cfg.getString("OverlayInputs","position","custom")).c_str(),
		g_cfg.getInt("OverlayInputs","opacity",100),
		boolStr(g_cfg.getBool("OverlayInputs","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayInputs","show_in_race",true)),
		escapeJson(g_cfg.getString("OverlayInputs","steering_wheel","builtin")).c_str(),
		// OverlayRelative config
		boolStr(g_cfg.getBool("OverlayRelative","enabled",true)),
		escapeJson(g_cfg.getString("OverlayRelative","toggle_hotkey","ctrl+4")).c_str(),
		escapeJson(g_cfg.getString("OverlayRelative","position","custom")).c_str(),
		g_cfg.getInt("OverlayRelative","opacity",100),
		boolStr(g_cfg.getBool("OverlayRelative","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayRelative","show_in_race",true)),
		// OverlayCover config
		boolStr(g_cfg.getBool("OverlayCover","enabled",true)),
		escapeJson(g_cfg.getString("OverlayCover","toggle_hotkey","ctrl+5")).c_str(),
		escapeJson(g_cfg.getString("OverlayCover","position","custom")).c_str(),
		g_cfg.getInt("OverlayCover","opacity",100),
		boolStr(g_cfg.getBool("OverlayCover","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayCover","show_in_race",true)),
		// OverlayWeather config  
		boolStr(g_cfg.getBool("OverlayWeather","enabled",true)),
		escapeJson(g_cfg.getString("OverlayWeather","toggle_hotkey","ctrl+6")).c_str(),
		escapeJson(g_cfg.getString("OverlayWeather","position","custom")).c_str(),
		g_cfg.getInt("OverlayWeather","opacity",100),
		boolStr(g_cfg.getBool("OverlayWeather","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayWeather","show_in_race",true)),
		g_cfg.getInt("OverlayWeather","preview_weather_type",1),
		// OverlayFlags config
		boolStr(g_cfg.getBool("OverlayFlags","enabled",true)),
		escapeJson(g_cfg.getString("OverlayFlags","toggle_hotkey","ctrl+7")).c_str(),
		escapeJson(g_cfg.getString("OverlayFlags","position","custom")).c_str(),
		g_cfg.getInt("OverlayFlags","opacity",100),
		boolStr(g_cfg.getBool("OverlayFlags","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayFlags","show_in_race",true)),
		escapeJson(g_cfg.getString("OverlayFlags","preview_flag","none")).c_str(),
		// OverlayDelta config
		boolStr(g_cfg.getBool("OverlayDelta","enabled",true)),
		escapeJson(g_cfg.getString("OverlayDelta","toggle_hotkey","ctrl+8")).c_str(),
		escapeJson(g_cfg.getString("OverlayDelta","position","custom")).c_str(),
		g_cfg.getInt("OverlayDelta","opacity",100),
		boolStr(g_cfg.getBool("OverlayDelta","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayDelta","show_in_race",true)),
		g_cfg.getInt("OverlayDelta","reference_mode",1),
		// OverlayRadar config
		boolStr(g_cfg.getBool("OverlayRadar","enabled",true)),
		escapeJson(g_cfg.getString("OverlayRadar","toggle_hotkey","ctrl+9")).c_str(),
		escapeJson(g_cfg.getString("OverlayRadar","position","custom")).c_str(),
		g_cfg.getInt("OverlayRadar","opacity",100),
		boolStr(g_cfg.getBool("OverlayRadar","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayRadar","show_in_race",true)),
		// OverlayTrack config
		boolStr(g_cfg.getBool("OverlayTrack","enabled",true)),
		escapeJson(g_cfg.getString("OverlayTrack","toggle_hotkey","ctrl+0")).c_str(),
		escapeJson(g_cfg.getString("OverlayTrack","position","custom")).c_str(),
		g_cfg.getInt("OverlayTrack","opacity",100),
		boolStr(g_cfg.getBool("OverlayTrack","show_in_menu",true)),
		boolStr(g_cfg.getBool("OverlayTrack","show_in_race",true)),
		boolStr(g_cfg.getBool("OverlayTrack","show_other_cars",false)),
		g_cfg.getFloat("OverlayTrack","track_width",6.0f)
	);
	return std::string(buf);
}

void app_handleConfigChange_external()
{
	if (s_overlays && s_status && s_onConfigChange)
		s_onConfigChange(*s_overlays, *s_status);
} 