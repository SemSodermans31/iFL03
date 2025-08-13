#include "GuiCEF.h"

#ifdef IRON_USE_CEF
#include <windows.h>
#include <string>
#include <utility>
#include <cstdint>
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_browser.h"
#include "include/cef_sandbox_win.h"
#include "include/cef_v8.h"
#include "include/base/cef_build.h"
#include "include/base/cef_ref_counted.h"
#include "include/wrapper/cef_message_router.h"
#include "AppControl.h"

namespace {
	static const wchar_t* kGuiWndClass = L"iRonGuiWindow";
	HWND g_parentHwnd = NULL;
	CefRefPtr<CefBrowser> g_browser;
	CefRefPtr<CefMessageRouterBrowserSide> g_router;
	CefRefPtr<CefMessageRouterRendererSide> g_router_renderer;

	LRESULT CALLBACK GuiWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		switch (msg)
		{
		case WM_SIZE:
		{
			if (g_browser.get())
			{
				HWND child = g_browser->GetHost()->GetWindowHandle();
				RECT rc; GetClientRect(hwnd, &rc);
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

	class SimpleClient : public CefClient, public CefLifeSpanHandler, public CefDisplayHandler, public CefRequestHandler {
	public:
		SimpleClient() {}

		CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
		CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
		CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }

		bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
			CefRefPtr<CefFrame> frame,
			CefRefPtr<CefRequest> request,
			bool user_gesture,
			bool is_redirect) override
		{
			if (g_router.get()) g_router->OnBeforeBrowse(browser, frame);
			return false;
		}

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
			if (has("\"cmd\":\"setUiEdit\"")) {
				bool on = false; (void)extractBoolField(req, "on", on);
				app_set_ui_edit(on);
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

static std::wstring getExecutableDirW()
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
	RegisterClassExW(&wc);

	RECT r = { 100, 100, 100 + 800, 100 + 600 };
	AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW);
	g_parentHwnd = CreateWindowExW(WS_EX_APPWINDOW, kGuiWndClass, L"iRon GUI", WS_OVERLAPPEDWINDOW,
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
	const std::wstring exeDir = getExecutableDirW();
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

	CefWindowInfo window_info;
	RECT rc; GetClientRect(g_parentHwnd, &rc);
	CefRect rect(0, 0, rc.right - rc.left, rc.bottom - rc.top);
	window_info.SetAsChild(g_parentHwnd, rect);

	CefBrowserSettings browser_settings;

	// Try to load local UI if present, else fall back to about:blank
	std::wstring exeDir = getExecutableDirW();
	std::wstring uiPath = exeDir + L"\\ui\\index.html";
	DWORD attrs = GetFileAttributesW(uiPath.c_str());
	std::string url = (attrs != INVALID_FILE_ATTRIBUTES) ? toFileUrl(uiPath) : std::string("about:blank");

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
	return false; // Not initialized until CEF is linked
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