#pragma once

// Preview Mode System
// Allows overlays to be visible without iRacing connection for testing/editing

// Global preview mode state
extern bool g_previewMode;

// Initialize preview mode system
void preview_mode_init();

// Set preview mode on/off
void preview_mode_set(bool enabled);

// Get current preview mode state
bool preview_mode_get();

// Check if an overlay should be visible in preview mode
bool preview_mode_should_show_overlay(const char* overlayName);

// Check if overlays should use stub data (when in preview mode and not connected)
bool preview_mode_should_use_stub_data();

// Update all overlays based on preview mode state
void preview_mode_update_overlays();
