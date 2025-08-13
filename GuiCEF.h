#pragma once

// Minimal CEF integration layer for iRon
// Initialize CEF, create a simple browser window, pump its message loop, and shutdown.

bool cefInitialize();
void cefCreateMainWindow();
void cefDoMessageLoopWork();
void cefShutdown();

// Execute JavaScript in the main frame if available (no-op when CEF is disabled)
void cefExecuteScript(const char* jsUtf8); 