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
            // Reset runtime state to avoid stale indicators on enable
            m_frontDistSm = 1e9f;
            m_rearDistSm  = 1e9f;
            m_frontRedUntil = 0.0f;
            m_rearRedUntil  = 0.0f;
            m_frontYellowUntil = 0.0f;
            m_rearYellowUntil  = 0.0f;
            m_lastSessionTime = -1.0f;
            m_radarOpacity = 0.1f;
        }

        virtual void onConfigChanged()
        {
            // Load settings - using fixed ranges for Racelab-style radar
            m_maxRangeM     = 8.0f;  // Fixed 8m detection range
            m_yellowRangeM  = 8.0f;  // Yellow zone from 8m to 2m
            m_redRangeM     = 2.0f;  // Red zone from 2m to 0m
            m_carScale      = g_cfg.getFloat(m_name, "car_scale", 1.0f);
            m_showBG        = g_cfg.getBool (m_name, "show_background", true);

        }

        virtual void onUpdate()
        {
            struct Blip { float dx; float dy; };
            std::vector<Blip> blips; // used only to synthesize preview distances
            blips.reserve(8);

            const bool useStubData = StubDataManager::shouldUseStubData();
            if (!useStubData && !ir_hasValidDriver()) {
                return;
            }

            const int selfIdx = useStubData ? 0 : ir_session.driverCarIdx;
            const float selfSpeed = std::max( 5.0f, ir_Speed.getFloat() ); // m/s

            // Raw, instantaneous measures this frame
            float minAheadNow = 1e9f, minBehindNow = 1e9f;
            bool hasLeft = false, hasRight = false;
            float minLeftDist = 1e9f, minRightDist = 1e9f; 
            float leftCarPos = 0.0f, rightCarPos = 0.0f;   

            if (useStubData)
            {
                // Enhanced demo: cars positioned to demonstrate lateral animation
                blips.push_back({ -1.5f,  1.0f });
                blips.push_back({  2.0f,  3.0f });
                blips.push_back({ -1.2f, -1.5f });
                blips.push_back({  1.8f, -4.0f }); 

                for (const Blip& b : blips)
                {
                    if (b.dy > 0) minAheadNow = std::min(minAheadNow, b.dy);
                    if (b.dy < 0) minBehindNow = std::min(minBehindNow, -b.dy);
                    if (b.dx < -0.5f && fabsf(b.dy) <= 2.0f) {
                        hasLeft = true;
                        minLeftDist = std::min(minLeftDist, -b.dx);
                        leftCarPos = std::max(leftCarPos, std::min(1.0f, (b.dx + 2.0f) / 4.0f)); // Scale to -1 to 1 range
                    }
                    if (b.dx >  0.5f && fabsf(b.dy) <= 2.0f) {
                        hasRight = true;
                        minRightDist = std::min(minRightDist, b.dx);
                        rightCarPos = std::min(rightCarPos, std::max(-1.0f, (b.dx - 2.0f) / 4.0f)); // Scale to -1 to 1 range
                    }
                }
            }
            else if (selfIdx >= 0)
            {
                const float trackLenM = ir_session.trackLengthMeters;
                const float selfPct = ir_CarIdxLapDistPct.getFloat(selfIdx);
                const float selfEst = ir_CarIdxEstTime.getFloat(selfIdx);
                for (int i=0; i<IR_MAX_CARS; ++i)
                {
                    if (i == selfIdx) continue;
                    const Car& car = ir_session.cars[i];
                    if (car.isSpectator || car.carNumber < 0) continue;
                    // Ignore cars that are on pit road between the cones
                    if (ir_CarIdxOnPitRoad.getBool(i)) continue;

                    float delta = 0.0f;
                    int lapDelta = ir_CarIdxLap.getInt(i) - ir_CarIdxLap.getInt(selfIdx);

                    // Prefer lap percent * track length when available, otherwise fallback to EstTime * speed
                    float alongM = 0.0f; // forward(+)/back(-) in meters
                    const float otherPct = ir_CarIdxLapDistPct.getFloat(i);
                    if (trackLenM > 0.1f)
                    {
                        float dPct = otherPct - selfPct;
                        if (fabsf(dPct) > 0.5f) dPct += (dPct > 0 ? -1.0f : 1.0f);
                        alongM = dPct * trackLenM;
                    }
                    else
                    {
                        const float otherEst = ir_CarIdxEstTime.getFloat(i);
                        const bool wrap = fabsf(otherPct - selfPct) > 0.5f;
                        if (wrap)
                        {
                            const float L = ir_estimateLaptime();
                            delta     = selfEst > otherEst ? (otherEst-selfEst)+L : (otherEst-selfEst)-L;
                            lapDelta += selfEst > otherEst ? -1 : 1;
                        }
                        else
                        {
                            delta = otherEst - selfEst;
                        }
                        alongM = delta * selfSpeed;
                    }

                    // Account for own car half-length to avoid early triggers
                    const float halfLen = m_carLengthM * 0.5f;
                    if (alongM > 0) minAheadNow = std::min(minAheadNow, std::max(0.0f, alongM - halfLen));
                    else            minBehindNow = std::min(minBehindNow, std::max(0.0f, -alongM - halfLen));
                }

                // Side awareness from iRacing aggregate - enhanced lateral positioning
                int clr = ir_CarLeftRight.getInt();
                hasLeft  = (clr == irsdk_LRCarLeft || clr == irsdk_LR2CarsLeft || clr == irsdk_LRCarLeftRight);
                hasRight = (clr == irsdk_LRCarRight || clr == irsdk_LR2CarsRight || clr == irsdk_LRCarLeftRight);

                // Estimate lateral positions and distances based on iRacing side detection
                if (hasLeft) {
                    minLeftDist = 1.5f;   
                    // Estimate lateral position - cars detected on left are typically alongside or slightly ahead/behind
                    leftCarPos = 0.0f;    
                    // For 2 cars on left, spread them out a bit
                    if (clr == irsdk_LR2CarsLeft) {
                        leftCarPos = -0.3f; 
                    }
                }
                if (hasRight) {
                    minRightDist = 1.5f;  
                    // Estimate lateral position - cars detected on right are typically alongside or slightly ahead/behind
                    rightCarPos = 0.0f;   
                    // For 2 cars on right, spread them out a bit
                    if (clr == irsdk_LR2CarsRight) {
                        rightCarPos = 0.3f; 
                    }
                }
            }

            // Smoothing and sticky timers
            const float now = ir_nowf();

            // Compute frame delta in seconds (used for smoothing and fade animation)
            float dt = 0.0f;
            if (m_lastSessionTime >= 0.0f)
            {
                dt = now - m_lastSessionTime;
                if (dt < 0.0f) dt = 0.0f;
                if (dt > 0.5f) dt = 0.5f;
            }

            if (m_lastSessionTime >= 0.0f && now + 0.001f < m_lastSessionTime)
            {
                m_frontDistSm = 1e9f;
                m_rearDistSm  = 1e9f;
                m_frontRedUntil = 0.0f;
                m_rearRedUntil  = 0.0f;
                m_frontYellowUntil = 0.0f;
                m_rearYellowUntil  = 0.0f;
            }
            m_lastSessionTime = now;
            auto smooth = [](float prev, float cur, float alpha){ return (prev > 1e8f) ? cur : (prev + alpha * (cur - prev)); };
            m_frontDistSm = smooth(m_frontDistSm, minAheadNow, 0.3f);
            m_rearDistSm  = smooth(m_rearDistSm,  minBehindNow, 0.3f);

            const float globalOpacity = getGlobalOpacity();

            const float2 size = float2((float)m_width, (float)m_height);
            const float cx = size.x * 0.5f;
            const float cy = size.y * 0.5f;
            const float radius = std::min(size.x, size.y) * 0.5f - 2.0f;

            // Determine whether any opponents are close enough to influence visibility
            const float fadeTriggerRange = m_yellowRangeM + 2.0f; // Start fade-in slightly before yellow zone
            const bool hasOpponentsInFadeRange =
                (m_frontDistSm <= fadeTriggerRange || m_rearDistSm <= fadeTriggerRange || hasLeft || hasRight);

            // Animate radar-specific opacity between 0.1 (idle) and 1.0 (active) based on nearby opponents
            const float minRadarOpacity = 0.1f;
            const float maxRadarOpacity = 1.0f;
            const float targetOpacity = hasOpponentsInFadeRange ? maxRadarOpacity : minRadarOpacity;
            if (dt > 0.0f)
            {
                // Use faster fade-in and slightly slower fade-out for responsiveness
                const float fadeInRate  = 5.0f; // per second
                const float fadeOutRate = 3.0f; // per second
                const float rate = (targetOpacity > m_radarOpacity) ? fadeInRate : fadeOutRate;
                const float maxStep = rate * dt;
                float delta = targetOpacity - m_radarOpacity;
                if (fabsf(delta) <= maxStep) m_radarOpacity = targetOpacity;
                else                          m_radarOpacity += (delta > 0.0f ? maxStep : -maxStep);
            }
            else
            {
                m_radarOpacity = targetOpacity;
            }
            if (m_radarOpacity < minRadarOpacity) m_radarOpacity = minRadarOpacity;
            if (m_radarOpacity > maxRadarOpacity) m_radarOpacity = maxRadarOpacity;

            const float effectiveOpacity = globalOpacity * m_radarOpacity;

            // Colors
            float4 bgCol        = g_cfg.getFloat4(m_name, "bg_col", float4(0,0,0,0.35f));
            const float4 selfCol      = g_cfg.getFloat4(m_name, "self_col", float4(1,1,1,0.95f));
            const float4 redColRaw    = g_cfg.getFloat4(m_name, "red_col", float4(0.95f,0.2f,0.2f,0.8f));
            const float4 yellowColRaw = g_cfg.getFloat4(m_name, "yellow_col", float4(0.95f,0.8f,0.2f,0.7f));

            float4 redCol = redColRaw;    redCol.w    *= effectiveOpacity;
            float4 yellowCol = yellowColRaw; yellowCol.w *= effectiveOpacity;
            bgCol.w *= effectiveOpacity;

            m_renderTarget->BeginDraw();
            m_renderTarget->Clear(float4(0,0,0,0));

            if (m_showBG)
            {
                D2D1_ELLIPSE e = { { cx, cy }, radius, radius };
                m_brush->SetColor(bgCol);
                m_renderTarget->FillEllipse(&e, m_brush.Get());
            }

            const bool hasOpponentsInRange = (m_frontDistSm <= m_yellowRangeM || m_rearDistSm <= m_yellowRangeM);

            bool frontYellowInst = hasOpponentsInRange && (m_frontDistSm <= m_yellowRangeM && m_frontDistSm > m_redRangeM);
            bool frontRedInst    = hasOpponentsInRange && (m_frontDistSm <= m_redRangeM);
            bool rearYellowInst  = hasOpponentsInRange && (m_rearDistSm  <= m_yellowRangeM && m_rearDistSm  > m_redRangeM);
            bool rearRedInst     = hasOpponentsInRange && (m_rearDistSm  <= m_redRangeM);

            
            const float stickRed = 0.20f;   // seconds
            const float stickYellow = 0.15f;
            if (frontRedInst)   m_frontRedUntil   = std::max(m_frontRedUntil,   now + stickRed);
            if (rearRedInst)    m_rearRedUntil    = std::max(m_rearRedUntil,    now + stickRed);
            if (frontYellowInst) m_frontYellowUntil = std::max(m_frontYellowUntil, now + stickYellow);
            if (rearYellowInst)  m_rearYellowUntil  = std::max(m_rearYellowUntil,  now + stickYellow);

            const bool frontRed   = now <= m_frontRedUntil   || frontRedInst;
            const bool rearRed    = now <= m_rearRedUntil    || rearRedInst;
            const bool frontYellow= now <= m_frontYellowUntil|| frontYellowInst;
            const bool rearYellow = now <= m_rearYellowUntil || rearYellowInst;


            const bool leftRed     = hasLeft;
            const bool rightRed    = hasRight;
            const bool leftYellow  = false;  
            const bool rightYellow = false; 

            // Draw guide lines and proximity zones
            const float pxPerM = radius / std::max(1.0f, m_maxRangeM);
            const float carWpx = m_carWidthM * pxPerM;
            const float carLpx = m_carLengthM * pxPerM;
            const float halfW = carWpx * 0.5f;
            const float halfL = carLpx * 0.5f;

            auto fillRect = [&](float l, float t, float r, float b, const float4& c){ D2D1_RECT_F rr = { l,t,r,b }; m_brush->SetColor(c); m_renderTarget->FillRectangle(&rr, m_brush.Get()); };
            auto drawLine = [&](float x1, float y1, float x2, float y2, const float4& c, float width = 1.0f) {
                m_brush->SetColor(c);
                m_renderTarget->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), m_brush.Get(), width);
            };

            // Draw guide lines with fading opacity
            const float4 guideLineCol = float4(1.0f, 1.0f, 1.0f, 0.5f * effectiveOpacity);
            
            // Vertical line: guide at yellow range
            const float frontLineY = cy - halfL - (m_yellowRangeM * pxPerM);
            const float rearLineY = cy + halfL + (m_yellowRangeM * pxPerM);
            drawLine(cx, cy - halfL, cx, frontLineY, guideLineCol, 1.5f); 
            drawLine(cx, cy + halfL, cx, rearLineY, guideLineCol, 1.5f);  
            
            // Horizontal lines: guide at red range left and right at car front/back
            const float leftLineX = cx - (m_redRangeM * pxPerM);
            const float rightLineX = cx + (m_redRangeM * pxPerM);
            drawLine(leftLineX, cy - halfL, rightLineX, cy - halfL, guideLineCol, 1.5f); 
            drawLine(leftLineX, cy + halfL, rightLineX, cy + halfL, guideLineCol, 1.5f); 

            // Helpers for radial gradient wedges fading to the outside of the circle
            auto makeRadialBrush = [&](float innerR, float outerR, const float4& baseCol, Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush>& outBrush)
            {
                const float a0 = baseCol.w; // base alpha already includes globalOpacity
                const float innerPos = std::max(0.0f, std::min(1.0f, innerR / radius));
                const float outerPos = std::max(innerPos, std::min(1.0f, outerR / radius));

                D2D1_GRADIENT_STOP stops[3];
                stops[0].position = 0.0f;            stops[0].color = D2D1::ColorF(baseCol.x, baseCol.y, baseCol.z, 0.0f);
                stops[1].position = innerPos;        stops[1].color = D2D1::ColorF(baseCol.x, baseCol.y, baseCol.z, a0);
                stops[2].position = outerPos;        stops[2].color = D2D1::ColorF(baseCol.x, baseCol.y, baseCol.z, 0.0f);

                Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> gsc;
                m_renderTarget->CreateGradientStopCollection(stops, 3, &gsc);

                D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES props = D2D1::RadialGradientBrushProperties(D2D1::Point2F(cx, cy), D2D1::Point2F(0,0), radius, radius);
                D2D1_BRUSH_PROPERTIES brushProps = D2D1::BrushProperties(1.0f, D2D1::Matrix3x2F::Identity());
                m_renderTarget->CreateRadialGradientBrush(props, brushProps, gsc.Get(), &outBrush);
            };

            auto polarPoint = [&](float r, float ang) -> D2D1_POINT_2F {
                return D2D1::Point2F(cx + r * sinf(ang), cy - r * cosf(ang));
            };

            auto fillRingSector = [&](float angCenter, float halfAng, float innerR, float outerR, ID2D1Brush* brush)
            {
                Microsoft::WRL::ComPtr<ID2D1PathGeometry> geo;
                Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
                m_d2dFactory->CreatePathGeometry(&geo);
                geo->Open(&sink);

                const float a0 = angCenter - halfAng;
                const float a1 = angCenter + halfAng;

                sink->BeginFigure(polarPoint(outerR, a0), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddArc(D2D1::ArcSegment(polarPoint(outerR, a1), D2D1::SizeF(outerR, outerR), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
                sink->AddLine(polarPoint(innerR, a1));
                sink->AddArc(D2D1::ArcSegment(polarPoint(innerR, a0), D2D1::SizeF(innerR, innerR), 0.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();

                m_renderTarget->FillGeometry(geo.Get(), brush);
            };

            const float PI = 3.1415926535f;

            // Compute half-angles so the sector matches car width/length near the inner radius
            const float innerFrontR = std::max(1.0f, halfL);
            const float innerSideR  = std::max(1.0f, halfW);
            const float frontHalfAng = std::max(0.20f, atanf(std::max(0.1f, (halfW*0.9f)) / innerFrontR));
            const float sideHalfAng  = std::max(0.20f, atanf(std::max(0.1f, (halfL*0.9f)) / innerSideR));

            // Front zones as ring sectors with radial fading
            if (frontYellow) {
                Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush> brush;
                const float innerR = halfL + m_redRangeM * pxPerM;
                const float outerR = halfL + m_yellowRangeM * pxPerM;
                makeRadialBrush(innerR, outerR, yellowCol, brush);
                fillRingSector(0.0f, frontHalfAng, innerR, outerR, brush.Get());
            }
            if (frontRed) {
                Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush> brush;
                const float innerR = halfL;
                const float outerR = halfL + m_redRangeM * pxPerM;
                makeRadialBrush(innerR, outerR, redCol, brush);
                fillRingSector(0.0f, frontHalfAng, innerR, outerR, brush.Get());
            }

            // Rear zones as ring sectors with radial fading
            if (rearYellow) {
                Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush> brush;
                const float innerR = halfL + m_redRangeM * pxPerM;
                const float outerR = halfL + m_yellowRangeM * pxPerM;
                makeRadialBrush(innerR, outerR, yellowCol, brush);
                fillRingSector(PI, frontHalfAng, innerR, outerR, brush.Get());
            }
            if (rearRed) {
                Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush> brush;
                const float innerR = halfL;
                const float outerR = halfL + m_redRangeM * pxPerM;
                makeRadialBrush(innerR, outerR, redCol, brush);
                fillRingSector(PI, frontHalfAng, innerR, outerR, brush.Get());
            }

            // Left/right zones as ring sectors with radial fading, centered on +/-90°
            if (leftRed) {
                Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush> brush;
                const float innerR = halfW;
                const float outerR = halfW + m_redRangeM * pxPerM;
                makeRadialBrush(innerR, outerR, redCol, brush);
                // Slightly bias sector center up/down based on along position estimate
                const float angCenter = -PI * 0.5f + (leftCarPos * 0.15f);
                fillRingSector(angCenter, sideHalfAng, innerR, outerR, brush.Get());
            }
            if (rightRed) {
                Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush> brush;
                const float innerR = halfW;
                const float outerR = halfW + m_redRangeM * pxPerM;
                makeRadialBrush(innerR, outerR, redCol, brush);
                const float angCenter =  PI * 0.5f + (rightCarPos * 0.15f);
                fillRingSector(angCenter, sideHalfAng, innerR, outerR, brush.Get());
            }

            // Draw self car glyph
            {
                const float w = carWpx;
                const float h = carLpx;
                D2D1_ROUNDED_RECT rr;
                rr.rect = { cx - w*0.5f, cy - h*0.5f, cx + w*0.5f, cy + h*0.5f };
                rr.radiusX = 3; rr.radiusY = 3;
                float4 sc = selfCol; sc.w *= effectiveOpacity;
                m_brush->SetColor(sc);
                m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
            }

            m_renderTarget->EndDraw();
        }

        virtual void onSessionChanged()
        {
            // Clear smoothed values and sticky timers when iRacing switches sessions
            m_frontDistSm = 1e9f;
            m_rearDistSm  = 1e9f;
            m_frontRedUntil = 0.0f;
            m_rearRedUntil  = 0.0f;
            m_frontYellowUntil = 0.0f;
            m_rearYellowUntil  = 0.0f;
            m_lastSessionTime = -1.0f;
            m_radarOpacity = 0.1f;
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
        // Settings - Fixed ranges for Racelab-style radar
        float m_maxRangeM = 8.0f;   // Fixed 8m detection range
        float m_yellowRangeM = 8.0f; // Yellow zone from 8m to 2m 
        float m_redRangeM = 2.0f;    // Red zone from 2m to 0m
        float m_carScale = 1.0f;
        bool  m_showBG = true;
        float m_carWidthM = 2.0f;
        float m_carLengthM = 4.5f;

        // Smoothed distances and sticky timers
        float m_frontDistSm = 1e9f;
        float m_rearDistSm  = 1e9f;
        float m_frontRedUntil = 0.0f;
        float m_rearRedUntil  = 0.0f;
        float m_frontYellowUntil = 0.0f;
        float m_rearYellowUntil  = 0.0f;

        // Session transition tracking
        float m_lastSessionTime = -1.0f;

        // Animated radar opacity (0.1 when idle, 1.0 when opponents are nearby)
        float m_radarOpacity = 0.1f;
};