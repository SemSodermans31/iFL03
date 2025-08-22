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
#include "preview_mode.h"
#include "stub_data.h"
#include <deque>
#include <vector>
#include <dwrite.h>
#include <dwrite_1.h>

class OverlayDelta : public Overlay
{
public:
    OverlayDelta()
        : Overlay("OverlayDelta")
        , m_currentDelta(0.0f)
        , m_isDeltaImproving(false)
        , m_referenceMode(ReferenceMode::SESSION_BEST)
        , m_trendSamples(10)
    {}

protected:
    enum class ReferenceMode
    {
        ALLTIME_BEST = 0,        // Player's all-time best lap (ir_LapDeltaToBestLap)
        SESSION_BEST = 1,        // Player's best lap in current session (ir_LapDeltaToSessionBestLap) 
        ALLTIME_OPTIMAL = 2,     // All-time optimal lap from sectors (ir_LapDeltaToOptimalLap)
        SESSION_OPTIMAL = 3,     // Session optimal lap from sectors (ir_LapDeltaToSessionOptimalLap)
        LAST_LAP = 4            // Last completed lap (custom calculation)
    };

    // Core delta data
    float m_currentDelta;
    bool m_isDeltaImproving;
    
    // Trend tracking for coloring
    std::deque<float> m_deltaTrendHistory;
    int m_trendSamples;
    
    // Settings
    ReferenceMode m_referenceMode;
    
    // Text formats
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_titleFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_deltaFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_smallFormat;
    
    // Scaled text formats (created dynamically)
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_scaledTitleFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_scaledDeltaFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_scaledSmallFormat;

    virtual void onEnable()
    {
        onConfigChanged();
    }

    virtual void onConfigChanged()
    {
        const std::string font = g_cfg.getString(m_name, "font", "Consolas");
        const float titleSize = g_cfg.getFloat(m_name, "title_font_size", 16.0f);
        const float deltaSize = g_cfg.getFloat(m_name, "delta_font_size", 32.0f);
        const float smallSize = g_cfg.getFloat(m_name, "small_font_size", 14.0f);
        const int fontWeight = g_cfg.getInt(m_name, "font_weight", 500);
        const std::string fontStyleStr = g_cfg.getString(m_name, "font_style", "normal");
        
        m_referenceMode = (ReferenceMode)g_cfg.getInt(m_name, "reference_mode", 1); // Default to session best
        m_trendSamples = g_cfg.getInt(m_name, "trend_samples", 10);

        // Convert font style string to enum
        DWRITE_FONT_STYLE fontStyle = DWRITE_FONT_STYLE_NORMAL;
        if (fontStyleStr == "italic") fontStyle = DWRITE_FONT_STYLE_ITALIC;
        else if (fontStyleStr == "oblique") fontStyle = DWRITE_FONT_STYLE_OBLIQUE;

        // Create text formats
        HRCHECK(m_dwriteFactory->CreateTextFormat(
            toWide(font).c_str(), NULL,
            (DWRITE_FONT_WEIGHT)fontWeight, fontStyle, DWRITE_FONT_STRETCH_NORMAL,
            titleSize, L"en-us", &m_titleFormat));
        m_titleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_titleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        HRCHECK(m_dwriteFactory->CreateTextFormat(
            toWide(font).c_str(), NULL,
            DWRITE_FONT_WEIGHT_BOLD, fontStyle, DWRITE_FONT_STRETCH_NORMAL,
            deltaSize, L"en-us", &m_deltaFormat));
        m_deltaFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_deltaFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        HRCHECK(m_dwriteFactory->CreateTextFormat(
            toWide(font).c_str(), NULL,
            (DWRITE_FONT_WEIGHT)fontWeight, fontStyle, DWRITE_FONT_STRETCH_NORMAL,
            smallSize, L"en-us", &m_smallFormat));
        m_smallFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_smallFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }



    void updateDelta()
    {
        if (StubDataManager::shouldUseStubData()) {
            // Use stub data for preview mode
            m_currentDelta = StubDataManager::getStubDeltaToSessionBest();
            updateDeltaTrend();
            return;
        }

        // Store previous delta for trend calculation
        float previousDelta = m_currentDelta;
        
        // Get delta based on selected reference mode using iRacing's native calculations
        bool deltaValid = false;
        float deltaValue = 0.0f;
        
        switch (m_referenceMode) {
            case ReferenceMode::ALLTIME_BEST:
                deltaValid = ir_LapDeltaToBestLap_OK.getBool();
                if (deltaValid) deltaValue = ir_LapDeltaToBestLap.getFloat();
                break;
                
            case ReferenceMode::SESSION_BEST:
                deltaValid = ir_LapDeltaToSessionBestLap_OK.getBool();
                if (deltaValid) {
                    deltaValue = ir_LapDeltaToSessionBestLap.getFloat();
                }
                break;
                
            case ReferenceMode::ALLTIME_OPTIMAL:
                deltaValid = ir_LapDeltaToOptimalLap_OK.getBool();
                if (deltaValid) {
                    deltaValue = ir_LapDeltaToOptimalLap.getFloat();
                }
                break;
                
            case ReferenceMode::SESSION_OPTIMAL:
                deltaValid = ir_LapDeltaToSessionOptimalLap_OK.getBool();
                if (deltaValid) {
                    deltaValue = ir_LapDeltaToSessionOptimalLap.getFloat();
                }
                break;
                
            case ReferenceMode::LAST_LAP:
                // For last lap, we'd need custom calculation - for now fall back to all-time best
                deltaValid = ir_LapDeltaToBestLap_OK.getBool();
                if (deltaValid) deltaValue = ir_LapDeltaToBestLap.getFloat();
                break;
        }
        
        if (deltaValid) {
            m_currentDelta = deltaValue;
            updateDeltaTrend();
        } else {
            m_currentDelta = 0.0f;
        }
    }

    void updateDeltaTrend()
    {
        // Add current delta to trend history
        m_deltaTrendHistory.push_back(m_currentDelta);
        
        // Keep only the last N values for trend calculation
        while (m_deltaTrendHistory.size() > m_trendSamples) {
            m_deltaTrendHistory.pop_front();
        }
        
        // Calculate trend - need at least 3 samples for meaningful trend
        if (m_deltaTrendHistory.size() >= 3) {
            // Compare recent average to older average to determine trend
            const int recentSamples = std::min(3, (int)m_deltaTrendHistory.size() / 2);
            const int olderSamples = (int)m_deltaTrendHistory.size() - recentSamples;
            
            // Calculate recent average (last few samples)
            float recentSum = 0.0f;
            for (int i = (int)m_deltaTrendHistory.size() - recentSamples; i < (int)m_deltaTrendHistory.size(); i++) {
                recentSum += m_deltaTrendHistory[i];
            }
            float recentAvg = recentSum / recentSamples;
            
            // Calculate older average (earlier samples)
            float olderSum = 0.0f;
            for (int i = 0; i < olderSamples; i++) {
                olderSum += m_deltaTrendHistory[i];
            }
            float olderAvg = olderSum / olderSamples;
            
            // Delta is improving if the recent average is more negative (smaller) than older average
            // This means you're gaining time on your reference
            m_isDeltaImproving = recentAvg < olderAvg;
        } else {
            // Not enough data, assume improving if delta is negative (faster than reference)
            m_isDeltaImproving = m_currentDelta < 0.0f;
        }
    }

    std::string getReferenceText() const
    {
        switch (m_referenceMode) {
            case ReferenceMode::ALLTIME_BEST: 
                return "ALL-TIME BEST";
            case ReferenceMode::SESSION_BEST:
                return "SESSION BEST";
            case ReferenceMode::ALLTIME_OPTIMAL:
                return "ALL-TIME OPTIMAL";
            case ReferenceMode::SESSION_OPTIMAL:
                return "SESSION OPTIMAL";
            case ReferenceMode::LAST_LAP: 
                return "LAST LAP";
            default: 
                return "SESSION BEST";
        }
    }

    float4 getDeltaColor(float delta) const
    {
        // Color based on trend: green if improving, red if getting worse
        if (m_isDeltaImproving) {
            return float4(0.0f, 0.9f, 0.2f, 1.0f); // Green (improving/gaining time)
        } else {
            return float4(1.0f, 0.2f, 0.2f, 1.0f); // Red (getting worse/losing time)
        }
    }

    bool shouldShowDelta() const
    {
        if (StubDataManager::shouldUseStubData()) {
            return StubDataManager::getStubDeltaValid(); // Show when stub data says delta is valid
        }

        // Must be on track
        const bool isOnTrack = ir_IsOnTrack.getBool();
        if (!isOnTrack) {
            return false;
        }

        // Don't show in pits
        const bool isInPits = ir_OnPitRoad.getBool();
        if (isInPits) {
            return false;
        }

        // Must have moved past first sector (iRacing waits until it can compare positions)
        const float lapDistPct = ir_LapDistPct.getFloat();
        if (lapDistPct < 0.05f) { // Before first sector
            return false;
        }

        // Check if delta is valid based on reference mode
        bool deltaValid = false;
        switch (m_referenceMode) {
            case ReferenceMode::ALLTIME_BEST:
                deltaValid = ir_LapDeltaToBestLap_OK.getBool();
                break;
            case ReferenceMode::SESSION_BEST:
                deltaValid = ir_LapDeltaToSessionBestLap_OK.getBool();
                break;
            case ReferenceMode::ALLTIME_OPTIMAL:
                deltaValid = ir_LapDeltaToOptimalLap_OK.getBool();
                break;
            case ReferenceMode::SESSION_OPTIMAL:
                deltaValid = ir_LapDeltaToSessionOptimalLap_OK.getBool();
                break;
            case ReferenceMode::LAST_LAP:
                deltaValid = ir_LapDeltaToBestLap_OK.getBool(); // Fallback
                break;
        }

        return deltaValid;
    }

    void drawCard(float x, float y, float width, float height, const float4& bgColor = float4(0.0f, 0.0f, 0.0f, 0.85f))
    {
        const float cornerRadius = height * 0.5f;
        
        m_brush->SetColor(bgColor);
        
        D2D1_ROUNDED_RECT cardRect;
        cardRect.rect = { x, y, x + width, y + height };
        cardRect.radiusX = cornerRadius;
        cardRect.radiusY = cornerRadius;
        m_renderTarget->FillRoundedRectangle(&cardRect, m_brush.Get());
        
        // Subtle border
        const float4 borderColor = float4(0.3f, 0.3f, 0.3f, 0.6f);
        m_brush->SetColor(borderColor);
        m_renderTarget->DrawRoundedRectangle(&cardRect, m_brush.Get(), 1.0f);
    }

    void createScaledTextFormats(float scale)
    {
        const std::string font = g_cfg.getString(m_name, "font", "Consolas");
        const float titleSize = g_cfg.getFloat(m_name, "title_font_size", 16.0f) * scale;
        const float deltaSize = g_cfg.getFloat(m_name, "delta_font_size", 32.0f) * scale;
        const float smallSize = g_cfg.getFloat(m_name, "small_font_size", 14.0f) * scale;
        const int fontWeight = g_cfg.getInt(m_name, "font_weight", 500);
        const std::string fontStyleStr = g_cfg.getString(m_name, "font_style", "normal");

        // Convert font style string to enum
        DWRITE_FONT_STYLE fontStyle = DWRITE_FONT_STYLE_NORMAL;
        if (fontStyleStr == "italic") fontStyle = DWRITE_FONT_STYLE_ITALIC;
        else if (fontStyleStr == "oblique") fontStyle = DWRITE_FONT_STYLE_OBLIQUE;

        // Create scaled text formats
        HRCHECK(m_dwriteFactory->CreateTextFormat(
            toWide(font).c_str(), NULL,
            (DWRITE_FONT_WEIGHT)fontWeight, fontStyle, DWRITE_FONT_STRETCH_NORMAL,
            titleSize, L"en-us", &m_scaledTitleFormat));
        m_scaledTitleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_scaledTitleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        HRCHECK(m_dwriteFactory->CreateTextFormat(
            toWide(font).c_str(), NULL,
            DWRITE_FONT_WEIGHT_BOLD, fontStyle, DWRITE_FONT_STRETCH_NORMAL,
            deltaSize, L"en-us", &m_scaledDeltaFormat));
        m_scaledDeltaFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_scaledDeltaFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        HRCHECK(m_dwriteFactory->CreateTextFormat(
            toWide(font).c_str(), NULL,
            (DWRITE_FONT_WEIGHT)fontWeight, fontStyle, DWRITE_FONT_STRETCH_NORMAL,
            smallSize, L"en-us", &m_scaledSmallFormat));
        m_scaledSmallFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_scaledSmallFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    void drawText(const std::wstring& text, float x, float y, float width, float height, 
                  IDWriteTextFormat* format, const float4& color)
    {
        m_brush->SetColor(color);
        D2D1_RECT_F textRect = { x, y, x + width, y + height };
        m_renderTarget->DrawTextA(text.c_str(), (int)text.size(), format, &textRect, m_brush.Get());
    }

    void drawCircularDelta(float centerX, float centerY, float radius, float delta, float scale)
    {
        // Colors
        const float4 circleOutline = float4(0.3f, 0.3f, 0.3f, 1.0f);
        const float4 circleBackground = float4(0.1f, 0.1f, 0.1f, 0.95f);
        const float4 deltaColor = getDeltaColor(delta);
        
        // Draw main circle background
        m_brush->SetColor(circleBackground);
        D2D1_ELLIPSE circle = { { centerX, centerY }, radius, radius };
        m_renderTarget->FillEllipse(&circle, m_brush.Get());
        
        // Draw outer ring (scaled thickness)
        m_brush->SetColor(circleOutline);
        m_renderTarget->DrawEllipse(&circle, m_brush.Get(), 2.0f * scale);
        
        // Draw delta accent ring (progress based on delta magnitude, color based on delta sign)
        float deltaProgress = std::min(1.0f, fabs(delta) / 2.0f); // Max 2 seconds for full ring
        if (deltaProgress > 0.1f) {
            drawArcProgress(centerX, centerY, radius - (4.0f * scale), deltaProgress, deltaColor, scale);
        }
        
        // Draw "DELTA" label at top (scaled positioning)
        const float4 labelColor = float4(0.6f, 0.6f, 0.6f, 1.0f);
        float labelWidth = 60.0f * scale;
        float labelHeight = 16.0f * scale;
        drawText(L"DELTA", centerX - labelWidth/2, centerY - radius + (15.0f * scale), 
                 labelWidth, labelHeight, m_scaledSmallFormat.Get(), labelColor);
        
        // Draw delta value in center (scaled positioning)
        wchar_t deltaBuffer[32];
        if (fabs(delta) < 0.005f) { // Less than half a hundredth
            swprintf_s(deltaBuffer, L"±0.00");
        } else {
            // Display in hundredths format (+0.48/-0.48)
            swprintf_s(deltaBuffer, L"%+.2f", delta);
        }
        
        float deltaWidth = 90.0f * scale;
        float deltaHeight = 40.0f * scale;
        drawText(deltaBuffer, centerX - deltaWidth/2, centerY - deltaHeight/2, 
                 deltaWidth, deltaHeight, m_scaledDeltaFormat.Get(), deltaColor);
    }

    void drawArcProgress(float centerX, float centerY, float radius, float progress, const float4& color, float scale)
    {
        if (progress <= 0.0f) return;
        
        // Create arc geometry
        Microsoft::WRL::ComPtr<ID2D1PathGeometry> arcGeometry;
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> geometrySink;
        
        HRCHECK(m_d2dFactory->CreatePathGeometry(&arcGeometry));
        HRCHECK(arcGeometry->Open(&geometrySink));
        
        // Calculate arc angles (start from top, go clockwise)
        float startAngle = -1.5708f; // -90 degrees (top)
        float endAngle = startAngle + (progress * 6.28318f); // Full circle = 2*PI
        
        // Calculate start and end points
        float startX = centerX + cosf(startAngle) * radius;
        float startY = centerY + sinf(startAngle) * radius;
        float endX = centerX + cosf(endAngle) * radius;
        float endY = centerY + sinf(endAngle) * radius;
        
        // Create arc path
        geometrySink->BeginFigure({ startX, startY }, D2D1_FIGURE_BEGIN_HOLLOW);
        
        D2D1_ARC_SEGMENT arcSegment;
        arcSegment.point = { endX, endY };
        arcSegment.size = { radius, radius };
        arcSegment.rotationAngle = 0.0f;
        arcSegment.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
        arcSegment.arcSize = (progress > 0.5f) ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;
        
        geometrySink->AddArc(arcSegment);
        geometrySink->EndFigure(D2D1_FIGURE_END_OPEN);
        
        HRCHECK(geometrySink->Close());
        
        // Draw the arc (scaled thickness)
        m_brush->SetColor(color);
        m_renderTarget->DrawGeometry(arcGeometry.Get(), m_brush.Get(), 4.0f * scale);
    }

    void drawSessionInfo(float x, float y, float width, float height, float scale)
    {
        const float4 bgColor = float4(0.1f, 0.1f, 0.1f, 0.95f);
        const float4 textColor = float4(0.8f, 0.8f, 0.8f, 1.0f);
        const float4 timeColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
        const float4 predictedColor = float4(0.0f, 1.0f, 0.3f, 1.0f);
        
        // Background
        drawCard(x, y, width, height, bgColor);
        
        // Calculate column widths for side-by-side layout (scaled padding)
        const float padding = 8.0f * scale;
        const float columnWidth = (width - (3.0f * padding)) / 2; // Split into two columns with padding
        const float leftX = x + padding;
        const float rightX = x + (2.0f * padding) + columnWidth;
        const float panelCenterY = y + height * 0.5f;
        const float innerSpacing = 6.0f * scale; // spacing between time and its label
        
        // Reference lap time section (left side)
        float referenceLapTime = getReferenceLapTime();
        if (referenceLapTime > 0.0f) {
            // Format time as MM:SS.mmm
            int minutes = (int)(referenceLapTime / 60.0f);
            float seconds = referenceLapTime - (minutes * 60.0f);
            
            wchar_t timeBuffer[32];
            swprintf_s(timeBuffer, L"%02d:%06.3f", minutes, seconds);
            
            float timeHeight = 25.0f * scale;
            float labelHeight = 15.0f * scale;
            // Vertically center the time + label block within the panel
            const float totalBlockH = timeHeight + innerSpacing + labelHeight;
            const float blockTop = panelCenterY - (totalBlockH * 0.5f);
            float timeY = blockTop;
            float labelY = blockTop + timeHeight + innerSpacing;
            
            drawText(timeBuffer, leftX, timeY, columnWidth, timeHeight, m_scaledDeltaFormat.Get(), timeColor);
            
            // Draw reference mode label
            std::string refText = getReferenceText();
            std::wstring refTextW(refText.begin(), refText.end());
            drawText(refTextW, leftX, labelY, columnWidth, labelHeight, m_scaledSmallFormat.Get(), textColor);
        }
        
        // Predicted time section (right side)
        if (shouldShowDelta()) {
            float currentDelta = m_currentDelta;
            
            // Calculate predicted time based on reference + current delta
            float predictedTime = (referenceLapTime > 0.0f) ? (referenceLapTime + currentDelta) : 0.0f;
            
            if (predictedTime > 0.0f) {
                int minutes = (int)(predictedTime / 60.0f);
                float seconds = predictedTime - (minutes * 60.0f);
                
                wchar_t timeBuffer[32];
                swprintf_s(timeBuffer, L"%02d:%06.3f", minutes, seconds);
                
                float timeHeight = 25.0f * scale;
                float labelHeight = 15.0f * scale;
                // Vertically center the time + label block within the panel
                const float totalBlockH = timeHeight + innerSpacing + labelHeight;
                const float blockTop = panelCenterY - (totalBlockH * 0.5f);
                float timeY = blockTop;
                float labelY = blockTop + timeHeight + innerSpacing;
                
                drawText(timeBuffer, rightX, timeY, columnWidth, timeHeight, m_scaledDeltaFormat.Get(), predictedColor);
                drawText(L"PREDICTED", rightX, labelY, columnWidth, labelHeight, m_scaledSmallFormat.Get(), textColor);
            }
        }
    }

    float getReferenceLapTime() const
    {
        if (StubDataManager::shouldUseStubData()) {
            return StubDataManager::getStubSessionBestLapTime();
        }

        switch (m_referenceMode) {
            case ReferenceMode::ALLTIME_BEST:
            case ReferenceMode::LAST_LAP: // Fallback
                return ir_LapBestLapTime.getFloat();
                
            case ReferenceMode::SESSION_BEST:
                // iRacing doesn't provide the actual session best lap time directly
                // Use player's best lap time as approximation
                return ir_LapBestLapTime.getFloat();
                
            case ReferenceMode::ALLTIME_OPTIMAL:
            case ReferenceMode::SESSION_OPTIMAL:
                // These don't have direct lap time equivalents
                return ir_LapBestLapTime.getFloat();
                
            default:
                return ir_LapBestLapTime.getFloat();
        }
    }

    virtual void onUpdate()
    {
        // Update delta calculation
        updateDelta();

        // Check if we should show the overlay (like iRacing delta bar)
        if (!shouldShowDelta()) {
            // Don't draw anything if conditions aren't met
            m_renderTarget->BeginDraw();
            m_renderTarget->Clear(float4(0, 0, 0, 0)); // Transparent background
            m_renderTarget->EndDraw();
            return;
        }

        // Start drawing
        m_renderTarget->BeginDraw();
        m_renderTarget->Clear(float4(0, 0, 0, 0)); // Transparent background

        // Get current delta
        float displayDelta = m_currentDelta;
        
        // Calculate scale factor based on current overlay size
        const float baseWidth = 500.0f;  // Reference width for 1.0 scale
        const float baseHeight = 190.0f; // Reference height for 1.0 scale
        const float scaleX = (float)m_width / baseWidth;
        const float scaleY = (float)m_height / baseHeight;
        const float scale = std::min(scaleX, scaleY); // Use smaller scale to maintain aspect ratio
        
        // Scaled layout constants
        const float circleRadius = 85.0f * scale;
        const float padding = 10.0f * scale;
        const float circleX = circleRadius + padding;
        const float circleY = circleRadius + padding;
        
        // Info panel takes remaining width
        const float infoX = circleX + circleRadius + (20.0f * scale);
        const float infoWidth = (float)m_width - infoX - padding;
        const float infoHeight = 100.0f * scale;
        // Allow fine-tuning of info panel vertical positioning relative to the circle center
        const float infoYOffset = g_cfg.getFloat(m_name, "info_vertical_offset", 0.0f) * scale; // +down, -up
        const float infoY = circleY - infoHeight / 2 + infoYOffset;

        // Create scaled text formats
        createScaledTextFormats(scale);

        // Draw circular delta display
        drawCircularDelta(circleX, circleY, circleRadius, displayDelta, scale);
        
        // Draw session info panel
        drawSessionInfo(infoX, infoY, infoWidth, infoHeight, scale);

        m_renderTarget->EndDraw();
    }

    virtual float2 getDefaultSize()
    {
        return float2(500, 190); // Proper size for the layout
    }

    virtual bool hasCustomBackground()
    {
        return true; // We draw our own background
    }

    virtual void sessionChanged()
    {
        // Reset trend data when session changes
        m_deltaTrendHistory.clear();
        m_isDeltaImproving = false;
    }
};