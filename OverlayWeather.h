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
#include <algorithm>
#include <deque>
#include <string>
#include <memory>
#include <wincodec.h>
#include <cmath>
#include "Overlay.h"
#include "iracing.h"
#include "Units.h"
#include "Config.h"
#include "OverlayDebug.h"
#include "stub_data.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class OverlayWeather : public Overlay
{
    public:

        OverlayWeather()
            : Overlay("OverlayWeather")
        {}

       #ifdef _DEBUG
       virtual bool    canEnableWhileNotDriving() const { return true; }
       virtual bool    canEnableWhileDisconnected() const { return true; }
       #else
       virtual bool    canEnableWhileDisconnected() const { return StubDataManager::shouldUseStubData(); }
       #endif

    protected:

        struct WeatherBox
        {
            float x0 = 0;
            float x1 = 0;
            float y0 = 0;
            float y1 = 0;
            float w = 0;
            float h = 0;
            std::string title;
            std::string iconPath;
        };

        virtual float2 getDefaultSize()
        {
            // Reference size for scaling calculations
            return float2(320, 800);
        }

        virtual void onEnable()
        {
            (void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            HRCHECK(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wicFactory)));
            
            onConfigChanged();
            loadIcons();

            // Recreate D2D brushes (render target may change)
            m_bgBrush.Reset();
            m_panelBrush.Reset();
        }

        virtual void onDisable()
        {
            m_text.reset();
            releaseIcons();
            m_wicFactory.Reset();
            m_bgBrush.Reset();
            m_panelBrush.Reset();
        }

        virtual void onConfigChanged()
        {
            const float2 refSize = getDefaultSize();
            
            if (m_width <= 0 || m_height <= 0 || refSize.x <= 0 || refSize.y <= 0) {
                m_scaleFactorX = m_scaleFactorY = m_scaleFactor = 1.0f;
            } else {
                m_scaleFactorX = (float)m_width / refSize.x;
                m_scaleFactorY = (float)m_height / refSize.y;
                m_scaleFactor = std::min(m_scaleFactorX, m_scaleFactorY); 
                
                m_scaleFactor = std::max(0.1f, std::min(10.0f, m_scaleFactor));
                m_scaleFactorX = std::max(0.1f, std::min(10.0f, m_scaleFactorX));
                m_scaleFactorY = std::max(0.1f, std::min(10.0f, m_scaleFactorY));
            }
            
            // Font setup with dynamic scaling (centralized)
            {
                m_text.reset( m_dwriteFactory.Get() );
                m_fontSpacing = getGlobalFontSpacing();
                const float baseScale = std::max(0.1f, std::min(10.0f, m_scaleFactor));
                createGlobalTextFormat(baseScale * 1.0f, (int)DWRITE_FONT_WEIGHT_BOLD, "", m_textFormat);
                createGlobalTextFormat(baseScale * 1.0f, (int)DWRITE_FONT_WEIGHT_BOLD, "", m_textFormatBold);
                createGlobalTextFormat(baseScale * 0.8f, (int)DWRITE_FONT_WEIGHT_BOLD, "", m_textFormatSmall);
                createGlobalTextFormat(baseScale * 1.5f, (int)DWRITE_FONT_WEIGHT_BOLD, "", m_textFormatLarge);
            }

            setupWeatherBoxes();
            
            // Load icons after render target is set up
            if (m_wicFactory.Get()) {
                loadIcons();
            }

            // Invalidate static text cache on layout changes
            m_staticTextBitmap.Reset();

            // Per-overlay FPS (configurable; default 10)
            setTargetFPS(g_cfg.getInt(m_name, "target_fps", 10));

            // Recreate D2D brushes (render target may change)
            m_bgBrush.Reset();
            m_panelBrush.Reset();
        }

                 virtual void onUpdate()
         {
             // Only update weather data at specified interval to improve performance
             const double currentTime = ir_now();
             if (currentTime - m_lastWeatherUpdate >= WEATHER_UPDATE_INTERVAL) {
                 m_lastWeatherUpdate = currentTime;
             }

            const float  fontSize           = g_cfg.getFloat( "Overlay", "font_size", 16.0f );
            const float4 textCol            = g_cfg.getFloat4( m_name, "text_col", float4(1,1,1,0.9f) );
            const float4 backgroundCol      = g_cfg.getFloat4( m_name, "background_col", float4(0,0,0,0.7f) );
            const float4 accentCol          = float4(0.2f, 0.75f, 0.95f, 0.9f);

            // Apply global opacity first; then derive text colors
            const float globalOpacity = getGlobalOpacity();
            const float4 finalTextCol = float4(textCol.x, textCol.y, textCol.z, textCol.w * globalOpacity);
            
            // All text white per requested style. Keep an accent color for wetness bar and arrow
            const float4 tempCol            = finalTextCol;
            const float4 trackTempCol       = finalTextCol;
            const float4 precipCol          = finalTextCol;
            const float4 windCol            = finalTextCol;

            // Use stub data in preview mode
            const bool useStubData = StubDataManager::shouldUseStubData();
            const bool imperial = isImperialUnits();

            wchar_t s[512];

            m_renderTarget->BeginDraw();
            m_renderTarget->Clear( float4(0,0,0,0) );

            ensureStyleBrushes();

            const float titlePadding = std::max(1.5f, 20.0f * m_scaleFactor);   
            const float titleMargin = std::max(1.5f, 20.0f * m_scaleFactor);   
            const float valuePadding = std::max(1.5f, 15.0f * m_scaleFactor);  
            const float iconSize = std::max(6.0f, std::min(300.0f, 42.0f * m_scaleFactor));
            const float iconAdjustment = std::max(1.5f, 18.0f * m_scaleFactor);

            const float W = (float)m_width;
            const float H = (float)m_height;
            const float minDim = std::max(1.0f, std::min(W, H));
            const float pad = std::clamp(minDim * 0.045f, 8.0f, 18.0f);
            const float corner = std::clamp(minDim * 0.070f, 10.0f, 26.0f);

            const float bgAlpha = std::clamp(backgroundCol.w, 0.0f, 1.0f);

            {
                D2D1_RECT_F rCard = { pad, pad, W - pad, H - pad };
                D2D1_ROUNDED_RECT rr = { rCard, corner, corner };
                if (m_bgBrush) {
                    m_bgBrush->SetStartPoint(D2D1_POINT_2F{ rCard.left, rCard.top });
                    m_bgBrush->SetEndPoint(D2D1_POINT_2F{ rCard.left, rCard.bottom });
                    m_bgBrush->SetOpacity(0.95f * bgAlpha * globalOpacity);
                    m_renderTarget->FillRoundedRectangle(&rr, m_bgBrush.Get());
                } else {
                    m_brush->SetColor(float4(0.05f, 0.05f, 0.06f, 0.92f * bgAlpha * globalOpacity));
                    m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
                }
            }

            // Helper function to draw section background (inner panels)
            auto drawSectionBackground = [&](const WeatherBox& box) {
                D2D1_RECT_F bgRect = { box.x0, box.y0, box.x1, box.y1 };
                const float panelCorner = std::clamp(corner * 0.75f, 8.0f, 22.0f);
                D2D1_ROUNDED_RECT roundedBg = { bgRect, panelCorner, panelCorner };

                if (m_panelBrush) {
                    m_panelBrush->SetStartPoint(D2D1_POINT_2F{ bgRect.left, bgRect.top });
                    m_panelBrush->SetEndPoint(D2D1_POINT_2F{ bgRect.left, bgRect.bottom });
                    m_panelBrush->SetOpacity(0.92f * bgAlpha * globalOpacity);
                    m_renderTarget->FillRoundedRectangle(&roundedBg, m_panelBrush.Get());
                } else {
                    // Fallback: solid background using legacy config color
                    float4 bgColor = backgroundCol;
                    bgColor.w *= globalOpacity;
                    m_brush->SetColor(bgColor);
                    m_renderTarget->FillRoundedRectangle(&roundedBg, m_brush.Get());
                }

                m_brush->SetColor(float4(0.9f, 0.9f, 0.95f, 0.18f * bgAlpha * globalOpacity));
                m_renderTarget->DrawRoundedRectangle(&roundedBg, m_brush.Get(), 1.5f);
            };

            if (!m_staticTextBitmap) {
                buildStaticTextBitmap();
            }
            if (m_staticTextBitmap) {
                D2D1_SIZE_F s = m_staticTextBitmap->GetSize();
                D2D1_RECT_F dest = { 0, 0, s.width, s.height };
                m_renderTarget->DrawBitmap(m_staticTextBitmap.Get(), dest);
            }

            // Track Temperature Section
            {
                drawSectionBackground(m_trackTempBox);
                
                float trackTemp = useStubData ? StubDataManager::getStubTrackTemp() : ir_TrackTempCrew.getFloat();
                
                if( imperial )
                    trackTemp = celsiusToFahrenheit( trackTemp );


                // Temperature value - larger, centered
                m_brush->SetColor( trackTempCol );
                const wchar_t degree = L'\x00B0';
                swprintf( s, _countof(s), L"%.1f%lc%c", trackTemp, degree, imperial ? 'F' : 'C' );
                const float tempValueY = m_trackTempBox.y0 + m_trackTempBox.h * 0.65f;

                // Icon on the left side of temperature value
                const float iconX = m_trackTempBox.x0 + valuePadding;
                drawIcon(m_trackTempIcon.Get(), iconX, tempValueY - iconAdjustment, iconSize, iconSize, true);

                // Adjust text position to be after the icon
                const float textOffset = iconX + iconSize + valuePadding;
                m_text.render( m_renderTarget.Get(), s, m_textFormatLarge.Get(), 
                              textOffset, m_trackTempBox.x1 - valuePadding, tempValueY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing );
            }

            // Track Wetness Section
            {
                drawSectionBackground(m_trackWetnessBox);
                
                // Normalize enum (0-7) to progress bar range (0.0-1.0) for visualization
                float trackWetnessBar = 0.0f;
                std::string wetnessText;
                if (useStubData)
                {
                    const float stubWet = StubDataManager::getStubTrackWetness();
                    trackWetnessBar = std::max(0.0f, std::min(1.0f, stubWet));
                    const int wetEnumForText = (int)std::round(trackWetnessBar * 7.0f);
                    wetnessText = getTrackWetnessText((float)wetEnumForText);
                }
                else
                {
                    const int wetEnum = ir_TrackWetness.getInt();
                    trackWetnessBar = std::max(0.0f, std::min(1.0f, (float)wetEnum / 7.0f));
                    wetnessText = getTrackWetnessText((float)wetEnum);
                }

                // Title is cached in static bitmap

                // Progress bar showing wetness level
                {
                    const float sideIconSize = 30 * m_scaleFactor;
                    const float sideIconAdjust = 7.5f * m_scaleFactor;
                    
                    // Calculate bar width to fit between icons at title padding positions
                    const float barWidth = m_trackWetnessBox.w - (2.5f * titlePadding) - (2.5f * sideIconSize);
                    const float barHeight = 12 * m_scaleFactor;
                    const float barX = m_trackWetnessBox.x0 + (m_trackWetnessBox.w - barWidth) * 0.5f;
                    const float barY = m_trackWetnessBox.y0 + m_trackWetnessBox.h * 0.6f;
                    
                    // Sun icon aligned with left title padding
                    drawIcon(m_sunIcon.Get(), m_trackWetnessBox.x0 + titlePadding, barY - sideIconAdjust, sideIconSize, sideIconSize, true);
                    
                    // Waterdrop icon aligned with right title padding
                    drawIcon(m_trackWetnessIcon.Get(), m_trackWetnessBox.x1 - titlePadding - sideIconSize, barY - sideIconAdjust, sideIconSize, sideIconSize, true);
                    
                    // Background bar with white outline
                    D2D1_RECT_F barBg = { barX, barY, barX + barWidth, barY + barHeight };
                    m_brush->SetColor( float4(0.3f, 0.3f, 0.3f, 0.8f) );
                    const float cornerRadius = 6 * m_scaleFactor;
                    D2D1_ROUNDED_RECT rrBg = { barBg, cornerRadius, cornerRadius };
                    m_renderTarget->FillRoundedRectangle( &rrBg, m_brush.Get() );
                    
                    // White outline around bar
                    m_brush->SetColor( float4(1.0f, 1.0f, 1.0f, 0.6f) );
                    const float outlineThickness = 1.0f * m_scaleFactor;
                    m_renderTarget->DrawRoundedRectangle( &rrBg, m_brush.Get(), outlineThickness );
                    
                    // Wetness fill
                    if (trackWetnessBar > 1) {
                        D2D1_RECT_F bar = { barX, barY, barX + (barWidth * trackWetnessBar), barY + barHeight };
                        m_brush->SetColor( accentCol );
                        D2D1_ROUNDED_RECT rr = { bar, cornerRadius, cornerRadius };
                        m_renderTarget->FillRoundedRectangle( &rr, m_brush.Get() );
                    }
                }

                // Wetness text description with increased padding from graph
                m_brush->SetColor( finalTextCol );
                m_text.render( m_renderTarget.Get(), toWide(wetnessText).c_str(), m_textFormatBold.Get(), 
                              m_trackWetnessBox.x0 + valuePadding, m_trackWetnessBox.x1 - valuePadding, m_trackWetnessBox.y0 + m_trackWetnessBox.h * 0.85f, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );
            }

            // Precipitation/Air Temperature Section
            {
                drawSectionBackground(m_precipitationBox);
                
                const bool showPrecip = shouldShowPrecipitation();
                
                // Title is cached in static bitmap (both labels)

                const float valueY = m_precipitationBox.y0 + m_precipitationBox.h * 0.65f;
                const float iconX = m_precipitationBox.x0 + titlePadding; 

                if (showPrecip) {
                    // Show precipitation data
                    float precipitation = useStubData ? StubDataManager::getStubPrecipitation() : getPrecipitationValue();

                    // Icon on the left
                    drawIcon(m_precipitationIcon.Get(), iconX, valueY - iconAdjustment, iconSize, iconSize, true);

                    // Percentage value - larger, to the right of the icon
                    m_brush->SetColor( precipCol );
                    swprintf( s, _countof(s), L"%.0f%%", precipitation * 100.0f );
                    const float textOffset = titlePadding + iconSize + (15 * m_scaleFactor);
                    m_text.render( m_renderTarget.Get(), s, m_textFormatLarge.Get(),
                                  m_precipitationBox.x0 + textOffset, m_precipitationBox.x1 - valuePadding, valueY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing );
                } else {
                    // Show air temperature
                    float airTemp = useStubData ? StubDataManager::getStubAirTemp() : ir_AirTemp.getFloat();
                    if (imperial) {
                        airTemp = celsiusToFahrenheit(airTemp);
                    }

                    // Use air temp icon
                    drawIcon(m_trackTempIcon.Get(), iconX, valueY - iconAdjustment, iconSize, iconSize, true);

                    // Temperature value - larger, to the right of the icon
                    m_brush->SetColor( precipCol );
                    const wchar_t degree = L'\x00B0';
                    swprintf( s, _countof(s), L"%.1f%lc%c", airTemp, degree, imperial ? 'F' : 'C' );
                    const float textOffset = titlePadding + iconSize + (15 * m_scaleFactor);
                    m_text.render( m_renderTarget.Get(), s, m_textFormatLarge.Get(),
                                  m_precipitationBox.x0 + textOffset, m_precipitationBox.x1 - valuePadding, valueY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing );
                }
            }

            // Wind Section (Compass)
            {
                drawSectionBackground(m_windBox);
                
                float windSpeed = useStubData ? StubDataManager::getStubWindSpeed() : ir_WindVel.getFloat();
                // Car yaw relative to north (0 in stub)
                const float carYaw = useStubData ? 0.0f : ir_YawNorth.getFloat();
                // Wind direction relative to car heading so the car is static and the arrow shows flow over the car
                float windDir = (useStubData ? StubDataManager::getStubWindDirection() : ir_WindDir.getFloat()) - carYaw;
                // Normalize to [0, 2π] for stable trig
                const float twoPi = (float)(2.0 * M_PI);
                while (windDir < 0.0f) windDir += twoPi;
                while (windDir >= twoPi) windDir -= twoPi;

                // Title is cached in static bitmap

                // Draw compass - dynamically positioned and sized with safety checks
                const float compassCenterX = m_windBox.x0 + m_windBox.w/2;
                const float compassCenterY = m_windBox.y0 + m_windBox.h * 0.5f; // Center in available box space
                const float compassRadius = std::max(22.5f, std::min(m_windBox.w, m_windBox.h) * 0.375f);
                
                D2D1_ELLIPSE compassCircle = { {compassCenterX, compassCenterY}, compassRadius, compassRadius };
                m_brush->SetColor(float4(0.1f, 0.1f, 0.1f, 1.0f));
                m_renderTarget->FillEllipse(&compassCircle, m_brush.Get());
                
                drawWindCompass(windDir, compassCenterX, compassCenterY, compassRadius, carYaw);

                // Wind speed at bottom with icon - left aligned like title
                const float windSpeedBottomMargin = 52.5f * m_scaleFactor;
                const float windSpeedY = m_windBox.y0 + m_windBox.h - windSpeedBottomMargin;
                
                // Convert wind speed for display
                if (imperial) {
                    windSpeed *= 2.237f; // m/s to mph
                    swprintf( s, _countof(s), L"%.0f MPH", windSpeed );
                } else {
                    windSpeed *= 3.6f; // m/s to km/h
                    swprintf( s, _countof(s), L"%.0f KPH", windSpeed );
                }
                
                // Wind icon aligned to the left like title
                const float windIconSize = 50 * m_scaleFactor;
                const float windIconAdjust = 25 * m_scaleFactor;
                drawIcon(m_windIcon.Get(), m_windBox.x0 + titlePadding, windSpeedY - windIconAdjust, windIconSize, windIconSize, true);
                
                // Wind speed text left-aligned like title
                const float windTextOffset = 75.0f * m_scaleFactor;
                m_brush->SetColor( windCol );
                m_text.render( m_renderTarget.Get(), s, m_textFormatLarge.Get(), 
                              m_windBox.x0 + windTextOffset, m_windBox.x1 - titleMargin, windSpeedY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing );
            }

            m_renderTarget->EndDraw();
        }

    private:

        void ensureStyleBrushes()
        {
            if (!m_renderTarget) return;
            if (m_bgBrush && m_panelBrush) return;

            // Card background gradient (same palette as OverlayPit/OverlayFlags)
            {
                D2D1_GRADIENT_STOP stops[3] = {};
                stops[0].position = 0.0f;  stops[0].color = D2D1::ColorF(0.16f, 0.18f, 0.22f, 1.0f);
                stops[1].position = 0.45f; stops[1].color = D2D1::ColorF(0.06f, 0.07f, 0.09f, 1.0f);
                stops[2].position = 1.0f;  stops[2].color = D2D1::ColorF(0.02f, 0.02f, 0.03f, 1.0f);

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

            // Inner panel gradient (same palette as OverlayPit/OverlayFlags)
            {
                D2D1_GRADIENT_STOP stops[3] = {};
                stops[0].position = 0.0f;  stops[0].color = D2D1::ColorF(0.08f, 0.09f, 0.11f, 1.0f);
                stops[1].position = 0.55f; stops[1].color = D2D1::ColorF(0.04f, 0.045f, 0.055f, 1.0f);
                stops[2].position = 1.0f;  stops[2].color = D2D1::ColorF(0.02f, 0.02f, 0.03f, 1.0f);

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

        void setupWeatherBoxes()
        {
            const float padding = std::max(1.5f, 22.5f * m_scaleFactor);
            const float spacing = std::max(1.5f, 15.0f * m_scaleFactor);

            const float availableHeight = std::max(60.0f, (float)m_height - 2 * padding - 3 * spacing);
            const float trackTempHeight = std::max(15.0f, availableHeight * 0.15f);
            const float trackWetnessHeight = std::max(15.0f, availableHeight * 0.15f);
            const float precipitationHeight = std::max(15.0f, availableHeight * 0.15f);
            const float windHeight = std::max(30.0f, availableHeight * 0.55f);
            
            float yPos = padding;
            
            const float boxWidth = std::max(30.0f, (float)m_width - 2*padding);
            m_trackTempBox = makeWeatherBox(padding, boxWidth, yPos, "Track Temperature", "assets/icons/track_temp.png");
            m_trackTempBox.h = trackTempHeight;
            m_trackTempBox.y1 = m_trackTempBox.y0 + trackTempHeight;
            
            yPos += trackTempHeight + spacing;
            m_trackWetnessBox = makeWeatherBox(padding, boxWidth, yPos, "Track Wetness", "assets/icons/waterdrop.png");
            m_trackWetnessBox.h = trackWetnessHeight;
            m_trackWetnessBox.y1 = m_trackWetnessBox.y0 + trackWetnessHeight;
            
            yPos += trackWetnessHeight + spacing;
            m_precipitationBox = makeWeatherBox(padding, boxWidth, yPos, "Precipitation", "assets/icons/precipitation.png");
            m_precipitationBox.h = precipitationHeight;
            m_precipitationBox.y1 = m_precipitationBox.y0 + precipitationHeight;
            
            yPos += precipitationHeight + spacing;
            m_windBox = makeWeatherBox(padding, boxWidth, yPos, "Wind", "assets/icons/wind.png");
            m_windBox.h = windHeight;
            m_windBox.y1 = m_windBox.y0 + windHeight;
        }

        WeatherBox makeWeatherBox(float x, float w, float y, const std::string& title, const std::string& iconPath)
        {
            WeatherBox box;
            box.x0 = x;
            box.x1 = x + w;
            box.y0 = y;
            box.y1 = y;
            box.w = w;
            box.h = 0;
            box.title = title;
            box.iconPath = iconPath;
            return box;
        }

        // Helpers for robust asset path resolution
        static bool ow_fileExistsW(const std::wstring& path)
        {
            DWORD a = GetFileAttributesW(path.c_str());
            return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
        }

        static std::wstring ow_getExecutableDirW()
        {
            wchar_t path[MAX_PATH] = {0};
            GetModuleFileNameW(NULL, path, MAX_PATH);
            wchar_t* last = wcsrchr(path, L'\\');
            if (last) *last = 0;
            return std::wstring(path);
        }

        static std::wstring ow_resolveAssetPath(const std::wstring& relative)
        {
            const std::wstring exeDir = ow_getExecutableDirW();
            // Candidate 1: repo root two levels up from exe dir (mirrors GUI HTML loader)
            std::wstring candidateRepo = exeDir + L"\\..\\..\\" + relative;
            if (ow_fileExistsW(candidateRepo)) return candidateRepo;
            // Candidate 2: next to the executable
            std::wstring candidateLocal = exeDir + L"\\" + relative;
            if (ow_fileExistsW(candidateLocal)) return candidateLocal;
            // Fallback: provided relative path
            return relative;
        }

        void loadIcons()
        {
            if (!m_wicFactory.Get() || !m_renderTarget.Get()) return;

            // Helper function to load a single PNG file
            auto loadPngIcon = [&](const std::wstring& filePath) -> Microsoft::WRL::ComPtr<ID2D1Bitmap> {
                Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
                
                try {
                    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
                    HRESULT hr = m_wicFactory->CreateDecoderFromFilename(filePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
                    if (FAILED(hr)) return bitmap;

                    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
                    hr = decoder->GetFrame(0, &frame);
                    if (FAILED(hr)) return bitmap;

                    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
                    hr = m_wicFactory->CreateFormatConverter(&converter);
                    if (FAILED(hr)) return bitmap;

                    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut);
                    if (FAILED(hr)) return bitmap;

                    hr = m_renderTarget->CreateBitmapFromWicBitmap(converter.Get(), nullptr, &bitmap);
                    if (FAILED(hr)) return bitmap;

                } catch (...) {
                    // Return null bitmap on any exception
                    return bitmap;
                }
                
                return bitmap;
            };

            // Load all weather icons using resolved absolute paths
            m_trackTempIcon     = loadPngIcon(ow_resolveAssetPath(L"assets\\icons\\track_temp.png"));
            m_trackWetnessIcon  = loadPngIcon(ow_resolveAssetPath(L"assets\\icons\\waterdrop.png"));
            m_sunIcon           = loadPngIcon(ow_resolveAssetPath(L"assets\\icons\\sun.png"));
            m_precipitationIcon = loadPngIcon(ow_resolveAssetPath(L"assets\\icons\\precipitation.png"));
            m_windIcon          = loadPngIcon(ow_resolveAssetPath(L"assets\\icons\\wind.png"));
            m_carIcon           = loadPngIcon(ow_resolveAssetPath(L"assets\\sports_car.png"));
            m_windArrowIcon     = loadPngIcon(ow_resolveAssetPath(L"assets\\wind_arrow.png"));
        }

        void releaseIcons()
        {
            m_trackTempIcon.Reset();
            m_trackWetnessIcon.Reset();
            m_sunIcon.Reset();
            m_precipitationIcon.Reset();
            m_windIcon.Reset();
            m_carIcon.Reset();
            m_windArrowIcon.Reset();
        }

        void drawIcon(ID2D1Bitmap* icon, float x, float y, float w, float h, bool keepAspect=false)
        {
            if (icon) {
                D2D1_RECT_F dest = { x, y, x + w, y + h };
                if (keepAspect) {
                    D2D1_SIZE_F sz = icon->GetSize();
                    if (sz.width > 0 && sz.height > 0) {
                        const float aspect = sz.width / sz.height;
                        float dw = w, dh = h;
                        if (aspect > 1.0f) {
                            dh = w / aspect;
                        } else {
                            dw = h * aspect;
                        }
                        float offsetX = (w - dw) * 0.5f;
                        float offsetY = (h - dh) * 0.5f;
                        dest = { x + offsetX, y + offsetY, x + offsetX + dw, y + offsetY + dh };
                    }
                }
                m_renderTarget->DrawBitmap(icon, &dest);
            } else {
                D2D1_RECT_F rect = { x, y, x + w, y + h };
                m_brush->SetColor(float4(0.5f, 0.5f, 0.5f, 0.8f));
                m_renderTarget->FillRectangle(&rect, m_brush.Get());
            }
        }


        void drawWindCompass(float windDirection, float centerX, float centerY, float radius, float cardinalRotation)
        {
            const float carSize = radius * 0.7f;

            // Draw compass circle
            D2D1_ELLIPSE compassCircle = { {centerX, centerY}, radius, radius };
            m_brush->SetColor(float4(0.3f, 0.3f, 0.3f, 1.0f));
            const float compassLineThickness = 3.0f * m_scaleFactor;
            m_renderTarget->DrawEllipse(&compassCircle, m_brush.Get(), compassLineThickness);

            // Draw cardinal directions (NESW) inside compass - fixed positions, centered and larger font
            m_brush->SetColor(float4(0.8f, 0.8f, 0.8f, 0.9f));
            const wchar_t* directions[] = { L"N", L"E", L"S", L"W" };
            const float textRadius = radius * 0.8f;

            // Create a larger font for the compass labels (m_textFormatSmall + scaled amount)
            Microsoft::WRL::ComPtr<IDWriteTextFormat> compassLabelFormat;
            if (m_textFormatSmall) {
                FLOAT fontSize = 0.0f;
                fontSize = m_textFormatSmall->GetFontSize();
                fontSize += 6.0f * m_scaleFactor;
                fontSize = std::max(6.0f, std::min(150.0f, fontSize));
                DWRITE_FONT_WEIGHT weight = m_textFormatSmall->GetFontWeight();
                DWRITE_FONT_STYLE style = m_textFormatSmall->GetFontStyle();
                DWRITE_FONT_STRETCH stretch = m_textFormatSmall->GetFontStretch();

                WCHAR fontFamilyName[128] = {};
                UINT32 len = m_textFormatSmall->GetFontFamilyNameLength();
                if (len < 128) {
                    m_textFormatSmall->GetFontFamilyName(fontFamilyName, 128);
                } else {
                    wcscpy_s(fontFamilyName, L"Segoe UI");
                }

                m_dwriteFactory->CreateTextFormat(
                    fontFamilyName, NULL, weight, style, stretch, fontSize, L"en-us", &compassLabelFormat
                );
                compassLabelFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                compassLabelFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                compassLabelFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            }

            for (int i = 0; i < 4; i++) {
                // Rotate NESW by car yaw so car stays static while world directions move around it
                float angle = (float)(i * M_PI / 2) - cardinalRotation;
                float textX = centerX + textRadius * (float)sin(angle);
                float textY = centerY - textRadius * (float)cos(angle);

                float labelBoxW = 48.0f * m_scaleFactor;
                float labelBoxH = 42.0f * m_scaleFactor;
                float boxLeft = textX - labelBoxW * 0.5f;
                float boxRight = textX + labelBoxW * 0.5f;
                float boxTop = textY - labelBoxH * 0.5f;

                float verticalNudge = 21.0f * m_scaleFactor;
                boxTop += verticalNudge;

                m_text.render(
                    m_renderTarget.Get(),
                    directions[i],
                    compassLabelFormat ? compassLabelFormat.Get() : m_textFormatSmall.Get(),
                    boxLeft, boxRight, boxTop,
                    m_brush.Get(),
                    DWRITE_TEXT_ALIGNMENT_CENTER
                );
            }

            // Draw car in EXACT center (always pointing north) - perfectly centered
            const float carX = centerX - carSize * 0.5f;
            const float carY = centerY - carSize * 0.5f;
            drawIcon(m_carIcon.Get(), carX, carY, carSize, carSize, true);

            const float arrowStartRadius = radius;
            const float arrowEndRadius = radius * 0.25f;

            const float startX = centerX + arrowStartRadius * (float)sin(windDirection);
            const float startY = centerY - arrowStartRadius * (float)cos(windDirection);
            const float endX = centerX + arrowEndRadius * (float)sin(windDirection);
            const float endY = centerY - arrowEndRadius * (float)cos(windDirection);

            const float arrowWidth = 54.0f * m_scaleFactor;
            const float arrowLength = (float)hypot(endX - startX, endY - startY);

            const float midX = (startX + endX) * 0.5f;
            const float midY = (startY + endY) * 0.5f;

            D2D1_MATRIX_3X2_F oldTx;
            m_renderTarget->GetTransform(&oldTx);
            const float angleDeg = windDirection * 180.0f / (float)M_PI + 180.0f;
            m_renderTarget->SetTransform(oldTx * D2D1::Matrix3x2F::Rotation(angleDeg, D2D1::Point2F(midX, midY)));

            m_renderTarget->DrawBitmap(
                m_windArrowIcon.Get(),
                D2D1::RectF(midX - arrowWidth*0.5f, midY - arrowLength*0.5f, midX + arrowWidth*0.5f, midY + arrowLength*0.5f),
                0.75f // Opacity
            );

            m_renderTarget->SetTransform(oldTx);
        }

        float getTrackWetnessValue()
        {
            // Use iRacing's direct track wetness telemetry
            return (float)ir_TrackWetness.getInt();
        }

        std::string getTrackWetnessText(float wetnessEnum)
        {
            // Map to IRSDK TrackWetness (0..7)
            switch ((int)wetnessEnum) {
                case 0: return "No Data Available";
                case 1: return "Dry";
                case 2: return "Mostly Dry";
                case 3: return "Very Lightly Wet";
                case 4: return "Lightly Wet";
                case 5: return "Moderately Wet";
                case 6: return "Very Wet";
                case 7: return "Extremely Wet";
                default: return "No Data Available";
            }
        }

        float getPrecipitationValue()
        {
            // Use iRacing's direct precipitation telemetry
            return ir_Precipitation.getFloat();
        }

        bool shouldShowPrecipitation() const
        {
            if (StubDataManager::shouldUseStubData()) {
                // In preview mode, use the preview_weather_type setting
                return g_cfg.getInt("OverlayWeather", "preview_weather_type", 1) == 1;
            }
            // Show precipitation if we detect rain intensity or meaningful wetness
            return ir_Precipitation.getFloat() > 0.01f || ir_TrackWetness.getInt() >= 3;
        }

        virtual bool hasCustomBackground()
        {
            return true;
        }

         protected:
         // Weather data update throttling
         static constexpr double WEATHER_UPDATE_INTERVAL = 20.0; // Update weather data every 20 seconds
         // Weather changes are gradual in iRacing, so frequent updates aren't needed
         double m_lastWeatherUpdate = 0.0;

         // Scaling factors for dynamic sizing
        float m_scaleFactorX = 1.0f;
        float m_scaleFactorY = 1.0f;
        float m_scaleFactor = 1.0f;

        WeatherBox m_trackTempBox;
        WeatherBox m_trackWetnessBox; 
        WeatherBox m_precipitationBox;
        WeatherBox m_windBox;

        Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormat;
        Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormatBold;
        Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormatSmall;
        Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormatLarge;

        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_trackTempIcon;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_trackWetnessIcon;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_sunIcon;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_precipitationIcon;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_windIcon;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_carIcon;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_windArrowIcon;

        Microsoft::WRL::ComPtr<IWICImagingFactory> m_wicFactory;
        TextCache m_text;
        float m_fontSpacing = getGlobalFontSpacing();

        // Cached static labels bitmap (section titles and common labels)
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_staticTextBitmap;
        void buildStaticTextBitmap()
        {
            if (!m_renderTarget) return;
            Microsoft::WRL::ComPtr<ID2D1BitmapRenderTarget> rt;
            if (FAILED(m_renderTarget->CreateCompatibleRenderTarget(&rt))) return;
            rt->BeginDraw();
            rt->Clear(float4(0,0,0,0));

            const float titlePadding = std::max(1.5f, 20.0f * m_scaleFactor);
            const float titleMargin  = std::max(1.5f, 20.0f * m_scaleFactor);

            m_brush->SetColor(float4(1,1,1,1));
            // Titles
            m_text.render( rt.Get(), L"TRACK TEMP", m_textFormatBold.Get(), m_trackTempBox.x0 + titlePadding, m_trackTempBox.x1 - titleMargin, m_trackTempBox.y0 + titlePadding, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing );
            m_text.render( rt.Get(), L"TRACK WETNESS", m_textFormatBold.Get(), m_trackWetnessBox.x0 + titlePadding, m_trackWetnessBox.x1 - titleMargin, m_trackWetnessBox.y0 + titlePadding, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing );

            // Precipitation/Air Temp: draw the appropriate title based on current conditions
            const bool showPrecip = shouldShowPrecipitation();
            if (showPrecip) {
                m_text.render( rt.Get(), L"PRECIPITATION", m_textFormatBold.Get(), m_precipitationBox.x0 + titlePadding, m_precipitationBox.x1 - titleMargin, m_precipitationBox.y0 + titlePadding, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing );
            } else {
                m_text.render( rt.Get(), L"AIR TEMP", m_textFormatBold.Get(), m_precipitationBox.x0 + titlePadding, m_precipitationBox.x1 - titleMargin, m_precipitationBox.y0 + titlePadding, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing );
            }

            m_text.render( rt.Get(), L"WIND", m_textFormatBold.Get(), m_windBox.x0 + titlePadding, m_windBox.x1 - titleMargin, m_windBox.y0 + titlePadding, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing );

            rt->EndDraw();
            rt->GetBitmap(&m_staticTextBitmap);
        }

        // Styling brushes (cached; recreated on config change / enable)
        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_bgBrush;
        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_panelBrush;
};