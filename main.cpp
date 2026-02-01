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

#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"dcomp.lib")
#pragma comment(lib,"dwrite.lib")


#include <string>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <atomic>
#include "irsdk/irsdk_client.h"
#include <thread>
#include <chrono>
#include <exception>
#include "iracing.h"
#include "Config.h"
#include "Logger.h"
#include "OverlayCover.h"
#include "OverlayRelative.h"
#include "OverlayInputs.h"
#include "OverlayStandings.h"
#include "OverlayDebug.h"
#include "OverlayDDU.h"
#include "OverlayWeather.h"
#include "OverlayFlags.h"
#include "OverlayDelta.h"
#include "OverlayRadar.h"
#include "OverlayTrack.h"
#include "OverlayFuel.h"
#include "OverlayTire.h"
#include "OverlayPit.h"
#include "OverlayTraffic.h"
#include "GuiCEF.h"
#include "AppControl.h"
#include "preview_mode.h"

// Helper: determine if this process is a CEF sub-process (renderer/gpu/utility)
static bool isCefSubprocess()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool isSub = false;
    if (argv)
    {
        for (int i = 0; i < argc; ++i)
        {
            if (wcsncmp(argv[i], L"--type=", 7) == 0 || wcscmp(argv[i], L"--type") == 0)
            {
                isSub = true;
                break;
            }
        }
        LocalFree(argv);
    }
    return isSub;
}

// Bring an already running main window to the foreground if we can find it
static void focusExistingMainWindow()
{
    HWND hwnd = FindWindowW(L"iFL03GuiWindow", NULL);
    if (hwnd)
    {
        ShowWindow(hwnd, SW_SHOWNORMAL);
        SetForegroundWindow(hwnd);
    }
}

enum class Hotkey
{
    UiEdit,
    Standings,
    DDU,
    Fuel,
    Tire,
    Inputs,
    Relative,
    Cover,
    Weather,
    Flags,
    Delta, 
    Radar, 
    Track,
    Pit,
    Traffic
};

static void registerHotkeys()
{
    UnregisterHotKey( NULL, (int)Hotkey::UiEdit );
    UnregisterHotKey( NULL, (int)Hotkey::Standings );
    UnregisterHotKey( NULL, (int)Hotkey::DDU );
    UnregisterHotKey( NULL, (int)Hotkey::Fuel );
    UnregisterHotKey( NULL, (int)Hotkey::Tire );
    UnregisterHotKey( NULL, (int)Hotkey::Inputs );
    UnregisterHotKey( NULL, (int)Hotkey::Relative );
    UnregisterHotKey( NULL, (int)Hotkey::Cover );
    UnregisterHotKey( NULL, (int)Hotkey::Weather );
    UnregisterHotKey( NULL, (int)Hotkey::Flags );
    UnregisterHotKey( NULL, (int)Hotkey::Delta );
    UnregisterHotKey( NULL, (int)Hotkey::Radar );
    UnregisterHotKey( NULL, (int)Hotkey::Track );
    UnregisterHotKey( NULL, (int)Hotkey::Pit );
    UnregisterHotKey( NULL, (int)Hotkey::Traffic );
    // Custom overlays can add more hotkeys by extending this enum & list

    UINT vk, mod;

    if( parseHotkey( g_cfg.getString("General","ui_edit_hotkey","alt-j"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::UiEdit, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayStandings","toggle_hotkey","ctrl+1"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Standings, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayDDU","toggle_hotkey","ctrl+2"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::DDU, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayFuel","toggle_hotkey","ctrl+2"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Fuel, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayTire","toggle_hotkey","ctrl+3"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Tire, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayInputs","toggle_hotkey","ctrl+4"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Inputs, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayRelative","toggle_hotkey","ctrl+5"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Relative, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayCover","toggle_hotkey","ctrl+6"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Cover, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayWeather","toggle_hotkey","ctrl+7"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Weather, mod, vk );
    
    if( parseHotkey( g_cfg.getString("OverlayFlags","toggle_hotkey","ctrl+8"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Flags, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayDelta","toggle_hotkey","ctrl+9"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Delta, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayRadar","toggle_hotkey","ctrl+0"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Radar, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayTrack","toggle_hotkey","ctrl+shift+1"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Track, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayPit","toggle_hotkey","ctrl+shift+2"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Pit, mod, vk );
    
    if( parseHotkey( g_cfg.getString("OverlayTraffic","toggle_hotkey","ctrl+shift+4"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Traffic, mod, vk );
    // Optional: user can bind OverlayTire via config; reuse General/ui to avoid extra enum churn
}

static void handleConfigChange( std::vector<Overlay*> overlays, ConnectionStatus status )
{
    registerHotkeys();

    ir_handleConfigChange();

    const bool replaySession = ir_session.isReplay;

    for( Overlay* o : overlays )
    {
        bool overlayEnabled = g_cfg.getBool(o->getName(),"enabled",true);
        
        // Check show_in_menu and show_in_race settings
        bool showInMenu = g_cfg.getBool(o->getName(), "show_in_menu", true);
        bool showInRace = g_cfg.getBool(o->getName(), "show_in_race", true);
        bool showInReplay = g_cfg.getBool(o->getName(), "show_in_replay", true);
        
        bool connectionAllows = false;
        
        if (status == ConnectionStatus::DRIVING) {
            // Replay *session* should use the "show_in_replay" toggle.
            // Live-session replay playback should NOT; treat it like normal driving.
            connectionAllows = replaySession ? showInReplay : showInRace;
        } else if (status == ConnectionStatus::CONNECTED) {
            connectionAllows = showInMenu && o->canEnableWhileNotDriving();
        } else if (status == ConnectionStatus::DISCONNECTED) {
            // When iRacing is disconnected, overlays are always hidden (except preview mode).
            connectionAllows = false;
        }
        
        // In preview mode, show enabled overlays regardless of connection status
        bool shouldEnable = overlayEnabled && (preview_mode_get() || connectionAllows);
        
        o->enable(shouldEnable);
        o->configChanged();
    }
}

static void giveFocusToIracing()
{
    HWND hwnd = FindWindow( "SimWinClass", NULL );
    if( hwnd )
        SetForegroundWindow( hwnd );
}

static void setWorkingDirectoryToExe()
{
    wchar_t path[MAX_PATH] = {};
    if( GetModuleFileNameW( NULL, path, MAX_PATH ) )
    {
        // strip filename
        for( int i=(int)wcslen(path)-1; i>=0; --i )
        {
            if( path[i] == L'\\' || path[i] == L'/' ) { path[i] = 0; break; }
        }
        SetCurrentDirectoryW( path );
    }
}

static std::string formatLastErrorMessage(DWORD err)
{
    if (err == 0)
        return {};

    char buffer[256] = {};
    DWORD len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                               nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               buffer, sizeof(buffer), nullptr);
    if (len == 0)
        return std::to_string(err);

    return std::to_string(err) + ": " + std::string(buffer, len);
}

static void logIfLastError(const char* context)
{
    DWORD err = GetLastError();
    if (err)
        Logger::instance().logError(std::string(context) + " failed: " + formatLastErrorMessage(err));
}

static std::atomic<DWORD> g_lastHeartbeatTick{0};
static std::atomic<bool> g_watchdogRunning{false};
static std::atomic<bool> g_watchdogStalled{false};
static DWORD g_lastConfigReloadLogTick = 0;

int main()
{
    Logger::instance().logInfo("iFL03 starting");

    // Single-instance guard for the main/browser process only (skip CEF sub-processes)
    HANDLE singleInstanceMutex = NULL;
    if (!isCefSubprocess())
    {
        singleInstanceMutex = CreateMutexW(NULL, TRUE, L"Global\\iFL03_SingleInstance_Mutex");
        if (singleInstanceMutex && GetLastError() == ERROR_ALREADY_EXISTS)
        {
            focusExistingMainWindow();
            MessageBoxW(NULL, L"iFL03 is already running. Please first close the existing instance and try again.", L"iFL03", MB_OK | MB_ICONINFORMATION);
            Logger::instance().logWarning("Second instance detected; exiting");
            return 0;
        }
        if (!singleInstanceMutex)
            logIfLastError("CreateMutexW");
    }

    // Bump priority up so we get time from the sim
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    // Ensure config.json is read/written next to the executable
    setWorkingDirectoryToExe();

    {
        wchar_t logPath[MAX_PATH] = {};
        if (GetModuleFileNameW(NULL, logPath, MAX_PATH))
        {
            for (int i = (int)wcslen(logPath) - 1; i >= 0; --i)
            {
                if (logPath[i] == L'\\' || logPath[i] == L'/')
                {
                    logPath[i + 1] = 0;
                    break;
                }
            }
            wcscat_s(logPath, L"logs.txt");
            Logger::instance().init(logPath);
        }
    }

    std::set_terminate([]
    {
        Logger::instance().logError("std::terminate invoked");
        Logger::instance().flush();
        std::abort();
    });

    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* info) -> LONG
    {
        DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
        char buf[9];
        sprintf_s(buf, "%08X", code);
        Logger::instance().logError(std::string("Unhandled exception code 0x") + buf);
        Logger::instance().flush();
        return EXCEPTION_EXECUTE_HANDLER;
    });

    SetConsoleCtrlHandler([](DWORD signal) -> BOOL
    {
        Logger::instance().logWarning("Console control signal " + std::to_string(signal));
        Logger::instance().flush();
        return FALSE;
    }, TRUE);

    g_lastHeartbeatTick = GetTickCount();
    g_watchdogRunning = true;
    std::thread([]
    {
        while (g_watchdogRunning.load())
        {
            DWORD now = GetTickCount();
            DWORD last = g_lastHeartbeatTick.load();
            bool stalled = (now - last) > 5000;
            bool wasStalled = g_watchdogStalled.load();
            if (stalled && !wasStalled)
            {
                g_watchdogStalled = true;
                Logger::instance().logWarning("Main loop heartbeat stalled for " + std::to_string(now - last) + " ms");
                Logger::instance().flush();
            }
            else if (!stalled && wasStalled)
            {
                g_watchdogStalled = false;
                Logger::instance().logInfo("Main loop heartbeat recovered");
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }).detach();

#ifdef IFL03_USE_CEF
    const bool cefOk = cefInitialize();
    if( cefOk )
    {
        Logger::instance().logInfo("CEF initialized successfully");
        cefCreateMainWindow();
    }
    else
    {
        Logger::instance().logError("cefInitialize failed");
    }
#endif

    // Load the config and watch it for changes
    if (!g_cfg.load())
        Logger::instance().logWarning("Initial config load failed");

    // Restore last active car config (if any) so the selected config persists across restarts.
    // Stored as plain UTF-8 text in the working directory.
    {
        std::string lastActive;
        if (loadFile("active_car_config.txt", lastActive))
        {
            auto trim = [](std::string& s) {
                auto isWs = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
                size_t b = 0;
                while (b < s.size() && isWs((unsigned char)s[b])) b++;
                size_t e = s.size();
                while (e > b && isWs((unsigned char)s[e - 1])) e--;
                s = s.substr(b, e - b);
            };
            trim(lastActive);
            if (!lastActive.empty())
            {
                if (!g_cfg.loadCarConfig(lastActive))
                {
                    Logger::instance().logWarning("Failed to restore active car config: " + lastActive);
                    // Keep default config.json if restore fails
                }
            }
        }
    }
    g_cfg.watchForChanges();

    // Initialize preview mode
    preview_mode_init();

    // Register global hotkeys
    registerHotkeys();

    printf("\n====================================================================================\n");
    printf("Welcome to iFL03! This app provides a few simple overlays for iRacing.\n\n");
    printf("Special thanks to lespalt for creating iRon, the original version of this app.\n\n");
    printf("NOTE: The overlays can be activated in the menu or in the race via the \'config.json\' file.\n\n");
    printf("Current hotkeys:\n");
    printf("    Move and resize overlays:     %s\n", g_cfg.getString("General","ui_edit_hotkey","").c_str() );
    printf("    Toggle standings overlay:     %s\n", g_cfg.getString("OverlayStandings","toggle_hotkey","").c_str() );
    printf("    Toggle DDU overlay:           %s\n", g_cfg.getString("OverlayDDU","toggle_hotkey","").c_str() );
    printf("    Toggle Fuel overlay:          %s\n", g_cfg.getString("OverlayFuel","toggle_hotkey","").c_str() );
    printf("    Toggle tire overlay:          %s\n", g_cfg.getString("OverlayTire","toggle_hotkey","").c_str() );
    printf("    Toggle inputs overlay:        %s\n", g_cfg.getString("OverlayInputs","toggle_hotkey","").c_str() );
    printf("    Toggle relative overlay:      %s\n", g_cfg.getString("OverlayRelative","toggle_hotkey","").c_str() );
    printf("    Toggle cover overlay:         %s\n", g_cfg.getString("OverlayCover","toggle_hotkey","").c_str() );
    printf("    Toggle weather overlay:       %s\n", g_cfg.getString("OverlayWeather","toggle_hotkey","").c_str() );
    printf("    Toggle flags overlay:         %s\n", g_cfg.getString("OverlayFlags","toggle_hotkey","").c_str() );
    printf("    Toggle delta overlay:         %s\n", g_cfg.getString("OverlayDelta","toggle_hotkey","").c_str() );
    printf("    Toggle radar overlay:         %s\n", g_cfg.getString("OverlayRadar","toggle_hotkey","").c_str() );
    printf("    Toggle track overlay:         %s\n", g_cfg.getString("OverlayTrack","toggle_hotkey","").c_str() );
    printf("    Toggle pit overlay:           %s\n", g_cfg.getString("OverlayPit","toggle_hotkey","").c_str() );
    printf("    Toggle traffic overlay:       %s\n", g_cfg.getString("OverlayTraffic","toggle_hotkey","").c_str() );
    printf("\niFL03 will generate a file called \'config.json\' in its current directory. This file\n"\
           "stores your settings. You can edit the file at any time, even while iFL03 is running,\n"\
           "to customize your overlays and hotkeys.\n\n");
    printf("To exit iFL03, simply close this console window.\n\n");
    printf("For the best experience use the GUI format of the application.\n\n");
    printf("For the latest version of the console application or to submit bug reports, go to:\n\n        https://github.com/lespalt/iRon\n\n");
    printf("For the latest version of the GUI application, go to:\n\n        https://github.com/SemSodermans31/iFL03\n\n");
    printf("\nHappy Racing!\n");
    printf("====================================================================================\n\n");

    // Preload car brand icons once
    std::map<std::string, IWICFormatConverter*> carBrandIcons;
    const bool carBrandIconsLoaded = loadCarBrandIcons(carBrandIcons);

    // Create overlays
    std::vector<Overlay*> overlays;
    overlays.push_back( new OverlayCover() );
    overlays.push_back( new OverlayRelative() );
    overlays.push_back( new OverlayInputs() );
    {
        auto* st = new OverlayStandings();
        st->setCarBrandIcons(carBrandIcons, carBrandIconsLoaded);
        overlays.push_back(st);
    }
    overlays.push_back( new OverlayDDU() );
    overlays.push_back( new OverlayFuel() );
    overlays.push_back( new OverlayTire() );
    overlays.push_back( new OverlayWeather() );
    overlays.push_back( new OverlayFlags() );
    overlays.push_back( new OverlayDelta() );
#ifdef IFL03_USE_CEF
    overlays.push_back( new OverlayRadar() );
#else
    overlays.push_back( new OverlayRadar() );
#endif
    overlays.push_back( new OverlayTrack() );
    overlays.push_back( new OverlayPit() );
    overlays.push_back( new OverlayTraffic() );
#ifdef _DEBUG
    overlays.push_back( new OverlayDebug() );
#endif

    ConnectionStatus  status   = ConnectionStatus::UNKNOWN;
    bool              uiEdit   = false;
    unsigned          frameCnt = 0;
    bool              quitRequested = false;

    // Expose pointers to bridge
    app_register_bridge(&overlays, &uiEdit, &status, &handleConfigChange);

    while( true )
    {
        ConnectionStatus prevStatus       = status;
        SessionType      prevSessionType  = ir_session.sessionType;
        int              prevSubsessionId = ir_session.subsessionId;
        int              prevStatusID     = irsdkClient::instance().getStatusID();
        bool             prevHasDriver    = ir_hasValidDriver();

        // Refresh connection and session info
        status = ir_tick();
        const bool nowHasDriver = ir_hasValidDriver();
        const int  nowStatusID  = irsdkClient::instance().getStatusID();
        if( status != prevStatus )
        {
            Logger::instance().logInfo(std::string("Connection status changed to ") + ConnectionStatusStr[(int)status]);
            if( status == ConnectionStatus::DISCONNECTED )
                printf("Waiting for iRacing connection...\n");
            else
                printf("iRacing connected (%s)\n", ConnectionStatusStr[(int)status]);

            // Enable user-selected overlays, but only if we're driving
            handleConfigChange( overlays, status );

#ifdef IFL03_USE_CEF
            // Push status to GUI
            if (cefOk) {
                std::string js = std::string("window.onIFL03State && window.onIFL03State(") + app_get_state_json() + ");";
                cefExecuteScript(js.c_str());
            }
#endif
        }

        if( ir_session.sessionType != prevSessionType || ir_session.subsessionId != prevSubsessionId )
        {
            for( Overlay* o : overlays )
                o->sessionChanged();
        }

        // When IRSDK header/var table changes, cached state across overlays can become stale.
        // Also, when the driver index becomes valid after connection, overlays should reset once.
        if( nowStatusID != prevStatusID || (nowHasDriver && !prevHasDriver) )
        {
            for( Overlay* o : overlays )
                o->sessionChanged();
        }

        // Update/render overlays
        {
            // Avoid rendering during the brief window right after connect/session-load where
            // IRSDK/YAML can be incomplete and values may temporarily be garbage.
            static int stableFrames = 0;
            static int lastStatusIDForStability = -1;
            const bool isConnectedToSim = (status == ConnectionStatus::CONNECTED || status == ConnectionStatus::DRIVING);

            if (!isConnectedToSim) {
                stableFrames = 0;
                lastStatusIDForStability = nowStatusID;
            } else if (nowStatusID != lastStatusIDForStability) {
                // IRSDK header/var-table changed -> reset stability window
                stableFrames = 0;
                lastStatusIDForStability = nowStatusID;
            } else if (nowHasDriver) {
                stableFrames = std::min(stableFrames + 1, 9999);
            } else {
                stableFrames = 0;
            }

            // Once the sim is connected and we have a valid driver and the IRSDK header has been stable
            // for a short moment, allow overlays to update both in menu (CONNECTED) and in-car (DRIVING).
            const bool allowOverlayUpdates = preview_mode_get() || (isConnectedToSim && nowHasDriver && stableFrames >= 15);
            if( !allowOverlayUpdates )
            {
                // Keep pumping messages / config changes but don't update overlays yet.
            }
            else if( !g_cfg.getBool("General", "performance_mode_30hz", false) )
            {
                // Update enabled overlays every frame, roughly every 16ms (~60Hz)
                for( Overlay* o : overlays )
                {
                    if( !o->isEnabled() )
                        continue;
                    o->update();
                }
            }
            else
            {
                // Update half of the enabled overlays on even frames and the other half on odd frames (~30Hz per overlay)
                int enabledIdx = 0;
                for( Overlay* o : overlays )
                {
                    if( !o->isEnabled() )
                        continue;

                    if( (enabledIdx & 1) == (frameCnt & 1) )
                        o->update();

                    enabledIdx++;
                }
            }
        }

        // Watch for config change signal
        if( g_cfg.hasChanged() )
        {
            if (!g_cfg.load())
            {
                Logger::instance().logError("Config reload failed");
            }
            else
            {
                DWORD nowTick = GetTickCount();
                if (nowTick - g_lastConfigReloadLogTick > 2000)
                {
                    Logger::instance().logInfo("Config reloaded from disk");
                    g_lastConfigReloadLogTick = nowTick;
                }
                handleConfigChange( overlays, status );
            }
#ifdef IFL03_USE_CEF
            if (cefOk) {
                std::string js = std::string("window.onIFL03State && window.onIFL03State(") + app_get_state_json() + ");";
                cefExecuteScript(js.c_str());
            }
#endif
        }

        // Message pump
        MSG msg = {};
        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if( msg.message == WM_QUIT )
            {
                Logger::instance().logInfo("WM_QUIT received");
                quitRequested = true;
                break;
            }
            // Handle hotkeys
            if( msg.message == WM_HOTKEY )
            {
                if( msg.wParam == (int)Hotkey::UiEdit )
                {
                    uiEdit = !uiEdit;
                    for( Overlay* o : overlays )
                        o->enableUiEdit( uiEdit );

                    if( !uiEdit )
                        giveFocusToIracing();
                }
                else
                {
                    switch( msg.wParam )
                    {
                    case (int)Hotkey::Standings:
                        g_cfg.setBool( "OverlayStandings", "enabled", !g_cfg.getBool("OverlayStandings","enabled",true) );
                        break;
                    case (int)Hotkey::DDU:
                        g_cfg.setBool( "OverlayDDU", "enabled", !g_cfg.getBool("OverlayDDU","enabled",true) );
                        break;
                    case (int)Hotkey::Fuel:
                        g_cfg.setBool( "OverlayFuel", "enabled", !g_cfg.getBool("OverlayFuel","enabled",true) );
                        break;
                    case (int)Hotkey::Tire:
                        g_cfg.setBool( "OverlayTire", "enabled", !g_cfg.getBool("OverlayTire","enabled",true) );
                        break;
                    case (int)Hotkey::Inputs:
                        g_cfg.setBool( "OverlayInputs", "enabled", !g_cfg.getBool("OverlayInputs","enabled",true) );
                        break;
                    case (int)Hotkey::Relative:
                        g_cfg.setBool( "OverlayRelative", "enabled", !g_cfg.getBool("OverlayRelative","enabled",true) );
                        break;
                    case (int)Hotkey::Cover:
                        g_cfg.setBool( "OverlayCover", "enabled", !g_cfg.getBool("OverlayCover","enabled",true) );
                        break;
                    case (int)Hotkey::Weather:
                        g_cfg.setBool( "OverlayWeather", "enabled", !g_cfg.getBool("OverlayWeather","enabled",true) );
                        break;
                    case (int)Hotkey::Flags:
                        g_cfg.setBool( "OverlayFlags", "enabled", !g_cfg.getBool("OverlayFlags","enabled",true) );
                        break;
                    case (int)Hotkey::Delta:
                        g_cfg.setBool( "OverlayDelta", "enabled", !g_cfg.getBool("OverlayDelta","enabled",true) );
                        break;
                    case (int)Hotkey::Radar:
                        g_cfg.setBool( "OverlayRadar", "enabled", !g_cfg.getBool("OverlayRadar","enabled",true) );
                        break;
                    case (int)Hotkey::Track:
                        g_cfg.setBool( "OverlayTrack", "enabled", !g_cfg.getBool("OverlayTrack","enabled",true) );
                        break;
                    case (int)Hotkey::Pit:
                        g_cfg.setBool( "OverlayPit", "enabled", !g_cfg.getBool("OverlayPit","enabled",true) );
                        break;
                    case (int)Hotkey::Traffic:
                        g_cfg.setBool( "OverlayTraffic", "enabled", !g_cfg.getBool("OverlayTraffic","enabled",true) );
                        break;
                    default: // no-op to avoid unannotated fallthrough warning
                        break;
                    }
                    
                    if (!g_cfg.save())
                        Logger::instance().logError("Failed to save config after hotkey toggle");
                    handleConfigChange( overlays, status );
                }
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);            
        }

        g_lastHeartbeatTick = GetTickCount();

        if( quitRequested )
            break;

#ifdef IFL03_USE_CEF
        // Allow CEF to process pending work when using the external pump
        cefDoMessageLoopWork();
#endif

        frameCnt++;
    }

    Logger::instance().logInfo("iFL03 shutting down");
    g_watchdogRunning = false;
    Logger::instance().flush();

    for( Overlay* o : overlays )
        delete o;
    overlays.clear();

    // Release car brand icon converters (loadCarBrandIcons() AddRef's them into this map).
    for (auto& it : carBrandIcons)
    {
        if (it.second) it.second->Release();
    }
    carBrandIcons.clear();

#ifdef IFL03_USE_CEF
    cefShutdown();
    Logger::instance().logInfo("CEF shutdown complete");
#endif

    if (singleInstanceMutex)
        CloseHandle(singleInstanceMutex);

    Logger::instance().logInfo("iFL03 shutting down cleanly");
    Logger::instance().flush();

    return 0;
}
