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

#include <algorithm>
#include <string>
#include "Overlay.h"
#include "iracing.h"
#include "Config.h"
#include "ClassColors.h"
#include "stub_data.h"

// Warning popup for multiclass: a faster-class car approaching from behind.
// Styling follows the Kapps-like "card" philosophy used by OverlayPit/Flags/Fuel.
class OverlayTraffic : public Overlay
{
public:
    OverlayTraffic()
        : Overlay("OverlayTraffic")
    {
    }

#ifdef _DEBUG
    virtual bool canEnableWhileNotDriving() const { return true; }
    virtual bool canEnableWhileDisconnected() const { return true; }
#else
    virtual bool canEnableWhileDisconnected() const { return StubDataManager::shouldUseStubData(); }
#endif

protected:
    virtual float2 getDefaultSize()
    {
        // Compact warning card
        return float2(520, 180);
    }

    virtual bool hasCustomBackground() { return true; }

    virtual void onEnable()
    {
        onConfigChanged();
        m_text.reset(m_dwriteFactory.Get());
        m_bgBrush.Reset();
        m_panelBrush.Reset();

        m_activeCarIdx = -1;
        m_showUntil = 0.0f;
        m_lastSessionTime = -1.0f;
        m_animAlpha = 0.0f;
    }

    virtual void onDisable()
    {
        m_text.reset();
        m_bgBrush.Reset();
        m_panelBrush.Reset();
    }

    virtual void onSessionChanged()
    {
        m_activeCarIdx = -1;
        m_showUntil = 0.0f;
        m_lastSessionTime = -1.0f;
        m_animAlpha = 0.0f;
    }

    virtual void onConfigChanged()
    {
        // Default to 15Hz; this is a lightweight popup
        setTargetFPS(g_cfg.getInt(m_name, "target_fps", 15));
        m_text.reset(m_dwriteFactory.Get());

        createGlobalTextFormat(1.0f, m_tf);
        createGlobalTextFormat(0.85f, m_tfSmall);
        createGlobalTextFormat(1.50f, (int)DWRITE_FONT_WEIGHT_BOLD, "", m_tfBig);

        // Recreate D2D brushes (render target may change)
        m_bgBrush.Reset();
        m_panelBrush.Reset();
    }

    virtual void onUpdate()
    {
        const bool useStub = StubDataManager::shouldUseStubData();
        if (!useStub && !ir_hasValidDriver()) {
            return;
        }

        // Drive the popup state (who is the best "faster class behind" candidate?)
        Target best = {};
        const bool found = selectBestTarget(best);

        const float now = useStub ? (float)GetTickCount() * 0.001f : ir_SessionTime.getFloat();

        // Session time going backwards (replay/seek) -> clear state
        if (m_lastSessionTime >= 0.0f && now + 0.001f < m_lastSessionTime)
        {
            m_activeCarIdx = -1;
            m_showUntil = 0.0f;
            m_animAlpha = 0.0f;
        }

        // Hold behavior: keep visible a bit after the target exits the threshold
        const float holdS = g_cfg.getFloat(m_name, "hold_seconds", 1.25f);
        if (found)
        {
            // Lock onto the closest faster-class car behind. If target changes, it's fine.
            m_activeCarIdx = best.carIdx;
            m_active = best;
            m_showUntil = std::max(m_showUntil, now + std::max(0.05f, holdS));
        }

        const bool shouldShow = useStub ? true : (m_activeCarIdx >= 0 && now <= m_showUntil);

        // Animate popup alpha (fast in, slightly slower out)
        {
            float dt = 0.0f;
            if (m_lastSessionTime >= 0.0f)
            {
                dt = now - m_lastSessionTime;
                if (dt < 0.0f) dt = 0.0f;
                if (dt > 0.5f) dt = 0.5f;
            }
            m_lastSessionTime = now;

            const float targetA = shouldShow ? 1.0f : 0.0f;
            const float fadeInRate = 7.0f;
            const float fadeOutRate = 4.0f;
            const float rate = (targetA > m_animAlpha) ? fadeInRate : fadeOutRate;
            const float maxStep = (dt > 0.0f) ? (rate * dt) : 1.0f;
            float d = targetA - m_animAlpha;
            if (fabsf(d) <= maxStep) m_animAlpha = targetA;
            else m_animAlpha += (d > 0.0f ? maxStep : -maxStep);
            m_animAlpha = std::clamp(m_animAlpha, 0.0f, 1.0f);
        }

        // Render
        m_renderTarget->BeginDraw();
        m_renderTarget->Clear(float4(0, 0, 0, 0));

        if (m_animAlpha <= 0.01f)
        {
            m_renderTarget->EndDraw();
            return;
        }

        ensureStyleBrushes();

        // Layout (Kapps-like card)
        const float W = (float)m_width;
        const float H = (float)m_height;
        const float minDim = std::max(1.0f, std::min(W, H));
        const float pad = std::clamp(minDim * 0.045f, 8.0f, 18.0f);
        const float innerPad = std::clamp(minDim * 0.045f, 10.0f, 20.0f);
        const float corner = std::clamp(minDim * 0.070f, 10.0f, 26.0f);
        const float globalOpacity = getGlobalOpacity() * m_animAlpha;

        D2D1_RECT_F rCard = { pad, pad, W - pad, H - pad };
        const float cardW = std::max(1.0f, rCard.right - rCard.left);
        const float cardH = std::max(1.0f, rCard.bottom - rCard.top);

        // Determine active visuals (stub fallback)
        Target drawT = shouldShow ? m_active : best;
        if (useStub)
        {
            drawT.carIdx = 12;
            drawT.carNumberStr = "12";
            drawT.userName = "Faster Class";
            drawT.classShort = "LMP2";
            drawT.classId = 2;
            drawT.gapBehindS = 1.3f;
            drawT.distanceBehindM = 45.0f;
            drawT.isUrgent = true;
        }

        const float4 classCol = ClassColors::get(drawT.classId);
        const float4 classLight = ClassColors::getLight(drawT.classId);

        // Card background gradient
        {
            D2D1_ROUNDED_RECT rr = { rCard, corner, corner };
            if (m_bgBrush) {
                m_bgBrush->SetStartPoint(D2D1_POINT_2F{ rCard.left, rCard.top });
                m_bgBrush->SetEndPoint(D2D1_POINT_2F{ rCard.left, rCard.bottom });
                m_renderTarget->FillRoundedRectangle(&rr, m_bgBrush.Get());
            } else {
                m_brush->SetColor(float4(0.05f, 0.05f, 0.06f, 0.92f * globalOpacity));
                m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
            }
        }

        // Banner (dark panel + colored accent strip)
        const float bannerH = std::clamp(cardH * 0.28f, 34.0f, 60.0f);
        D2D1_RECT_F rBanner = {
            rCard.left + innerPad,
            rCard.top + innerPad,
            rCard.right - innerPad,
            rCard.top + innerPad + bannerH
        };
        const float bannerRadius = bannerH * 0.22f;

        const bool flash = drawT.isUrgent && (((GetTickCount() / 260) % 2) == 0);

        {
            D2D1_ROUNDED_RECT rrBan = { rBanner, bannerRadius, bannerRadius };
            if (m_panelBrush) {
                m_panelBrush->SetStartPoint(D2D1_POINT_2F{ rBanner.left, rBanner.top });
                m_panelBrush->SetEndPoint(D2D1_POINT_2F{ rBanner.left, rBanner.bottom });
                m_renderTarget->FillRoundedRectangle(&rrBan, m_panelBrush.Get());
            } else {
                m_brush->SetColor(float4(0.03f, 0.03f, 0.04f, 0.88f * globalOpacity));
                m_renderTarget->FillRoundedRectangle(&rrBan, m_brush.Get());
            }

            // Subtle border
            m_brush->SetColor(float4(0.9f, 0.9f, 0.95f, 0.18f * globalOpacity));
            m_renderTarget->DrawRoundedRectangle(&rrBan, m_brush.Get(), 1.5f);

            // Banner text
            if (m_tfBig) {
                m_tfBig->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                m_tfBig->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
            m_brush->SetColor(float4(0.95f, 0.95f, 0.98f, 0.92f * globalOpacity));
            m_text.render(
                m_renderTarget.Get(),
                drawT.isUrgent ? L"TRAFFIC APPROACHING" : L"TRAFFIC",
                m_tfBig.Get(),
                rBanner.left + innerPad,
                rBanner.right - innerPad,
                (rBanner.top + rBanner.bottom) * 0.5f,
                m_brush.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER,
                getGlobalFontSpacing()
            );
        }

        // Main panel: details
        const float gap = std::clamp(cardH * 0.040f, 8.0f, 14.0f);
        D2D1_RECT_F rPanel = {
            rCard.left + innerPad,
            rBanner.bottom + gap,
            rCard.right - innerPad,
            rCard.bottom - innerPad
        };

        if (rPanel.bottom > rPanel.top + 20.0f)
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

            // Build detail strings
            wchar_t line1[256];
            wchar_t line2[256];

            // Line 1: class + car number + name
            {
                std::wstring wClass = toWide(drawT.classShort.empty() ? std::string("FASTER") : drawT.classShort);
                std::wstring wName = toWide(drawT.userName.empty() ? std::string("Car") : drawT.userName);

                swprintf_s(line1, L"%s  #%S  %s", wClass.c_str(), drawT.carNumberStr.c_str(), wName.c_str());
            }

            // Line 2: gap and distance
            {
                const bool showMeters = g_cfg.getBool(m_name, "show_distance_m", true);
                if (showMeters && drawT.distanceBehindM > 0.0f) {
                    swprintf_s(line2, L"Behind: %.1fs  (~%.0fm)", std::max(0.0f, drawT.gapBehindS), std::max(0.0f, drawT.distanceBehindM));
                } else {
                    swprintf_s(line2, L"Behind: %.1fs", std::max(0.0f, drawT.gapBehindS));
                }
            }

            // Text colors (use class color as subtle accent)
            float4 accent = classCol;
            accent.w = 0.95f * globalOpacity;

            m_brush->SetColor(float4(0.95f, 0.95f, 0.98f, 0.92f * globalOpacity));
            if (m_tf) {
                m_tf->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                m_tf->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }

            // Line positions
            const float midY = (rPanel.top + rPanel.bottom) * 0.5f;
            const float y1 = midY - (cardH * 0.12f);
            const float y2 = midY + (cardH * 0.12f);

            m_text.render(
                m_renderTarget.Get(),
                line1,
                m_tf.Get(),
                rPanel.left + innerPad,
                rPanel.right - innerPad,
                y1,
                m_brush.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER,
                getGlobalFontSpacing()
            );

            // Secondary line (accent)
            m_brush->SetColor(accent);
            if (m_tfSmall) {
                m_tfSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                m_tfSmall->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
            m_text.render(
                m_renderTarget.Get(),
                line2,
                m_tfSmall.Get(),
                rPanel.left + innerPad,
                rPanel.right - innerPad,
                y2,
                m_brush.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER,
                getGlobalFontSpacing()
            );
        }

        m_renderTarget->EndDraw();
    }

private:
    struct Target
    {
        int carIdx = -1;
        int classId = 0;
        std::string classShort;
        std::string carNumberStr;
        std::string userName;
        float gapBehindS = 0.0f;      // seconds (positive)
        float distanceBehindM = 0.0f; // meters (approx)
        bool isUrgent = false;
    };

    bool selectBestTarget(Target& out)
    {
        out = Target();

        const bool useStub = StubDataManager::shouldUseStubData();
        if (useStub)
        {
            out.carIdx = 12;
            out.classId = 2;
            out.classShort = "LMP2";
            out.carNumberStr = "12";
            out.userName = "Faster Class";
            out.gapBehindS = 1.3f;
            out.distanceBehindM = 45.0f;
            out.isUrgent = true;
            return true;
        }

        const int selfIdx = ir_session.driverCarIdx;
        if (selfIdx < 0 || selfIdx >= IR_MAX_CARS) return false;

        const bool hideIfSelfPit = g_cfg.getBool(m_name, "hide_if_self_on_pit_road", true);
        if (hideIfSelfPit && ir_OnPitRoad.getBool()) return false;

        const float selfClassEst = ir_session.cars[selfIdx].carClassEstLapTime;
        const int selfClassId = ir_getClassId(selfIdx);

        const float warnGapS = std::max(0.1f, g_cfg.getFloat(m_name, "warn_gap_seconds", 2.5f));
        const float urgentGapS = std::max(0.05f, g_cfg.getFloat(m_name, "urgent_gap_seconds", 1.2f));
        const float fasterMarginS = std::max(0.0f, g_cfg.getFloat(m_name, "faster_class_laptime_margin_s", 1.0f));
        const bool requireDifferentClass = g_cfg.getBool(m_name, "require_different_class", true);
        const bool ignoreCarsOnPitRoad = g_cfg.getBool(m_name, "ignore_cars_on_pit_road", true);

        const float trackLenM = ir_session.trackLengthMeters;
        const float selfPct = std::clamp(ir_LapDistPct.getFloat(), 0.0f, 1.0f);
        const float selfEst = ir_CarIdxEstTime.getFloat(selfIdx);

        // Use an estimate of *our* lap time to normalize wrap correction and distance conversion.
        float lapTimeRef = selfClassEst;
        if (lapTimeRef <= 0.1f) lapTimeRef = ir_estimateLaptime();
        if (lapTimeRef <= 0.1f) lapTimeRef = 120.0f;

        float bestGap = 1e9f;

        for (int i = 0; i < IR_MAX_CARS; ++i)
        {
            if (i == selfIdx) continue;
            const Car& car = ir_session.cars[i];
            if (car.isSpectator || car.carNumber < 0) continue;
            if (car.isPaceCar) continue;
            if (ignoreCarsOnPitRoad && ir_CarIdxOnPitRoad.getBool(i)) continue;

            const int otherClassId = ir_getClassId(i);
            if (requireDifferentClass && otherClassId == selfClassId) continue;

            const float otherClassEst = car.carClassEstLapTime;
            if (selfClassEst > 0.1f && otherClassEst > 0.1f)
            {
                // Must be a "faster" class: lower estimated class lap time.
                if (otherClassEst >= (selfClassEst - fasterMarginS))
                    continue;
            }
            else
            {
                // If class pace is unknown, we can't confidently call it "faster class"
                continue;
            }

            const float otherPct = ir_CarIdxLapDistPct.getFloat(i);
            const float otherEst = ir_CarIdxEstTime.getFloat(i);
            if (otherPct < 0.0f || otherEst <= 0.0f || selfEst <= 0.0f) continue;

            // Delta in seconds, wrap-safe. Positive means other ahead, negative means other behind.
            float delta = otherEst - selfEst;
            const bool wrap = fabsf(otherPct - selfPct) > 0.5f;
            if (wrap)
            {
                // If we're "ahead" in lap pct, and the other is just after S/F, the other is behind.
                delta = (selfPct > otherPct) ? (delta + lapTimeRef) : (delta - lapTimeRef);
            }

            if (delta >= 0.0f) continue; // only behind

            const float gapBehindS = -delta;
            if (gapBehindS <= 0.0f || gapBehindS > warnGapS) continue;

            if (gapBehindS < bestGap)
            {
                bestGap = gapBehindS;
                out.carIdx = i;
                out.classId = otherClassId;
                out.classShort = car.carClassShortName;
                out.carNumberStr = car.carNumberStr.empty() ? std::to_string(car.carNumber) : car.carNumberStr;
                out.userName = car.userName;
                out.gapBehindS = gapBehindS;
                out.distanceBehindM = (trackLenM > 1.0f) ? (trackLenM * (gapBehindS / lapTimeRef)) : 0.0f;
                out.isUrgent = gapBehindS <= urgentGapS;
            }
        }

        return out.carIdx >= 0;
    }

    void ensureStyleBrushes()
    {
        if (!m_renderTarget) return;
        if (m_bgBrush && m_panelBrush) return;

        // Card background gradient (same palette as OverlayPit/OverlayFlags/OverlayFuel)
        {
            D2D1_GRADIENT_STOP stops[3] = {};
            stops[0].position = 0.0f;  stops[0].color = D2D1::ColorF(0.16f, 0.18f, 0.22f, 0.95f);
            stops[1].position = 0.45f; stops[1].color = D2D1::ColorF(0.06f, 0.07f, 0.09f, 0.95f);
            stops[2].position = 1.0f;  stops[2].color = D2D1::ColorF(0.02f, 0.02f, 0.03f, 0.95f);

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

        // Inner panel gradient (same palette as OverlayPit/OverlayFlags/OverlayFuel)
        {
            D2D1_GRADIENT_STOP stops[3] = {};
            stops[0].position = 0.0f;  stops[0].color = D2D1::ColorF(0.08f, 0.09f, 0.11f, 0.92f);
            stops[1].position = 0.55f; stops[1].color = D2D1::ColorF(0.04f, 0.045f, 0.055f, 0.92f);
            stops[2].position = 1.0f;  stops[2].color = D2D1::ColorF(0.02f, 0.02f, 0.03f, 0.92f);

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

private:
    TextCache m_text;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_tf;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_tfSmall;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_tfBig;

    // Styling brushes (cached; recreated on config change / enable)
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_bgBrush;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_panelBrush;

    // Runtime state
    int m_activeCarIdx = -1;
    Target m_active;
    float m_showUntil = 0.0f;
    float m_lastSessionTime = -1.0f;
    float m_animAlpha = 0.0f;
};


