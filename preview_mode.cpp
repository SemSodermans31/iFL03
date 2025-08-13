#include "preview_mode.h"
#include "Config.h"
#include "AppControl.h"
#include "iracing.h"
#include <vector>

extern Config g_cfg;

// Global preview mode state
bool g_previewMode = false;

void preview_mode_init()
{
    // Load preview mode state from config
    g_previewMode = g_cfg.getBool("General", "preview_mode", false);
}

void preview_mode_set(bool enabled)
{
    if (g_previewMode == enabled) return;
    
    g_previewMode = enabled;
    
    // Save to config
    g_cfg.setBool("General", "preview_mode", enabled);
    g_cfg.save();
    
    // Update overlays immediately
    preview_mode_update_overlays();
}

bool preview_mode_get()
{
    return g_previewMode;
}

bool preview_mode_should_show_overlay(const char* overlayName)
{
    if (!overlayName) return false;
    
    // Check if overlay is enabled in config
    bool enabled = g_cfg.getBool(overlayName, "enabled", true);
    
    if (!enabled) return false;
    
    // If preview mode is on, show all enabled overlays
    if (g_previewMode) return true;
    
    // Otherwise use normal connection-based logic
    // This is handled by the main overlay enable logic
    return false;
}

bool preview_mode_should_use_stub_data()
{
    // Use stub data when in preview mode and not connected to iRacing
    // We can check if iRacing is connected by calling ir_tick to get current status
    // But for now, assume preview mode always uses stub data to simplify
    return g_previewMode;
}

void preview_mode_update_overlays()
{
    // Trigger the external config change handler which will re-evaluate overlay visibility
    app_handleConfigChange_external();
}
