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
#include "ClassColors.h"
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
        m_bgBrush.Reset();
        m_panelBrush.Reset();
    }

    virtual void onConfigChanged()
    {
        // Default to 30Hz like other light telemetry overlays
        setTargetFPS(g_cfg.getInt(m_name, "target_fps", 30));
        m_text.reset(m_dwriteFactory.Get());
        // Recreate D2D brushes (render target may change)
        m_bgBrush.Reset();
        m_panelBrush.Reset();
    }

    virtual void onSessionChanged()
    {
        m_learnedPitEntryPct = -1.0f;
        m_lastOnPitRoad = false;
    }

    virtual bool hasCustomBackground() { return true; }

    virtual float2 getDefaultSize() { return float2(320, 320); }

    virtual void onUpdate()
    {
        m_renderTarget->BeginDraw();
        m_renderTarget->Clear(float4(0,0,0,0));

        if (!StubDataManager::shouldUseStubData() && !ir_isReplayActive()) {
            if (!ir_IsOnTrack.getBool() || !ir_IsOnTrackCar.getBool()) {
                m_renderTarget->EndDraw();
                return;
            }
        }

        const float lapPct = StubDataManager::shouldUseStubData() ?
            0.9f :
            std::clamp(ir_LapDistPct.getFloat(), 0.0f, 1.0f);

        const float trackLenM = ir_session.trackLengthMeters > 1.0f ? ir_session.trackLengthMeters : 4000.0f;

        float pitEntryPct = -1.0f;
        {
            extern irsdkCVar ir_TrackPitEntryPct;
            float v = ir_TrackPitEntryPct.getFloat();
            if (v > 0.0f && v <= 1.0f) pitEntryPct = v;
        }

        bool onPitRoadNow = StubDataManager::shouldUseStubData() ? false : ir_OnPitRoad.getBool();
        bool inPitStall = StubDataManager::shouldUseStubData() ? false : ir_PlayerCarInPitStall.getBool();
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
            distanceM = 0.0f;
            label = L"Pit Box";
        }
        else if (onPitRoadNow) {
            pitState = PitState::ON_PIT_ROAD;
            if (m_learnedPitStallPct >= 0.0f) {
                float diffPct = 0.0f;
                if (m_learnedPitStallPct >= lapPct) diffPct = m_learnedPitStallPct - lapPct; else diffPct = (1.0f - lapPct) + m_learnedPitStallPct;
                distanceM = diffPct * trackLenM;
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
            label = L"Pit Entry";
            pitState = PitState::APPROACHING_ENTRY;
        }

        // Convert to units
        float displayVal = imperial ? distanceM * 3.28084f : distanceM;
        const float warn100 = imperial ? 328.0f : 100.0f;
        const float warn50  = imperial ? 164.0f : 50.0f;

        // Only show in legit pit contexts:
        // - iRacing "ApproachingPits" track surface (pre-entry heads-up)
        // - On pit road / in pit stall
        // - Short grace window after exiting pit road (user feedback)
        bool shouldShow = false;
        if (StubDataManager::shouldUseStubData()) {
            shouldShow = true;
        } else {
            const int surf = ir_PlayerTrackSurface.getInt();
            const bool approaching = (surf == irsdk_AproachingPits);
            const bool onPitOrStall = onPitRoadNow || inPitStall;
            const DWORD nowTs = GetTickCount();
            const bool justExited = (m_lastEvent == LastEvent::ExitedPitRoad) && ((nowTs - m_stateChangeTick) < 3000);
            shouldShow = approaching || onPitOrStall || justExited;
        }
        if (!shouldShow) {
            m_renderTarget->EndDraw();
            return;
        }

        ensureStyleBrushes();

        const float W = (float)m_width;
        const float H = (float)m_height;
        const float globalOpacity = getGlobalOpacity();
        const float minDim = std::max(1.0f, std::min(W, H));
        const float pad = std::clamp(minDim * 0.045f, 8.0f, 18.0f);
        const float innerPad = std::clamp(minDim * 0.045f, 10.0f, 20.0f);
        const float corner = std::clamp(minDim * 0.070f, 10.0f, 26.0f);

        D2D1_RECT_F rCard = { pad, pad, W - pad, H - pad };
        const float cardW = std::max(1.0f, rCard.right - rCard.left);
        const float cardH = std::max(1.0f, rCard.bottom - rCard.top);

        // Numeric value for progress bar
        wchar_t buf[64];
        if (imperial) swprintf_s(buf, L"%d ft", (int)roundf(displayVal));
        else swprintf_s(buf, L"%d m", (int)roundf(displayVal));

        Microsoft::WRL::ComPtr<IDWriteTextFormat> tfValue;
        createGlobalTextFormat(1.0f, tfValue);
        if (tfValue) tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

        // Accent colors (use the shared palette)
        const float4 colBlue  = ClassColors::self();
        const float4 colGreen = ClassColors::get(3);
        const float4 colRed   = ClassColors::get(0);

        // Color based on state (flat single-tone for the bar)
        float4 col = colBlue;
        if (pitState == PitState::ON_PIT_ROAD) {
            col = colGreen;
        } else if (pitState == PitState::IN_PIT_STALL) {
            col = colBlue;
        }

        // Top pitlimiter banner state (keep logic as-is)
        bool limiterOn = false;
        if (!StubDataManager::shouldUseStubData()) {
            const int engWarn = ir_EngineWarnings.getInt();
            limiterOn = (engWarn & irsdk_pitSpeedLimiter) != 0;
        } else {
            limiterOn = true;
        }
        const bool flash = !limiterOn && ((GetTickCount()/350)%2==0);

        // No drop shadow (per request)

        // Card background gradient (no outer grey borderline per request)
        {
            D2D1_ROUNDED_RECT rr = { rCard, corner, corner };
            if (m_bgBrush) {
                // Background gradient points follow the card
                m_bgBrush->SetStartPoint(D2D1_POINT_2F{ rCard.left, rCard.top });
                m_bgBrush->SetEndPoint(D2D1_POINT_2F{ rCard.left, rCard.bottom });
                m_renderTarget->FillRoundedRectangle(&rr, m_bgBrush.Get());
            } else {
                // Fallback (should be rare): solid background
                m_brush->SetColor(float4(0.05f, 0.05f, 0.06f, 0.92f * globalOpacity));
                m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
            }
        }

        // Banner geometry
        const float bannerH = std::clamp(cardH * 0.18f, 32.0f, 56.0f);
        D2D1_RECT_F rBanner = {
            rCard.left + innerPad,
            rCard.top + innerPad,
            rCard.right - innerPad,
            rCard.top + innerPad + bannerH
        };
        const float bannerRadius = bannerH * 0.22f;

        // Banner fill + border + gloss highlight
        {
            // Flat single-tone banner color (green/blue) per request
            // Use ClassColors palette (green when ON, red when OFF)
            float4 banCol = limiterOn ? float4(colGreen.x, colGreen.y, colGreen.z, 0.95f)
                                      : float4(colRed.x,   colRed.y,   colRed.z,   flash ? 0.95f : 0.65f);
            banCol.w *= globalOpacity;
            m_brush->SetColor(banCol);
            D2D1_ROUNDED_RECT rrBan = { rBanner, bannerRadius, bannerRadius };
            m_renderTarget->FillRoundedRectangle(&rrBan, m_brush.Get());

            Microsoft::WRL::ComPtr<IDWriteTextFormat> tfBan;
            createGlobalTextFormat(0.95f, (int)DWRITE_FONT_WEIGHT_BOLD, "", tfBan);
            if (tfBan) {
                tfBan->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                tfBan->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
            m_brush->SetColor(float4(1, 1, 1, 0.95f * globalOpacity));
            m_text.render(
                m_renderTarget.Get(),
                limiterOn ? L"PIT LIMITER ON" : L"PIT LIMITER OFF",
                tfBan.Get(),
                rBanner.left,
                rBanner.right,
                (rBanner.top + rBanner.bottom) * 0.5f,
                m_brush.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER,
                getGlobalFontSpacing()
            );
        }

        // Middle "panel": big 'PE' indicator (same showEvent timing as before)
        const DWORD now = GetTickCount();
        const bool showEvent = StubDataManager::shouldUseStubData() ?
            true : // Always show in preview mode
            (now - m_stateChangeTick) < 3000;

        // Panel rect
        const float barH = std::clamp(cardH * 0.11f, 22.0f, 34.0f);
        const float gap = std::clamp(cardH * 0.035f, 8.0f, 14.0f);
        D2D1_RECT_F rPanel = {
            rCard.left + innerPad,
            rBanner.bottom + gap,
            rCard.right - innerPad,
            rCard.bottom - innerPad - barH - gap
        };
        if (rPanel.bottom < rPanel.top + 24.0f) {
            // Not enough vertical space; collapse panel into remaining area above the bar
            rPanel.bottom = rPanel.top + 24.0f;
        }

        // Panel background gradient + border
        {
            const float panelCorner = std::clamp(corner * 0.75f, 8.0f, 22.0f);
            D2D1_ROUNDED_RECT rrPanel = { rPanel, panelCorner, panelCorner };
            if (m_panelBrush) {
                m_panelBrush->SetStartPoint(D2D1_POINT_2F{ rPanel.left, rPanel.top });
                m_panelBrush->SetEndPoint(D2D1_POINT_2F{ rPanel.left, rPanel.bottom });
                m_renderTarget->FillRoundedRectangle(&rrPanel, m_panelBrush.Get());
            } else {
                m_brush->SetColor(float4(0.03f, 0.03f, 0.04f, 0.88f * globalOpacity));
                m_renderTarget->FillRoundedRectangle(&rrPanel, m_brush.Get());
            }

            m_brush->SetColor(float4(0.9f, 0.9f, 0.95f, 0.18f * globalOpacity));
            m_renderTarget->DrawRoundedRectangle(&rrPanel, m_brush.Get(), 1.5f);
        }

        // Center panel text:
        // - Outside pit lane: show PIT ENTRY (or PIT EXIT for a short time after exit)
        // - Inside pit lane: show current speed
        {
            const bool inPitLane = onPitRoadNow || inPitStall;

            std::wstring centerText;
            float textScale = 2.6f;

            if (inPitLane)
            {
                // Speed (ir_Speed is m/s)
                const float speedMps = StubDataManager::shouldUseStubData()
                    ? StubDataManager::getStubSpeed()
                    : ir_Speed.getFloat();

                float speed = speedMps;
                const wchar_t* unit = L"m/s";
                if (imperial) { speed = speedMps * 2.23694f; unit = L"mph"; }
                else { speed = speedMps * 3.6f; unit = L"km/h"; }

                wchar_t sSpeed[64];
                swprintf_s(sSpeed, L"%d %s", (int)std::round(std::max(0.0f, speed)), unit);
                centerText = sSpeed;
                textScale = 2.2f;
            }
            else
            {
                // Outside pit lane: show PIT EXIT briefly after exit, otherwise PIT ENTRY
                if (showEvent && m_lastEvent == LastEvent::ExitedPitRoad) centerText = L"PIT EXIT";
                else centerText = L"PIT ENTRY";
                textScale = 2.6f;
            }

            Microsoft::WRL::ComPtr<IDWriteTextFormat> tfBig;
            createGlobalTextFormat(textScale, (int)DWRITE_FONT_WEIGHT_BOLD, "", tfBig);
            if (tfBig) {
                tfBig->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                tfBig->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }

            // Keep it bright/consistent; event timing only affects which text is shown (outside) and flashing (banner)
            m_brush->SetColor(float4(0.95f, 0.95f, 0.98f, 0.92f * globalOpacity));
            m_text.render(
                m_renderTarget.Get(),
                centerText.c_str(),
                tfBig.Get(),
                rPanel.left,
                rPanel.right,
                (rPanel.top + rPanel.bottom) * 0.5f,
                m_brush.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER,
                getGlobalFontSpacing()
            );
        }

        D2D1_RECT_F rBar = {
            rCard.left + innerPad,
            rCard.bottom - innerPad - barH,
            rCard.right - innerPad,
            rCard.bottom - innerPad
        };
        if (rBar.bottom > rBar.top + 8.0f)
        {
            const float barCorner = std::clamp(barH * 0.22f, 4.0f, 10.0f);
            D2D1_ROUNDED_RECT rrBg = { rBar, barCorner, barCorner };

            // Bar background
            m_brush->SetColor(float4(0.04f, 0.05f, 0.06f, 0.70f * globalOpacity));
            m_renderTarget->FillRoundedRectangle(&rrBg, m_brush.Get());

            // Bar border
            m_brush->SetColor(float4(0.80f, 0.82f, 0.86f, 0.28f * globalOpacity));
            m_renderTarget->DrawRoundedRectangle(&rrBg, m_brush.Get(), 1.5f);

            // Fill amount based on pit state (distance-to-target) - unchanged logic
            float progress = 0.0f;
            const float cap = imperial ? 1000.0f : 300.0f;

            if (pitState == PitState::APPROACHING_ENTRY) {
                progress = 1.0f - std::min(1.0f, std::max(0.0f, displayVal / cap));
            } else if (pitState == PitState::ON_PIT_ROAD) {
                progress = std::min(1.0f, distanceM / pitRoadLengthM);
            } else if (pitState == PitState::IN_PIT_STALL) {
                progress = 1.0f;
            }

            if (progress > 0.0f) {
                float fillW = (rBar.right - rBar.left) * progress;
                D2D1_RECT_F rFill = { rBar.left, rBar.top, rBar.left + fillW, rBar.bottom };
                D2D1_ROUNDED_RECT rrFill = { rFill, barCorner, barCorner };

                // Flat single-tone fill (no gradient/gloss)
                m_brush->SetColor(float4(col.x, col.y, col.z, 0.95f * globalOpacity));
                m_renderTarget->FillRoundedRectangle(&rrFill, m_brush.Get());
            }

            // Distance text INSIDE the bar (centered), like the old design
            if (tfValue) {
                tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                tfValue->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
            m_brush->SetColor(float4(1, 1, 1, 0.92f * globalOpacity));
            m_text.render(
                m_renderTarget.Get(),
                buf,
                tfValue.Get(),
                rBar.left,
                rBar.right,
                (rBar.top + rBar.bottom) * 0.5f,
                m_brush.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER,
                getGlobalFontSpacing()
            );
        }

        m_renderTarget->EndDraw();
    }

private:
    void ensureStyleBrushes()
    {
        if (!m_renderTarget) return;
        if (m_bgBrush && m_panelBrush) return;

        // Card background gradient
        {
            D2D1_GRADIENT_STOP stops[3] = {};
            stops[0].position = 0.0f; stops[0].color = D2D1::ColorF(0.16f, 0.18f, 0.22f, 0.95f);
            stops[1].position = 0.45f; stops[1].color = D2D1::ColorF(0.06f, 0.07f, 0.09f, 0.95f);
            stops[2].position = 1.0f; stops[2].color = D2D1::ColorF(0.02f, 0.02f, 0.03f, 0.95f);

            Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> stopCollection;
            HRESULT hr = m_renderTarget->CreateGradientStopCollection(
                stops,
                ARRAYSIZE(stops),
                D2D1_GAMMA_2_2,
                D2D1_EXTEND_MODE_CLAMP,
                stopCollection.GetAddressOf()
            );
            if (SUCCEEDED(hr)) {
                D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props = {};
                props.startPoint = D2D1_POINT_2F{ 0,0 };
                props.endPoint = D2D1_POINT_2F{ 0,1 };
                m_renderTarget->CreateLinearGradientBrush(props, stopCollection.Get(), m_bgBrush.GetAddressOf());
            }
        }

        // Inner panel gradient
        {
            D2D1_GRADIENT_STOP stops[3] = {};
            stops[0].position = 0.0f; stops[0].color = D2D1::ColorF(0.08f, 0.09f, 0.11f, 0.92f);
            stops[1].position = 0.55f; stops[1].color = D2D1::ColorF(0.04f, 0.045f, 0.055f, 0.92f);
            stops[2].position = 1.0f; stops[2].color = D2D1::ColorF(0.02f, 0.02f, 0.03f, 0.92f);

            Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> stopCollection;
            HRESULT hr = m_renderTarget->CreateGradientStopCollection(
                stops,
                ARRAYSIZE(stops),
                D2D1_GAMMA_2_2,
                D2D1_EXTEND_MODE_CLAMP,
                stopCollection.GetAddressOf()
            );
            if (SUCCEEDED(hr)) {
                D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props = {};
                props.startPoint = D2D1_POINT_2F{ 0,0 };
                props.endPoint = D2D1_POINT_2F{ 0,1 };
                m_renderTarget->CreateLinearGradientBrush(props, stopCollection.Get(), m_panelBrush.GetAddressOf());
            }
        }

    }

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

    // Styling brushes (cached; recreated on config change / enable)
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_bgBrush;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_panelBrush;
};
