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

#include <vector>
#include <string>
#include <algorithm>
#include <limits>
#include <d2d1.h>
#include "picojson.h"
#include "Overlay.h"
#include "iracing.h"
#include "Config.h"
#include "util.h"
#include "ClassColors.h"
#include "stub_data.h"

class OverlayTrack : public Overlay
{
public:

    OverlayTrack()
        : Overlay("OverlayTrack")
    {}

    virtual bool canEnableWhileDisconnected() const { return StubDataManager::shouldUseStubData(); }

protected:

    virtual void onEnable()
    {   
        loadPathFromJson();
        onConfigChanged();
        m_autoOffset = 0.0f;
        m_hasAutoOffset = false;
        m_prevPctSample = -1.0f;
        resetSectorTiming();
    }

    virtual void onDisable()
    {
        m_trackPath.clear();
        m_autoOffset = 0.0f;
        m_hasAutoOffset = false;
        m_prevPctSample = -1.0f;
        m_text.reset();
        resetSectorTiming();
    }

    virtual void onSessionChanged()
    {
        loadPathFromJson();
        m_autoOffset = 0.0f;
        m_hasAutoOffset = false;
        m_prevPctSample = -1.0f;
        resetSectorTiming();
    }

    virtual void onConfigChanged()
    {
        m_text.reset( m_dwriteFactory.Get() );
        createGlobalTextFormat(1.0f, m_textFormat);
        createGlobalTextFormat(0.7f, m_textFormatSmall);
        // Rebuild cached geometry on size changes
        m_cachedPathGeometry.Reset();
        // Per-overlay FPS (configurable; default 10)
        setTargetFPS(g_cfg.getInt(m_name, "target_fps", 15));
    }

    virtual float2 getDefaultSize()
    {
        return float2(420, 420);
    }

    virtual bool hasCustomBackground()
    {
        return true;
    }

    virtual void onUpdate()
    {
        // Cache global opacity to avoid multiple function calls
        const float globalOpacity = getGlobalOpacity();

        // Pre-compute colors with opacity applied
        const float4 bgCol = float4(0.05f, 0.05f, 0.05f, 0.65f * globalOpacity);
        const bool useDarkMode = g_cfg.getBool(m_name, "dark_mode", false);

        const float4 trackCol = useDarkMode
            ? float4(0.070588f, 0.070588f, 0.070588f, 0.9f * globalOpacity)
            : float4(0.8f, 0.8f, 0.8f, 0.9f * globalOpacity);
        const float4 trackBorderCol = useDarkMode
            ? float4(1.0f, 1.0f, 1.0f, 0.9f * globalOpacity)
            : float4(0.0f, 0.0f, 0.0f, 0.8f * globalOpacity);
        const float4 markerCol = ClassColors::self();
        const float4 outlineCol = float4(0.2f, 0.8f, 0.2f, 0.9f * globalOpacity);

        // Update animation once per frame if using stub data
        if (StubDataManager::shouldUseStubData()) {
            StubDataManager::updateAnimation();
        }

        // Determine current lap percentage
        float pct = 0.0f;
        if (StubDataManager::shouldUseStubData()) {
            static float s_p = 0.0f;
            float baseSpeed = 0.0005f;
            float speedVariation = 0.0001f * std::sin(StubDataManager::getAnimationTime() * 0.06f);
            s_p += baseSpeed + speedVariation;
            if (s_p >= 1.0f) s_p -= 1.0f;
            pct = s_p;
        } else {
            int self = ir_session.driverCarIdx;
            if (self >= 0) pct = ir_CarIdxLapDistPct.getFloat(self);
        }

        // Auto-align S/F crossing (detect wrap high->low once) and allow manual offset and reverse
        if (!StubDataManager::shouldUseStubData() && g_cfg.getBool(m_name, "auto_start_offset", true))
        {
            if (!m_hasAutoOffset && m_prevPctSample >= 0.0f)
            {
                if (m_prevPctSample > 0.70f && pct < 0.30f)
                {
                    m_autoOffset = -pct;
                    while (m_autoOffset < 0.0f) m_autoOffset += 1.0f;
                    while (m_autoOffset >= 1.0f) m_autoOffset -= 1.0f;
                    m_hasAutoOffset = true;
                }
            }
            m_prevPctSample = pct;
        }

        if (g_cfg.getBool(m_name, "reverse_direction", false))
            pct = 1.0f - pct;
        float startOffset = g_cfg.getFloat(m_name, "start_offset_pct", 0.0f);
        if (m_hasAutoOffset) startOffset += m_autoOffset;
        pct = pct + startOffset;
        while (pct >= 1.0f) pct -= 1.0f;
        while (pct < 0.0f) pct += 1.0f;
        pct = std::clamp(pct, 0.0f, 0.9999f);

        // Track sector timing based on boundary crossings (live only)
        if (!StubDataManager::shouldUseStubData()) {
            buildAdjustedSectorStarts();
            ensureSectorArraysSized();
            updateSectorTiming();
        }

        // Begin draw
        m_renderTarget->BeginDraw();
        m_renderTarget->Clear(float4(0,0,0,0));

        // Use full overlay area
        D2D1_RECT_F dest = { 0, 0, (float)m_width, (float)m_height };

        // Cache overlay dimensions to avoid repeated calculations
        const float overlayW = dest.right - dest.left;
        const float overlayH = dest.bottom - dest.top;

        // Scale and draw track path to fill the overlay area
        if (m_trackPath.size() >= 2)
        {
            float minX = 1.0f, maxX = 0.0f, minY = 1.0f, maxY = 0.0f;
            for (const auto& pt : m_trackPath) {
                minX = std::min(minX, pt.x);
                maxX = std::max(maxX, pt.x);
                minY = std::min(minY, pt.y);
                maxY = std::max(maxY, pt.y);
            }

            // Add padding
            const float padding = 0.1f;
            const float padW = (maxX - minX) * padding;
            const float padH = (maxY - minY) * padding;
            minX -= padW; maxX += padW;
            minY -= padH; maxY += padH;

            const float pathW = maxX - minX;
            const float pathH = maxY - minY;

            // Scale to fit overlay while preserving aspect ratio
            const float scale = std::min(overlayW / pathW, overlayH / pathH);
            const float scaledW = pathW * scale;
            const float scaledH = pathH * scale;
            const float offsetX = (overlayW - scaledW) * 0.5f;
            const float offsetY = (overlayH - scaledH) * 0.5f;

            if (!m_cachedPathGeometry) {
                if (m_trackPath.size() >= 3) {
                    Microsoft::WRL::ComPtr<ID2D1PathGeometry> pathGeometry;
                    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
                    if (SUCCEEDED(m_d2dFactory->CreatePathGeometry(&pathGeometry)) && SUCCEEDED(pathGeometry->Open(&sink))) {
                        const float2 start = m_trackPath[0];
                        const float startX = dest.left + offsetX + (start.x - minX) * scale;
                        const float startY = dest.top + offsetY + (start.y - minY) * scale;
                        sink->BeginFigure(D2D1::Point2F(startX, startY), D2D1_FIGURE_BEGIN_HOLLOW);
                        for (size_t i = 1; i < m_trackPath.size(); ++i) {
                            const float2 pt = m_trackPath[i];
                            const float ptX = dest.left + offsetX + (pt.x - minX) * scale;
                            const float ptY = dest.top + offsetY + (pt.y - minY) * scale;
                            sink->AddLine(D2D1::Point2F(ptX, ptY));
                        }
                        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                        sink->Close();
                        m_cachedPathGeometry = pathGeometry;
                        m_cachedTrackWidth = g_cfg.getFloat(m_name, "track_width", 6.0f);
                        // Cache total path length in normalized space
                        m_totalPathLength = 0.0f;
                        for (size_t i = 1; i < m_trackPath.size(); ++i)
                            m_totalPathLength += distance(m_trackPath[i-1], m_trackPath[i]);
                    }
                }
            }

            if (m_cachedPathGeometry) {
                const float trackWidth = m_cachedTrackWidth;
                const float outlineWidth = trackWidth * 2.0f;
                m_brush->SetColor(trackBorderCol);
                m_renderTarget->DrawGeometry(m_cachedPathGeometry.Get(), m_brush.Get(), outlineWidth);
                m_brush->SetColor(trackCol);
                m_renderTarget->DrawGeometry(m_cachedPathGeometry.Get(), m_brush.Get(), trackWidth);

                // Colored sector overlays on top of base path
                if (g_cfg.getBool(m_name, "color_sectors", false) && m_sectorColors.size() >= 1)
                {
                    buildAdjustedSectorStarts();
                    ensureSectorArraysSized();
                    for (int s = 0; s + 1 < (int)m_sectorStartsAdjusted.size(); ++s)
                    {
                        const float4 col = m_sectorColors[s];
                        if (col.a <= 0.0f) continue;
                        drawTrackSubPath(
                            m_sectorStartsAdjusted[s],
                            m_sectorStartsAdjusted[s+1],
                            col,
                            trackWidth,
                            minX, minY, scale, offsetX, offsetY,
                            dest.left, dest.top
                        );
                    }
                }
            }

            // Store scaling info for marker positioning
            m_pathBounds = float4(minX, minY, maxX, maxY);
            m_pathScale = scale;
            m_pathOffset = float2(offsetX, offsetY);
        }

        // Draw sector and extended lines
        if (m_trackPath.size() >= 2) {
            auto drawColoredLineAt = [&](float linePos, const float4& col, float thickness)
            {
                m_brush->SetColor(col);
                float2 pos = computeMarkerPosition(linePos);
                const float centerX = dest.left + m_pathOffset.x + (pos.x - m_pathBounds.x) * m_pathScale;
                const float centerY = dest.top + m_pathOffset.y + (pos.y - m_pathBounds.y) * m_pathScale;
                float2 dir = float2(0, 1);
                if (m_trackPath.size() >= 2) {
                    float total = 0.0f;
                    for (size_t i = 1; i < m_trackPath.size(); ++i)
                        total += distance(m_trackPath[i-1], m_trackPath[i]);
                    float target = linePos * total;
                    float acc = 0.0f;
                    for (size_t i = 1; i < m_trackPath.size(); ++i) {
                        float seg = distance(m_trackPath[i-1], m_trackPath[i]);
                        if (acc + seg >= target) {
                            float2 tangent = float2(
                                m_trackPath[i].x - m_trackPath[i-1].x,
                                m_trackPath[i].y - m_trackPath[i-1].y
                            );
                            float len = sqrtf(tangent.x * tangent.x + tangent.y * tangent.y);
                            if (len > 0.0001f) {
                                tangent.x /= len;
                                tangent.y /= len;
                                dir = float2(-tangent.y, tangent.x);
                            }
                            break;
                        }
                        acc += seg;
                    }
                }
                const float lineLength = std::min(overlayW, overlayH) * 0.06f;
                const float halfLen = lineLength * 0.5f;
                const float x1 = centerX + dir.x * halfLen;
                const float y1 = centerY + dir.y * halfLen;
                const float x2 = centerX - dir.x * halfLen;
                const float y2 = centerY - dir.y * halfLen;
                m_renderTarget->DrawLine(float2(x1, y1), float2(x2, y2), m_brush.Get(), thickness);
            };
            // Sector lines include S/F at 0.0; skip 1.0 to avoid double-draw at same location
            if (g_cfg.getBool(m_name, "show_sector_lines", false))
            {
                buildAdjustedSectorStarts();
                const float4 white = float4(1.0f, 1.0f, 1.0f, 0.9f * globalOpacity);
                const float thickness = 2.0f;
                for (int s = 0; s < (int)m_sectorStartsAdjusted.size(); ++s)
                {
                    float pos = m_sectorStartsAdjusted[s];
                    if (pos >= 0.9999f) continue; // skip 1.0
                    drawColoredLineAt(pos, white, thickness);
                }
            }

            // Extended lines remain (optional extra splits); always draw them
            if (!m_extendedLines.empty()) {
                const float4 white = float4(1.0f, 1.0f, 1.0f, 0.9f * globalOpacity);
                for (float linePos : m_extendedLines) drawColoredLineAt(linePos, white, 2.0f);
            }
        }

        // Draw other cars if enabled
        if (g_cfg.getBool(m_name, "show_other_cars", false) && m_trackPath.size() >= 2) {
            const float4 opponentCol = g_cfg.getFloat4(m_name, "opponent_col", float4(0.8f, 0.2f, 0.2f, 1)); // Red
            const float4 paceCarCol = g_cfg.getFloat4(m_name, "pace_car_col", float4(1.0f, 1.0f, 0.0f, 1)); // Yellow
            const float4 safetyCarCol = g_cfg.getFloat4(m_name, "safety_car_col", float4(1.0f, 0.5f, 0.0f, 1)); // Orange
            const float4 carOutlineCol = float4(0.0f, 0.0f, 0.0f, 0.8f * globalOpacity);
            
            int selfIdx = ir_session.driverCarIdx;
            
            for (int i = 0; i < IR_MAX_CARS; ++i) {
                if (i == selfIdx) continue;
                
                float carPct = 0.0f;
                if (StubDataManager::shouldUseStubData()) {
                    // Generate animated car positions for preview
                    static float s_baseOffset[64] = {0};
                    static float s_speed[64] = {0};
                    static bool s_init = false;
                    if (!s_init) {
                        for (int j = 0; j < 64; ++j) {
                            s_baseOffset[j] = (float)j * 0.12f;
                            while (s_baseOffset[j] >= 1.0f) s_baseOffset[j] -= 1.0f;
                            
                            s_speed[j] = 0.0004f + (float)(j % 5) * 0.0001f;
                        }
                        s_init = true;
                    }
                    
                    if (i < 64) {
                        // Animate car position with individual speeds
                        s_baseOffset[i] += s_speed[i];
                        while (s_baseOffset[i] >= 1.0f) s_baseOffset[i] -= 1.0f;
                        carPct = s_baseOffset[i];
                    }
                } else {
                    carPct = ir_CarIdxLapDistPct.getFloat(i);
                }
                
                if (carPct < 0.0f) continue;
                
                if (StubDataManager::shouldUseStubData()) {
                    if (!StubDataManager::getStubCar(i)) {
                        continue;
                    }
                } else {
                    if (ir_session.cars[i].userName.empty()) {
                        continue;
                    }
                }
                float4 carColor = opponentCol;
                bool isPaceCar = false;
                bool isSafetyCar = false;

                if (!StubDataManager::shouldUseStubData()) {

                    bool carIsPaceCar = ir_session.cars[i].isPaceCar != 0;
                    if (carIsPaceCar) {
                        bool isRaceSession = ir_session.sessionType == SessionType::RACE;
                        int currentLap = ir_Lap.getInt();
                        bool isFirstLap = currentLap <= 1;
                        int sessionFlags = ir_SessionFlags.getInt();
                        bool underCaution = (sessionFlags & (irsdk_caution | irsdk_cautionWaving)) != 0;
                        bool preStart = ir_isPreStart();

                        isPaceCar = (isRaceSession && isFirstLap) || underCaution || preStart;
                    }

                    // Safety car logic - only show during caution/yellow flag periods
                    int carNumber = ir_session.cars[i].carNumber;
                    bool carIsSafetyCar = (carNumber == 911 || carNumber == 999 || carNumber == 0);
                    if (carIsSafetyCar) {
                        // Only show safety car during caution or yellow flag periods
                        int sessionFlags = ir_SessionFlags.getInt();
                        bool underCaution = (sessionFlags & (irsdk_caution | irsdk_cautionWaving)) != 0;
                        bool underYellow = (sessionFlags & (irsdk_yellow | irsdk_yellowWaving)) != 0;

                        isSafetyCar = underCaution || underYellow;
                    }
                }

                // Assign colors based on car type
                if (isPaceCar) {
                    carColor = ClassColors::paceCar();
                } else if (isSafetyCar) {
                    carColor = ClassColors::safetyCar();
                } else {
                    // Get class-based color for regular opponents
                    int classId = StubDataManager::shouldUseStubData() ?
                        (StubDataManager::getStubCar(i) ? StubDataManager::getStubCar(i)->classId : 0) :
                        ir_session.cars[i].classId;
                    carColor = ClassColors::get(classId);
                }
                
                // Apply manual offset and direction for consistency with self marker
                if (g_cfg.getBool(m_name, "reverse_direction", false))
                    carPct = 1.0f - carPct;
                float startOffset = g_cfg.getFloat(m_name, "start_offset_pct", 0.0f);
                if (m_hasAutoOffset) startOffset += m_autoOffset;
                carPct = carPct + startOffset;
                while (carPct >= 1.0f) carPct -= 1.0f;
                while (carPct < 0.0f) carPct += 1.0f;
                carPct = std::clamp(carPct, 0.0f, 0.9999f);
                
                float2 pos = computeMarkerPosition(carPct);
                
                // Apply same scaling as track path
                const float screenX = dest.left + m_pathOffset.x + (pos.x - m_pathBounds.x) * m_pathScale;
                const float screenY = dest.top + m_pathOffset.y + (pos.y - m_pathBounds.y) * m_pathScale;
                
                const float r = std::max(9.0f, std::min(overlayW, overlayH) * 0.03f);
                D2D1_ELLIPSE el; el.point = { screenX, screenY }; el.radiusX = r; el.radiusY = r;
                
                // Draw car marker
                m_brush->SetColor(carColor);
                m_renderTarget->FillEllipse(&el, m_brush.Get());
                m_brush->SetColor(carOutlineCol);
                m_renderTarget->DrawEllipse(&el, m_brush.Get(), 1.5f);

                // Draw car number text (centered) - 12pt for other cars
                if (m_textFormatSmall)
                {
                    int carNum = 0;
                    if (StubDataManager::shouldUseStubData())
                    {
                        if (const auto* sc = StubDataManager::getStubCar(i))
                            carNum = atoi(sc->carNumber);
                    }
                    else
                    {
                        carNum = ir_session.cars[i].carNumber;
                    }
                    if (carNum != 0)
                    {
                        wchar_t buf[8];
                        _snwprintf_s(buf, _TRUNCATE, L"%d", carNum);
                        const float xmin = screenX - r;
                        const float xmax = screenX + r;
                        // Shadow
                        m_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Black, 0.8f * globalOpacity));
                        m_text.render(m_renderTarget.Get(), buf, m_textFormatSmall.Get(), xmin + 1.0f, xmax + 1.0f, screenY + 1.0f, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
                        // Text
                        m_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White, 1.0f * globalOpacity));
                        m_text.render(m_renderTarget.Get(), buf, m_textFormatSmall.Get(), xmin, xmax, screenY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
                    }
                }
            }
        }

        // Draw self marker (always on top)
        if (m_trackPath.size() >= 2) {
            float2 pos = computeMarkerPosition(pct);
            
            // Apply same scaling as track path
            const float screenX = dest.left + m_pathOffset.x + (pos.x - m_pathBounds.x) * m_pathScale;
            const float screenY = dest.top + m_pathOffset.y + (pos.y - m_pathBounds.y) * m_pathScale;
            
            const float r = std::max(13.5f, std::min(overlayW, overlayH) * 0.045f);
            D2D1_ELLIPSE el; el.point = { screenX, screenY }; el.radiusX = r; el.radiusY = r;
            m_brush->SetColor(markerCol);
            m_renderTarget->FillEllipse(&el, m_brush.Get());
            m_brush->SetColor(outlineCol);
            m_renderTarget->DrawEllipse(&el, m_brush.Get(), 2.0f);

            // Self car number (16pt font)
            if (m_textFormat)
            {
                int selfIdx = ir_session.driverCarIdx;
                int carNum = 0;
                if (StubDataManager::shouldUseStubData())
                {
                    if (selfIdx >= 0)
                    {
                        if (const auto* sc = StubDataManager::getStubCar(selfIdx))
                            carNum = atoi(sc->carNumber);
                    }
                    else
                    {
                        if (const auto* sc0 = StubDataManager::getStubCar(0))
                            carNum = atoi(sc0->carNumber);
                    }
                }
                else if (selfIdx >= 0)
                {
                    carNum = ir_session.cars[selfIdx].carNumber;
                }
                if (carNum != 0)
                {
                    wchar_t buf[8];
                    _snwprintf_s(buf, _TRUNCATE, L"%d", carNum);
                    const float xmin = screenX - r;
                    const float xmax = screenX + r;
                    // Shadow
                    m_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Black, 0.8f * globalOpacity));
                    m_text.render(m_renderTarget.Get(), buf, m_textFormat.Get(), xmin + 1.0f, xmax + 1.0f, screenY + 1.0f, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
                    // Text
                    m_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White, 1.0f * globalOpacity));
                    m_text.render(m_renderTarget.Get(), buf, m_textFormat.Get(), xmin, xmax, screenY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER);
                }
            }
        }

        m_renderTarget->EndDraw();
    }

private:
    int m_lastTrackId = -1;
    std::vector<float2> m_trackPath;
    std::vector<float> m_extendedLines;
    std::vector<float> m_sectorLines; 
    std::vector<float> m_sectorBoundaries;
    float m_autoOffset = 0.0f;
    bool  m_hasAutoOffset = false;
    float m_prevPctSample = -1.0f;
    
    // Path scaling info
    float4 m_pathBounds = float4(0, 0, 1, 1);
    float m_pathScale = 1.0f;
    float2 m_pathOffset = float2(0, 0);

    // Text rendering for car numbers
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormatSmall;
    TextCache    m_text;
    // Cached drawing data to avoid rebuilding each frame
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> m_cachedPathGeometry;
    float m_cachedTrackWidth = 6.0f;

    // Sector timing coloring
    std::vector<float> m_sectorBestPersonal;
    std::vector<float> m_sectorBestSession;
    std::vector<float4> m_sectorColors;
    float m_totalPathLength = 0.0f;

    // Per-car sector tracking state
    float  m_prevPctPerCar[IR_MAX_CARS] = { -1.0f };
    double m_lastBoundaryTimePerCar[IR_MAX_CARS] = { -1.0 };
    bool   m_perCarInitialized = false;
    bool   m_hasCrossedSFPerCar[IR_MAX_CARS] = { false };
    int    m_lastIncidentCountPerCar[IR_MAX_CARS] = { 0 };
    bool   m_wasInPitStallSelf = false;
    double m_lastTimingNow = -1.0;


    void loadPathFromJson()
    {
        // Reset current path and extended lines
        m_trackPath.clear();
        m_extendedLines.clear();
        m_sectorLines.clear();
        m_lastTrackId = -1;

        // Load JSON
        std::wstring jsonPath = resolveAssetPathW(L"assets\\tracks\\track-paths.json");
        std::string jsonText;
        if (!loadFileW(jsonPath, jsonText)) return;

        picojson::value root;
        std::string err = picojson::parse(root, jsonText);
        if (!err.empty() || !root.is<picojson::object>()) return;
        const auto& obj = root.get<picojson::object>();

        auto getArray = [&](const picojson::object& o, const std::string& key)->const picojson::array*{
            auto it = o.find(key);
            if (it == o.end() || !it->second.is<picojson::array>()) return nullptr;
            return &it->second.get<picojson::array>();
        };
        auto getObject = [&](const picojson::object& o, const std::string& key)->const picojson::object*{
            auto it = o.find(key);
            if (it == o.end() || !it->second.is<picojson::object>()) return nullptr;
            return &it->second.get<picojson::object>();
        };

        const picojson::object* byId = getObject(obj, "tracksById");
        if (!byId) return;

        int id = ir_session.trackId;
        // In preview mode, default to Snetterton 300
        if (StubDataManager::shouldUseStubData() && id <= 0) {
            id = 297; // Snetterton 300
        }
        if (id <= 0) return;
        
        const std::string idStr = std::to_string(id);
        auto itId = byId->find(idStr);
        if (itId == byId->end() || !itId->second.is<picojson::object>()) {
            OutputDebugStringA("OverlayTrack: no entry in track-paths.json for current trackId\n");
            return;
        }
        const picojson::object& trk = itId->second.get<picojson::object>();
        const picojson::array* pts = getArray(trk, "points");
        if (!pts) return;

        for (const auto& v : *pts) {
            if (!v.is<picojson::array>()) continue;
            const auto& pair = v.get<picojson::array>();
            if (pair.size() < 2) continue;
            if (!pair[0].is<double>() || !pair[1].is<double>()) continue;
            float nx = (float)pair[0].get<double>();
            float ny = (float)pair[1].get<double>();
            nx = std::clamp(nx, 0.0f, 1.0f);
            ny = std::clamp(ny, 0.0f, 1.0f);
            m_trackPath.emplace_back(nx, ny);
        }

        if (!m_trackPath.empty() && (m_trackPath.front().x != m_trackPath.back().x || m_trackPath.front().y != m_trackPath.back().y))
            m_trackPath.push_back(m_trackPath.front());

        // Load extendedLine data if present
        const picojson::array* extLines = getArray(trk, "extendedLine");
        if (extLines) {
            for (const auto& v : *extLines) {
                if (v.is<double>()) {
                    float pos = (float)v.get<double>();
                    pos = std::clamp(pos, 0.0f, 1.0f);
                    m_extendedLines.push_back(pos);
                }
            }
        }

        // Build sector lines and boundaries from SDK-provided SplitTimeInfo
        m_sectorLines.clear();
        m_sectorBoundaries.clear();
        if (!ir_session.sectorStartPct.empty())
        {
            m_sectorBoundaries = ir_session.sectorStartPct;
            for (size_t i = 1; i + 1 < m_sectorBoundaries.size(); ++i)
                m_sectorLines.push_back(m_sectorBoundaries[i]);
        }

        m_lastTrackId = id;
        
        char buf[128];
        _snprintf_s(buf, _countof(buf), _TRUNCATE, "OverlayTrack: loaded %zu points, %zu extended lines, %zu sector boundaries for trackId %d\n", 
               m_trackPath.size(), m_extendedLines.size(), m_sectorBoundaries.size(), id);
        OutputDebugStringA(buf);
    }

    float2 computeMarkerPosition(float pct)
    {
        if (m_trackPath.size() >= 2) {
            // Compute total length
            float total = 0.0f;
            for (size_t i = 1; i < m_trackPath.size(); ++i)
                total += distance(m_trackPath[i-1], m_trackPath[i]);
            if (total <= 0.0f) return float2(0.5f, 0.5f);
            float target = pct * total;
            float acc = 0.0f;
            for (size_t i = 1; i < m_trackPath.size(); ++i) {
                float seg = distance(m_trackPath[i-1], m_trackPath[i]);
                if (acc + seg >= target) {
                    float t = (target - acc) / std::max(0.0001f, seg);
                    return lerp(m_trackPath[i-1], m_trackPath[i], t);
                }
                acc += seg;
            }
            return m_trackPath.back();
        }
        const float ang = pct * 6.2831853f - 1.5707963f;
        const float rx = 0.40f, ry = 0.40f;
        return float2(0.5f + cosf(ang) * rx, 0.5f + sinf(ang) * ry);
    }

    static inline float distance(const float2& a, const float2& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return sqrtf(dx*dx + dy*dy);
    }

    static inline float2 lerp(const float2& a, const float2& b, float t)
    {
        return float2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
    }

    // Sector timing and rendering support
    std::vector<float> m_sectorStartsAdjusted;
    bool  m_sectorsInitialized = false;

    void resetSectorTiming()
    {
        m_sectorStartsAdjusted.clear();
        m_sectorsInitialized = false;

        m_sectorBestPersonal.clear();
        m_sectorBestSession.clear();
        m_sectorColors.clear();

        for (int i = 0; i < IR_MAX_CARS; ++i) {
            m_prevPctPerCar[i] = -1.0f;
            m_lastBoundaryTimePerCar[i] = -1.0;
            m_hasCrossedSFPerCar[i] = false;
            m_lastIncidentCountPerCar[i] = 0;
        }
        m_perCarInitialized = false;
        m_lastTimingNow = -1.0;
    }

    void buildAdjustedSectorStarts()
    {
        std::vector<float> base = ir_session.sectorStartPct;
        if (base.empty())
        {
            // In preview mode there is no SDK session YAML, so synthesize equal thirds
            if (StubDataManager::shouldUseStubData())
            {
                base = { 0.0f, 1.0f/3.0f, 2.0f/3.0f, 1.0f };
            }
            else
            {
                return;
            }
        }
        float startOffset = g_cfg.getFloat(m_name, "start_offset_pct", 0.0f);
        if (m_hasAutoOffset) startOffset += m_autoOffset;
        const bool rev = g_cfg.getBool(m_name, "reverse_direction", false);
        m_sectorStartsAdjusted.clear();
        for (float p : base)
        {
            float q = rev ? (1.0f - p) : p;
            q += startOffset;
            while (q >= 1.0f) q -= 1.0f;
            while (q < 0.0f) q += 1.0f;
            m_sectorStartsAdjusted.push_back(q);
        }
        std::sort(m_sectorStartsAdjusted.begin(), m_sectorStartsAdjusted.end());
        if (m_sectorStartsAdjusted.empty() || m_sectorStartsAdjusted.front() > 0.0001f)
            m_sectorStartsAdjusted.insert(m_sectorStartsAdjusted.begin(), 0.0f);
        if (m_sectorStartsAdjusted.back() < 0.9999f)
            m_sectorStartsAdjusted.push_back(1.0f);
        m_sectorsInitialized = true;

        ensureSectorArraysSized();
    }
    
    // Helpers for sector overlay & timing
    float adjustPctForOverlay(float pct) const
    {
        if (g_cfg.getBool(m_name, "reverse_direction", false))
            pct = 1.0f - pct;
        float startOffset = g_cfg.getFloat(m_name, "start_offset_pct", 0.0f);
        if (m_hasAutoOffset) startOffset += m_autoOffset;
        pct = pct + startOffset;
        while (pct >= 1.0f) pct -= 1.0f;
        while (pct < 0.0f) pct += 1.0f;
        return std::clamp(pct, 0.0f, 0.9999f);
    }

    int boundaryIndexAt(float pct) const
    {
        if (m_sectorStartsAdjusted.size() < 2) return 0;
        int idx = 0;
        for (int i = 0; i < (int)m_sectorStartsAdjusted.size(); ++i)
            if (m_sectorStartsAdjusted[i] <= pct) idx = i;
        return idx;
    }

    bool crossedBoundary(float prevPct, float curPct, float boundary) const
    {
        if (prevPct < 0.0f) return false;
        if (prevPct <= curPct) {
            return prevPct < boundary && boundary <= curPct;
        } else {
            // wrap around
            return boundary > prevPct || boundary <= curPct;
        }
    }

    void ensureSectorArraysSized()
    {
        const int nBounds = (int)m_sectorStartsAdjusted.size();
        const int nSectors = nBounds > 0 ? nBounds - 1 : 0;
        if ((int)m_sectorBestPersonal.size() != nSectors) {
            m_sectorBestPersonal.assign(nSectors, std::numeric_limits<float>::infinity());
        }
        if ((int)m_sectorBestSession.size() != nSectors) {
            m_sectorBestSession.assign(nSectors, std::numeric_limits<float>::infinity());
        }
        if ((int)m_sectorColors.size() != nSectors) {
            m_sectorColors.assign(nSectors, float4(0,0,0,0));
        }
    }

    // Draw a sub-path covering [startPct, endPct)
    void drawTrackSubPath(float startPct, float endPct, const float4& col, float width,
                          float minX, float minY, float scale, float offsetX, float offsetY,
                          float left, float top)
    {
        if (m_trackPath.size() < 2 || m_totalPathLength <= 0.0f) return;

        const float startDist = startPct * m_totalPathLength;
        const float endDist   = endPct   * m_totalPathLength;

        Microsoft::WRL::ComPtr<ID2D1PathGeometry> segGeom;
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
        if (FAILED(m_d2dFactory->CreatePathGeometry(&segGeom)) || FAILED(segGeom->Open(&sink)))
            return;

        float acc = 0.0f;
        bool started = false;

        auto toScreen = [&](const float2& p)->D2D1_POINT_2F {
            const float sx = left + offsetX + (p.x - minX) * scale;
            const float sy = top  + offsetY + (p.y - minY) * scale;
            return D2D1::Point2F(sx, sy);
        };

        for (size_t i = 1; i < m_trackPath.size(); ++i) {
            const float2 A = m_trackPath[i-1];
            const float2 B = m_trackPath[i];
            const float seg = distance(A, B);
            if (seg <= 0.0f) continue;

            const float segStart = acc;
            const float segEnd   = acc + seg;

            if (segEnd > startDist && segStart < endDist) {
                // Clamp to [startDist, endDist]
                const float t1 = std::max(0.0f, (startDist - segStart) / seg);
                const float t2 = std::min(1.0f, (endDist   - segStart) / seg);
                const float2 P1 = lerp(A, B, std::max(0.0f, t1));
                const float2 P2 = lerp(A, B, std::min(1.0f, t2));

                if (!started) {
                    sink->BeginFigure(toScreen(P1), D2D1_FIGURE_BEGIN_HOLLOW);
                    started = true;
                }
                if (t2 > t1) {
                    sink->AddLine(toScreen(P2));
                }
            }

            acc += seg;
            if (acc >= endDist) break;
        }

        if (started) {
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            sink->Close();
            m_brush->SetColor(col);
            m_renderTarget->DrawGeometry(segGeom.Get(), m_brush.Get(), width);
        }
    }

    void updateSectorTiming()
    {
        if (m_sectorStartsAdjusted.size() < 2) return;

        const double now = ir_now();
        const int selfIdx = ir_session.driverCarIdx;

        // Replay scrubbing/seek can make time jump backwards. Reset timing state so we
        // don't produce negative sector times or get stuck.
        if (m_lastTimingNow >= 0.0 && now + 0.001 < m_lastTimingNow)
        {
            resetSectorTiming();
            return;
        }
        m_lastTimingNow = now;

        // Initialize per-car state once
        if (!m_perCarInitialized) {
            for (int i = 0; i < IR_MAX_CARS; ++i) {
                float raw = ir_CarIdxLapDistPct.getFloat(i);
                if (raw < 0.0f) { m_prevPctPerCar[i] = -1.0f; continue; }
                m_prevPctPerCar[i] = adjustPctForOverlay(raw);
                m_lastBoundaryTimePerCar[i] = -1.0;
            }
            m_perCarInitialized = true;
            return;
        }

        auto isValidCar = [&](int i)->bool {
            if (i < 0 || i >= IR_MAX_CARS) return false;
            if (ir_session.cars[i].userName.empty()) return false;
            if (ir_session.cars[i].isSpectator) return false;
            return true;
        };

        const float4 purple = float4(0.70f, 0.30f, 1.00f, 0.9f);
        const float4 green  = float4(0.20f, 0.85f, 0.25f, 0.9f);
        const float4 yellow = float4(1.00f, 0.85f, 0.00f, 0.9f);

        const int nBounds = (int)m_sectorStartsAdjusted.size();
        for (int i = 0; i < IR_MAX_CARS; ++i) {
            float raw = ir_CarIdxLapDistPct.getFloat(i);
            if (raw < 0.0f) continue;

            float cur = adjustPctForOverlay(raw);
            float prev = m_prevPctPerCar[i];

            // Optional gating for self to avoid flicker when stationary/pitting
            if (i == selfIdx) {
                bool inPitStall = ir_PlayerCarInPitStall.getBool();
                bool onPitRoad  = ir_OnPitRoad.getBool();
                float speedMs   = ir_Speed.getFloat();

                // If we just entered pit stall, treat next run as a fresh outlap
                if (inPitStall && !m_wasInPitStallSelf) {
                    m_hasCrossedSFPerCar[selfIdx] = false;
                    m_lastBoundaryTimePerCar[selfIdx] = -1.0;
                    ensureSectorArraysSized();
                    for (size_t si = 0; si < m_sectorColors.size(); ++si) m_sectorColors[si] = float4(0,0,0,0);
                }
                m_wasInPitStallSelf = inPitStall;

                if (inPitStall || onPitRoad || speedMs < 0.5f) {
                    m_prevPctPerCar[i] = cur;
                    continue;
                }
            }

            // Determine which boundaries were crossed between prev and cur
            int idxPrev = (prev < 0.0f) ? boundaryIndexAt(cur) : boundaryIndexAt(prev);
            int idxCur  = boundaryIndexAt(cur);

            // Start processing from the next boundary after prev
            int idx = (idxPrev + 1) % nBounds;
            bool wrapped = prev > cur; // wrapped around 1.0 -> 0.0

            // helper lambda to test if boundary idx lies between prev and cur on unit circle
            auto boundaryInRange = [&](int bIdx)->bool {
                float b = m_sectorStartsAdjusted[bIdx];
                if (!wrapped) return prev < b && b <= cur;
                return (b > prev) || (b <= cur);
            };

            for (int processed = 0; processed < nBounds; ++processed) {
                if (boundaryInRange(idx)) {
                    // We crossed boundary idx
                    int s = idx - 1; if (s < 0) s = nBounds - 2;

                    if (m_lastBoundaryTimePerCar[i] >= 0.0) {
                        float sectorTime = float(now - m_lastBoundaryTimePerCar[i]);
                        const float kMinValidSectorTime = 0.05f;
                        if (sectorTime > kMinValidSectorTime && sectorTime < 600.0f) {
                            bool isValidSector = true;
                            if (i == selfIdx && isValidCar(i)) {
                                int incNow = ir_session.cars[i].incidentCount;
                                isValidSector = (incNow <= m_lastIncidentCountPerCar[i]);
                                m_lastIncidentCountPerCar[i] = incNow;
                            }

                            if (sectorTime < m_sectorBestSession[s])
                                m_sectorBestSession[s] = sectorTime;

                            if (i == selfIdx && isValidCar(i)) {
                                const bool newPB = (sectorTime < m_sectorBestPersonal[s]);
                                if (newPB) m_sectorBestPersonal[s] = sectorTime;

                                if (m_hasCrossedSFPerCar[selfIdx]) {
                                    if (isValidSector && sectorTime <= m_sectorBestSession[s] + 1e-4f) {
                                        m_sectorColors[s] = purple;
                                    } else if (isValidSector && newPB) {
                                        m_sectorColors[s] = green;
                                    } else {
                                        m_sectorColors[s] = yellow;
                                    }
                                }
                            }
                        }
                    }
                    // Start timing next sector from now
                    m_lastBoundaryTimePerCar[i] = now;

                    // Clear color for the sector just entered (idx)
                    if (i == selfIdx && isValidCar(i)) {
                        int sNext = idx; if (sNext >= nBounds - 1) sNext = 0;
                        if (sNext >= 0 && sNext < (int)m_sectorColors.size())
                            m_sectorColors[sNext] = float4(0,0,0,0);
                    }

                    // Mark S/F crossed for self
                    if (i == selfIdx && idx == 0) {
                        m_hasCrossedSFPerCar[selfIdx] = true;
                    }

                }

                if (idx == idxCur) break;
                idx = (idx + 1) % nBounds;
            }

            m_prevPctPerCar[i] = cur;
        }
    }
};