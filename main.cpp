/*
MIT License

Copyright (c) 2021-2022 L. E. Spalt

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


#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <windows.h>
#include "iracing.h"
#include "Config.h"
#include "OverlayCover.h"
#include "OverlayRelative.h"
#include "OverlayInputs.h"
#include "OverlayStandings.h"
#include "OverlayDebug.h"
#include "OverlayDDU.h"
#include "OverlayWeather.h"
#include "OverlayFlags.h"
#include "GuiCEF.h"
#include "AppControl.h"
#include "preview_mode.h"

enum class Hotkey
{
    UiEdit,
    Standings,
    DDU,
    Inputs,
    Relative,
    Cover,
    Weather,
    Flags
};

static void registerHotkeys()
{
    UnregisterHotKey( NULL, (int)Hotkey::UiEdit );
    UnregisterHotKey( NULL, (int)Hotkey::Standings );
    UnregisterHotKey( NULL, (int)Hotkey::DDU );
    UnregisterHotKey( NULL, (int)Hotkey::Inputs );
    UnregisterHotKey( NULL, (int)Hotkey::Relative );
    UnregisterHotKey( NULL, (int)Hotkey::Cover );
    UnregisterHotKey( NULL, (int)Hotkey::Weather );

    UINT vk, mod;

    if( parseHotkey( g_cfg.getString("General","ui_edit_hotkey","alt-j"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::UiEdit, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayStandings","toggle_hotkey","ctrl-space"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Standings, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayDDU","toggle_hotkey","ctrl-1"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::DDU, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayInputs","toggle_hotkey","ctrl-2"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Inputs, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayRelative","toggle_hotkey","ctrl-3"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Relative, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayCover","toggle_hotkey","ctrl-4"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Cover, mod, vk );

    if( parseHotkey( g_cfg.getString("OverlayWeather","toggle_hotkey","ctrl-5"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Weather, mod, vk );
    
    if( parseHotkey( g_cfg.getString("OverlayFlags","toggle_hotkey","ctrl-6"),&mod,&vk) )
        RegisterHotKey( NULL, (int)Hotkey::Flags, mod, vk );
}

static void handleConfigChange( std::vector<Overlay*> overlays, ConnectionStatus status )
{
    registerHotkeys();

    ir_handleConfigChange();

    for( Overlay* o : overlays )
    {
        bool overlayEnabled = g_cfg.getBool(o->getName(),"enabled",true);
        
        // Check show_in_menu and show_in_race settings
        bool showInMenu = g_cfg.getBool(o->getName(), "show_in_menu", true);
        bool showInRace = g_cfg.getBool(o->getName(), "show_in_race", true);
        
        bool connectionAllows = false;
        if (status == ConnectionStatus::DRIVING) {
            connectionAllows = showInRace;
        } else if (status == ConnectionStatus::CONNECTED) {
            connectionAllows = showInMenu && o->canEnableWhileNotDriving();
        } else if (status == ConnectionStatus::DISCONNECTED) {
            connectionAllows = o->canEnableWhileDisconnected();
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

int main()
{
    // Bump priority up so we get time from the sim
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    // Ensure config.json is read/written next to the executable
    setWorkingDirectoryToExe();

#ifdef IRON_USE_CEF
    const bool cefOk = cefInitialize();
    if( cefOk )
        cefCreateMainWindow();
#endif

    // Load the config and watch it for changes
    g_cfg.load();
    g_cfg.watchForChanges();

    // Initialize preview mode
    preview_mode_init();

    // Register global hotkeys
    registerHotkeys();

    printf("\n====================================================================================\n");
    printf("Welcome to iRon! This app provides a few simple overlays for iRacing.\n\n");
    printf("NOTE: Most overlays are only active when iRacing is running and the car is on track.\n\n");
    printf("Current hotkeys:\n");
    printf("    Move and resize overlays:     %s\n", g_cfg.getString("General","ui_edit_hotkey","").c_str() );
    printf("    Toggle standings overlay:     %s\n", g_cfg.getString("OverlayStandings","toggle_hotkey","").c_str() );
    printf("    Toggle DDU overlay:           %s\n", g_cfg.getString("OverlayDDU","toggle_hotkey","").c_str() );
    printf("    Toggle inputs overlay:        %s\n", g_cfg.getString("OverlayInputs","toggle_hotkey","").c_str() );
    printf("    Toggle relative overlay:      %s\n", g_cfg.getString("OverlayRelative","toggle_hotkey","").c_str() );
    printf("    Toggle cover overlay:         %s\n", g_cfg.getString("OverlayCover","toggle_hotkey","").c_str() );
    printf("    Toggle weather overlay:       %s\n", g_cfg.getString("OverlayWeather","toggle_hotkey","").c_str() );
    printf("    Toggle flags overlay:         %s\n", g_cfg.getString("OverlayFlags","toggle_hotkey","").c_str() );
    printf("\niRon will generate a file called \'config.json\' in its current directory. This file\n"\
           "stores your settings. You can edit the file at any time, even while iRon is running,\n"\
           "to customize your overlays and hotkeys.\n\n");
    printf("To exit iRon, simply close this console window.\n\n");
    printf("For the latest version or to submit bug reports, go to:\n\n        https://github.com/lespalt/iRon\n\n");
    printf("\nHappy Racing!\n");
    printf("====================================================================================\n\n");

    // Create overlays
    std::vector<Overlay*> overlays;
    overlays.push_back( new OverlayCover() );
    overlays.push_back( new OverlayRelative() );
    overlays.push_back( new OverlayInputs() );
    overlays.push_back( new OverlayStandings() );
    overlays.push_back( new OverlayDDU() );
    overlays.push_back( new OverlayWeather() );
    overlays.push_back( new OverlayFlags() );
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

        // Refresh connection and session info
        status = ir_tick();
        if( status != prevStatus )
        {
            if( status == ConnectionStatus::DISCONNECTED )
                printf("Waiting for iRacing connection...\n");
            else
                printf("iRacing connected (%s)\n", ConnectionStatusStr[(int)status]);

            // Enable user-selected overlays, but only if we're driving
            handleConfigChange( overlays, status );

#ifdef IRON_USE_CEF
            // Push status to GUI
            if (cefOk) {
                std::string js = std::string("window.onIronState && window.onIronState(") + app_get_state_json() + ");";
                cefExecuteScript(js.c_str());
            }
#endif
        }

        if( ir_session.sessionType != prevSessionType )
        {
            for( Overlay* o : overlays )
                o->sessionChanged();
        }

        dbg( "connection status: %s, session type: %s, session state: %d, pace mode: %d, on track: %d, flags: 0x%X", ConnectionStatusStr[(int)status], SessionTypeStr[(int)ir_session.sessionType], ir_SessionState.getInt(), ir_PaceMode.getInt(), (int)ir_IsOnTrackCar.getBool(), ir_SessionFlags.getInt() );

        // Update/render overlays
        {
            if( !g_cfg.getBool("General", "performance_mode_30hz", false) )
            {
                // Update everything every frame, roughly every 16ms (~60Hz)
                for( Overlay* o : overlays )
                    o->update();
            }
            else
            {
                // To save perf, update half of the (enabled) overlays on even frames and the other half on odd, for ~30Hz overall
                int cnt = 0;
                for( Overlay* o : overlays )
                {
                    if( o->isEnabled() )
                        cnt++;

                    if( (cnt & 1) == (frameCnt & 1) )
                        o->update();
                }
            }
        }

        // Watch for config change signal
        if( g_cfg.hasChanged() )
        {
            g_cfg.load();
            handleConfigChange( overlays, status );
#ifdef IRON_USE_CEF
            if (cefOk) {
                std::string js = std::string("window.onIronState && window.onIronState(") + app_get_state_json() + ");";
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

                    // When we're exiting edit mode, attempt to make iRacing the foreground window again for best perf
                    // without the user having to manually click into iRacing.
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
                    }
                    
                    g_cfg.save();
                    handleConfigChange( overlays, status );
                }
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);            
        }

        if( quitRequested )
            break;

#ifdef IRON_USE_CEF
        // Allow CEF to process pending work when using the external pump
        cefDoMessageLoopWork();
#endif

        frameCnt++;
    }

    for( Overlay* o : overlays )
        delete o;

#ifdef IRON_USE_CEF
    cefShutdown();
#endif
}
