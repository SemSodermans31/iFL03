#include "AppControl.h"
#include "Config.h"
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

std::string app_get_state_json()
{
	// Build a tiny JSON string without extra deps
	auto boolStr = [](bool v){ return v ? "true" : "false"; };
	int st = s_status ? (int)(*s_status) : 0;
	char buf[1024];
	snprintf(buf, sizeof(buf),
		"{\"uiEdit\":%s,\"connectionStatus\":\"%s\",\"overlays\":{"
		"\"OverlayStandings\":%s,\"OverlayDDU\":%s,\"OverlayInputs\":%s,\"OverlayRelative\":%s,\"OverlayCover\":%s} }",
		(s_uiEdit && *s_uiEdit) ? "true":"false",
		ConnectionStatusStr[st],
		boolStr(g_cfg.getBool("OverlayStandings","enabled",true)),
		boolStr(g_cfg.getBool("OverlayDDU","enabled",true)),
		boolStr(g_cfg.getBool("OverlayInputs","enabled",true)),
		boolStr(g_cfg.getBool("OverlayRelative","enabled",true)),
		boolStr(g_cfg.getBool("OverlayCover","enabled",true))
	);
	return std::string(buf);
}

void app_handleConfigChange_external()
{
	if (s_overlays && s_status && s_onConfigChange)
		s_onConfigChange(*s_overlays, *s_status);
} 