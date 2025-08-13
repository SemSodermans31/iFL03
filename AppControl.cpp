#include "AppControl.h"
#include "Config.h"
#include "preview_mode.h"
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

std::string app_get_state_json()
{
	// Build a tiny JSON string without extra deps
	auto boolStr = [](bool v){ return v ? "true" : "false"; };
	int st = s_status ? (int)(*s_status) : 0;
	char buf[2048]; // Increased buffer size for full config
	
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
	
	snprintf(buf, sizeof(buf),
		"{\"uiEdit\":%s,\"previewMode\":%s,\"connectionStatus\":\"%s\","
		"\"overlays\":{"
		"\"OverlayStandings\":%s,\"OverlayDDU\":%s,\"OverlayInputs\":%s,\"OverlayRelative\":%s,\"OverlayCover\":%s},"
		"\"config\":{"
		"\"OverlayStandings\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d},"
		"\"OverlayDDU\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d},"
		"\"OverlayInputs\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d},"
		"\"OverlayRelative\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d},"
		"\"OverlayCover\":{\"enabled\":%s,\"toggle_hotkey\":\"%s\",\"position\":\"%s\",\"opacity\":%d}"
		"}}",
		(s_uiEdit && *s_uiEdit) ? "true":"false",
		boolStr(preview_mode_get()),
		ConnectionStatusStr[st],
		boolStr(g_cfg.getBool("OverlayStandings","enabled",true)),
		boolStr(g_cfg.getBool("OverlayDDU","enabled",true)),
		boolStr(g_cfg.getBool("OverlayInputs","enabled",true)),
		boolStr(g_cfg.getBool("OverlayRelative","enabled",true)),
		boolStr(g_cfg.getBool("OverlayCover","enabled",true)),
		// OverlayStandings config
		boolStr(g_cfg.getBool("OverlayStandings","enabled",true)),
		escapeJson(g_cfg.getString("OverlayStandings","toggle_hotkey","ctrl-space")).c_str(),
		escapeJson(g_cfg.getString("OverlayStandings","position","custom")).c_str(),
		g_cfg.getInt("OverlayStandings","opacity",100),
		// OverlayDDU config
		boolStr(g_cfg.getBool("OverlayDDU","enabled",true)),
		escapeJson(g_cfg.getString("OverlayDDU","toggle_hotkey","ctrl-1")).c_str(),
		escapeJson(g_cfg.getString("OverlayDDU","position","custom")).c_str(),
		g_cfg.getInt("OverlayDDU","opacity",100),
		// OverlayInputs config
		boolStr(g_cfg.getBool("OverlayInputs","enabled",true)),
		escapeJson(g_cfg.getString("OverlayInputs","toggle_hotkey","ctrl-2")).c_str(),
		escapeJson(g_cfg.getString("OverlayInputs","position","custom")).c_str(),
		g_cfg.getInt("OverlayInputs","opacity",100),
		// OverlayRelative config
		boolStr(g_cfg.getBool("OverlayRelative","enabled",true)),
		escapeJson(g_cfg.getString("OverlayRelative","toggle_hotkey","ctrl-3")).c_str(),
		escapeJson(g_cfg.getString("OverlayRelative","position","custom")).c_str(),
		g_cfg.getInt("OverlayRelative","opacity",100),
		// OverlayCover config
		boolStr(g_cfg.getBool("OverlayCover","enabled",true)),
		escapeJson(g_cfg.getString("OverlayCover","toggle_hotkey","ctrl-4")).c_str(),
		escapeJson(g_cfg.getString("OverlayCover","position","custom")).c_str(),
		g_cfg.getInt("OverlayCover","opacity",100)
	);
	return std::string(buf);
}

void app_handleConfigChange_external()
{
	if (s_overlays && s_status && s_onConfigChange)
		s_onConfigChange(*s_overlays, *s_status);
} 