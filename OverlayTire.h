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
        };

        virtual float2 getDefaultSize()
        {
            // Wide, short bar for 4 gauges in a row; a bit taller to allow advanced bars
            return float2(600, 190);
        }

        virtual void onEnable()
        {
            onConfigChanged();
        }

        virtual void onDisable()
        {
            m_text.reset();
            m_backgroundBitmap.Reset();
            m_backgroundTarget.Reset();
        }

        virtual void onConfigChanged()
        {
            m_text.reset(m_dwriteFactory.Get());
            createGlobalTextFormat(0.7f, m_tfSmall);
            createGlobalTextFormat(1.0f, (int)DWRITE_FONT_WEIGHT_BOLD, "", m_tfMediumBold);
            createGlobalTextFormat(1.4f, (int)DWRITE_FONT_WEIGHT_BOLD, "", m_tfTitle);

            // Target FPS (moderate, tire data changes slowly)
            setTargetFPS(g_cfg.getInt(m_name, "target_fps", 10));

            // Create cached background bitmap with rounded corners
            {
                m_renderTarget->CreateCompatibleRenderTarget(&m_backgroundTarget);
                m_backgroundTarget->BeginDraw();
                m_backgroundTarget->Clear(float4(0, 0, 0, 0)); // Transparent background first

                // Draw rounded rectangle background
                const float cornerRadius = g_cfg.getFloat(m_name, "corner_radius", 6.0f);
                const float4 bgColor = g_cfg.getFloat4(m_name, "global_background_col", float4(0, 0, 0, 0.8f));
                const float w = (float)m_width;
                const float h = (float)m_height;

                Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> bgBrush;
                m_backgroundTarget->CreateSolidColorBrush(bgColor, &bgBrush);

                D2D1_ROUNDED_RECT rr;
                rr.rect = D2D1::RectF(0.5f, 0.5f, w - 0.5f, h - 0.5f);
                rr.radiusX = cornerRadius;
                rr.radiusY = cornerRadius;

                m_backgroundTarget->FillRoundedRectangle(&rr, bgBrush.Get());
                m_backgroundTarget->EndDraw();
                m_backgroundTarget->GetBitmap(&m_backgroundBitmap);
            }

            layoutGauges();
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

            const float4 textCol   = g_cfg.getFloat4(m_name, "text_col",   float4(1,1,1,0.9f));
            const float4 goodCol   = g_cfg.getFloat4(m_name, "good_col",   float4(0.0f,0.8f,0.0f,0.85f));
            const float4 warnCol   = g_cfg.getFloat4(m_name, "warn_col",   float4(1.0f,0.7f,0.0f,0.9f));
            const float4 badCol    = g_cfg.getFloat4(m_name, "bad_col",    float4(0.9f,0.2f,0.2f,0.9f));
            const float4 outlineCol= g_cfg.getFloat4(m_name, "outline_col",float4(0.7f,0.7f,0.7f,0.9f));

            // Foreground elements (text, gauges) should NOT be affected by global opacity
            const float4 finalTextCol = textCol;

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

            // Draw cached background with global opacity applied ONLY to background
            m_renderTarget->DrawBitmap(
                m_backgroundBitmap.Get(),
                nullptr,
                getGlobalOpacity(),
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                nullptr
            );

            // Draw title
            m_brush->SetColor(finalTextCol);
            m_text.render(m_renderTarget.Get(), L"Pitwall Tire Wear", m_tfTitle.Get(),
                         0, (float)m_width, 25.0f, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);

            // Draw each gauge
            drawTireGauge(m_gFR, finalTextCol, goodCol, warnCol, badCol, outlineCol,
                          /*wear*/ min3(ir_RFwearL.getFloat(), ir_RFwearM.getFloat(), ir_RFwearR.getFloat()),
                          /*tempC*/ avg3(ir_RFtempCL.getFloat(), ir_RFtempCM.getFloat(), ir_RFtempCR.getFloat()),
                          /*pressKPa*/ ir_RFcoldPressure.getFloat(),
                          /*laps*/ m_lapsOnRF);

            drawTireGauge(m_gFL, finalTextCol, goodCol, warnCol, badCol, outlineCol,
                          min3(ir_LFwearL.getFloat(), ir_LFwearM.getFloat(), ir_LFwearR.getFloat()),
                          avg3(ir_LFtempCL.getFloat(), ir_LFtempCM.getFloat(), ir_LFtempCR.getFloat()),
                          ir_LFcoldPressure.getFloat(),
                          m_lapsOnLF);

            drawTireGauge(m_gRR, finalTextCol, goodCol, warnCol, badCol, outlineCol,
                          min3(ir_RRwearL.getFloat(), ir_RRwearM.getFloat(), ir_RRwearR.getFloat()),
                          avg3(ir_RRtempCL.getFloat(), ir_RRtempCM.getFloat(), ir_RRtempCR.getFloat()),
                          ir_RRcoldPressure.getFloat(),
                          m_lapsOnRR);

            drawTireGauge(m_gRL, finalTextCol, goodCol, warnCol, badCol, outlineCol,
                          min3(ir_LRwearL.getFloat(), ir_LRwearM.getFloat(), ir_LRwearR.getFloat()),
                          avg3(ir_LRtempCL.getFloat(), ir_LRtempCM.getFloat(), ir_LRtempCR.getFloat()),
                          ir_LRcoldPressure.getFloat(),
                          m_lapsOnLR);

            m_renderTarget->EndDraw();
        }

        void layoutGauges()
        {
            const float w = (float)m_width;
            const float h = (float)m_height;
            const float margin = 12.0f;
            const float gaugeWidth = (w - margin*5) / 4.0f;
            const float radius = std::min(gaugeWidth*0.45f, h*0.36f);
            const float thickness = std::max(4.0f, radius * 0.18f);
            const float titleHeight = 25.0f + 15.0f; 
            const float availableHeight = h - titleHeight - margin;
            const float cy = titleHeight + availableHeight * 0.60f; 

            auto make = [&](int idx, const wchar_t* name){
                Gauge g; g.w = thickness; g.r = radius; g.cy = cy; g.label = name;
                const float cx = margin + gaugeWidth * (idx + 0.5f) + margin * idx;
                g.cx = cx;
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

            
            auto deg2rad = [](float d){ return d * (3.14159265358979323846f/180.0f); };
            drawArcStroke(g.cx, g.cy, g.r, g.w, deg2rad(180.0f), deg2rad(360.0f), outlineCol);

            
            const float start = (3.14159265358979323846f);
            const float end   = (3.14159265358979323846f) + (3.14159265358979323846f) * (std::clamp(health,0.0f,100.0f)/100.0f);
            drawArcStroke(g.cx, g.cy, g.r, g.w, start, end, healthCol);

            
            m_brush->SetColor(textCol);
            m_text.render(m_renderTarget.Get(), g.label.c_str(), m_tfSmall.Get(), g.cx - g.r, g.cx + g.r, g.cy - g.r - 20, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);

            // Center numeric: temperature (with extra padding when advanced mode is enabled)
            const bool advancedMode = g_cfg.getBool(m_name, "advanced_mode", true);
            const float tempVerticalOffset = advancedMode ? g.w * 3.5f : g.w * 0.2f; 

            wchar_t s[64];
            swprintf(s, _countof(s), L"%d\x00B0", (int)(temp + 0.5f));
            m_text.render(m_renderTarget.Get(), s, m_tfMediumBold.Get(), g.cx - g.r*0.9f, g.cx + g.r*0.9f, g.cy + tempVerticalOffset, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);

            // Bottom row: pressure + laps (with more bottom padding)
            wchar_t sb[64];
            if (showPsi)
                swprintf(sb, _countof(sb), L"PSI %d  L% d", (int)(pressure + 0.5f), lapsOnTire);
            else
                swprintf(sb, _countof(sb), L"BAR %.1f  L% d", pressure, lapsOnTire);
            m_text.render(m_renderTarget.Get(), sb, m_tfSmall.Get(), g.cx - g.r, g.cx + g.r, g.cy + tempVerticalOffset * 2.0f, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);

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
            // Foreground temperature bars should ignore global opacity
            return c;
        }

        void drawCarcassBars(const Gauge& g, float tempCavg)
        {
            // Individual carcass temps per tire
            float cl=0, cm=0, cr=0;
            if (g.label == L"FR") { cl = ir_RFtempCL.getFloat(); cm = ir_RFtempCM.getFloat(); cr = ir_RFtempCR.getFloat(); }
            else if (g.label == L"FL") { cl = ir_LFtempCL.getFloat(); cm = ir_LFtempCM.getFloat(); cr = ir_LFtempCR.getFloat(); }
            else if (g.label == L"RR") { cl = ir_RRtempCL.getFloat(); cm = ir_RRtempCM.getFloat(); cr = ir_RRtempCR.getFloat(); }
            else if (g.label == L"RL") { cl = ir_LRtempCL.getFloat(); cm = ir_LRtempCM.getFloat(); cr = ir_LRtempCR.getFloat(); }

            const float totalWidth = g.r * 0.65f;
            const float centerX = g.cx;
            
            // Vertical position
            const float baseY = g.cy - g.r * 0.1f;

            // Middle section
            const float midWidth = totalWidth * 0.4f;
            const float midHeight = g.w * 5.0f;        

            // Left and right sections
            const float sideWidth = totalWidth * 0.2f;
            const float sideHeight = g.w * 5.0f;
            const float uniformRadius = sideWidth * 0.3f;

            // Draw middle section
            m_brush->SetColor(tempToColorC(cm));
            D2D1_RECT_F midRect = {
                centerX - midWidth/2, baseY - midHeight/2,
                centerX + midWidth/2, baseY + midHeight/2
            };
            m_renderTarget->FillRectangle(&midRect, m_brush.Get());

            // Draw left section
            m_brush->SetColor(tempToColorC(cl));
            D2D1_ROUNDED_RECT leftRect = {
                D2D1::RectF(centerX - totalWidth/2, baseY - sideHeight/2, centerX - totalWidth/2 + sideWidth, baseY + sideHeight/2),
                uniformRadius, uniformRadius
            };
            m_renderTarget->FillRoundedRectangle(&leftRect, m_brush.Get());

            // Draw right section
            m_brush->SetColor(tempToColorC(cr));
            D2D1_ROUNDED_RECT rightRect = {
                D2D1::RectF(centerX + totalWidth/2 - sideWidth, baseY - sideHeight/2, centerX + totalWidth/2, baseY + sideHeight/2),
                uniformRadius, uniformRadius
            };
            m_renderTarget->FillRoundedRectangle(&rightRect, m_brush.Get());
        }

    protected:
        Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_tfSmall;
        Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_tfMediumBold;
        Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_tfTitle;

        Microsoft::WRL::ComPtr<ID2D1BitmapRenderTarget> m_backgroundTarget;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_backgroundBitmap;

        Gauge m_gFR, m_gFL, m_gRR, m_gRL;
        TextCache m_text;

        int m_lastLFTiresUsed = 0;
        int m_lastRFTiresUsed = 0;
        int m_lastLRTiresUsed = 0;
        int m_lastRRTiresUsed = 0;
        int m_prevCompletedLap = 0;
        int m_lapsOnLF = 0, m_lapsOnRF = 0, m_lapsOnLR = 0, m_lapsOnRR = 0;
};

