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
#include <math.h>
#include <string>
#include "Overlay.h"
#include "iracing.h"
#include "Units.h"
#include "Config.h"
#include "ClassColors.h"
#include "util.h"
#include "stub_data.h"

// Dedicated tire overlay: wear/health (0-100), temperature, pressure (psi or bar), laps on tire
// Visual spec: four semicircle gauges in a row (FR, FL, RR, RL) with label above and numeric inside

class OverlayTire : public Overlay
{
    public:

        OverlayTire()
            : Overlay("OverlayTire")
        {}

       #ifdef _DEBUG
       virtual bool    canEnableWhileNotDriving() const { return true; }
       virtual bool    canEnableWhileDisconnected() const { return true; }
       #else
       virtual bool    canEnableWhileDisconnected() const { return StubDataManager::shouldUseStubData(); }
       #endif

    protected:

        struct Gauge
        {
            float cx = 0;   // center x
            float cy = 0;   // center y
            float r  = 0;   // radius
            float w  = 0;   // arc thickness
            std::wstring label; // FR/FL/RR/RL
            D2D1_RECT_F tile = {}; // per-tire card rect
        };

        virtual float2 getDefaultSize()
        {
            return float2(640, 240);
        }

        virtual void onEnable()
        {
            onConfigChanged();
        }

        virtual void onDisable()
        {
            m_text.reset();
            m_bgBrush.Reset();
            m_panelBrush.Reset();
            m_lastStyleRenderTarget = nullptr;
        }

        virtual void onConfigChanged()
        {
            m_text.reset(m_dwriteFactory.Get());

            createGlobalTextFormat(0.85f, m_tfSmall);
            createGlobalTextFormat(1.05f, (int)DWRITE_FONT_WEIGHT_BOLD, "", m_tfMediumBold);
            createGlobalTextFormat(1.25f, (int)DWRITE_FONT_WEIGHT_BOLD, "", m_tfTitle);

            // Target FPS (moderate, tire data changes slowly)
            setTargetFPS(g_cfg.getInt(m_name, "target_fps", 10));

            // Recreate D2D brushes (render target may change)
            m_bgBrush.Reset();
            m_panelBrush.Reset();
            m_lastStyleRenderTarget = nullptr;
        }

        virtual bool hasCustomBackground() const { return true; }

        virtual void onSessionChanged()
        {
            // Reset lap counters at session change
            m_lastLFTiresUsed = ir_LFTiresUsed.getInt();
            m_lastRFTiresUsed = ir_RFTiresUsed.getInt();
            m_lastLRTiresUsed = ir_LRTiresUsed.getInt();
            m_lastRRTiresUsed = ir_RRTiresUsed.getInt();
            m_lapsOnLF = m_lapsOnRF = m_lapsOnLR = m_lapsOnRR = 0;
            m_prevCompletedLap = ir_LapCompleted.getInt();
        }

        virtual void onUpdate()
        {
            // Check if we should only show in pitlane
            const bool showOnlyInPitlane = g_cfg.getBool(m_name, "show_only_in_pitlane", false);
            if (showOnlyInPitlane && !ir_OnPitRoad.getBool()) {
                // Outside pitlane: clear our layer so previous frame doesn't linger
                m_renderTarget->BeginDraw();
                m_renderTarget->Clear(float4(0, 0, 0, 0));
                m_renderTarget->EndDraw();
                return;
            }

            const float globalOpacity = getGlobalOpacity();

            // Default palette aligned with ClassColors (still configurable)
            float4 textCol    = g_cfg.getFloat4(m_name, "text_col",    float4(0.95f, 0.95f, 0.98f, 0.92f));
            float4 goodCol    = g_cfg.getFloat4(m_name, "good_col",    ClassColors::get(3)); // green
            float4 warnCol    = g_cfg.getFloat4(m_name, "warn_col",    ClassColors::get(1)); // yellow-ish
            float4 badCol     = g_cfg.getFloat4(m_name, "bad_col",     ClassColors::get(0)); // red-ish
            float4 outlineCol = g_cfg.getFloat4(m_name, "outline_col", float4(0.80f, 0.82f, 0.86f, 0.28f));

            textCol.a    *= globalOpacity;
            goodCol.a    *= globalOpacity;
            warnCol.a    *= globalOpacity;
            badCol.a     *= globalOpacity;
            outlineCol.a *= globalOpacity;

            // Track laps on tire using TiresUsed counters and completed laps
            const int lapCompleted = ir_LapCompleted.getInt();
            if (lapCompleted > m_prevCompletedLap)
            {
                const int lfUsed = ir_LFTiresUsed.getInt();
                const int rfUsed = ir_RFTiresUsed.getInt();
                const int lrUsed = ir_LRTiresUsed.getInt();
                const int rrUsed = ir_RRTiresUsed.getInt();
                if (lfUsed != m_lastLFTiresUsed) { m_lapsOnLF = 0; m_lastLFTiresUsed = lfUsed; } else { m_lapsOnLF++; }
                if (rfUsed != m_lastRFTiresUsed) { m_lapsOnRF = 0; m_lastRFTiresUsed = rfUsed; } else { m_lapsOnRF++; }
                if (lrUsed != m_lastLRTiresUsed) { m_lapsOnLR = 0; m_lastLRTiresUsed = lrUsed; } else { m_lapsOnLR++; }
                if (rrUsed != m_lastRRTiresUsed) { m_lapsOnRR = 0; m_lastRRTiresUsed = rrUsed; } else { m_lapsOnRR++; }
                m_prevCompletedLap = lapCompleted;
            }

            m_renderTarget->BeginDraw();
            m_renderTarget->Clear(float4(0, 0, 0, 0));

            ensureStyleBrushes();

            const float W = (float)m_width;
            const float H = (float)m_height;
            const float minDim = std::max(1.0f, std::min(W, H));
            const float pad = std::clamp(minDim * 0.045f, 8.0f, 18.0f);
            const float innerPad = std::clamp(minDim * 0.045f, 10.0f, 20.0f);
            const float corner = std::clamp(minDim * 0.070f, 10.0f, 26.0f);

            D2D1_RECT_F rCard = { pad, pad, W - pad, H - pad };
            const float cardH = std::max(1.0f, rCard.bottom - rCard.top);

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

            // Header/banner (dark panel)
            const float bannerH = std::clamp(cardH * 0.20f, 34.0f, 60.0f);
            D2D1_RECT_F rBanner = {
                rCard.left + innerPad,
                rCard.top + innerPad,
                rCard.right - innerPad,
                rCard.top + innerPad + bannerH
            };
            const float bannerRadius = bannerH * 0.22f;
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

                // Title
                if (m_tfTitle) {
                    m_tfTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    m_tfTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                }
                m_brush->SetColor(textCol);
                m_text.render(
                    m_renderTarget.Get(),
                    L"TIRES",
                    m_tfTitle.Get(),
                    rBanner.left,
                    rBanner.right,
                    (rBanner.top + rBanner.bottom) * 0.5f,
                    m_brush.Get(),
                    DWRITE_TEXT_ALIGNMENT_CENTER,
                    getGlobalFontSpacing()
                );
            }

            // Gauge panel area (rest of card)
            const float gap = std::clamp(cardH * 0.035f, 8.0f, 14.0f);
            D2D1_RECT_F rGaugePanel = {
                rCard.left + innerPad,
                rBanner.bottom + gap,
                rCard.right - innerPad,
                rCard.bottom - innerPad
            };
            {
                const float panelCorner = std::clamp(corner * 0.75f, 8.0f, 22.0f);
                D2D1_ROUNDED_RECT rrPanel = { rGaugePanel, panelCorner, panelCorner };
                if (m_panelBrush) {
                    m_panelBrush->SetStartPoint(D2D1_POINT_2F{ rGaugePanel.left, rGaugePanel.top });
                    m_panelBrush->SetEndPoint(D2D1_POINT_2F{ rGaugePanel.left, rGaugePanel.bottom });
                    m_renderTarget->FillRoundedRectangle(&rrPanel, m_panelBrush.Get());
                } else {
                    m_brush->SetColor(float4(0.03f, 0.03f, 0.04f, 0.88f * globalOpacity));
                    m_renderTarget->FillRoundedRectangle(&rrPanel, m_brush.Get());
                }
                m_brush->SetColor(float4(0.9f, 0.9f, 0.95f, 0.12f * globalOpacity));
                m_renderTarget->DrawRoundedRectangle(&rrPanel, m_brush.Get(), 1.5f);
            }

            layoutGauges(rGaugePanel);

            // Draw each gauge (functionality unchanged)
            drawTireGauge(m_gFR, textCol, goodCol, warnCol, badCol, outlineCol,
                          /*wear*/ min3(ir_RFwearL.getFloat(), ir_RFwearM.getFloat(), ir_RFwearR.getFloat()),
                          /*tempC*/ avg3(ir_RFtempCL.getFloat(), ir_RFtempCM.getFloat(), ir_RFtempCR.getFloat()),
                          /*pressKPa*/ ir_RFcoldPressure.getFloat(),
                          /*laps*/ m_lapsOnRF);

            drawTireGauge(m_gFL, textCol, goodCol, warnCol, badCol, outlineCol,
                          min3(ir_LFwearL.getFloat(), ir_LFwearM.getFloat(), ir_LFwearR.getFloat()),
                          avg3(ir_LFtempCL.getFloat(), ir_LFtempCM.getFloat(), ir_LFtempCR.getFloat()),
                          ir_LFcoldPressure.getFloat(),
                          m_lapsOnLF);

            drawTireGauge(m_gRR, textCol, goodCol, warnCol, badCol, outlineCol,
                          min3(ir_RRwearL.getFloat(), ir_RRwearM.getFloat(), ir_RRwearR.getFloat()),
                          avg3(ir_RRtempCL.getFloat(), ir_RRtempCM.getFloat(), ir_RRtempCR.getFloat()),
                          ir_RRcoldPressure.getFloat(),
                          m_lapsOnRR);

            drawTireGauge(m_gRL, textCol, goodCol, warnCol, badCol, outlineCol,
                          min3(ir_LRwearL.getFloat(), ir_LRwearM.getFloat(), ir_LRwearR.getFloat()),
                          avg3(ir_LRtempCL.getFloat(), ir_LRtempCM.getFloat(), ir_LRtempCR.getFloat()),
                          ir_LRcoldPressure.getFloat(),
                          m_lapsOnLR);

            m_renderTarget->EndDraw();
        }

        void layoutGauges(const D2D1_RECT_F& rArea)
        {
            const float w = std::max(1.0f, rArea.right - rArea.left);
            const float h = std::max(1.0f, rArea.bottom - rArea.top);
            const float margin = std::clamp(std::min(w, h) * 0.06f, 10.0f, 18.0f);
            const float tileW = (w - margin * 5) / 4.0f;
            const float tileH = std::max(1.0f, h - margin * 2);

            // Keep legacy center/radius available for the carcass visualization (advanced mode)
            const float radius = std::min(tileW * 0.40f, tileH * 0.32f);
            const float thickness = std::max(4.0f, radius * 0.18f);

            auto make = [&](int idx, const wchar_t* name) {
                Gauge g;
                const float l = rArea.left + margin + tileW * idx + margin * idx;
                const float t = rArea.top + margin;
                g.tile = { l, t, l + tileW, t + tileH };

                g.w = thickness;
                g.r = radius;
                g.cx = (g.tile.left + g.tile.right) * 0.5f;
                g.cy = g.tile.top + tileH * 0.62f;
                g.label = name;
                return g;
            };

            m_gFR = make(0, L"FR");
            m_gFL = make(1, L"FL");
            m_gRR = make(2, L"RR");
            m_gRL = make(3, L"RL");
        }

        // Helpers
        static float min3(float a, float b, float c) { return std::min(a, std::min(b,c)); }
        static float avg3(float a, float b, float c) { return (a+b+c)/3.0f; }

        void drawArcStroke(float cx, float cy, float r, float thickness, float startRad, float endRad, const float4& color)
        {
            Microsoft::WRL::ComPtr<ID2D1PathGeometry> geo;
            Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
            m_d2dFactory->CreatePathGeometry(&geo);
            geo->Open(&sink);

            D2D1_POINT_2F pStart = { cx + cosf(startRad) * r, cy + sinf(startRad) * r };
            D2D1_POINT_2F pEnd   = { cx + cosf(endRad)   * r, cy + sinf(endRad)   * r };
            const float sweep = endRad - startRad;
            const bool largeArc = fabsf(sweep) > 3.14159265f; 
            const D2D1_SWEEP_DIRECTION dir = (sweep >= 0) ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;

            sink->BeginFigure(pStart, D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddArc(D2D1::ArcSegment(pEnd, D2D1::SizeF(r, r), 0.0f, dir, largeArc ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            sink->Close();

            m_brush->SetColor(color);
            m_renderTarget->DrawGeometry(geo.Get(), m_brush.Get(), thickness);
        }

        void drawTireGauge(const Gauge& g, const float4& textCol, const float4& goodCol, const float4& warnCol, const float4& badCol, const float4& outlineCol,
                           float wearPct, float tempC, float pressureKPa, int lapsOnTire)
        {
            // Normalize/convert inputs
            
            float health = std::clamp(wearPct * 100.0f, 0.0f, 100.0f);

            bool imperialUnits = isImperialUnits();
            float temp = imperialUnits ? celsiusToFahrenheit(tempC) : tempC;

            
            float pressure = pressureKPa; // kPa
            const bool showPsi = g_cfg.getBool(m_name, "pressure_use_psi", true);
            if (showPsi)
                pressure = pressureKPa * 0.1450377f; 
            else
                pressure = pressureKPa * 0.01f;       

            // Colors by health thresholds
            const float4 healthCol = (health >= 70.0f) ? goodCol : (health >= 40.0f ? warnCol : badCol);

            const D2D1_RECT_F rt = g.tile;
            const float tileW = std::max(1.0f, rt.right - rt.left);
            const float tileH = std::max(1.0f, rt.bottom - rt.top);
            const float pad = std::clamp(std::min(tileW, tileH) * 0.08f, 8.0f, 14.0f);
            const float corner = std::clamp(std::min(tileW, tileH) * 0.14f, 10.0f, 22.0f);

            // Tile background + border
            {
                D2D1_ROUNDED_RECT rr = { rt, corner, corner };
                m_brush->SetColor(float4(0.05f, 0.055f, 0.07f, 0.55f * getGlobalOpacity()));
                m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
                m_brush->SetColor(float4(0.9f, 0.9f, 0.95f, 0.14f * getGlobalOpacity()));
                m_renderTarget->DrawRoundedRectangle(&rr, m_brush.Get(), 1.5f);
            }

            // Label pill
            const float pillH = std::clamp(tileH * 0.16f, 22.0f, 30.0f);
            const float pillR = pillH * 0.5f;
            D2D1_RECT_F rPill = {
                rt.left + pad,
                rt.top + pad * 0.75f,
                rt.right - pad,
                rt.top + pad * 0.75f + pillH
            };
            {
                D2D1_ROUNDED_RECT rrP = { rPill, pillR, pillR };
                m_brush->SetColor(float4(0.03f, 0.03f, 0.04f, 0.70f * getGlobalOpacity()));
                m_renderTarget->FillRoundedRectangle(&rrP, m_brush.Get());
                m_brush->SetColor(float4(0.9f, 0.9f, 0.95f, 0.16f * getGlobalOpacity()));
                m_renderTarget->DrawRoundedRectangle(&rrP, m_brush.Get(), 1.0f);

                m_brush->SetColor(textCol);
                m_text.render(
                    m_renderTarget.Get(),
                    g.label.c_str(),
                    m_tfSmall.Get(),
                    rPill.left,
                    rPill.right,
                    (rPill.top + rPill.bottom) * 0.5f,
                    m_brush.Get(),
                    DWRITE_TEXT_ALIGNMENT_CENTER,
                    getGlobalFontSpacing()
                );
            }

            // Main temperature (big)
            const float yTemp = rPill.bottom + tileH * 0.28f;
            wchar_t s[64];
            swprintf(s, _countof(s), L"%d\x00B0", (int)(temp + 0.5f));
            m_brush->SetColor(textCol);
            m_text.render(
                m_renderTarget.Get(),
                s,
                m_tfMediumBold.Get(),
                rt.left + pad,
                rt.right - pad,
                yTemp,
                m_brush.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER,
                getGlobalFontSpacing()
            );

            // Secondary line: pressure + laps
            wchar_t sb[64];
            if (showPsi)
                swprintf(sb, _countof(sb), L"PSI %d   L%d", (int)(pressure + 0.5f), lapsOnTire);
            else
                swprintf(sb, _countof(sb), L"BAR %.1f   L%d", pressure, lapsOnTire);
            const float ySub = yTemp + tileH * 0.18f;
            m_brush->SetColor(float4(textCol.x, textCol.y, textCol.z, textCol.w * 0.90f));
            m_text.render(
                m_renderTarget.Get(),
                sb,
                m_tfSmall.Get(),
                rt.left + pad,
                rt.right - pad,
                ySub,
                m_brush.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER,
                getGlobalFontSpacing()
            );

            const float barH = std::clamp(tileH * 0.10f, 10.0f, 14.0f);
            D2D1_RECT_F rBar = {
                rt.left + pad,
                rt.bottom - pad - barH,
                rt.right - pad,
                rt.bottom - pad
            };
            if (rBar.bottom > rBar.top + 4.0f)
            {
                const float barCorner = std::clamp(barH * 0.5f, 5.0f, 10.0f);
                D2D1_ROUNDED_RECT rrBg = { rBar, barCorner, barCorner };

                // Background + border
                m_brush->SetColor(float4(0.02f, 0.02f, 0.03f, 0.70f * getGlobalOpacity()));
                m_renderTarget->FillRoundedRectangle(&rrBg, m_brush.Get());
                m_brush->SetColor(float4(0.9f, 0.9f, 0.95f, 0.18f * getGlobalOpacity()));
                m_renderTarget->DrawRoundedRectangle(&rrBg, m_brush.Get(), 1.0f);

                // Fill
                const float t = std::clamp(health / 100.0f, 0.0f, 1.0f);
                const float fillW = (rBar.right - rBar.left) * t;
                if (fillW > 1.0f) {
                    D2D1_RECT_F rFill = { rBar.left, rBar.top, rBar.left + fillW, rBar.bottom };
                    D2D1_ROUNDED_RECT rrFill = { rFill, barCorner, barCorner };
                    m_brush->SetColor(healthCol);
                    m_renderTarget->FillRoundedRectangle(&rrFill, m_brush.Get());
                }

                // Percent label above bar
                wchar_t hp[32];
                swprintf(hp, _countof(hp), L"%d%%", (int)(health + 0.5f));
                m_brush->SetColor(float4(textCol.x, textCol.y, textCol.z, textCol.w * 0.85f));
                m_text.render(
                    m_renderTarget.Get(),
                    hp,
                    m_tfSmall.Get(),
                    rBar.left,
                    rBar.right,
                    rBar.top - barH * 0.70f,
                    m_brush.Get(),
                    DWRITE_TEXT_ALIGNMENT_CENTER,
                    getGlobalFontSpacing()
                );
            }

            // Advanced carcass visualization
            if (g_cfg.getBool(m_name, "advanced_mode", true))
            {
                drawCarcassBars(g, tempC);
            }
        }

        float4 tempToColorC(float tempC)
        {
            const float cool = g_cfg.getFloat(m_name, "temp_cool_c", 60.0f);
            const float opt  = g_cfg.getFloat(m_name, "temp_opt_c", 85.0f);
            const float hot  = g_cfg.getFloat(m_name, "temp_hot_c", 105.0f);
            float4 c;
            if (tempC <= cool)
            {
                c = float4(0.30f, 0.55f, 1.00f, 0.90f);
            }
            else if (tempC < opt)
            {
                const float t = (tempC - cool) / (opt - cool);
                c = float4(0.30f + t*(0.00f-0.30f), 0.55f + t*(0.80f-0.55f), 1.00f + t*(0.00f-1.00f), 0.90f);
            }
            else if (tempC <= hot)
            {
                const float t = (tempC - opt) / (hot - opt);
                c = float4(0.00f + t*(0.90f-0.00f), 0.80f + t*(0.20f-0.80f), 0.00f + t*(0.20f-0.00f), 0.90f);
            }
            else
            {
                c = float4(0.90f, 0.20f, 0.20f, 0.90f);
            }
            // Apply global opacity consistently with the rest of the overlay
            c.w *= getGlobalOpacity();
            return c;
        }

        void drawCarcassBars(const Gauge& g, float tempCavg)
        {
            float cl=0, cm=0, cr=0;
            if (g.label == L"FR") { cl = ir_RFtempCL.getFloat(); cm = ir_RFtempCM.getFloat(); cr = ir_RFtempCR.getFloat(); }
            else if (g.label == L"FL") { cl = ir_LFtempCL.getFloat(); cm = ir_LFtempCM.getFloat(); cr = ir_LFtempCR.getFloat(); }
            else if (g.label == L"RR") { cl = ir_RRtempCL.getFloat(); cm = ir_RRtempCM.getFloat(); cr = ir_RRtempCR.getFloat(); }
            else if (g.label == L"RL") { cl = ir_LRtempCL.getFloat(); cm = ir_LRtempCM.getFloat(); cr = ir_LRtempCR.getFloat(); }

            const D2D1_RECT_F rt = g.tile;
            const float tileW = std::max(1.0f, rt.right - rt.left);
            const float tileH = std::max(1.0f, rt.bottom - rt.top);
            const float pad = std::clamp(std::min(tileW, tileH) * 0.08f, 8.0f, 14.0f);

            const float stripH = std::clamp(tileH * 0.10f, 10.0f, 14.0f);
            const float y = rt.top + pad * 0.75f + std::clamp(tileH * 0.16f, 22.0f, 30.0f) + tileH * 0.10f;
            D2D1_RECT_F rStrip = { rt.left + pad, y - stripH * 0.5f, rt.right - pad, y + stripH * 0.5f };

            const float totalW = std::max(1.0f, rStrip.right - rStrip.left);
            const float gap = std::clamp(totalW * 0.03f, 3.0f, 6.0f);
            const float midW = totalW * 0.42f;
            const float sideW = (totalW - midW - gap * 2.0f) * 0.5f;
            const float r = std::clamp(stripH * 0.45f, 4.0f, 10.0f);

            D2D1_RECT_F rL = { rStrip.left, rStrip.top, rStrip.left + sideW, rStrip.bottom };
            D2D1_RECT_F rM = { rL.right + gap, rStrip.top, rL.right + gap + midW, rStrip.bottom };
            D2D1_RECT_F rR = { rM.right + gap, rStrip.top, rStrip.right, rStrip.bottom };

            {
                D2D1_ROUNDED_RECT rrBg = { rStrip, r, r };
                m_brush->SetColor(float4(0.02f, 0.02f, 0.03f, 0.55f * getGlobalOpacity()));
                m_renderTarget->FillRoundedRectangle(&rrBg, m_brush.Get());
                m_brush->SetColor(float4(0.9f, 0.9f, 0.95f, 0.14f * getGlobalOpacity()));
                m_renderTarget->DrawRoundedRectangle(&rrBg, m_brush.Get(), 1.0f);
            }

            // Fill each segment
            m_brush->SetColor(tempToColorC(cl));
            { D2D1_ROUNDED_RECT rr = { rL, r, r }; m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get()); }
            m_brush->SetColor(tempToColorC(cm));
            { D2D1_ROUNDED_RECT rr = { rM, r, r }; m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get()); }
            m_brush->SetColor(tempToColorC(cr));
            { D2D1_ROUNDED_RECT rr = { rR, r, r }; m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get()); }
        }

    protected:
        Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_tfSmall;
        Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_tfMediumBold;
        Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_tfTitle;

        // Styling brushes (cached; recreated on config change / enable; also reset when render target changes)
        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_bgBrush;
        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_panelBrush;
        ID2D1RenderTarget* m_lastStyleRenderTarget = nullptr;

        Gauge m_gFR, m_gFL, m_gRR, m_gRL;
        TextCache m_text;

        int m_lastLFTiresUsed = 0;
        int m_lastRFTiresUsed = 0;
        int m_lastLRTiresUsed = 0;
        int m_lastRRTiresUsed = 0;
        int m_prevCompletedLap = 0;
        int m_lapsOnLF = 0, m_lapsOnRF = 0, m_lapsOnLR = 0, m_lapsOnRR = 0;

    private:
        void ensureStyleBrushes()
        {
            if (!m_renderTarget) return;

            // Handle render-target recreation (e.g., during live resize)
            if (m_lastStyleRenderTarget != m_renderTarget.Get()) {
                m_bgBrush.Reset();
                m_panelBrush.Reset();
                m_lastStyleRenderTarget = m_renderTarget.Get();
            }

            if (m_bgBrush && m_panelBrush) return;

            // Card background gradient (same palette as OverlayPit / OverlayFlags)
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
                    (void)m_renderTarget->CreateLinearGradientBrush(props, stopCollection.Get(), m_bgBrush.GetAddressOf());
                }
            }

            // Inner panel gradient (same palette as OverlayPit / OverlayFlags)
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
                    (void)m_renderTarget->CreateLinearGradientBrush(props, stopCollection.Get(), m_panelBrush.GetAddressOf());
                }
            }
        }
};

