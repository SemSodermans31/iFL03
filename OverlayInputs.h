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
#include "Config.h"
#include "OverlayDebug.h"
#include "stub_data.h"
#include <wincodec.h>

class OverlayInputs : public Overlay
{
    public:

        OverlayInputs()
            : Overlay("OverlayInputs")
        {}

        virtual void onEnable()
        {
            onConfigChanged();
        }

    protected:

        virtual bool hasCustomBackground()
        {
            return true;
        }

        virtual float2 getDefaultSize()
        {
            return float2(600,200);
        }

        virtual void onConfigChanged()
        {
            m_showSteeringWheel = g_cfg.getBool( m_name, "show_steering_wheel", true );

            const float wheelFrac = m_showSteeringWheel ? 0.2f : 0.0f;
            const float barFrac = m_showSteeringWheel ? 0.16f : 0.26f;
            const float graphFrac = std::max(0.1f, 1.0f - wheelFrac - barFrac);

            const int horizontalWidthInt = std::max(1, (int)(m_width * graphFrac));
            m_throttleVtx.resize( horizontalWidthInt );
            m_brakeVtx.resize( horizontalWidthInt );
            m_steeringVtx.resize( horizontalWidthInt );
            for( int i=0; i<horizontalWidthInt; ++i )
            {
                m_throttleVtx[i].x = float(i);
                m_brakeVtx[i].x = float(i);
                m_steeringVtx[i].x = float(i);
            }
            
            // Create text format for labels and values using centralized settings
            createGlobalTextFormat(1.0f, (int)DWRITE_FONT_WEIGHT_BOLD, "", m_textFormatBold);
            m_textFormatBold->SetParagraphAlignment( DWRITE_PARAGRAPH_ALIGNMENT_CENTER );
            m_textFormatBold->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_CENTER );
            m_textFormatBold->SetWordWrapping( DWRITE_WORD_WRAPPING_NO_WRAP );
            
            createGlobalTextFormat(0.8f, (int)DWRITE_FONT_WEIGHT_BOLD, "", m_textFormatPercent);
            m_textFormatPercent->SetParagraphAlignment( DWRITE_PARAGRAPH_ALIGNMENT_CENTER );
            m_textFormatPercent->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_CENTER );
            m_textFormatPercent->SetWordWrapping( DWRITE_WORD_WRAPPING_NO_WRAP );

            // Load selected steering wheel image if any
            if( m_showSteeringWheel )
                loadSteeringWheelBitmap();
            else
                m_wheelBitmap.Reset();

            // Per-overlay FPS (configurable; default 60)
            setTargetFPS(g_cfg.getInt(m_name, "target_fps", 60));
        }

        virtual void onUpdate()
        {
            const float w = (float)m_width;
            const float h = (float)m_height;

            // Layout sections
            const bool showWheel = m_showSteeringWheel;
            const float wheelFrac = showWheel ? 0.2f : 0.0f;
            const float barFrac = showWheel ? 0.15f : 0.3f;
            const float graphFrac = 1.0f - wheelFrac - barFrac;

            const float horizontalWidth = w * graphFrac;
            const float barsWidth = w * barFrac;
            const float wheelWidth = w * wheelFrac;

            const bool leftSide = g_cfg.getBool( m_name, "left_side", false );

            const float horizontalPadding = 8.0f; // Padding from overlay edge for horizontal graphs
            const float sectionPadding = 8.0f; // Padding between sections
            const float horizontalStartX = leftSide ? (wheelWidth + barsWidth) : horizontalPadding;
            const float barsStartX = leftSide ? wheelWidth : horizontalWidth;
            const float wheelStartX = leftSide ? 0.0f : (horizontalWidth + barsWidth);

            // Adjust horizontal end to leave room for bars when not in left-side mode
            const float horizontalEndX = leftSide ? (horizontalStartX + horizontalWidth) :
                std::min(horizontalStartX + horizontalWidth, barsStartX - sectionPadding);

            // Calculate effective width for vertex arrays and scaling
            const float effectiveHorizontalWidth = horizontalEndX - horizontalStartX;
            
            if( m_throttleVtx.empty() )
                m_throttleVtx.resize( 1 );
            if( m_brakeVtx.empty() )
                m_brakeVtx.resize( 1 );
            if( m_steeringVtx.empty() )
                m_steeringVtx.resize( 1 );

            // Get current input values (use stub data in preview mode)
            const bool useStubData = StubDataManager::shouldUseStubData();
            const float currentThrottle = useStubData ? StubDataManager::getStubThrottle() : ir_Throttle.getFloat();
            const float currentBrake = useStubData ? StubDataManager::getStubBrake() : ir_Brake.getFloat();
            const float currentSteeringAngle = useStubData ?
                (StubDataManager::getStubSteering() - 0.5f) * 2.0f * 3.14159f * 0.25f :
                ir_SteeringWheelAngle.getFloat();

            // Advance input vertices for horizontal graphs
            {
                for( int i=0; i<(int)m_throttleVtx.size()-1; ++i )
                    m_throttleVtx[i].y = m_throttleVtx[i+1].y;
                m_throttleVtx[(int)m_throttleVtx.size()-1].y = currentThrottle;

                for( int i=0; i<(int)m_brakeVtx.size()-1; ++i )
                    m_brakeVtx[i].y = m_brakeVtx[i+1].y;
                m_brakeVtx[(int)m_brakeVtx.size()-1].y = currentBrake;

                float s = currentSteeringAngle / (3.14159f * 0.5f); 
                if( s < -1.0f ) s = -1.0f;
                if( s > 1.0f ) s = 1.0f;
                const float steeringNorm = 0.5f - s * 0.5f;
                for( int i=0; i<(int)m_steeringVtx.size()-1; ++i )
                    m_steeringVtx[i].y = m_steeringVtx[i+1].y;
                m_steeringVtx[(int)m_steeringVtx.size()-1].y = steeringNorm;
            }

            const float thickness = g_cfg.getFloat( m_name, "line_thickness", 2.0f );
            
            // Transform function for horizontal graphs
            auto vtx2coord = [&]( const float2& v )->float2 {
                float scaledX = (v.x / (float)m_throttleVtx.size()) * effectiveHorizontalWidth;
                return float2( horizontalStartX + scaledX + 0.5f, h - 0.5f*thickness - v.y*(h*0.8f-thickness) - h*0.1f );
            };

            m_renderTarget->BeginDraw();
            // Clear and draw custom full background with larger circular right corners
            m_renderTarget->Clear( float4(0,0,0,0) );
            {
                const float cornerRadius = g_cfg.getFloat( m_name, "corner_radius", 2.0f );
                float4 bgColor = g_cfg.getFloat4( m_name, "background_col", float4(0.1215686f, 0.1215686f, 0.1215686f, 0.5f) );
                bgColor.w *= getGlobalOpacity();

                const float left   = 0.5f;
                const float top    = 0.5f;
                const float right  = w - 0.5f;
                const float bottom = h - 0.5f;

                if( !showWheel )
                {
                    m_brush->SetColor( bgColor );
                    D2D1_ROUNDED_RECT rr = { D2D1::RectF(left, top, right, bottom), cornerRadius, cornerRadius };
                    m_renderTarget->FillRoundedRectangle( rr, m_brush.Get() );

                    const float4 borderColor = float4(0.3f, 0.3f, 0.3f, 0.6f);
                    m_brush->SetColor(borderColor);
                    m_renderTarget->DrawRoundedRectangle(rr, m_brush.Get(), 2.0f);
                }
                else
                {
                    const float arcRadius = h * 0.5f;

                    Microsoft::WRL::ComPtr<ID2D1PathGeometry> geom;
                    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
                    if (SUCCEEDED(m_d2dFactory->CreatePathGeometry(&geom)) && SUCCEEDED(geom->Open(&sink)))
                    {
                        if( !leftSide )
                        {
                            // Rounded on right side (default)
                            sink->BeginFigure( float2(left + cornerRadius, top), D2D1_FIGURE_BEGIN_FILLED );
                            sink->AddLine( float2(right - arcRadius, top) );
                            {
                                D2D1_ARC_SEGMENT arc = {};
                                arc.point = float2(right, top + arcRadius);
                                arc.size = D2D1::SizeF(arcRadius, arcRadius);
                                arc.rotationAngle = 0.0f;
                                arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
                                arc.arcSize = D2D1_ARC_SIZE_SMALL;
                                sink->AddArc(arc);
                            }
                            sink->AddLine( float2(right, bottom - arcRadius) );
                            {
                                D2D1_ARC_SEGMENT arc = {};
                                arc.point = float2(right - arcRadius, bottom);
                                arc.size = D2D1::SizeF(arcRadius, arcRadius);
                                arc.rotationAngle = 0.0f;
                                arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
                                arc.arcSize = D2D1_ARC_SIZE_SMALL;
                                sink->AddArc(arc);
                            }
                            sink->AddLine( float2(left + cornerRadius, bottom) );
                            {
                                D2D1_ARC_SEGMENT arc = {};
                                arc.point = float2(left, bottom - cornerRadius);
                                arc.size = D2D1::SizeF(cornerRadius, cornerRadius);
                                arc.rotationAngle = 0.0f;
                                arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
                                arc.arcSize = D2D1_ARC_SIZE_SMALL;
                                sink->AddArc(arc);
                            }
                            sink->AddLine( float2(left, top + cornerRadius) );
                            {
                                D2D1_ARC_SEGMENT arc = {};
                                arc.point = float2(left + cornerRadius, top);
                                arc.size = D2D1::SizeF(cornerRadius, cornerRadius);
                                arc.rotationAngle = 0.0f;
                                arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
                                arc.arcSize = D2D1_ARC_SIZE_SMALL;
                                sink->AddArc(arc);
                            }
                        }
                        else
                        {
                            // Rounded on left side (mirrored)
                            sink->BeginFigure( float2(left + arcRadius, top), D2D1_FIGURE_BEGIN_FILLED );
                            sink->AddLine( float2(right - cornerRadius, top) );
                            {
                                D2D1_ARC_SEGMENT arc = {};
                                arc.point = float2(right, top + cornerRadius);
                                arc.size = D2D1::SizeF(cornerRadius, cornerRadius);
                                arc.rotationAngle = 0.0f;
                                arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
                                arc.arcSize = D2D1_ARC_SIZE_SMALL;
                                sink->AddArc(arc);
                            }
                            sink->AddLine( float2(right, bottom - cornerRadius) );
                            {
                                D2D1_ARC_SEGMENT arc = {};
                                arc.point = float2(right - cornerRadius, bottom);
                                arc.size = D2D1::SizeF(cornerRadius, cornerRadius);
                                arc.rotationAngle = 0.0f;
                                arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
                                arc.arcSize = D2D1_ARC_SIZE_SMALL;
                                sink->AddArc(arc);
                            }
                            sink->AddLine( float2(left + arcRadius, bottom) );
                            {
                                D2D1_ARC_SEGMENT arc = {};
                                arc.point = float2(left, bottom - arcRadius);
                                arc.size = D2D1::SizeF(arcRadius, arcRadius);
                                arc.rotationAngle = 0.0f;
                                arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
                                arc.arcSize = D2D1_ARC_SIZE_SMALL;
                                sink->AddArc(arc);
                            }
                            sink->AddLine( float2(left, top + arcRadius) );
                            {
                                D2D1_ARC_SEGMENT arc = {};
                                arc.point = float2(left + arcRadius, top);
                                arc.size = D2D1::SizeF(arcRadius, arcRadius);
                                arc.rotationAngle = 0.0f;
                                arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
                                arc.arcSize = D2D1_ARC_SIZE_SMALL;
                                sink->AddArc(arc);
                            }
                        }

                        sink->EndFigure( D2D1_FIGURE_END_CLOSED );
                        if (SUCCEEDED(sink->Close()))
                        {
                            m_brush->SetColor( bgColor );
                            m_renderTarget->FillGeometry( geom.Get(), m_brush.Get() );

                            const float4 borderColor = float4(0.3f, 0.3f, 0.3f, 0.6f);
                            m_brush->SetColor(borderColor);
                            m_renderTarget->DrawGeometry(geom.Get(), m_brush.Get(), 1.0f);
                        }
                    }
                }
            }

            // SECTION 1: Horizontal Throttle/Brake Graphs
            if( !m_throttleVtx.empty() && !m_brakeVtx.empty() )
            {
                {
                    const float graphTop = h * 0.1f;
                    const float graphBottom = h * 0.9f;
                    D2D1_RECT_F teleRect = { horizontalStartX, graphTop, horizontalEndX, graphBottom };

                    float4 teleBg = float4(0.1215686f, 0.1215686f, 0.1215686f, 0.5f);
                    teleBg.w *= getGlobalOpacity();
                    m_brush->SetColor( teleBg );
                    m_renderTarget->FillRectangle( teleRect, m_brush.Get() );

                    m_brush->SetColor( float4(0.0705882f, 0.0705882f, 0.0705882f, 1.0f) );
                    for( int i = 1; i <= 3; ++i )
                    {
                        float y = graphTop + (graphBottom - graphTop) * (float)i / 4.0f;
                        m_renderTarget->DrawLine( float2(horizontalStartX, y), float2(horizontalEndX, y), m_brush.Get(), 1.0f );
                    }
                    m_brush->SetColor( float4(0.0f, 0.0f, 0.0f, 1.0f) );
                    m_renderTarget->DrawRectangle( teleRect, m_brush.Get(), 1.0f );
                }

                // Throttle (fill)
                Microsoft::WRL::ComPtr<ID2D1PathGeometry1> throttleFillPath;
                Microsoft::WRL::ComPtr<ID2D1GeometrySink>  throttleFillSink;
                m_d2dFactory->CreatePathGeometry( &throttleFillPath );
                throttleFillPath->Open( &throttleFillSink );
                throttleFillSink->BeginFigure( float2(horizontalStartX, h*0.9f), D2D1_FIGURE_BEGIN_FILLED );
                for( int i=0; i<(int)m_throttleVtx.size(); ++i )
                    throttleFillSink->AddLine( vtx2coord(m_throttleVtx[i]) );
                throttleFillSink->AddLine( float2(horizontalEndX, h*0.9f) );
                throttleFillSink->EndFigure( D2D1_FIGURE_END_CLOSED );
                throttleFillSink->Close();

                // Brake (fill)
                Microsoft::WRL::ComPtr<ID2D1PathGeometry1> brakeFillPath;
                Microsoft::WRL::ComPtr<ID2D1GeometrySink>  brakeFillSink;
                m_d2dFactory->CreatePathGeometry( &brakeFillPath );
                brakeFillPath->Open( &brakeFillSink );
                brakeFillSink->BeginFigure( float2(horizontalStartX, h*0.9f), D2D1_FIGURE_BEGIN_FILLED );
                for( int i=0; i<(int)m_brakeVtx.size(); ++i )
                    brakeFillSink->AddLine( vtx2coord(m_brakeVtx[i]) );
                brakeFillSink->AddLine( float2(horizontalEndX, h*0.9f) );
                brakeFillSink->EndFigure( D2D1_FIGURE_END_CLOSED );
                brakeFillSink->Close();

                // Draw fills
                m_brush->SetColor( g_cfg.getFloat4( m_name, "throttle_fill_col", float4(0.2f,0.45f,0.15f,0.6f) ) );
                m_renderTarget->FillGeometry( throttleFillPath.Get(), m_brush.Get() );
                m_brush->SetColor( g_cfg.getFloat4( m_name, "brake_fill_col", float4(0.46f,0.01f,0.06f,0.6f) ) );
                m_renderTarget->FillGeometry( brakeFillPath.Get(), m_brush.Get() );

                // Throttle (line)
                Microsoft::WRL::ComPtr<ID2D1PathGeometry1> throttleLinePath;
                Microsoft::WRL::ComPtr<ID2D1GeometrySink>  throttleLineSink;
                m_d2dFactory->CreatePathGeometry( &throttleLinePath );
                throttleLinePath->Open( &throttleLineSink );
                throttleLineSink->BeginFigure( vtx2coord(m_throttleVtx[0]), D2D1_FIGURE_BEGIN_HOLLOW );
                for( int i=1; i<(int)m_throttleVtx.size(); ++i )
                    throttleLineSink->AddLine( vtx2coord(m_throttleVtx[i]) );
                throttleLineSink->EndFigure( D2D1_FIGURE_END_OPEN );
                throttleLineSink->Close();

                // Brake (line)
                Microsoft::WRL::ComPtr<ID2D1PathGeometry1> brakeLinePath;
                Microsoft::WRL::ComPtr<ID2D1GeometrySink>  brakeLineSink;
                m_d2dFactory->CreatePathGeometry( &brakeLinePath );
                brakeLinePath->Open( &brakeLineSink );
                brakeLineSink->BeginFigure( vtx2coord(m_brakeVtx[0]), D2D1_FIGURE_BEGIN_HOLLOW );
                for( int i=1; i<(int)m_brakeVtx.size(); ++i )
                    brakeLineSink->AddLine( vtx2coord(m_brakeVtx[i]) );
                brakeLineSink->EndFigure( D2D1_FIGURE_END_OPEN );
                brakeLineSink->Close();

                // Draw lines
                m_brush->SetColor( g_cfg.getFloat4( m_name, "throttle_col", float4(0.38f,0.91f,0.31f,0.8f) ) );
                m_renderTarget->DrawGeometry( throttleLinePath.Get(), m_brush.Get(), thickness );
                m_brush->SetColor( g_cfg.getFloat4( m_name, "brake_col", float4(0.93f,0.03f,0.13f,0.8f) ) );
                m_renderTarget->DrawGeometry( brakeLinePath.Get(), m_brush.Get(), thickness );

                // Optional steering angle line (white)
                if( g_cfg.getBool( m_name, "show_steering_line", false ) && !m_steeringVtx.empty() )
                {
                    Microsoft::WRL::ComPtr<ID2D1PathGeometry1> steerLinePath;
                    Microsoft::WRL::ComPtr<ID2D1GeometrySink>  steerLineSink;
                    m_d2dFactory->CreatePathGeometry( &steerLinePath );
                    steerLinePath->Open( &steerLineSink );
                    steerLineSink->BeginFigure( vtx2coord(m_steeringVtx[0]), D2D1_FIGURE_BEGIN_HOLLOW );
                    for( int i=1; i<(int)m_steeringVtx.size(); ++i )
                        steerLineSink->AddLine( vtx2coord(m_steeringVtx[i]) );
                    steerLineSink->EndFigure( D2D1_FIGURE_END_OPEN );
                    steerLineSink->Close();

                    m_brush->SetColor( g_cfg.getFloat4( m_name, "steering_line_col", float4(1.0f,1.0f,1.0f,0.9f) ) );
                    m_renderTarget->DrawGeometry( steerLinePath.Get(), m_brush.Get(), thickness );
                }
            }

            // SECTION 2: Vertical Percentage Bars
            const float barWidth = barsWidth / 3.0f;
            const float barHeight = h * 0.65f;
            const float barY = h * 0.25f;
            
            const float clutchValue = useStubData ? StubDataManager::getStubClutch() : (1.0f - ir_Clutch.getFloat());
            const float brakeValue = currentBrake;
            const float throttleValue = currentThrottle;
            
            // Draw vertical bars
            struct BarInfo {
                float value;
                float4 color;
                float x;
            };
            
            BarInfo bars[] = {
                { clutchValue, float4(0.0f, 0.5f, 1.0f, 0.8f), barsStartX + barWidth * 0.5f },
                { brakeValue, float4(0.93f, 0.03f, 0.13f, 0.8f), barsStartX + barWidth * 1.5f },
                { throttleValue, float4(0.38f, 0.91f, 0.31f, 0.8f), barsStartX + barWidth * 2.5f }
            };
            
            for( int i = 0; i < 3; ++i )
            {
                const BarInfo& bar = bars[i];
                
                // Draw bar background
                const float borderPx = 1.0f;
                m_brush->SetColor( float4(0.2f, 0.2f, 0.2f, 0.8f) );
                D2D1_RECT_F bgRect = { bar.x - barWidth*0.3f, barY, bar.x + barWidth*0.3f, barY + barHeight };
                m_renderTarget->FillRectangle( bgRect, m_brush.Get() );
                
                // Draw bar fill first (slightly inset), then border on top so fill never appears wider
                float fillHeight = barHeight * bar.value;
                D2D1_RECT_F innerRect = { bgRect.left + borderPx, bgRect.top + borderPx, bgRect.right - borderPx, bgRect.bottom - borderPx };
                D2D1_RECT_F fillRect = { innerRect.left, std::max(innerRect.top, innerRect.bottom - fillHeight), innerRect.right, innerRect.bottom };
                m_brush->SetColor( bar.color );
                m_renderTarget->FillRectangle( fillRect, m_brush.Get() );

                // Black border #000000 around the bar background (drawn last)
                m_brush->SetColor( float4(0.0f, 0.0f, 0.0f, 1.0f) );
                m_renderTarget->DrawRectangle( bgRect, m_brush.Get(), 1.0f );
                
                // Draw percentage text
                wchar_t percentText[32];
                int percentValue = (int)(bar.value * 100);
                percentValue = std::max(-999, std::min(999, percentValue)); // Clamp to reasonable range
                swprintf_s( percentText, L"%d", percentValue );
                m_brush->SetColor( float4(1.0f, 1.0f, 1.0f, 1.0f) );
                D2D1_RECT_F percentRect = { bar.x - barWidth*0.5f, barY - 20, bar.x + barWidth*0.5f, barY };
                m_renderTarget->DrawText( percentText, (UINT)wcslen(percentText), m_textFormatPercent.Get(), &percentRect, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );
            }

            if( showWheel )
            {
                // SECTION 3: Steering Wheel with Speed/Gear or Image
                const float wheelCenterX = wheelStartX + wheelWidth * 0.5f;
                const float wheelCenterY = h * 0.5f;
                const float wheelRadius = std::min(wheelWidth, h * 0.5f) * 0.9f;
                const float innerRadius = wheelRadius * 0.8f;

                const std::string wheelMode = g_cfg.getString(m_name, "steering_wheel", "builtin");
                const bool useImageWheel = (wheelMode != "builtin");

                const float steeringAngle = useStubData ?
                    (StubDataManager::getStubSteering() - 0.5f) * 2.0f * 3.14159f * 0.25f :
                    ir_SteeringWheelAngle.getFloat();

                const float columnWidth = wheelRadius * 0.15f;
                const float columnHeight = (wheelRadius - innerRadius) * 0.95f;

                const float4 ringColor      = g_cfg.getFloat4( m_name, "steering_ring_col",   float4(0.3f, 0.3f, 0.3f, 1.0f) );
                const float4 columnColor    = g_cfg.getFloat4( m_name, "steering_column_col", float4(0.93f, 0.03f, 0.13f, 1.0f) );
                const float4 telemetryColor = g_cfg.getFloat4( m_name, "steering_text_col",   float4(1.0f, 1.0f, 1.0f, 1.0f) );

                if (!useImageWheel) {
                    const float ringStroke = std::max(1.0f, wheelRadius - innerRadius);
                    const float ringRadius = innerRadius + ringStroke * 0.5f;
                    m_brush->SetColor( ringColor );
                    D2D1_ELLIPSE ring = { {wheelCenterX, wheelCenterY}, ringRadius, ringRadius };
                    m_renderTarget->DrawEllipse( ring, m_brush.Get(), ringStroke );
                }
                D2D1_RECT_F columnRect = {
                    wheelCenterX - columnWidth*0.7f,
                    wheelCenterY - wheelRadius,
                    wheelCenterX + columnWidth*0.7f,
                    wheelCenterY - wheelRadius + columnHeight
                };

                D2D1::Matrix3x2F rotation = D2D1::Matrix3x2F::Rotation(
                    -steeringAngle * (180.0f / 3.14159f),
                    D2D1::Point2F(wheelCenterX, wheelCenterY)
                );

                D2D1_MATRIX_3X2_F previousTransform;
                m_renderTarget->GetTransform(&previousTransform);
                m_renderTarget->SetTransform(rotation);
                if (useImageWheel && m_wheelBitmap) {
                    D2D1_SIZE_F bmpSize = m_wheelBitmap->GetSize();
                    float scale = 1.0f;
                    if (bmpSize.width > 0 && bmpSize.height > 0) {
                        const float maxDim = wheelRadius * 2.0f;
                        const float sx = maxDim / bmpSize.width;
                        const float sy = maxDim / bmpSize.height;
                        scale = std::min(sx, sy);
                    }
                    const float drawW = bmpSize.width * scale;
                    const float drawH = bmpSize.height * scale;
                    const float left = wheelCenterX - drawW * 0.5f;
                    const float top  = wheelCenterY - drawH * 0.5f;
                    D2D1_RECT_F dest = { left, top, left + drawW, top + drawH };
                    m_renderTarget->DrawBitmap(m_wheelBitmap.Get(), dest);
                } else {
                    m_brush->SetColor( columnColor );
                    m_renderTarget->FillRectangle( columnRect, m_brush.Get() );
                }
                m_renderTarget->SetTransform(previousTransform);

                const float speed = useStubData ?
                    StubDataManager::getStubSpeed() :
                    ir_Speed.getFloat() * 3.6f;
                const int gear = useStubData ?
                    StubDataManager::getStubGear() :
                    ir_Gear.getInt();

                // Optional steering angle in degrees for builtin wheel (center 0°, left negative, right positive)
                const float degrees = -steeringAngle * (180.0f / 3.14159f);
                const float clampedDegrees = std::max(-999.0f, std::min(999.0f, degrees));

                if (!useImageWheel) {
                    m_brush->SetColor( telemetryColor );

                    // Speed text 
                    wchar_t speedText[32];
                    float clampedSpeed = std::max(-999.0f, std::min(999.0f, speed));
                    swprintf_s( speedText, L"%.0f", clampedSpeed );
                    D2D1_RECT_F speedRect = { wheelCenterX - wheelRadius*0.5f, wheelCenterY - 25, wheelCenterX + wheelRadius*0.5f, wheelCenterY - 10 };
                    m_renderTarget->DrawText( speedText, (UINT)wcslen(speedText), m_textFormatBold.Get(), &speedRect, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );

                    // Gear text
                    wchar_t gearText[16];
                    if( gear == -1 )
                        wcscpy_s( gearText, L"R" );
                    else if( gear == 0 )
                        wcscpy_s( gearText, L"N" );
                    else
                    {
                        int clampedGear = std::max(-99, std::min(99, gear));
                        swprintf_s( gearText, L"%d", clampedGear );
                    }
                    D2D1_RECT_F gearRect = { wheelCenterX - wheelRadius*0.5f, wheelCenterY - 12, wheelCenterX + wheelRadius*0.5f, wheelCenterY + 10 };
                    m_renderTarget->DrawText( gearText, (UINT)wcslen(gearText), m_textFormatBold.Get(), &gearRect, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );
                    
                    // Steering degrees text
                    if (g_cfg.getBool(m_name, "show_steering_degrees", true)) {
                        wchar_t degText[32];
                        swprintf_s(degText, L"%.0f\u00B0", clampedDegrees);
                        D2D1_RECT_F degRect = { wheelCenterX - wheelRadius*0.5f, wheelCenterY + 15, wheelCenterX + wheelRadius*0.5f, wheelCenterY + 25 };
                        m_renderTarget->DrawText( degText, (UINT)wcslen(degText), m_textFormatPercent.Get(), &degRect, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP );
                    }
                }
            }

            m_renderTarget->EndDraw();
        }

    protected:

        std::vector<float2> m_throttleVtx;
        std::vector<float2> m_brakeVtx;
        std::vector<float2> m_steeringVtx;
        Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormatBold;
        Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormatPercent;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_wheelBitmap;
        bool m_showSteeringWheel = true;

        void loadSteeringWheelBitmap()
        {
            m_wheelBitmap.Reset();
            const std::string mode = g_cfg.getString(m_name, "steering_wheel", "builtin");
            if (mode == "builtin") return;
            std::string fileName;
            if (mode == "moza_ks") fileName = "assets/wheels/moza_ks.png";
            if (mode == "moza_rs_v2") fileName = "assets/wheels/moza_rs_v2.png";
            if (fileName.empty()) return;

            auto fileExistsW = [](const std::wstring& path) -> bool {
                DWORD a = GetFileAttributesW(path.c_str());
                return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
            };
            auto getExecutableDirW = []() -> std::wstring {
                wchar_t path[MAX_PATH] = {0};
                GetModuleFileNameW(NULL, path, MAX_PATH);
                wchar_t* last = wcsrchr(path, L'\\');
                if (last) *last = 0;
                return std::wstring(path);
            };
            auto resolveAssetPath = [&](const std::wstring& relative) -> std::wstring {
                const std::wstring exeDir = getExecutableDirW();
                std::wstring repo = exeDir + L"\\..\\..\\" + relative;
                if (fileExistsW(repo)) return repo;
                std::wstring local = exeDir + L"\\" + relative;
                if (fileExistsW(local)) return local;
                return relative;
            };

            if (!m_renderTarget.Get()) return;

            std::wstring pathW = resolveAssetPath(toWide(fileName));
            Microsoft::WRL::ComPtr<IWICImagingFactory> wic;
            if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)))) return;
            Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
            if (FAILED(wic->CreateDecoderFromFilename(pathW.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))) return;
            Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
            if (FAILED(decoder->GetFrame(0, &frame))) return;
            Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
            if (FAILED(wic->CreateFormatConverter(&converter))) return;
            if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut))) return;
            m_renderTarget->CreateBitmapFromWicBitmap(converter.Get(), nullptr, &m_wheelBitmap);
        }
};