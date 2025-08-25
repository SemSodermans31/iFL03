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

#pragma once

#include "Overlay.h"
#include "iracing.h"
#include "Config.h"
#include "util.h"
#include "stub_data.h"
#include <vector>
#include <algorithm>

class OverlayRadar : public Overlay
{
    public:

        OverlayRadar()
            : Overlay("OverlayRadar")
        {}

        virtual bool canEnableWhileDisconnected() const { return StubDataManager::shouldUseStubData(); }

    protected:

        virtual void onEnable()
        {
            onConfigChanged();
        }

        virtual void onConfigChanged()
        {
            // Load settings
            m_maxRangeM     = g_cfg.getFloat(m_name, "max_range_m", 10.0f);
            m_yellowRangeM  = g_cfg.getFloat(m_name, "yellow_range_m", 5.0f);
            m_redRangeM     = g_cfg.getFloat(m_name, "red_range_m", 2.0f);
            m_carScale      = g_cfg.getFloat(m_name, "car_scale", 1.0f);
            m_showBG        = g_cfg.getBool (m_name, "show_background", true);

            m_text.reset(m_dwriteFactory.Get());

            const std::string font = g_cfg.getString(m_name, "font", "Poppins");
            const float fontSize = g_cfg.getFloat(m_name, "font_size", 14.0f);
            const int fontWeight = g_cfg.getInt(m_name, "font_weight", 500);
            const std::string fontStyleStr = g_cfg.getString(m_name, "font_style", "normal");
            DWRITE_FONT_STYLE fontStyle = DWRITE_FONT_STYLE_NORMAL;
            if (fontStyleStr == "italic") fontStyle = DWRITE_FONT_STYLE_ITALIC;
            else if (fontStyleStr == "oblique") fontStyle = DWRITE_FONT_STYLE_OBLIQUE;

            HRCHECK(m_dwriteFactory->CreateTextFormat( toWide(font).c_str(), NULL, (DWRITE_FONT_WEIGHT)fontWeight, fontStyle, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"en-us", &m_textFormat ));
            m_textFormat->SetParagraphAlignment( DWRITE_PARAGRAPH_ALIGNMENT_CENTER );
            m_textFormat->SetWordWrapping( DWRITE_WORD_WRAPPING_NO_WRAP );
        }

        virtual void onUpdate()
        {
            // Build simplified proximity measures instead of per-car dots
            struct Blip { float dx; float dy; };
            std::vector<Blip> blips; // used only to synthesize preview distances
            blips.reserve(8);

            const bool useStubData = StubDataManager::shouldUseStubData();

            const int selfIdx = useStubData ? 0 : ir_session.driverCarIdx;
            const float selfSpeed = std::max( 5.0f, ir_Speed.getFloat() ); // m/s

            float minAhead = 1e9f, minBehind = 1e9f;
            bool hasLeft = false, hasRight = false;

            if (useStubData)
            {
                // Simple demo: a few cars around
                blips.push_back({ -1.5f,  3.0f }); // front-left
                blips.push_back({  1.0f,  5.0f }); // front-right
                blips.push_back({ -0.8f, -2.0f }); // rear-left
                blips.push_back({  0.6f, -6.0f }); // rear-right

                for (const Blip& b : blips)
                {
                    if (b.dy > 0) minAhead = std::min(minAhead, b.dy);
                    if (b.dy < 0) minBehind = std::min(minBehind, -b.dy);
                    if (b.dx < -0.5f && fabsf(b.dy) <= m_yellowRangeM) hasLeft = true;
                    if (b.dx >  0.5f && fabsf(b.dy) <= m_yellowRangeM) hasRight = true;
                }
            }
            else if (selfIdx >= 0)
            {
                const float L = ir_estimateLaptime();
                const float S = ir_CarIdxEstTime.getFloat(selfIdx);
                for (int i=0; i<IR_MAX_CARS; ++i)
                {
                    if (i == selfIdx) continue;
                    const Car& car = ir_session.cars[i];
                    if (car.isSpectator || car.carNumber < 0) continue;

                    float delta = 0.0f;
                    int lapDelta = ir_CarIdxLap.getInt(i) - ir_CarIdxLap.getInt(selfIdx);
                    const float C = ir_CarIdxEstTime.getFloat(i);
                    const bool wrap = fabsf(ir_CarIdxLapDistPct.getFloat(i) - ir_CarIdxLapDistPct.getFloat(selfIdx)) > 0.5f;
                    if (wrap)
                    {
                        delta     = S > C ? (C-S)+L : (C-S)-L;
                        lapDelta += S > C ? -1 : 1;
                    }
                    else
                    {
                        delta = C - S;
                    }

                    if (lapDelta != 0) continue; // only same lap

                    const float alongM = delta * selfSpeed; // forward(+)/back(-) in meters
                    if (alongM > 0) minAhead = std::min(minAhead, alongM);
                    else            minBehind = std::min(minBehind, -alongM);
                }

                // Side awareness from iRacing aggregate
                int clr = ir_CarLeftRight.getInt();
                hasLeft  = (clr == irsdk_LRCarLeft || clr == irsdk_LR2CarsLeft || clr == irsdk_LRCarLeftRight);
                hasRight = (clr == irsdk_LRCarRight || clr == irsdk_LR2CarsRight || clr == irsdk_LRCarLeftRight);
            }

            // Rendering
            const float globalOpacity = getGlobalOpacity();

            const float2 size = float2((float)m_width, (float)m_height);
            const float cx = size.x * 0.5f;
            const float cy = size.y * 0.5f;
            const float radius = std::min(size.x, size.y) * 0.5f - 2.0f;

            // Colors
            const float4 bgCol        = g_cfg.getFloat4(m_name, "bg_col", float4(0,0,0,0.35f));
            const float4 selfCol      = g_cfg.getFloat4(m_name, "self_col", float4(1,1,1,0.95f));
            const float4 redColRaw    = g_cfg.getFloat4(m_name, "red_col", float4(0.95f,0.2f,0.2f,0.8f));
            const float4 yellowColRaw = g_cfg.getFloat4(m_name, "yellow_col", float4(0.95f,0.8f,0.2f,0.7f));

            float4 redCol = redColRaw; redCol.w *= globalOpacity;
            float4 yellowCol = yellowColRaw; yellowCol.w *= globalOpacity;

            m_renderTarget->BeginDraw();
            m_renderTarget->Clear(float4(0,0,0,0));

            if (m_showBG)
            {
                D2D1_ELLIPSE e = { { cx, cy }, radius, radius };
                m_brush->SetColor(bgCol);
                m_renderTarget->FillEllipse(&e, m_brush.Get());
            }

            // Decide which zones to light up
            const bool frontYellow = (minAhead <= m_yellowRangeM);
            const bool frontRed    = (minAhead <= m_redRangeM);
            const bool rearYellow  = (minBehind <= m_yellowRangeM);
            const bool rearRed     = (minBehind <= m_redRangeM);

            // Side zones: use iRacing side alerts if available; escalate to red if very close ahead/behind
            const bool leftYellow  = hasLeft;
            const bool rightYellow = hasRight;
            const bool leftRed     = hasLeft  && (frontRed || rearRed);
            const bool rightRed    = hasRight && (frontRed || rearRed);

            // Draw proximity rectangles around the car (no opponent dots)
            const float pxPerM = radius / std::max(1.0f, m_maxRangeM);
            const float carWpx = m_carWidthM * pxPerM;
            const float carLpx = m_carLengthM * pxPerM;
            const float halfW = carWpx * 0.5f;
            const float halfL = carLpx * 0.5f;

            auto fillRect = [&](float l, float t, float r, float b, const float4& c){ D2D1_RECT_F rr = { l,t,r,b }; m_brush->SetColor(c); m_renderTarget->FillRectangle(&rr, m_brush.Get()); };

            // Front zones
            if (frontYellow) {
                const float len = m_yellowRangeM * pxPerM;
                fillRect(cx - halfW*0.9f, cy - halfL - len, cx + halfW*0.9f, cy - halfL, yellowCol);
            }
            if (frontRed) {
                const float len = m_redRangeM * pxPerM;
                fillRect(cx - halfW*0.9f, cy - halfL - len, cx + halfW*0.9f, cy - halfL, redCol);
            }

            // Rear zones
            if (rearYellow) {
                const float len = m_yellowRangeM * pxPerM;
                fillRect(cx - halfW*0.9f, cy + halfL, cx + halfW*0.9f, cy + halfL + len, yellowCol);
            }
            if (rearRed) {
                const float len = m_redRangeM * pxPerM;
                fillRect(cx - halfW*0.9f, cy + halfL, cx + halfW*0.9f, cy + halfL + len, redCol);
            }

            // Left/right zones
            if (leftYellow) {
                const float len = m_yellowRangeM * pxPerM;
                fillRect(cx - halfW - len, cy - halfL, cx - halfW, cy + halfL, yellowCol);
            }
            if (leftRed) {
                const float len = m_redRangeM * pxPerM;
                fillRect(cx - halfW - len, cy - halfL, cx - halfW, cy + halfL, redCol);
            }
            if (rightYellow) {
                const float len = m_yellowRangeM * pxPerM;
                fillRect(cx + halfW, cy - halfL, cx + halfW + len, cy + halfL, yellowCol);
            }
            if (rightRed) {
                const float len = m_redRangeM * pxPerM;
                fillRect(cx + halfW, cy - halfL, cx + halfW + len, cy + halfL, redCol);
            }

            // Draw self car glyph
            {
                const float w = carWpx;
                const float h = carLpx;
                D2D1_ROUNDED_RECT rr;
                rr.rect = { cx - w*0.5f, cy - h*0.5f, cx + w*0.5f, cy + h*0.5f };
                rr.radiusX = 3; rr.radiusY = 3;
                float4 sc = selfCol; sc.w *= getGlobalOpacity();
                m_brush->SetColor(sc);
                m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
            }

            m_renderTarget->EndDraw();
        }

        virtual float2 getDefaultSize()
        {
            return float2(180, 180);
        }

        virtual bool hasCustomBackground()
        {
            return true;
        }

    private:
        // Settings
        float m_maxRangeM = 10.0f;
        float m_yellowRangeM = 5.0f;
        float m_redRangeM = 2.0f;
        float m_carScale = 1.0f;
        bool  m_showBG = true;
        float m_carWidthM = 2.0f;
        float m_carLengthM = 4.5f;

        // Text helpers (for future labels)
        Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormat;
        TextCache    m_text;
};


