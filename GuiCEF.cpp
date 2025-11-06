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

#include "GuiCEF.h"

#ifdef IFL03_USE_CEF
#include <windows.h>
#include "resource.h"
#include <string>
#include <utility>
#include <cstdint>
#include <cctype>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// Define DWMWA_USE_IMMERSIVE_DARK_MODE if not available (for older SDKs)
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_browser.h"
#include "include/cef_sandbox_win.h"
#include "include/cef_v8.h"
#include "include/cef_context_menu_handler.h"
#include "include/cef_keyboard_handler.h"
#include "include/cef_dom.h"
#include <shellapi.h>
#include "include/base/cef_build.h"
#include "include/base/cef_ref_counted.h"
#include "include/wrapper/cef_message_router.h"
#include "AppControl.h"
#include "Config.h"

namespace {
	static const wchar_t* kGuiWndClass = L"iFL03GuiWindow";
	HWND g_parentHwnd = NULL;
	CefRefPtr<CefBrowser> g_browser;
	CefRefPtr<CefMessageRouterBrowserSide> g_router;
	CefRefPtr<CefMessageRouterRendererSide> g_router_renderer;
    static std::atomic<bool> g_editableFocused{false};
    static const UINT WM_TRAYICON = WM_APP + 1;
    static bool g_trayIconAdded = false;

	static void OpenUrlWithShellExecute(const std::string& urlUtf8)
	{
		if (urlUtf8.empty()) return;
		std::wstring urlW = CefString(urlUtf8);
		ShellExecuteW(NULL, L"open", urlW.c_str(), NULL, NULL, SW_SHOWNORMAL);
	}

	// Helper function to enable dark mode for window borders
	static void EnableDarkModeForWindow(HWND hwnd)
	{
		BOOL darkMode = TRUE;
		HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

		if (FAILED(hr)) {
			const int DWMWA_USE_IMMERSIVE_DARK_MODE_OLD = 19;
			DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, &darkMode, sizeof(darkMode));
		}
	}

	LRESULT CALLBACK GuiWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
				switch (msg)
	{
	case WM_CREATE:
		EnableDarkModeForWindow(hwnd);
		break;
	case WM_THEMECHANGED:
		EnableDarkModeForWindow(hwnd);
		InvalidateRect(hwnd, NULL, TRUE);
		break;
	case WM_GETMINMAXINFO:
	{
		LPMINMAXINFO lpMMI = (LPMINMAXINFO)lparam;
		// Set minimum window size to 1368x768
		lpMMI->ptMinTrackSize.x = 1368;
		lpMMI->ptMinTrackSize.y = 768;
		return 0;
	}
	case WM_SIZE:
		{
            if (g_browser.get())
            {
                HWND child = g_browser->GetHost()->GetWindowHandle();
                RECT rc = {0,0,0,0};
                if (!GetClientRect(hwnd, &rc)) {
                    rc.left = rc.top = 0; rc.right = 0; rc.bottom = 0;
                }
                MoveWindow(child, 0, 0, rc.right - rc.left, rc.bottom - rc.top, TRUE);
            }
			break;
		}
		case WM_CLOSE:
			DestroyWindow(hwnd);
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProcW(hwnd, msg, wparam, lparam);
	}

	class SimpleClient : public CefClient, public CefLifeSpanHandler, public CefDisplayHandler, public CefRequestHandler, public CefContextMenuHandler, public CefKeyboardHandler {
	public:
		SimpleClient() {}

		CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
		CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
		CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
		CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
		CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }

		static void OpenInDefaultBrowser(const std::string& urlUtf8)
		{
			// Only attempt for http/https/mailto
			if (!(urlUtf8.rfind("http://", 0) == 0 || urlUtf8.rfind("https://", 0) == 0 || urlUtf8.rfind("mailto:", 0) == 0))
				return;
			OpenUrlWithShellExecute(urlUtf8);
		}

		bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
			CefRefPtr<CefFrame> frame,
			CefRefPtr<CefRequest> request,
			bool user_gesture,
			bool is_redirect) override
		{
			if (g_router.get()) g_router->OnBeforeBrowse(browser, frame);
			return false;
		}

		// Disable right-click context menu entirely
		void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
			CefRefPtr<CefFrame> frame,
			CefRefPtr<CefContextMenuParams> params,
			CefRefPtr<CefMenuModel> model) override
		{
			if (model.get()) model->Clear();
		}

		// Block browser shortcuts; allow typing only when an editable field has focus
		bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
			const CefKeyEvent& event,
			CefEventHandle os_event,
			bool* is_keyboard_shortcut) override
		{
			if (is_keyboard_shortcut) *is_keyboard_shortcut = false;
			if (event.type != KEYEVENT_RAWKEYDOWN && event.type != KEYEVENT_KEYDOWN && event.type != KEYEVENT_CHAR)
				return false;

			const int key = event.windows_key_code;
			const bool ctrl  = (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0;
			const bool shift = (event.modifiers & EVENTFLAG_SHIFT_DOWN) != 0;
			const bool alt   = (event.modifiers & EVENTFLAG_ALT_DOWN) != 0;

			// Always block common browser shortcuts
			if (key == VK_F5 || key == VK_F11 || key == VK_F12) return true;
			if (ctrl && (key == 'R' || key == VK_BROWSER_REFRESH)) return true;
			if (ctrl && shift && (key == 'I' || key == VK_F12)) return true;
			if (ctrl && (key == 'L' || key == 'T' || key == 'N' || key == 'W')) return true;
			if (alt && key == VK_LEFT) return true;
			if (alt && key == VK_RIGHT) return true;

			// If no editable element is focused, swallow all keyboard input
			if (!event.focus_on_editable_field)
				return true;

			// Otherwise allow typing into inputs
			return false;
		}

		// Note: We rely on JS interception and OnBeforeBrowse for external links

		void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
			g_browser = browser;
			if (g_parentHwnd)
				SendMessage(g_parentHwnd, WM_SIZE, 0, 0);
		}

		void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
			if (g_router.get()) g_router->OnBeforeClose(browser);
			if (g_browser.get() == browser)
				g_browser = nullptr;
		}

		bool OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level, const CefString& message, const CefString& source, int line) override {
			return false; // allow console messages
		}

		bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override {
			if (g_router.get())
				return g_router->OnProcessMessageReceived(browser, frame, source_process, message);
			return false;
		}

		IMPLEMENT_REFCOUNTING(SimpleClient);
	};

	class QueryHandler : public CefMessageRouterBrowserSide::Handler, public CefBaseRefCounted {
	public:
		static bool extractStringField(const std::string& s, const char* field, std::string& out)
		{
			std::string pat = std::string("\"") + field + "\":\"";
			size_t p = s.find(pat);
			if (p == std::string::npos) return false;
			p += pat.size();
			size_t q = s.find('"', p);
			if (q == std::string::npos || q < p) return false;
			out.assign(s, p, q - p);
			return true;
		}

		static bool extractBoolField(const std::string& s, const char* field, bool& out)
		{
			std::string pat = std::string("\"") + field + "\":";
			size_t p = s.find(pat);
			if (p == std::string::npos) return false;
			p += pat.size();
			// Skip whitespace
			while (p < s.size() && (s[p]==' '||s[p]=='\t')) p++;
			if (s.compare(p, 4, "true") == 0) { out = true; return true; }
			if (s.compare(p, 5, "false") == 0) { out = false; return true; }
			return false;
		}

		static bool extractIntField(const std::string& s, const char* field, int& out)
		{
			std::string pat = std::string("\"") + field + "\":";
			size_t p = s.find(pat);
			if (p == std::string::npos) return false;
			p += pat.size();
			// Skip whitespace
			while (p < s.size() && (s[p]==' '||s[p]=='\t')) p++;
			// Find end of number (digit, minus sign, or end of string)
			size_t end = p;
			while (end < s.size() && (isdigit(s[end]) || s[end] == '-')) end++;
			if (end == p) return false; // No digits found
			std::string numStr = s.substr(p, end - p);
			try {
				out = std::stoi(numStr);
				return true;
			} catch (...) {
				return false;
			}
		}

		bool OnQuery(CefRefPtr<CefBrowser> browser,
			CefRefPtr<CefFrame> frame,
			int64_t query_id,
			const CefString& request,
			bool persistent,
			CefRefPtr<CefMessageRouterBrowserSide::Callback> callback) override
		{
			std::string req = request.ToString();
			auto has = [&](const char* k){ return req.find(k) != std::string::npos; };
			if (has("\"cmd\":\"getState\"")) {
				callback->Success(app_get_state_json());
				return true;
			}
			// Performance mode (30Hz) toggle from Settings UI
			if (has("\"cmd\":\"setPerformanceMode\"")) {
				bool on = false; (void)extractBoolField(req, "on", on);
				app_set_config_bool("General", "performance_mode_30hz", on);
				callback->Success(app_get_state_json());
				return true;
			}
			// Startup at Windows login
			if (has("\"cmd\":\"setStartup\"")) {
				bool on = false; (void)extractBoolField(req, "on", on);
				app_set_startup_enabled(on);
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"openExternal\"")) {
				std::string url;
				if (extractStringField(req, "url", url)) {
					OpenUrlWithShellExecute(url);
					callback->Success(app_get_state_json());
					return true;
				}
			}
			if (has("\"cmd\":\"setUiEdit\"")) {
				bool on = false; (void)extractBoolField(req, "on", on);
				app_set_ui_edit(on);
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"setPreviewMode\"")) {
				bool on = false; (void)extractBoolField(req, "on", on);
				app_set_preview_mode(on);
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"toggleOverlay\"")) {
				std::string key; if (extractStringField(req, "key", key)) app_toggle_overlay(key.c_str());
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"setOverlay\"")) {
				std::string key; bool on=false;
				if (extractStringField(req, "key", key) && extractBoolField(req, "on", on))
					app_set_overlay(key.c_str(), on);
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"setHotkey\"")) {
				std::string component, key, value;
				if (extractStringField(req, "component", component) && 
					extractStringField(req, "key", key) && 
					extractStringField(req, "value", value)) {
					app_set_config_string(component.c_str(), key.c_str(), value.c_str());
				}
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"setOverlayOpacity\"")) {
				std::string component;
				if (extractStringField(req, "component", component)) {

					size_t opacityPos = req.find("\"opacity\":");
					if (opacityPos != std::string::npos) {
						size_t valueStart = opacityPos + 10;
						while (valueStart < req.size() && (req[valueStart] == ' ' || req[valueStart] == '\t')) valueStart++;
						size_t valueEnd = valueStart;
						while (valueEnd < req.size() && std::isdigit(req[valueEnd])) valueEnd++;
						if (valueEnd > valueStart) {
							int opacity = std::stoi(req.substr(valueStart, valueEnd - valueStart));
							app_set_config_int(component.c_str(), "opacity", opacity);
						}
					}
				}
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"moveOverlay\"")) {
				std::string component, direction;
				if (extractStringField(req, "component", component) &&
					extractStringField(req, "direction", direction)) {
					int deltaX = 0, deltaY = 0;
					if (direction == "up") deltaY = -10;
					else if (direction == "down") deltaY = 10;
					else if (direction == "left") deltaX = -10;
					else if (direction == "right") deltaX = 10;

					if (deltaX != 0 || deltaY != 0) {
						app_move_overlay(component.c_str(), deltaX, deltaY);
					}
				}
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"moveOverlayDelta\"")) {
				std::string component;
				int deltaX = 0, deltaY = 0;
				if (extractStringField(req, "component", component) &&
					extractIntField(req, "deltaX", deltaX) &&
					extractIntField(req, "deltaY", deltaY)) {
					app_move_overlay(component.c_str(), deltaX, deltaY);
				}
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"centerOverlay\"")) {
				std::string component;
				if (extractStringField(req, "component", component)) {
					app_center_overlay(component.c_str());
				}
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"setPreviewFlag\"")) {
				std::string value;
				if (extractStringField(req, "value", value)) {
					app_set_config_string("OverlayFlags", "preview_flag", value.c_str());
				}
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"setPreviewWeatherType\"")) {
				size_t valuePos = req.find("\"value\":");
				if (valuePos != std::string::npos) {
					size_t valueStart = valuePos + 8;
					while (valueStart < req.size() && (req[valueStart] == ' ' || req[valueStart] == '\t')) valueStart++;
					if (valueStart < req.size() && (req[valueStart] == '0' || req[valueStart] == '1')) {
						int weatherType = req[valueStart] - '0';
						app_set_config_int("OverlayWeather", "preview_weather_type", weatherType);
					}
				}
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"setConfigBool\"")) {
				std::string component, key;
				bool value = false;
				if (extractStringField(req, "component", component) && 
					extractStringField(req, "key", key) &&
					extractBoolField(req, "value", value)) {
					app_set_config_bool(component.c_str(), key.c_str(), value);
				}
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"setConfigString\"")) {
				std::string component, key, value;
				if (extractStringField(req, "component", component) &&
					extractStringField(req, "key", key) &&
					extractStringField(req, "value", value)) {
					app_set_config_string(component.c_str(), key.c_str(), value.c_str());
				}
				callback->Success(app_get_state_json());
				return true;
			}
            // Backward compat: allow direct set for General.ghost_telemetry_file via setConfigString too
			if (has("\"cmd\":\"setConfigInt\"")) {
				std::string component, key;
				if (extractStringField(req, "component", component) && 
					extractStringField(req, "key", key)) {
					// Extract integer value - look for "value":123 pattern
					size_t valuePos = req.find("\"value\":");
					if (valuePos != std::string::npos) {
						size_t valueStart = valuePos + 8;
						while (valueStart < req.size() && (req[valueStart] == ' ' || req[valueStart] == '\t')) valueStart++;
						size_t valueEnd = valueStart;
						while (valueEnd < req.size() && (req[valueEnd] == '-' || std::isdigit((unsigned char)req[valueEnd]))) valueEnd++;
						if (valueEnd > valueStart) {
							int ivalue = std::stoi(req.substr(valueStart, valueEnd - valueStart));
							app_set_config_int(component.c_str(), key.c_str(), ivalue);
						}
					}
				}
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"setConfigFloat\"")) {
				std::string component, key;
				if (extractStringField(req, "component", component) &&
					extractStringField(req, "key", key)) {
					// Extract float value - look for "value":123.45 pattern
					size_t valuePos = req.find("\"value\":");
					if (valuePos != std::string::npos) {
						size_t valueStart = valuePos + 8;
						while (valueStart < req.size() && (req[valueStart] == ' ' || req[valueStart] == '\t')) valueStart++;
						size_t valueEnd = valueStart;
						while (valueEnd < req.size() && (req[valueEnd] == '-' || req[valueEnd] == '.' || std::isdigit((unsigned char)req[valueEnd]))) valueEnd++;
						if (valueEnd > valueStart) {
							float fvalue = std::stof(req.substr(valueStart, valueEnd - valueStart));
							app_set_config_float(component.c_str(), key.c_str(), fvalue);
						}
					}
				}
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"setConfigStringVec\"")) {
				std::string component, key;
				if (extractStringField(req, "component", component) &&
					extractStringField(req, "key", key)) {
					// Parse very simple JSON array of strings: "values":["a","b"]
					size_t p = req.find("\"values\":");
					std::vector<std::string> values;
					if (p != std::string::npos) {
						size_t lb = req.find('[', p);
						size_t rb = req.find(']', lb == std::string::npos ? p : lb);
						if (lb != std::string::npos && rb != std::string::npos && rb > lb) {
							std::string arr = req.substr(lb+1, rb-lb-1);
							// Split on "," and unquote entries
							size_t start = 0;
							while (start < arr.size()) {
								while (start < arr.size() && (arr[start] == ' ' || arr[start] == '\t' || arr[start] == ',')) start++;
								if (start >= arr.size()) break;
								if (arr[start] == '"') {
									size_t end = arr.find('"', start+1);
									if (end != std::string::npos) {
										values.push_back(arr.substr(start+1, end-(start+1)));
										start = end + 1;
									} else break;
								} else {
									break;
								}
							}
						}
					}
					app_set_config_string_vec(component.c_str(), key.c_str(), values);
				}
				callback->Success(app_get_state_json());
				return true;
			}
			if (has("\"cmd\":\"loadCarConfig\"")) {
				std::string carName;
				if (extractStringField(req, "carName", carName)) {
					if (g_cfg.loadCarConfig(carName)) {
						app_handleConfigChange_external();
						callback->Success(app_get_state_json());
					} else {
						callback->Failure(400, "failed to load car config");
					}
				} else {
					callback->Failure(400, "carName required");
				}
				return true;
			}
			if (has("\"cmd\":\"saveCarConfig\"")) {
				std::string carName;
				if (extractStringField(req, "carName", carName)) {
					if (g_cfg.saveCarConfig(carName)) {
						callback->Success(app_get_state_json());
					} else {
						callback->Failure(400, "failed to save car config");
					}
				} else {
					callback->Failure(400, "carName required");
				}
				return true;
			}
			if (has("\"cmd\":\"copyCarConfig\"")) {
				std::string fromCar, toCar;
				if (extractStringField(req, "fromCar", fromCar) &&
					extractStringField(req, "toCar", toCar)) {
					if (g_cfg.copyConfigToCar(fromCar, toCar)) {
						callback->Success(app_get_state_json());
					} else {
						callback->Failure(400, "failed to copy car config");
					}
				} else {
					callback->Failure(400, "fromCar and toCar required");
				}
				return true;
			}
			if (has("\"cmd\":\"deleteCarConfig\"")) {
				std::string carName;
				if (extractStringField(req, "carName", carName)) {
					if (g_cfg.deleteCarConfig(carName)) {
						callback->Success(app_get_state_json());
					} else {
						callback->Failure(400, "failed to delete car config");
					}
				} else {
					callback->Failure(400, "carName required");
				}
				return true;
			}
			if (has("\"cmd\":\"resetConfig\"")) {
				// Remove config.json to reset to defaults, then reload and propagate
				DeleteFileW(L"config.json");
				g_cfg.load();
				app_handleConfigChange_external();
				callback->Success(app_get_state_json());
				return true;
			}
			// SaveSettings supports a small set of keys sent from UI (currently performance_mode_30hz, launch_at_startup)
			if (has("\"cmd\":\"saveSettings\"")) {
				bool perf30 = false; (void)extractBoolField(req, "performance_mode_30hz", perf30);
				app_set_config_bool("General", "performance_mode_30hz", perf30);
				bool startup = false; (void)extractBoolField(req, "launch_at_startup", startup);
				app_set_startup_enabled(startup);
				callback->Success(app_get_state_json());
				return true;
			}
			callback->Failure(400, "unknown command");
			return true;
		}

		void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
			CefRefPtr<CefFrame> frame,
			int64_t query_id) override
		{
			// no-op
		}
		IMPLEMENT_REFCOUNTING(QueryHandler);
	};

	class SimpleApp : public CefApp, public CefBrowserProcessHandler, public CefRenderProcessHandler {
	public:
		void OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override {
			command_line->AppendSwitch("disable-gpu");
			command_line->AppendSwitch("disable-gpu-compositing");
			command_line->AppendSwitchWithValue("use-angle", "d3d11");
			command_line->AppendSwitchWithValue("disable-features", "WebUSB,WebHID,WebSerial,UseGCMFromChrome,WebBluetooth");
			command_line->AppendSwitch("disable-background-networking");
			// Allow fetch/XHR to read local files like ui/version.json when loaded from file://
			command_line->AppendSwitch("allow-file-access-from-files");
		}
		CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
		CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

		void OnContextInitialized() override {
			CefMessageRouterConfig cfg;
			g_router = CefMessageRouterBrowserSide::Create(cfg);
			g_router->AddHandler(new QueryHandler(), false);
		}

		// Renderer-side to expose window.cefQuery
		void OnContextCreated(CefRefPtr<CefBrowser> browser,
			CefRefPtr<CefFrame> frame,
			CefRefPtr<CefV8Context> context) override
		{
			if (!g_router_renderer.get()) {
				CefMessageRouterConfig cfg;
				g_router_renderer = CefMessageRouterRendererSide::Create(cfg);
			}
			g_router_renderer->OnContextCreated(browser, frame, context);
		}

		void OnContextReleased(CefRefPtr<CefBrowser> browser,
			CefRefPtr<CefFrame> frame,
			CefRefPtr<CefV8Context> context) override
		{
			if (g_router_renderer.get())
				g_router_renderer->OnContextReleased(browser, frame, context);
		}

		bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
			CefRefPtr<CefFrame> frame,
			CefProcessId source_process,
			CefRefPtr<CefProcessMessage> message) override
		{
			if (g_router_renderer.get() && g_router_renderer->OnProcessMessageReceived(browser, frame, source_process, message))
				return true;
			return false;
		}

		IMPLEMENT_REFCOUNTING(SimpleApp);
	};

	CefRefPtr<SimpleClient> g_client;
	CefRefPtr<SimpleApp> g_app;
}

static bool g_cefInitialized = false;

static std::wstring getExecutableDirW_CEF()
{
	wchar_t path[MAX_PATH] = {0};
	GetModuleFileNameW(NULL, path, MAX_PATH);
	wchar_t *last = wcsrchr(path, L'\\');
	if (last) *last = 0;
	return std::wstring(path);
}

static std::string toFileUrl(const std::wstring &pathW)
{
	std::wstring p = pathW;
	for (auto &ch : p) { if (ch == L'\\') ch = L'/'; }
	return std::string("file:///") + std::string(CefString(p));
}

static void createParentWindow()
{
	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.lpfnWndProc = GuiWndProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = kGuiWndClass;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
	wc.hIconSm = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
	RegisterClassExW(&wc);

	RECT r = { 100, 100, 100 + 1920, 100 + 1080 };
	AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
	g_parentHwnd = CreateWindowExW(0, kGuiWndClass, L"iFL03", WS_OVERLAPPEDWINDOW,
		r.left, r.top, r.right - r.left, r.bottom - r.top, NULL, NULL, GetModuleHandle(NULL), NULL);
	
	ShowWindow(g_parentHwnd, SW_SHOW);
	UpdateWindow(g_parentHwnd);
}

bool cefInitialize()
{
	if (g_cefInitialized) return true;

	// Sub-processes will execute this block if using the same exe path
	CefMainArgs main_args(GetModuleHandle(NULL));
	g_app = new SimpleApp();
	int exit_code = CefExecuteProcess(main_args, g_app.get(), nullptr);
	if (exit_code >= 0) {
		ExitProcess(static_cast<UINT>(exit_code));
	}

	CefSettings settings;
	settings.no_sandbox = true;
	settings.external_message_pump = true;
	settings.log_severity = LOGSEVERITY_DISABLE;

	// Set a cache path so cookies/local storage persist
	const std::wstring exeDir = getExecutableDirW_CEF();
	CefString(&settings.cache_path) = exeDir + L"\\cef_cache";

	g_client = new SimpleClient();

	g_cefInitialized = CefInitialize(main_args, settings, g_app.get(), nullptr);
	return g_cefInitialized;
}

void cefCreateMainWindow()
{
	if (!g_cefInitialized) return;
	if (!g_client.get()) g_client = new SimpleClient();

    if (!g_parentHwnd)
        createParentWindow();

    if (!g_parentHwnd)
        return;

    CefWindowInfo window_info;
    RECT rc = {0,0,0,0};
    if (!GetClientRect(g_parentHwnd, &rc)) { rc.left = rc.top = 0; rc.right = 1280; rc.bottom = 720; }
	CefRect rect(0, 0, rc.right - rc.left, rc.bottom - rc.top);
	window_info.SetAsChild(g_parentHwnd, rect);

	CefBrowserSettings browser_settings;
	browser_settings.chrome_status_bubble = STATE_DISABLED;

	std::wstring exeDir = getExecutableDirW_CEF();
	std::wstring candidateRepo = exeDir + L"\\..\\..\\ui\\index.html";
	std::wstring candidateLocal = exeDir + L"\\ui\\index.html";

	auto fileExists = [](const std::wstring& p) -> bool {
		DWORD a = GetFileAttributesW(p.c_str());
		return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
	};

	std::string url = "about:blank";
	if (fileExists(candidateRepo)) {
		url = toFileUrl(candidateRepo);
	} else if (fileExists(candidateLocal)) {
		url = toFileUrl(candidateLocal);
	}

	CefBrowserHost::CreateBrowser(window_info, g_client.get(), url, browser_settings, nullptr, nullptr);
}

void cefDoMessageLoopWork()
{
	if (!g_cefInitialized) return;
	CefDoMessageLoopWork();
}

void cefExecuteScript(const char* jsUtf8)
{
	if (!g_cefInitialized || !g_browser.get() || !jsUtf8) return;
	CefString src = CefString(jsUtf8);
	g_browser->GetMainFrame()->ExecuteJavaScript(src, "about:blank", 0);
}

void cefShutdown()
{
	if (!g_cefInitialized) return;
	CefShutdown();
	g_cefInitialized = false;
}

#else

// Stubs when CEF is not enabled/linked

bool cefInitialize()
{
	return false;
}

void cefCreateMainWindow()
{
}

void cefDoMessageLoopWork()
{
}

void cefExecuteScript(const char* jsUtf8)
{
}

void cefShutdown()
{
}

#endif 