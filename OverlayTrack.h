/*
MIT License

Copyright (c) 2025
*/

#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <d2d1.h>
#include "picojson.h"
#include "Overlay.h"
#include "iracing.h"
#include "Config.h"
#include "util.h"
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
        m_autoOffset = 0.0f;
        m_hasAutoOffset = false;
        m_prevPctSample = -1.0f;
    }

    virtual void onDisable()
    {
        m_trackPath.clear();
        m_autoOffset = 0.0f;
        m_hasAutoOffset = false;
        m_prevPctSample = -1.0f;
    }

    virtual void onSessionChanged()
    {
        loadPathFromJson();
        m_autoOffset = 0.0f;
        m_hasAutoOffset = false;
        m_prevPctSample = -1.0f;
    }

    virtual void onConfigChanged()
    {
        // Nothing yet; reserved for future config
    }

    virtual float2 getDefaultSize()
    {
        return float2(420, 420);
    }

    virtual bool hasCustomBackground()
    {
        return true; // We render our own background (transparent)
    }

    virtual void onUpdate()
    {
        const float4 bgCol = float4(0.05f, 0.05f, 0.05f, 0.65f * getGlobalOpacity());
        const float4 trackCol = float4(0.8f, 0.8f, 0.8f, 0.9f * getGlobalOpacity());
        const float4 markerCol = g_cfg.getFloat4(m_name, "self_col", float4(0.94f, 0.67f, 0.13f, 1));
        const float4 outlineCol = float4(0.2f, 0.8f, 0.2f, 0.9f * getGlobalOpacity());

        // Determine current lap percentage
        float pct = 0.0f;
        if (StubDataManager::shouldUseStubData()) {
            static float s_p = 0.0f;
            s_p += 0.0025f; if (s_p >= 1.0f) s_p -= 1.0f;
            pct = s_p;
        } else {
            int self = ir_session.driverCarIdx;
            if (self >= 0) pct = ir_CarIdxLapDistPct.getFloat(self);
        }

        // Auto-align S/F crossing (detect wrap high->low once) and allow manual offset and reverse
        if (!StubDataManager::shouldUseStubData() && g_cfg.getBool(m_name, "auto_start_offset", true))
        {
            if (m_prevPctSample >= 0.0f)
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

        // Begin draw
        m_renderTarget->BeginDraw();
        m_renderTarget->Clear(float4(0,0,0,0));

        // Use full overlay area
        D2D1_RECT_F dest = { 0, 0, (float)m_width, (float)m_height };

        // Background panel (subtle)
        m_brush->SetColor(bgCol);
        D2D1_ROUNDED_RECT rr; rr.rect = dest; rr.radiusX = 8; rr.radiusY = 8;
        m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());

        // Scale and draw track path to fill the overlay area
        if (m_trackPath.size() >= 2)
        {
            // Calculate path bounds
            float minX = 1.0f, maxX = 0.0f, minY = 1.0f, maxY = 0.0f;
            for (const auto& pt : m_trackPath) {
                minX = std::min(minX, pt.x);
                maxX = std::max(maxX, pt.x);
                minY = std::min(minY, pt.y);
                maxY = std::max(maxY, pt.y);
            }

            // Add padding
            const float padding = 0.1f; // 10% padding
            const float padW = (maxX - minX) * padding;
            const float padH = (maxY - minY) * padding;
            minX -= padW; maxX += padW;
            minY -= padH; maxY += padH;

            const float pathW = maxX - minX;
            const float pathH = maxY - minY;
            const float overlayW = dest.right - dest.left;
            const float overlayH = dest.bottom - dest.top;

            // Scale to fit overlay while preserving aspect ratio
            const float scale = std::min(overlayW / pathW, overlayH / pathH);
            const float scaledW = pathW * scale;
            const float scaledH = pathH * scale;
            const float offsetX = (overlayW - scaledW) * 0.5f;
            const float offsetY = (overlayH - scaledH) * 0.5f;

            // Draw track path as a smooth outlined path
            if (m_trackPath.size() >= 3)
            {
                // Create geometry for the track path
                Microsoft::WRL::ComPtr<ID2D1PathGeometry> pathGeometry;
                Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
                
                if (SUCCEEDED(m_d2dFactory->CreatePathGeometry(&pathGeometry)))
                {
                    if (SUCCEEDED(pathGeometry->Open(&sink)))
                    {
                        // Start the path
                        const float2 start = m_trackPath[0];
                        const float startX = dest.left + offsetX + (start.x - minX) * scale;
                        const float startY = dest.top + offsetY + (start.y - minY) * scale;
                        sink->BeginFigure(D2D1::Point2F(startX, startY), D2D1_FIGURE_BEGIN_HOLLOW);
                        
                        // Add all points to create a smooth path
                        for (size_t i = 1; i < m_trackPath.size(); ++i)
                        {
                            const float2 pt = m_trackPath[i];
                            const float ptX = dest.left + offsetX + (pt.x - minX) * scale;
                            const float ptY = dest.top + offsetY + (pt.y - minY) * scale;
                            sink->AddLine(D2D1::Point2F(ptX, ptY));
                        }
                        
                        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                        sink->Close();
                        
                        // Draw black outline first (thicker)
                        m_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Black, 0.8f * getGlobalOpacity()));
                        m_renderTarget->DrawGeometry(pathGeometry.Get(), m_brush.Get(), 12.0f);
                        
                        // Draw white track path on top (thinner)
                        m_brush->SetColor(trackCol);
                        m_renderTarget->DrawGeometry(pathGeometry.Get(), m_brush.Get(), 6.0f);
                    }
                }
            }

            // Store scaling info for marker positioning
            m_pathBounds = float4(minX, minY, maxX, maxY);
            m_pathScale = scale;
            m_pathOffset = float2(offsetX, offsetY);
        }

        // Draw start/finish lines
        if (!m_extendedLines.empty() && m_trackPath.size() >= 2) {
            const float4 lineCol = float4(1.0f, 1.0f, 1.0f, 0.9f * getGlobalOpacity());
            m_brush->SetColor(lineCol);
            
            for (float linePos : m_extendedLines) {
                float2 pos = computeMarkerPosition(linePos);
                
                // Apply same scaling as track path
                const float centerX = dest.left + m_pathOffset.x + (pos.x - m_pathBounds.x) * m_pathScale;
                const float centerY = dest.top + m_pathOffset.y + (pos.y - m_pathBounds.y) * m_pathScale;
                
                // Calculate perpendicular direction for line
                float2 dir = float2(0, 1); // Default vertical line
                if (m_trackPath.size() >= 2) {
                    // Find approximate tangent direction at this position
                    float total = 0.0f;
                    for (size_t i = 1; i < m_trackPath.size(); ++i)
                        total += distance(m_trackPath[i-1], m_trackPath[i]);
                    
                    float target = linePos * total;
                    float acc = 0.0f;
                    for (size_t i = 1; i < m_trackPath.size(); ++i) {
                        float seg = distance(m_trackPath[i-1], m_trackPath[i]);
                        if (acc + seg >= target) {
                            // Found the segment, calculate tangent
                            float2 tangent = float2(
                                m_trackPath[i].x - m_trackPath[i-1].x,
                                m_trackPath[i].y - m_trackPath[i-1].y
                            );
                            float len = sqrtf(tangent.x * tangent.x + tangent.y * tangent.y);
                            if (len > 0.0001f) {
                                tangent.x /= len;
                                tangent.y /= len;
                                // Perpendicular direction (rotate 90 degrees)
                                dir = float2(-tangent.y, tangent.x);
                            }
                            break;
                        }
                        acc += seg;
                    }
                }
                
                // Draw line perpendicular to track direction
                const float lineLength = std::min(dest.right - dest.left, dest.bottom - dest.top) * 0.03f;
                const float halfLen = lineLength * 0.5f;
                const float x1 = centerX + dir.x * halfLen;
                const float y1 = centerY + dir.y * halfLen;
                const float x2 = centerX - dir.x * halfLen;
                const float y2 = centerY - dir.y * halfLen;
                
                m_renderTarget->DrawLine(float2(x1, y1), float2(x2, y2), m_brush.Get(), 2.0f);
            }
        }

        // Draw other cars if enabled
        if (g_cfg.getBool(m_name, "show_other_cars", false) && m_trackPath.size() >= 2) {
            const float4 opponentCol = g_cfg.getFloat4(m_name, "opponent_col", float4(0.8f, 0.2f, 0.2f, 1)); // Red
            const float4 paceCarCol = g_cfg.getFloat4(m_name, "pace_car_col", float4(1.0f, 1.0f, 0.0f, 1)); // Yellow
            const float4 safetyCarCol = g_cfg.getFloat4(m_name, "safety_car_col", float4(1.0f, 0.5f, 0.0f, 1)); // Orange
            const float4 carOutlineCol = float4(0.0f, 0.0f, 0.0f, 0.8f * getGlobalOpacity());
            
            int selfIdx = ir_session.driverCarIdx;
            
            for (int i = 0; i < IR_MAX_CARS; ++i) {
                if (i == selfIdx) continue; // Skip self
                
                float carPct = 0.0f;
                if (StubDataManager::shouldUseStubData()) {
                    // Generate some fake car positions for preview
                    static float s_offset[64] = {0};
                    static bool s_init = false;
                    if (!s_init) {
                        for (int j = 0; j < 64; ++j) {
                            s_offset[j] = (float)j * 0.05f;
                            while (s_offset[j] >= 1.0f) s_offset[j] -= 1.0f;
                        }
                        s_init = true;
                    }
                    if (i < 64) carPct = s_offset[i];
                } else {
                    carPct = ir_CarIdxLapDistPct.getFloat(i);
                }
                
                if (carPct < 0.0f) continue; // Car not on track
                
                // Check if this car slot is valid (has a driver)
                if (!StubDataManager::shouldUseStubData() && ir_session.cars[i].userName.empty()) {
                    continue; // No driver in this slot
                }
                
                // Determine car color based on type
                float4 carColor = opponentCol;
                
                if (!StubDataManager::shouldUseStubData()) {
                    // Use the isPaceCar flag from iRacing first
                    if (ir_session.cars[i].isPaceCar) {
                        carColor = paceCarCol; // Official pace car
                    } else {
                        // Check car number for safety cars (common numbers)
                        int carNumber = ir_session.cars[i].carNumber;
                        if (carNumber == 911 || carNumber == 999 || carNumber == 0) {
                            carColor = safetyCarCol; // Safety car (common numbers)
                        }
                    }
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
                
                const float r = std::max(3.0f, std::min(dest.right - dest.left, dest.bottom - dest.top) * 0.012f);
                D2D1_ELLIPSE el; el.point = { screenX, screenY }; el.radiusX = r; el.radiusY = r;
                
                // Draw car marker
                m_brush->SetColor(carColor);
                m_renderTarget->FillEllipse(&el, m_brush.Get());
                m_brush->SetColor(carOutlineCol);
                m_renderTarget->DrawEllipse(&el, m_brush.Get(), 1.5f);
            }
        }

        // Draw self marker (always on top)
        if (m_trackPath.size() >= 2) {
            float2 pos = computeMarkerPosition(pct);
            
            // Apply same scaling as track path
            const float screenX = dest.left + m_pathOffset.x + (pos.x - m_pathBounds.x) * m_pathScale;
            const float screenY = dest.top + m_pathOffset.y + (pos.y - m_pathBounds.y) * m_pathScale;
            
            const float r = std::max(4.0f, std::min(dest.right - dest.left, dest.bottom - dest.top) * 0.015f);
            D2D1_ELLIPSE el; el.point = { screenX, screenY }; el.radiusX = r; el.radiusY = r;
            m_brush->SetColor(markerCol);
            m_renderTarget->FillEllipse(&el, m_brush.Get());
            m_brush->SetColor(outlineCol);
            m_renderTarget->DrawEllipse(&el, m_brush.Get(), 2.0f);
        }

        m_renderTarget->EndDraw();
    }

private:
    int m_lastTrackId = -1;
    std::vector<float2> m_trackPath; // normalized 0..1 coords
    std::vector<float> m_extendedLines; // start/finish line positions (0..1)
    float m_autoOffset = 0.0f;
    bool  m_hasAutoOffset = false;
    float m_prevPctSample = -1.0f;
    
    // Path scaling info
    float4 m_pathBounds = float4(0, 0, 1, 1); // minX, minY, maxX, maxY
    float m_pathScale = 1.0f;
    float2 m_pathOffset = float2(0, 0);



    void loadPathFromJson()
    {
        // Reset current path and extended lines
        m_trackPath.clear();
        m_extendedLines.clear();
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

        m_lastTrackId = id;
        
        char buf[128];
        sprintf(buf, "OverlayTrack: loaded %zu points and %zu extended lines for trackId %d\n", 
               m_trackPath.size(), m_extendedLines.size(), id);
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
        // Fallback: unit circle parameterization (centered)
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
};


