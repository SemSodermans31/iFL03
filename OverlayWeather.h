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

#include <vector>
#include <algorithm>
#include <deque>
#include <string>
#include <memory>
#include <wincodec.h>
#include "Overlay.h"
#include "iracing.h"
#include "Config.h"
#include "OverlayDebug.h"
#include "stub_data.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class OverlayWeather : public Overlay
{
    public:

        const float DefaultFontSize = 16;

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
            // Make the default size taller to accommodate bigger boxes
            return float2(320, 800);
        }

        virtual void onEnable()
        {
            // Initialize COM and WIC factory (COM may already be initialized; that's fine)
            (void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            HRCHECK(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wicFactory)));
            
            onConfigChanged();
            loadIcons();
        }

        virtual void onDisable()
        {
            m_text.reset();
            releaseIcons();
            m_wicFactory.Reset();
        }

        virtual void onConfigChanged()
        {
            // Font setup
            {
                m_text.reset( m_dwriteFactory.Get() );

                const std::string font = g_cfg.getString( m_name, "font", "Arial" );
                const float fontSize = g_cfg.getFloat( m_name, "font_size", DefaultFontSize );
                
                HRCHECK(m_dwriteFactory->CreateTextFormat( toWide(font).c_str(), NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"en-us", &m_textFormat ));
                m_textFormat->SetParagraphAlignment( DWRITE_PARAGRAPH_ALIGNMENT_CENTER );
                m_textFormat->SetWordWrapping( DWRITE_WORD_WRAPPING_NO_WRAP );

                HRCHECK(m_dwriteFactory->CreateTextFormat( toWide(font).c_str(), NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"en-us", &m_textFormatBold ));
                m_textFormatBold->SetParagraphAlignment( DWRITE_PARAGRAPH_ALIGNMENT_CENTER );
                m_textFormatBold->SetWordWrapping( DWRITE_WORD_WRAPPING_NO_WRAP );

                HRCHECK(m_dwriteFactory->CreateTextFormat( toWide(font).c_str(), NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize*0.8f, L"en-us", &m_textFormatSmall ));
                m_textFormatSmall->SetParagraphAlignment( DWRITE_PARAGRAPH_ALIGNMENT_CENTER );
                m_textFormatSmall->SetWordWrapping( DWRITE_WORD_WRAPPING_NO_WRAP );

                HRCHECK(m_dwriteFactory->CreateTextFormat( toWide(font).c_str(), NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize*1.5f, L"en-us", &m_textFormatLarge ));
                m_textFormatLarge->SetParagraphAlignment( DWRITE_PARAGRAPH_ALIGNMENT_CENTER );
                m_textFormatLarge->SetWordWrapping( DWRITE_WORD_WRAPPING_NO_WRAP );
            }

            setupWeatherBoxes();
            
            // Load icons after render target is set up
            if (m_wicFactory.Get()) {
                loadIcons();
            }
        }

        virtual void onUpdate()
        {
            const float  fontSize           = g_cfg.getFloat( m_name, "font_size", DefaultFontSize );
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
            const bool imperial = useStubData ? false : (ir_DisplayUnits.getInt() == 0);

            wchar_t s[512];

            m_renderTarget->BeginDraw();
            m_renderTarget->Clear( float4(0,0,0,0) );

            // Helper function to draw section background
            auto drawSectionBackground = [&](const WeatherBox& box) {
                float4 bgColor = backgroundCol;
                bgColor.w *= globalOpacity;
                m_brush->SetColor( bgColor );
                D2D1_RECT_F bgRect = { box.x0, box.y0, box.x1, box.y1};
                D2D1_ROUNDED_RECT roundedBg = { bgRect, 8, 8 };
                m_renderTarget->FillRoundedRectangle( &roundedBg, m_brush.Get() );
            };

            // Track Temperature Section
            {
                drawSectionBackground(m_trackTempBox);
                
                float trackTemp = useStubData ? StubDataManager::getStubTrackTemp() : ir_TrackTempCrew.getFloat();
                
                if( imperial )
                    trackTemp = celsiusToFahrenheit( trackTemp );

                // Title - left aligned, larger and bolder with more room from edge
                m_brush->SetColor( finalTextCol );
                m_text.render( m_renderTarget.Get(), L"TRACK TEMP", m_textFormatBold.Get(), 
                              m_trackTempBox.x0 + 15, m_trackTempBox.x1 - 10, m_trackTempBox.y0 + 15, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING );

                // Temperature value - larger, centered
                m_brush->SetColor( trackTempCol );
                const wchar_t degree = L'\x00B0';
                swprintf( s, _countof(s), L"%.1f%lc%c", trackTemp, degree, imperial ? 'F' : 'C' );
                const float tempValueY = m_trackTempBox.y0 + m_trackTempBox.h * 0.65f;
                m_text.render( m_renderTarget.Get(), s, m_textFormatLarge.Get(), 
                              m_trackTempBox.x0 + 10, m_trackTempBox.x1 - 10, tempValueY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING );

                // Icon horizontally aligned with temperature value
                const float iconX = m_trackTempBox.x0 + m_trackTempBox.w/2 - 40; // Left of center
                drawIcon(m_trackTempIcon.Get(), iconX, tempValueY - 12, 24, 24, true);
            }

            // Track Wetness Section
            {
                drawSectionBackground(m_trackWetnessBox);
                
                float trackWetness = useStubData ? StubDataManager::getStubTrackWetness() : getTrackWetnessValue();
                std::string wetnessText = getTrackWetnessText(trackWetness);

                // Wetness title - left aligned
                m_brush->SetColor( finalTextCol );
                m_text.render( m_renderTarget.Get(), L"TRACK WETNESS", m_textFormatBold.Get(), 
                              m_trackWetnessBox.x0 + 15, m_trackWetnessBox.x1 - 10, m_trackWetnessBox.y0 + 15, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING );

                // Progress bar showing wetness level
                {
                    const float barWidth = m_trackWetnessBox.w * 0.6f;
                    const float barHeight = 8;
                    const float barX = m_trackWetnessBox.x0 + (m_trackWetnessBox.w - barWidth) / 2;
                    const float barY = m_trackWetnessBox.y0 + m_trackWetnessBox.h * 0.6f;
                    
                    // Sun icon on left (dry side)
                    drawIcon(m_sunIcon.Get(), barX - 30, barY - 5, 20, 20, true);
                    
                    // Waterdrop icon on right (wet side) - aligned with bar
                    drawIcon(m_trackWetnessIcon.Get(), barX + barWidth + 10, barY - 5, 20, 20, true);
                    
                    // Background bar with white outline
                    D2D1_RECT_F barBg = { barX, barY, barX + barWidth, barY + barHeight };
                    m_brush->SetColor( float4(0.3f, 0.3f, 0.3f, 0.8f) );
                    D2D1_ROUNDED_RECT rrBg = { barBg, 4, 4 };
                    m_renderTarget->FillRoundedRectangle( &rrBg, m_brush.Get() );
                    
                    // White outline around bar
                    m_brush->SetColor( float4(1.0f, 1.0f, 1.0f, 0.6f) );
                    m_renderTarget->DrawRoundedRectangle( &rrBg, m_brush.Get(), 1.0f );
                    
                    // Wetness fill
                    if (trackWetness > 0) {
                        D2D1_RECT_F bar = { barX, barY, barX + (barWidth * trackWetness), barY + barHeight };
                        m_brush->SetColor( accentCol );
                        D2D1_ROUNDED_RECT rr = { bar, 4, 4 };
                        m_renderTarget->FillRoundedRectangle( &rr, m_brush.Get() );
                    }
                }

                // Wetness text description
                m_brush->SetColor( finalTextCol );
                m_text.render( m_renderTarget.Get(), toWide(wetnessText).c_str(), m_textFormatBold.Get(), 
                              m_trackWetnessBox.x0 + 10, m_trackWetnessBox.x1 - 10, m_trackWetnessBox.y0 + m_trackWetnessBox.h * 0.8f, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER );
            }

            // Precipitation Section
            {
                drawSectionBackground(m_precipitationBox);
                
                float precipitation = useStubData ? StubDataManager::getStubPrecipitation() : getPrecipitationValue();

                // Title - left aligned, larger and bolder with more room from edge
                m_brush->SetColor( finalTextCol );
                m_text.render( m_renderTarget.Get(), L"PRECIPITATION", m_textFormatBold.Get(), 
                              m_precipitationBox.x0 + 15, m_precipitationBox.x1 - 10, m_precipitationBox.y0 + 15, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING );

                // Percentage value - larger, centered
                m_brush->SetColor( precipCol );
                swprintf( s, _countof(s), L"%.0f%%", precipitation * 100.0f );
                const float precipValueY = m_precipitationBox.y0 + m_precipitationBox.h * 0.65f;
                m_text.render( m_renderTarget.Get(), s, m_textFormatLarge.Get(), 
                              m_precipitationBox.x0 + 10, m_precipitationBox.x1 - 10, precipValueY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING );

                // Icon horizontally aligned with precipitation value
                const float precipIconX = m_precipitationBox.x0 + m_precipitationBox.w/2 - 50; // Left of center
                drawIcon(m_precipitationIcon.Get(), precipIconX, precipValueY - 12, 24, 24, true);
            }

            // Wind Section (Compass)
            {
                drawSectionBackground(m_windBox);
                
                float windSpeed = useStubData ? StubDataManager::getStubWindSpeed() : ir_WindVel.getFloat();
                float windDir = useStubData ? StubDataManager::getStubWindDirection() : ir_WindDir.getFloat();

                // Title - left aligned, larger and bolder with more room from edge
                m_brush->SetColor( finalTextCol );
                m_text.render( m_renderTarget.Get(), L"WIND", m_textFormatBold.Get(), 
                              m_windBox.x0 + 15, m_windBox.x1 - 10, m_windBox.y0 + 15, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING );

                // Draw compass - move further down from the top edge
                const float compassCenterX = m_windBox.x0 + m_windBox.w/2;
                const float compassCenterY = m_windBox.y0 + 110; // Increased from 75 to 110 for more vertical padding
                drawWindCompass(windDir, compassCenterX, compassCenterY, 70); // Larger radius for bigger box

                // Wind speed at bottom with icon - left aligned like title
                const float windSpeedY = m_windBox.y0 + m_windBox.h - 35;
                
                // Convert wind speed for display
                if (imperial) {
                    windSpeed *= 2.237f; // m/s to mph
                    swprintf( s, _countof(s), L"%.0f MPH", windSpeed );
                } else {
                    windSpeed *= 3.6f; // m/s to km/h
                    swprintf( s, _countof(s), L"%.0f KPH", windSpeed );
                }
                
                // Wind icon aligned to the left like title
                drawIcon(m_windIcon.Get(), m_windBox.x0 + 15, windSpeedY - 10, 20, 20, true);
                
                // Wind speed text left-aligned like title
                m_brush->SetColor( windCol );
                m_text.render( m_renderTarget.Get(), s, m_textFormatBold.Get(), 
                              m_windBox.x0 + 45, m_windBox.x1 - 10, windSpeedY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING );
            }

            m_renderTarget->EndDraw();
        }

    private:

        void setupWeatherBoxes()
        {
            const float padding = 15;
            const float spacing = 10;
            // Make the boxes vertically bigger by increasing their heights
            const float trackTempHeight = 110;
            const float trackWetnessHeight = 110;
            const float precipitationHeight = 110;
            const float windHeight = 250;
            
            float yPos = padding;
            
            m_trackTempBox = makeWeatherBox(padding, m_width - 2*padding, yPos, "Track Temperature", "assets/icons/track_temp.png");
            m_trackTempBox.h = trackTempHeight;
            m_trackTempBox.y1 = m_trackTempBox.y0 + trackTempHeight;
            
            yPos += trackTempHeight + spacing;
            m_trackWetnessBox = makeWeatherBox(padding, m_width - 2*padding, yPos, "Track Wetness", "assets/icons/waterdrop.png");
            m_trackWetnessBox.h = trackWetnessHeight;
            m_trackWetnessBox.y1 = m_trackWetnessBox.y0 + trackWetnessHeight;
            
            yPos += trackWetnessHeight + spacing;
            m_precipitationBox = makeWeatherBox(padding, m_width - 2*padding, yPos, "Precipitation", "assets/icons/precipitation.png");
            m_precipitationBox.h = precipitationHeight;
            m_precipitationBox.y1 = m_precipitationBox.y0 + precipitationHeight;
            
            yPos += precipitationHeight + spacing;
            m_windBox = makeWeatherBox(padding, m_width - 2*padding, yPos, "Wind", "assets/icons/wind.png");
            m_windBox.h = windHeight;
            m_windBox.y1 = m_windBox.y0 + windHeight;
        }

        WeatherBox makeWeatherBox(float x, float w, float y, const std::string& title, const std::string& iconPath)
        {
            WeatherBox box;
            box.x0 = x;
            box.x1 = x + w;
            box.y0 = y;
            // Make the wind box even taller, others also bigger
            if (title == "Wind") {
                box.y1 = y + 250;
            } else {
                box.y1 = y + 110;
            }
            box.w = w;
            box.h = box.y1 - box.y0;
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
                    if (FAILED(hr)) return bitmap; // Return null bitmap on failure

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
                        // Center adjusted size inside given box
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


        void drawWindCompass(float windDirection, float centerX, float centerY, float radius)
        {
            const float carSize = 50.0f; // Make car icon bigger for larger box

            // Draw compass circle
            D2D1_ELLIPSE compassCircle = { {centerX, centerY}, radius, radius };
            m_brush->SetColor(float4(0.3f, 0.3f, 0.3f, 0.8f));
            m_renderTarget->DrawEllipse(&compassCircle, m_brush.Get(), 2.0f);

            // Draw cardinal directions (NESW) inside compass - fixed positions, centered and larger font
            m_brush->SetColor(float4(0.8f, 0.8f, 0.8f, 0.9f));
            const wchar_t* directions[] = { L"N", L"E", L"S", L"W" };
            const float textRadius = radius * 0.8f;

            // Create a larger font for the compass labels (m_textFormatSmall + 4)
            Microsoft::WRL::ComPtr<IDWriteTextFormat> compassLabelFormat;
            if (m_textFormatSmall) {
                FLOAT fontSize = 0.0f;
                fontSize = m_textFormatSmall->GetFontSize();
                fontSize += 4.0f;
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
                float angle = (float)(i * M_PI / 2); // Fixed cardinal positions
                float textX = centerX + textRadius * (float)sin(angle);
                float textY = centerY - textRadius * (float)cos(angle);

                // Center the label horizontally and vertically
                float labelBoxW = 32.0f;
                float labelBoxH = 28.0f;
                float boxLeft = textX - labelBoxW * 0.5f;
                float boxRight = textX + labelBoxW * 0.5f;
                float boxTop = textY - labelBoxH * 0.5f;

                float verticalNudge = 14.0f; // Adjust this value as needed for perfect centering
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

            // Draw wind arrow pointing INWARD toward the car (fixed direction)
            if (m_windArrowIcon.Get()) {
                // Arrow starts fully at the edge of the compass, but ends partway toward the center (shorter arrow)
                const float arrowStartRadius = radius;
                const float arrowEndRadius = radius * 0.25f; // Shorter, but still starts at edge

                const float startX = centerX + arrowStartRadius * (float)sin(windDirection);
                const float startY = centerY - arrowStartRadius * (float)cos(windDirection);
                const float endX = centerX + arrowEndRadius * (float)sin(windDirection);
                const float endY = centerY - arrowEndRadius * (float)cos(windDirection);

                const float arrowWidth = 36.0f;
                const float arrowLength = (float)hypot(endX - startX, endY - startY);

                const float midX = (startX + endX) * 0.5f;
                const float midY = (startY + endY) * 0.5f;

                D2D1_MATRIX_3X2_F oldTx;
                m_renderTarget->GetTransform(&oldTx);
                const float angleDeg = windDirection * 180.0f / (float)M_PI + 180.0f;
                m_renderTarget->SetTransform(oldTx * D2D1::Matrix3x2F::Rotation(angleDeg, D2D1::Point2F(midX, midY)));

                // Draw with opacity 0.75
                m_renderTarget->DrawBitmap(
                    m_windArrowIcon.Get(),
                    D2D1::RectF(midX - arrowWidth*0.5f, midY - arrowLength*0.5f, midX + arrowWidth*0.5f, midY + arrowLength*0.5f),
                    0.75f // Opacity
                );

                m_renderTarget->SetTransform(oldTx);
            }
            else {
                // Fallback to vector arrow pointing INWARD toward car
                const float arrowStartRadius = radius * 0.85f;
                const float arrowEndRadius = radius * 0.45f;

                const float startX = centerX + arrowStartRadius * (float)sin(windDirection);
                const float startY = centerY - arrowStartRadius * (float)cos(windDirection);
                const float endX = centerX + arrowEndRadius * (float)sin(windDirection);
                const float endY = centerY - arrowEndRadius * (float)cos(windDirection);

                m_brush->SetColor(float4(0.2f, 0.8f, 1.0f, 0.9f));
                m_renderTarget->DrawLine({startX, startY}, {endX, endY}, m_brush.Get(), 4.0f);

                // Draw arrowhead pointing INWARD toward center
                const float arrowHeadLength = 12;
                const float arrowHeadAngle = (float)(M_PI / 6); // 30 degrees

                // Arrowhead points toward center (inward direction)
                float headX1 = endX - arrowHeadLength * (float)sin(windDirection + arrowHeadAngle);
                float headY1 = endY + arrowHeadLength * (float)cos(windDirection + arrowHeadAngle);
                float headX2 = endX - arrowHeadLength * (float)sin(windDirection - arrowHeadAngle);
                float headY2 = endY + arrowHeadLength * (float)cos(windDirection - arrowHeadAngle);

                m_renderTarget->DrawLine({endX, endY}, {headX1, headY1}, m_brush.Get(), 4.0f);
                m_renderTarget->DrawLine({endX, endY}, {headX2, headY2}, m_brush.Get(), 4.0f);
            }
        }

        float getTrackWetnessValue()
        {
            // TODO: Replace with actual iRacing track wetness telemetry when available
            // For now, return a placeholder value
            return 0.0f;
        }

        std::string getTrackWetnessText(float wetness)
        {
            if (wetness <= 0.1f) return "Dry";
            if (wetness <= 0.3f) return "Slightly Wet";
            if (wetness <= 0.5f) return "Lightly Wet";
            if (wetness <= 0.7f) return "Wet";
            if (wetness <= 0.9f) return "Very Wet";
            return "Extremely Wet";
        }

        float getPrecipitationValue()
        {
            // TODO: Replace with actual iRacing precipitation telemetry when available
            // For now, return a placeholder value
            return 0.0f;
        }

        virtual bool hasCustomBackground()
        {
            return true;
        }

    protected:

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
};
