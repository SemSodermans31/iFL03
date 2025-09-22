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

#pragma once

#include "Overlay.h"
#include "iracing.h"
#include "Config.h"
#include "Units.h"
#include "preview_mode.h"
#include "stub_data.h"

class OverlayPit : public Overlay
{
public:
    OverlayPit()
        : Overlay("OverlayPit")
    {
    }

protected:
    virtual void onEnable()
    {
        onConfigChanged();
        m_text.reset(m_dwriteFactory.Get());
    }

    virtual void onConfigChanged()
    {
        // Default to 30Hz like other light telemetry overlays
        setTargetFPS(g_cfg.getInt(m_name, "target_fps", 30));
        m_text.reset(m_dwriteFactory.Get());
    }

    virtual void onSessionChanged()
    {
        m_learnedPitEntryPct = -1.0f;
        m_lastOnPitRoad = false;
    }

    virtual bool hasCustomBackground() { return true; }

    virtual float2 getDefaultSize() { return float2(360, 110); }

    virtual void onUpdate()
    {
        // Start render with transparent background
        m_renderTarget->BeginDraw();
        m_renderTarget->Clear(float4(0,0,0,0));

        if (!StubDataManager::shouldUseStubData()) {
            if (!ir_IsOnTrack.getBool() || !ir_IsOnTrackCar.getBool()) {
                m_renderTarget->EndDraw();
                return;
            }
        }

        // Gather inputs - use static preview data to avoid flicker
        const float lapPct = StubDataManager::shouldUseStubData() ?
            0.9f :
            std::clamp(ir_LapDistPct.getFloat(), 0.0f, 1.0f);

        const float trackLenM = ir_session.trackLengthMeters > 1.0f ? ir_session.trackLengthMeters : 4000.0f;

        // Try to get pit entry pct from telemetry if available, else learned fallback
        float pitEntryPct = -1.0f;
        {
            extern irsdkCVar ir_TrackPitEntryPct;
            float v = ir_TrackPitEntryPct.getFloat();
            if (v > 0.0f && v <= 1.0f) pitEntryPct = v;
        }

        // Learn pit entry pct by observing first OnPitRoad transition this lap if not provided
        bool onPitRoadNow = StubDataManager::shouldUseStubData() ? false : ir_OnPitRoad.getBool();
        bool inPitStall = StubDataManager::shouldUseStubData() ? false : ir_PlayerCarInPitStall.getBool();
        // Learn pit stall position the first time we detect being properly in the pit stall
        if (!StubDataManager::shouldUseStubData())
        {
            if (inPitStall && !m_lastInPitStall) {
                m_learnedPitStallPct = lapPct;
            }
        }

        if (!StubDataManager::shouldUseStubData())
        {
            if (onPitRoadNow && !m_lastOnPitRoad) {
                // Just crossed pit entry line
                m_learnedPitEntryPct = lapPct;
                m_stateChangeTick = GetTickCount();
                m_lastEvent = LastEvent::EnteredPitRoad;
            }
            m_lastOnPitRoad = onPitRoadNow;
            if (!onPitRoadNow && m_prevOnPitRoad) {
                m_stateChangeTick = GetTickCount();
                m_lastEvent = LastEvent::ExitedPitRoad;
            }
            m_prevOnPitRoad = onPitRoadNow;
            m_lastInPitStall = inPitStall;
        }
        if (pitEntryPct < 0.0f && m_learnedPitEntryPct >= 0.0f) pitEntryPct = m_learnedPitEntryPct;

        // If still unknown, fabricate a stable preview value
        if (pitEntryPct < 0.0f) pitEntryPct = 0.95f;

        // Determine pit state and calculate appropriate distance (to pit stall when known)
        enum class PitState { APPROACHING_ENTRY, ON_PIT_ROAD, IN_PIT_STALL, APPROACHING_EXIT };
        PitState pitState = PitState::APPROACHING_ENTRY;
        float distanceM = 0.0f;
        std::wstring label;

        const bool imperial = isImperialUnits();
        const float pitRoadLengthM = 200.0f;

        if (inPitStall) {
            pitState = PitState::IN_PIT_STALL;
            // Estimate distance to pit exit (half pit road length from stall)
            distanceM = pitRoadLengthM * 0.5f;
            label = L"Pit Exit";
        }
        else if (onPitRoadNow) {
            pitState = PitState::ON_PIT_ROAD;
            // Distance to our stall if we know it, else progress along pit road from entry
            if (m_learnedPitStallPct >= 0.0f) {
                float diffPct = 0.0f;
                if (m_learnedPitStallPct >= lapPct) diffPct = m_learnedPitStallPct - lapPct; else diffPct = (1.0f - lapPct) + m_learnedPitStallPct;
                distanceM = diffPct * trackLenM;
                // If we already passed the stall this lap, clamp to zero
                if (distanceM > pitRoadLengthM) distanceM = 0.0f;
                label = L"To Pit Box";
            } else {
                float distanceFromEntryPct = (lapPct >= pitEntryPct) ? (lapPct - pitEntryPct) : ((1.0f - pitEntryPct) + lapPct);
                distanceM = distanceFromEntryPct * trackLenM;
                distanceM = std::min(distanceM, pitRoadLengthM);
                label = L"Along Pit Road";
            }
        }
        else {
            // Calculate distance to pit entry
            float diffPct = 0.0f;
            if (pitEntryPct >= lapPct) {
                diffPct = pitEntryPct - lapPct;
            } else {
                diffPct = (1.0f - lapPct) + pitEntryPct;
            }
            distanceM = diffPct * trackLenM;
            // If we know our stall pct, show distance to stall directly
            if (m_learnedPitStallPct >= 0.0f) {
                float dp = 0.0f;
                if (m_learnedPitStallPct >= lapPct) dp = m_learnedPitStallPct - lapPct; else dp = (1.0f - lapPct) + m_learnedPitStallPct;
                distanceM = dp * trackLenM;
            }
            label = L"Pit Box";
            pitState = PitState::APPROACHING_ENTRY;
        }

        // Convert to units
        float displayVal = imperial ? distanceM * 3.28084f : distanceM;
        const float warn100 = imperial ? 328.0f : 100.0f;
        const float warn50  = imperial ? 164.0f : 50.0f;

        // Only show when approaching pits, on pit road, or in pit stall
        bool shouldShow = false;
        if (StubDataManager::shouldUseStubData()) {
            shouldShow = true;
        } else {
            const int surf = ir_PlayerTrackSurface.getInt();
            shouldShow = (surf == irsdk_AproachingPits) ||
                        (pitState != PitState::APPROACHING_ENTRY) ||
                        (displayVal <= (imperial ? 600.0f : 200.0f));
        }
        if (!shouldShow) {
            m_renderTarget->EndDraw();
            return;
        }

        // Layout
        const float W = (float)m_width;
        const float H = (float)m_height;
        const float barH = 26.0f;

        // Numeric value for progress bar
        wchar_t buf[64];
        if (imperial) swprintf_s(buf, L"%d ft", (int)roundf(displayVal));
        else swprintf_s(buf, L"%d m", (int)roundf(displayVal));

        Microsoft::WRL::ComPtr<IDWriteTextFormat> tfValue;
        createGlobalTextFormat(1.0f, tfValue);
        if (tfValue) tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

        // Color based on state and distance
        float4 col = float4(0.2f, 0.6f, 1.0f, 1.0f);
        if (pitState == PitState::APPROACHING_ENTRY) {
            if (displayVal < warn50) col = float4(1.0f, 0.2f, 0.2f, 1.0f);
            else if (displayVal < warn100) col = float4(1.0f, 0.65f, 0.0f, 1.0f);
        } else if (pitState == PitState::ON_PIT_ROAD) {
            col = float4(0.2f, 0.8f, 0.2f, 1.0f);
        } else if (pitState == PitState::IN_PIT_STALL) {
            col = float4(0.2f, 0.6f, 1.0f, 1.0f);
        }

        // Top pitlimiter banner (rectangular, no rounded corners, dark border)
        bool limiterOn = false;
        if (!StubDataManager::shouldUseStubData()) {
            const int engWarn = ir_EngineWarnings.getInt();
            limiterOn = (engWarn & irsdk_pitSpeedLimiter) != 0;
        } else {
            limiterOn = true; // Static ON state in preview mode
        }
        const bool flash = !limiterOn && ((GetTickCount()/350)%2==0);
        const float bannerH = 28.0f;
        const float bannerTop = 0.0f; // Start at very top, no padding
        const float bannerBottom = bannerTop + bannerH;

        if (bannerBottom < H)
        {
            D2D1_RECT_F rrBan = { 0.0f, bannerTop, W, bannerBottom };
            float4 banCol = limiterOn ? float4(0.0f,0.6f,0.0f,0.95f) : float4(0.7f,0.0f,0.0f, flash?0.95f:0.65f);
            float4 borderCol = limiterOn ? float4(0.0f,0.4f,0.0f,1.0f) : float4(0.5f,0.0f,0.0f,1.0f);

            m_brush->SetColor(banCol);
            m_renderTarget->FillRectangle(&rrBan, m_brush.Get());

            // 3.0f dark border
            m_brush->SetColor(borderCol);
            m_renderTarget->DrawRectangle(&rrBan, m_brush.Get(), 3.0f);

            Microsoft::WRL::ComPtr<IDWriteTextFormat> tfBan;
            createGlobalTextFormat(0.9f, tfBan);
            if (tfBan) tfBan->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_brush->SetColor(float4(1,1,1,0.95f));
            m_text.render(m_renderTarget.Get(), limiterOn?L"PIT LIMITER ON":L"PIT LIMITER OFF", tfBan.Get(), 0.0f, W, bannerTop + bannerH*0.6f, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, getGlobalFontSpacing());
        }

        // Middle LED panel: digital 'PE' warning when entering/exiting pit road
        const DWORD now = GetTickCount();
        const bool showEvent = StubDataManager::shouldUseStubData() ?
            true : // Always show in preview mode
            (now - m_stateChangeTick) < 3000;
        const float panelTop = bannerBottom;
        const float panelSize = std::min(W, H - panelTop - barH); 
        const float panelLeft = (W - panelSize) * 0.5f;
        if (panelSize > 20.0f)
        {
            // Panel background
            m_brush->SetColor(float4(0.05f,0.05f,0.05f,0.85f));
            D2D1_RECT_F pr = { panelLeft, panelTop, panelLeft+panelSize, panelTop+panelSize };
            m_renderTarget->FillRectangle(&pr, m_brush.Get());

            // LED grid - 20x20 with layered structure
            const int leds = 20; // 20x20 grid
            const float cell = panelSize / (float)leds;
            const float radius = cell * 0.32f;
            for (int y=0; y<leds; ++y)
            {
                for (int x=0; x<leds; ++x)
                {
                    // Rings:
                    // ring0 (outermost 20x20) and ring1 (18x18): yellow
                    // ring2 (16x16): gray
                    const bool ring0 = (x==0 || y==0 || x==leds-1 || y==leds-1);
                    const bool ring1 = (x==1 || y==1 || x==leds-2 || y==leds-2);
                    const bool ring2 = (x==2 || y==2 || x==leds-3 || y==leds-3);

                    float4 dotCol = float4(0.45f,0.45f,0.45f,0.35f);

                    if (ring0 || ring1)
                    {
                        dotCol = float4(0.95f,0.85f,0.1f, showEvent?1.0f:0.6f);
                    }
                    else if (ring2)
                    {}
                    else if (showEvent)
                    {
                        // Center 14x14 area for letters: indices 3..16 (inclusive)
                        if (x>=3 && x<=16 && y>=3 && y<=16)
                        {
                            const int gx = x - 3; // 0..13
                            const int gy = y - 3; // 0..13
                            bool on = false;

                            if (gx >= 0 && gx <= 5)
                            {
                                // Left vertical stroke (2 px)
                                if (gx <= 1) on = (gy >= 0 && gy <= 13);
                                // Top horizontal stroke (2 px)
                                if (gy <= 1) on = on || (gx >= 2 && gx <= 5);
                                // Middle horizontal stroke (2 px)
                                if (gy >= 6 && gy <= 7) on = on || (gx >= 2 && gx <= 5);
                                // Right vertical (top half) (2 px)
                                if (gx >= 4 && gx <= 5 && gy > 1 && gy < 6) on = true;
                            }

                            // 'E' on right: columns 8..13, 2-pixel strokes
                            if (gx >= 8 && gx <= 13)
                            {
                                // Left vertical stroke (2 px)
                                if (gx <= 9) on = on || (gy >= 0 && gy <= 13);
                                // Top horizontal stroke (2 px)
                                if (gy <= 1) on = on || (gx >= 10 && gx <= 13);
                                // Middle horizontal stroke (2 px)
                                if (gy >= 6 && gy <= 7) on = on || (gx >= 10 && gx <= 13);
                                // Bottom horizontal stroke (2 px)
                                if (gy >= 12 && gy <= 13) on = on || (gx >= 10 && gx <= 13);
                            }

                            if (on)
                                dotCol = float4(0.95f,0.95f,0.95f,1.0f);
                        }
                    }

                    m_brush->SetColor(dotCol);
                    const float cx = panelLeft + x*cell + cell*0.5f;
                    const float cy = panelTop + y*cell + cell*0.5f;
                    D2D1_ELLIPSE e = { {cx, cy}, radius, radius };
                    m_renderTarget->FillEllipse(&e, m_brush.Get());
                }
            }
        }

        // Bottom distance-to-pitbox progress bar (rectangular with white border)
        const float barTop = panelTop + panelSize;
        const float barBottom = barTop + barH;

        if (barBottom <= H)
        {
            D2D1_RECT_F rrBg = { 0.0f, barTop, W, barBottom };

            // Semi-transparent background
            m_brush->SetColor(float4(0.1f, 0.1f, 0.1f, 0.6f));
            m_renderTarget->FillRectangle(&rrBg, m_brush.Get());

            // White border
            m_brush->SetColor(float4(1.0f, 1.0f, 1.0f, 1.0f));
            m_renderTarget->DrawRectangle(&rrBg, m_brush.Get(), 1.0f);

            // Fill amount based on pit state (distance-to-target)
            float progress = 0.0f;
            const float cap = imperial ? 1000.0f : 300.0f;

            if (pitState == PitState::APPROACHING_ENTRY) {
                // For approaching: more filled = closer to pits
                progress = 1.0f - std::min(1.0f, std::max(0.0f, displayVal / cap));
            } else if (pitState == PitState::ON_PIT_ROAD) {
                // For on pit road: show progress along pit road (0-100%)
                progress = std::min(1.0f, distanceM / pitRoadLengthM);
            } else if (pitState == PitState::IN_PIT_STALL) {
                // For in pit stall: show as mostly filled
                progress = 0.8f;
            }

            if (progress > 0.0f) {
                float fillW = W * progress;
                D2D1_RECT_F rrFill = { 0.0f, barTop, fillW, barBottom };
                m_brush->SetColor(col);
                m_renderTarget->FillRectangle(&rrFill, m_brush.Get());
            }

            // Center the numeric text in the bar
            m_brush->SetColor(float4(1,1,1,0.9f));
            m_text.render(m_renderTarget.Get(), buf, tfValue.Get(), 0.0f, W, barTop + barH*0.5f, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, getGlobalFontSpacing());
        }

        // No extra dots; the LED panel and banner indicate state now

        m_renderTarget->EndDraw();
    }

private:
    // Runtime-learned fallback if telemetry does not expose pit entry pct
    float m_learnedPitEntryPct = -1.0f;
    float m_learnedPitStallPct = -1.0f;
    bool  m_lastOnPitRoad = false;
    bool  m_prevOnPitRoad = false;
    bool  m_lastInPitStall = false;
    DWORD m_stateChangeTick = 0;
    enum class LastEvent { None, EnteredPitRoad, ExitedPitRoad };
    LastEvent m_lastEvent = LastEvent::None;

    TextCache m_text;
};


