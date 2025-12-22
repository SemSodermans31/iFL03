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
#include <string>
#include <format>
#include "Overlay.h"
#include "iracing.h"
#include "ClassColors.h"
#include "Config.h"
#include "Units.h"
#include "stub_data.h"
#include <wincodec.h>

class OverlayRelative : public Overlay
{
    public:

        virtual bool canEnableWhileDisconnected() const { return StubDataManager::shouldUseStubData(); }

        

        OverlayRelative()
            : Overlay("OverlayRelative")
        {}

    protected:

        enum class Columns { POSITION, CAR_NUMBER, NAME, POSITIONS_GAINED, DELTA, LICENSE, SAFETY_RATING, IRATING, IR_PRED, PIT, LAST, TIRE_COMPOUND };

        std::string tireCompoundToString(int compound) const
        {
            switch (compound)
            {
            case 0: return "Dry";
            case 1: {
                // In two-compound series (Dry/Wet), 1 represents Wet.
                // Use weather to disambiguate when series provide Primary/Alternate instead.
                const int trackWetness = ir_TrackWetness.getInt();
                const float precip = ir_Precipitation.getFloat();
                if (trackWetness > irsdk_TrackWetness_Dry || precip > 0.01f) return "Wet";
                return "Pri";
            }
            case 2: return "Alt";
            case 3: return "Wet";
            default: return "-";
            }
        }

        virtual void onEnable()
        {
            onConfigChanged();
            loadPositionIcons();
            loadFooterIcons();
        }

        virtual void onDisable()
        {
            m_text.reset();
            releasePositionIcons();
            releaseFooterIcons();
        }

        virtual void onConfigChanged()
        {
            // Fonts (centralized)
            m_text.reset( m_dwriteFactory.Get() );
            createGlobalTextFormat(1.0f, m_textFormat);
            createGlobalTextFormat(0.8f, m_textFormatSmall);
            // Per-overlay FPS (configurable; default 10)
            setTargetFPS(g_cfg.getInt(m_name, "target_fps", 10));

            // Determine widths of text columns
            m_columns.reset();
            const float fontSize = g_cfg.getFloat( "Overlay", "font_size", 16.0f );
            m_columns.add( (int)Columns::POSITION,   computeTextExtent( L"P99", m_dwriteFactory.Get(), m_textFormat.Get() ).x, fontSize/2 );
            m_columns.add( (int)Columns::CAR_NUMBER, computeTextExtent( L"#999", m_dwriteFactory.Get(), m_textFormat.Get() ).x, fontSize/2 );
            m_columns.add( (int)Columns::NAME,       0, fontSize/2 );
            if( g_cfg.getBool(m_name, "show_positions_gained", true) )
            {
                const float posTextW = computeTextExtent( L"99", m_dwriteFactory.Get(), m_textFormat.Get() ).x;
                m_columns.add( (int)Columns::POSITIONS_GAINED, posTextW + fontSize * 1.8f, fontSize / 2 );
            }
            if( g_cfg.getBool(m_name,"show_pit_age",true) )
                m_columns.add( (int)Columns::PIT,           computeTextExtent( L"999", m_dwriteFactory.Get(), m_textFormatSmall.Get() ).x, fontSize/4 );
            if( g_cfg.getBool(m_name,"show_license",true) && !g_cfg.getBool(m_name,"show_sr",false) )
                m_columns.add( (int)Columns::LICENSE,       computeTextExtent( L" A ", m_dwriteFactory.Get(), m_textFormatSmall.Get() ).x*1.6f, fontSize/10 );
            if( g_cfg.getBool(m_name,"show_sr",false) )
                m_columns.add( (int)Columns::SAFETY_RATING, computeTextExtent( L"A 4.44", m_dwriteFactory.Get(), m_textFormatSmall.Get() ).x, fontSize/8 );
            if( g_cfg.getBool(m_name,"show_irating",true) )
                m_columns.add( (int)Columns::IRATING,       computeTextExtent( L"999.9k", m_dwriteFactory.Get(), m_textFormatSmall.Get() ).x, fontSize/8 );

            // iRating prediction column (estimated gain/loss) - only show in race sessions
            if( g_cfg.getBool(m_name, "show_ir_pred", false) && ir_session.sessionType == SessionType::RACE ) {
                const float irPredScale = g_cfg.getFloat( m_name, "ir_pred_col_scale", 1.0f );
                m_columns.add( (int)Columns::IR_PRED,    computeTextExtent( L"+999", m_dwriteFactory.Get(), m_textFormatSmall.Get() ).x * irPredScale, fontSize/8 );
            }

            if( g_cfg.getBool(m_name, "show_tire_compound", false) )
                m_columns.add( (int)Columns::TIRE_COMPOUND, computeTextExtent( L"Comp 00", m_dwriteFactory.Get(), m_textFormatSmall.Get() ).x, fontSize/8 );

            // Allow user to scale the LAST column width via config
            const float lastColScale = g_cfg.getFloat( m_name, "last_col_scale", 2.0f );
            if( g_cfg.getBool(m_name, "show_last", true) )
                m_columns.add( (int)Columns::LAST,       computeTextExtent( L"99.99", m_dwriteFactory.Get(), m_textFormat.Get() ).x * lastColScale, fontSize/2 );
            m_columns.add( (int)Columns::DELTA,      computeTextExtent( L"+99L  -99.9", m_dwriteFactory.Get(), m_textFormat.Get() ).x, 1, fontSize/2 );
            

        }

        virtual void onUpdate()
        {
            if (!StubDataManager::shouldUseStubData() && !ir_hasValidDriver()) {
                return;
            }
            struct CarInfo {
                int     carIdx = 0;
                float   delta = 0;
                float   lapDistPct = 0;
                int     wrappedSum = 0;
                int     lapDelta = 0;
                int     pitAge = 0;
                float   last = 0;
                int     tireCompound = -1;
                int     positionsChanged = 0;
            };
            std::vector<CarInfo> relatives;
            relatives.reserve( IR_MAX_CARS );
            const float ownClassEstLaptime = ir_session.cars[ir_session.driverCarIdx].carClassEstLapTime;
            const int lapcountSelf = ir_Lap.getInt();
            const float selfLapDistPct = ir_LapDistPct.getFloat();
            const float SelfEstLapTime = ir_CarIdxEstTime.getFloat(ir_session.driverCarIdx);
            // Use stub data in preview mode
            const bool useStubData = StubDataManager::shouldUseStubData();
            if (useStubData) {
                StubDataManager::populateSessionCars();
            }
            
            // Apply global opacity
            const float globalOpacity = getGlobalOpacity();
            
            if (useStubData) {
                // Generate stub data for preview mode using centralized data
                auto relativeData = StubDataManager::getRelativeData();
                for (const auto& rel : relativeData) {
                    CarInfo ci;
                    ci.carIdx = rel.carIdx;
                    ci.delta = rel.delta;
                    ci.lapDelta = rel.lapDelta;
                    ci.pitAge = rel.pitAge;
                    if (const StubDataManager::StubCar* sc = StubDataManager::getStubCar(rel.carIdx)) {
                        ci.last = sc->lastLapTime;
                        ci.tireCompound = sc->tireCompound;
                        ci.positionsChanged = (rel.carIdx % 3) - 1;
                    } else {
                        ci.last = 0.0f;
                    }
                    relatives.push_back(ci);
                }
            } else {
                // Populate cars with the ones for which a relative/delta comparison is valid
                for( int i=0; i<IR_MAX_CARS; ++i )
                {
                    const Car& car = ir_session.cars[i];

                    const int lapcountCar = ir_CarIdxLap.getInt(i);

                    if( lapcountCar >= 0 && !car.isSpectator && car.carNumber>=0 )
                    {
                        // Add the pace car only under yellow or initial pace lap
                        if( car.isPaceCar && !(ir_SessionFlags.getInt() & (irsdk_caution|irsdk_cautionWaving)) && !ir_isPreStart() )
                            continue;

                        // If the other car is up to half a lap in front, we consider the delta 'ahead', otherwise 'behind'.

                        float delta = 0;
                        int   lapDelta = lapcountCar - lapcountSelf;

                        const float LClassRatio = car.carClassEstLapTime / ownClassEstLaptime;
                        const float CarEstLapTime = ir_CarIdxEstTime.getFloat(i) / LClassRatio;
                        const float carLapDistPct = ir_CarIdxLapDistPct.getFloat(i);

                        // Does the delta between us and the other car span across the start/finish line?
                        const bool wrap = fabsf(carLapDistPct - selfLapDistPct) > 0.5f;
                        int wrappedSum = 0;

                        if( wrap )
                        {
                            if (selfLapDistPct > carLapDistPct) {
                                delta = (CarEstLapTime - SelfEstLapTime) + ownClassEstLaptime;
                                lapDelta += -1;
                                wrappedSum = 1;
                            }
                            else {
                                delta = (CarEstLapTime - SelfEstLapTime) - ownClassEstLaptime;
                                lapDelta += 1;
                                wrappedSum = -1;
                            }

                        }
                        else
                        {
                            delta = CarEstLapTime - SelfEstLapTime;
                        }
                        if( ir_session.sessionType!=SessionType::RACE || ir_isPreStart() || car.isPaceCar )
                        {
                            lapDelta = 0;
                        }

                        CarInfo ci;
                        ci.carIdx = i;
                        ci.delta = delta;
                        ci.lapDelta = lapDelta;
                        ci.lapDistPct = ir_CarIdxLapDistPct.getFloat(i);
                        ci.wrappedSum = wrappedSum;
                        ci.pitAge = ir_CarIdxLap.getInt(i) - car.lastLapInPits;
                        ci.last = ir_CarIdxLastLapTime.getFloat(i);
                        ci.tireCompound = ir_CarIdxTireCompound.isValid() ? ir_CarIdxTireCompound.getInt(i) : -1;
                        if (ci.tireCompound < 0 && car.tireCompound >= 0)
                            ci.tireCompound = car.tireCompound;
                        ci.positionsChanged = ir_getPositionsChanged(i);
                        relatives.push_back( ci );
                    }
                }
            }

            // Sort by lap % completed, in case deltas are a bit desynced
            std::sort( relatives.begin(), relatives.end(), 
                []( const CarInfo& a, const CarInfo&b ) {return a.lapDistPct + a.wrappedSum > b.lapDistPct + b.wrappedSum ;} );

            // Locate our driver's index in the new array
            int selfCarInfoIdx = -1;
            for( int i=0; i<(int)relatives.size(); ++i )
            {
                if( relatives[i].carIdx == ir_session.driverCarIdx ) {
                    selfCarInfoIdx = i;
                    break;
                }
            }

            // Something's wrong if we didn't find our driver. Bail.
            if( selfCarInfoIdx < 0 )
                return;

            // Display such that our driver is in the vertical center of the area where we're listing cars

            const float  fontSize           = g_cfg.getFloat( "Overlay", "font_size", 16.0f );
            const float  lineSpacing        = g_cfg.getFloat( m_name, "line_spacing", 6 );
            const float  lineHeight         = fontSize + lineSpacing;
            const float4 selfCol            = g_cfg.getFloat4( m_name, "self_col", float4(0.94f,0.67f,0.13f,1) );
            const float4 sameLapCol         = g_cfg.getFloat4( m_name, "same_lap_col", float4(1,1,1,1) );
            const float4 lapAheadCol        = g_cfg.getFloat4( m_name, "lap_ahead_col", float4(0.9f,0.17f,0.17f,1) );
            const float4 lapBehindCol       = g_cfg.getFloat4( m_name, "lap_behind_col", float4(0,0.71f,0.95f,1) );
            const float4 iratingTextCol     = g_cfg.getFloat4( m_name, "irating_text_col", float4(0,0,0,0.9f) );
            const float4 iratingBgCol       = g_cfg.getFloat4( m_name, "irating_background_col", float4(1,1,1,0.85f) );
            const float4 licenseTextCol     = g_cfg.getFloat4( m_name, "license_text_col", float4(1,1,1,0.9f) );
            const float  licenseBgAlpha     = g_cfg.getFloat( m_name, "license_background_alpha", 0.8f );
            const float4 alternateLineBgCol = g_cfg.getFloat4( m_name, "alternate_line_background_col", float4(0.5f,0.5f,0.5f,0.1f) );
            const float4 buddyCol           = g_cfg.getFloat4( m_name, "buddy_col", float4(0.2f,0.75f,0,1) );
            const float4 flaggedCol         = g_cfg.getFloat4( m_name, "flagged_col", float4(0.6f,0.35f,0.2f,1) );
            const float4 headerCol          = g_cfg.getFloat4( m_name, "header_col", float4(0.7f,0.7f,0.7f,0.9f) );
            const float4 carNumberBgCol     = g_cfg.getFloat4( m_name, "car_number_background_col", float4(1,1,1,0.9f) );
            const float4 carNumberTextCol   = g_cfg.getFloat4( m_name, "car_number_text_col", float4(0,0,0,0.9f) );
            const float4 pitCol             = g_cfg.getFloat4( m_name, "pit_col", float4(0.94f,0.8f,0.13f,1) );
            const bool   minimapEnabled     = g_cfg.getBool( m_name, "minimap_enabled", true );
            const bool   minimapIsRelative  = g_cfg.getBool( m_name, "minimap_is_relative", true );
            const float4 minimapBgCol       = g_cfg.getFloat4( m_name, "minimap_background_col", float4(0,0,0,0.13f) );
            const float  listingAreaTop     = minimapEnabled ? 30 : 10.0f;
            const float  listingAreaBot     = m_height - 10.0f;
            const float  yself              = listingAreaTop + (listingAreaBot-listingAreaTop) / 2.0f;
            const int    entriesAbove       = int( (yself - lineHeight/2 - listingAreaTop) / lineHeight );

            float y = yself - entriesAbove * lineHeight;

            // Reserve space for footer (1.5x line height like OverlayStandings)
            const float  ybottomFooter      = m_height - lineHeight * 1.5f;

            const float xoff = 10.0f;
            m_columns.layout( (float)m_width - 20 );

            // Prepare participant list for iRating prediction
            struct Participant { int carIdx; int position; int irating; };
            std::vector<Participant> participants;
            participants.reserve(IR_MAX_CARS);
            for( int i=0; i<IR_MAX_CARS; ++i )
            {
                const Car& car = ir_session.cars[i];
                if( car.isSpectator || car.carNumber < 0 )
                    continue;
                int pos = 0;
                if( useStubData )
                {
                    if (const StubDataManager::StubCar* sc = StubDataManager::getStubCar(i))
                        pos = sc->position;
                }
                else
                {
                    pos = ir_getPosition(i);
                }
                if( pos <= 0 )
                    continue;
                participants.push_back( Participant{ i, pos, car.irating } );
            }

            const float irPredKTotal = g_cfg.getFloat( m_name, "ir_pred_k_total", 80.0f );
            auto predictIrDeltaFor = [&](int targetCarIdx)->int
            {
                int n = (int)participants.size();
                if( n <= 1 ) return 0;
                int targetPos = 0;
                int targetRating = 0;
                for( const auto& p : participants )
                {
                    if( p.carIdx == targetCarIdx ) { targetPos = p.position; targetRating = p.irating; break; }
                }
                if( targetPos <= 0 ) return 0;
                const float kPerOpponent = irPredKTotal / std::max(1, n - 1);
                float delta = 0.0f;
                for( const auto& opp : participants )
                {
                    if( opp.carIdx == targetCarIdx ) continue;
                    const float expected = 1.0f / (1.0f + powf(10.0f, (opp.irating - targetRating) / 400.0f));
                    const float actual = (targetPos < opp.position) ? 1.0f : (targetPos > opp.position ? 0.0f : 0.5f);
                    delta += (actual - expected) * kPerOpponent;
                }
                return (int)roundf(delta);
            };

            m_renderTarget->BeginDraw();
            for( int cnt=0, i=selfCarInfoIdx-entriesAbove; i<(int)relatives.size() && y<=ybottomFooter-lineHeight/2; ++i, y+=lineHeight, ++cnt )
            {
                // Alternating line backgrounds
                if( cnt & 1 && alternateLineBgCol.a > 0 )
                {
                    D2D1_RECT_F r = { 0, y-lineHeight/2, (float)m_width,  y+lineHeight/2 };
                    m_brush->SetColor( alternateLineBgCol );
                    m_renderTarget->FillRectangle( &r, m_brush.Get() );
                }

                // Skip if we don't have a car to list for this line
                if( i < 0 )
                    continue;

                const CarInfo& ci  = relatives[i];
                const Car&     car = ir_session.cars[ci.carIdx];

                // Determine text color
                float4 col = sameLapCol;
                if( ci.lapDelta > 0 )
                    col = lapAheadCol;
                if( ci.lapDelta < 0 )
                    col = lapBehindCol;

                if( car.isSelf )
                    col = selfCol;
                else if( !useStubData && ir_CarIdxOnPitRoad.getBool(ci.carIdx) )
                    col.a *= 0.5f;
                
                // Apply global opacity
                col.w *= globalOpacity;
                
                wchar_t s[512];
                std::string str;
                D2D1_RECT_F r = {};
                D2D1_ROUNDED_RECT rr = {};
                const ColumnLayout::Column* clm = nullptr;
                
                // Position
                int position = 0;
                if (useStubData) {
                    // For stub data, use the position in the current display order
                    // The first car shown is P1, second is P2, etc.
                    int displayIndex = selfCarInfoIdx - entriesAbove + cnt;
                    if (displayIndex >= 0 && displayIndex < (int)relatives.size()) {
                        position = displayIndex + 1; // P1, P2, P3, etc.
                    }
                } else {
                    position = ir_getPosition(ci.carIdx);
                }
                if( position > 0 )
                {
                    clm = m_columns.get( (int)Columns::POSITION );
                    // Standings-style position label: fully-opaque white, square left edge + pill right edge, centered black text
                    {
                        // Use full column width (text + borders) for a wider label background
                        const float prL = xoff + (clm->textL - clm->borderL);
                        const float prR = xoff + (clm->textR + clm->borderR);
                        const float inset = 1.0f;
                        D2D1_RECT_F pr = { prL, y - lineHeight / 2, prR, y + lineHeight / 2 };
                        D2D1_RECT_F rrRect = { pr.left + inset, pr.top + inset, pr.right - inset, pr.bottom - inset };
                        const float h = rrRect.bottom - rrRect.top;
                        const float rCap = h * 0.5f;

                        m_brush->SetColor(float4(1, 1, 1, 1)); // 100% white
                        const float w = rrRect.right - rrRect.left;
                        if (w <= (rCap * 2.0f + 1.0f))
                        {
                            rr.rect = rrRect;
                            rr.radiusX = 3.0f;
                            rr.radiusY = 3.0f;
                            m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
                        }
                        else
                        {
                            D2D1_RECT_F baseRect = rrRect;
                            baseRect.right = rrRect.right - rCap;
                            m_renderTarget->FillRectangle(&baseRect, m_brush.Get());

                            D2D1_ROUNDED_RECT capRR = {};
                            capRR.rect = { rrRect.right - 2.0f * rCap, rrRect.top, rrRect.right, rrRect.bottom };
                            capRR.radiusX = rCap;
                            capRR.radiusY = rCap;
                            m_renderTarget->FillRoundedRectangle(&capRR, m_brush.Get());
                        }
                    }

                    m_brush->SetColor(float4(0,0,0,1));
                    swprintf( s, _countof(s), L"P%d", position );
                    m_text.render( m_renderTarget.Get(), s, m_textFormat.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );
                }

                // Car number
                {
                    clm = m_columns.get( (int)Columns::CAR_NUMBER );
                    swprintf( s, _countof(s), L"#%S", car.carNumberStr.c_str() );
                    r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
                    rr.rect = { r.left-2, r.top+1, r.right+2, r.bottom-1 };
                    rr.radiusX = 3;
                    rr.radiusY = 3;

                    // Standings-style number badge:
                    // - base fill uses class base color
                    // - left accent strip (3px) uses class light color
                    // - text is white
                    const int classId = car.classId;
                    float4 bg = ClassColors::get(classId);
                    bg.a = licenseBgAlpha;
                    m_brush->SetColor( bg );
                    m_renderTarget->FillRoundedRectangle( &rr, m_brush.Get() );

                    // Left accent strip
                    {
                        float4 stripCol = ClassColors::getLight(classId);
                        stripCol.a = bg.a;
                        m_brush->SetColor(stripCol);
                        const float stripW = 3.0f;
                        D2D1_RECT_F strip = { rr.rect.left + 1.0f, rr.rect.top + 1.0f, rr.rect.left + 1.0f + stripW, rr.rect.bottom - 1.0f };
                        m_renderTarget->FillRectangle(&strip, m_brush.Get());
                    }

                    // Car number text color (white)
                    m_brush->SetColor( float4(1,1,1,1) );
                    m_text.render( m_renderTarget.Get(), s, m_textFormat.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );
                }

                // Name
                {
                    clm = m_columns.get( (int)Columns::NAME );
                    std::string displayName = car.userName;
                    if (!g_cfg.getBool(m_name, "show_full_name", true)) {
                        // Show only first name
                        size_t spacePos = displayName.find(' ');
                        if (spacePos != std::string::npos) {
                            displayName = displayName.substr(0, spacePos);
                        }
                    }
                    swprintf( s, _countof(s), L"%S", displayName.c_str() );
                    m_brush->SetColor( col );
                    m_text.render( m_renderTarget.Get(), s, m_textFormat.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing );
                }

                // Pit age
                if( (clm = m_columns.get((int)Columns::PIT)) && !ir_isPreStart() && (ci.pitAge>=0||ir_CarIdxOnPitRoad.getBool(ci.carIdx)) )
                {
                    r = { xoff+clm->textL, y-lineHeight/2+2, xoff+clm->textR, y+lineHeight/2-2 };
                    m_brush->SetColor( pitCol );
                    m_renderTarget->DrawRectangle( &r, m_brush.Get() );
                    if( ir_CarIdxOnPitRoad.getBool(ci.carIdx) ) {
                        swprintf( s, _countof(s), L"PIT" );
                        m_renderTarget->FillRectangle( &r, m_brush.Get() );
                        m_brush->SetColor( float4(0,0,0,1) );
                    }
                    else {
                        swprintf( s, _countof(s), L"%d", ci.pitAge );
                        m_renderTarget->DrawRectangle( &r, m_brush.Get() );
                    }
                    m_text.render( m_renderTarget.Get(), s, m_textFormatSmall.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );
                }

                // License without SR
                if( clm = m_columns.get( (int)Columns::LICENSE ) )
                {
                    swprintf( s, _countof(s), L"%C", car.licenseChar );
                    r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
                    rr.rect = { r.left+1, r.top+1, r.right-1, r.bottom-1 };
                    rr.radiusX = 3;
                    rr.radiusY = 3;
                    float4 c = car.licenseCol;
                    c.a = licenseBgAlpha;
                    m_brush->SetColor( c );
                    m_renderTarget->FillRoundedRectangle( &rr, m_brush.Get() );
                    m_brush->SetColor( licenseTextCol );
                    m_text.render( m_renderTarget.Get(), s, m_textFormatSmall.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );
                }

                // License with SR
                if( clm = m_columns.get( (int)Columns::SAFETY_RATING ) )
                {
                    swprintf( s, _countof(s), L"%C %.1f", car.licenseChar, car.licenseSR );
                    r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
                    rr.rect = { r.left+1, r.top+1, r.right-1, r.bottom-1 };
                    rr.radiusX = 3;
                    rr.radiusY = 3;
                    float4 c = car.licenseCol;
                    c.a = licenseBgAlpha;
                    m_brush->SetColor( c );
                    m_renderTarget->FillRoundedRectangle( &rr, m_brush.Get() );
                    m_brush->SetColor( licenseTextCol );
                    m_text.render( m_renderTarget.Get(), s, m_textFormatSmall.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );
                }

                // Irating
                if( clm = m_columns.get( (int)Columns::IRATING ) )
                {
                    swprintf( s, _countof(s), L"%.1fk", (float)car.irating/1000.0f );
                    r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
                    rr.rect = { r.left+1, r.top+1, r.right-1, r.bottom-1 };
                    rr.radiusX = 3;
                    rr.radiusY = 3;
                    m_brush->SetColor( iratingBgCol );
                    m_renderTarget->FillRoundedRectangle( &rr, m_brush.Get() );
                    m_brush->SetColor( iratingTextCol );
                    m_text.render( m_renderTarget.Get(), s, m_textFormatSmall.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );
                }

                // Positions gained/lost
                if( clm = m_columns.get( (int)Columns::POSITIONS_GAINED ) )
                {
                    r = { xoff + clm->textL, y - lineHeight / 2, xoff + clm->textR, y + lineHeight / 2 };
                    rr.rect = { r.left + 1, r.top + 1, r.right - 1, r.bottom - 1 };
                    rr.radiusX = 3;
                    rr.radiusY = 3;
                    m_brush->SetColor(float4(1, 1, 1, 1));
                    m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());

                    const int deltaPos = ci.positionsChanged;
                    ID2D1Bitmap* icon = nullptr;
                    if (deltaPos > 0)      icon = m_posUpIcon.Get();
                    else if (deltaPos < 0) icon = m_posDownIcon.Get();
                    else                   icon = m_posEqualIcon.Get();

                    const float iconPad  = 4.0f;
                    const float iconSize = std::max(0.0f, lineHeight - 6.0f);
                    if (icon) {
                        D2D1_RECT_F ir = { r.left + iconPad, y - iconSize * 0.5f, r.left + iconPad + iconSize, y + iconSize * 0.5f };
                        m_renderTarget->DrawBitmap(icon, &ir);
                    }

                    m_brush->SetColor(float4(0, 0, 0, 1));
                    swprintf(s, _countof(s), L"%d", abs(deltaPos));
                    const float textL = r.left + iconPad + (icon ? iconSize + 2.0f : 0.0f);
                    m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), textL, r.right - 15.0f, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
                }

                if( clm = m_columns.get( (int)Columns::TIRE_COMPOUND ) )
                {
                    int compound = ci.tireCompound;
                    if (compound < 0 && car.tireCompound >= 0)
                        compound = car.tireCompound;
                    if (compound < 0 && ir_CarIdxTireCompound.isValid())
                        compound = ir_CarIdxTireCompound.getInt(ci.carIdx);
                    const std::string compoundText = tireCompoundToString(compound);
                    m_brush->SetColor( col );
                    m_text.render( m_renderTarget.Get(), toWide(compoundText).c_str(), m_textFormatSmall.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );
                }

                // iRating prediction - only show in race sessions
                if( (clm = m_columns.get( (int)Columns::IR_PRED )) && ir_session.sessionType == SessionType::RACE )
                {
                    const int irDelta = predictIrDeltaFor(ci.carIdx);
                    swprintf( s, _countof(s), L"%+d", irDelta );
                    r = { xoff+clm->textL, y-lineHeight/2, xoff+clm->textR, y+lineHeight/2 };
                    rr.rect = { r.left+1, r.top+1, r.right-1, r.bottom-1 };
                    rr.radiusX = 3;
                    rr.radiusY = 3;
                    float4 bg = irDelta > 0 ? float4(0.2f, 0.75f, 0.2f, 0.85f) : (irDelta < 0 ? float4(0.9f, 0.2f, 0.2f, 0.85f) : float4(1,1,1,0.85f));
                    bg.w *= globalOpacity;
                    m_brush->SetColor( bg );
                    m_renderTarget->FillRoundedRectangle( &rr, m_brush.Get() );
                    float4 tcol = (irDelta == 0) ? float4(0,0,0,0.9f) : float4(1,1,1,0.95f);
                    tcol.w *= globalOpacity;
                    m_brush->SetColor( tcol );
                    m_text.render( m_renderTarget.Get(), s, m_textFormatSmall.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );
                }

                // Last
                if( clm = m_columns.get((int)Columns::LAST) )
                {
                    str.clear();
                    if (ci.last > 0)
                        str = formatLaptime(ci.last);
                    m_brush->SetColor(col);
                    m_text.render(m_renderTarget.Get(), toWide(str).c_str(), m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
                }

                // Delta
                {
                    clm = m_columns.get((int)Columns::DELTA);
                    swprintf(s, _countof(s), L"%.1f", ci.delta);
                    m_brush->SetColor(col);
                    m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
                }
            }

            // Footer: left (session time), right (track temp + laps)
            {
                const bool imperial = isImperialUnits();
                float trackTemp = ir_TrackTempCrew.getFloat();
                char  tempUnit  = 'C';
                if( imperial ) { trackTemp = celsiusToFahrenheit(trackTemp); tempUnit = 'F'; }

                int hours = 0, mins = 0, secs = 0;
                ir_getSessionTimeRemaining(hours, mins, secs);

                const int laps = std::max(ir_CarIdxLap.getInt(ir_session.driverCarIdx), ir_CarIdxLapCompleted.getInt(ir_session.driverCarIdx));
                const int remainingLaps = ir_getLapsRemaining();
                const int irTotalLaps = ir_SessionLapsTotal.getInt();
                int totalLaps = (irTotalLaps == 32767) ? (laps + remainingLaps) : irTotalLaps;

                const float ybottom = ybottomFooter;

                // Divider line
                m_brush->SetColor(float4(1,1,1,0.4f));
                m_renderTarget->DrawLine(float2(0,ybottom), float2((float)m_width,ybottom), m_brush.Get());

                struct FooterItem { ID2D1Bitmap* icon; std::wstring text; float width; };
                std::vector<FooterItem> leftItems;
                std::vector<FooterItem> rightItems;

                auto measure = [&](const std::wstring& w)->float {
                    return computeTextExtent(w.c_str(), m_dwriteFactory.Get(), m_textFormat.Get(), m_fontSpacing).x;
                };

                if (g_cfg.getBool(m_name, "show_session_end", true)) {
                    leftItems.push_back({ m_iconSessionTime.Get(), toWide(std::vformat("{}:{:0>2}:{:0>2}", std::make_format_args(hours, mins, secs))), 0.0f });
                }
                if (g_cfg.getBool(m_name, "show_track_temp", true)) {
                    wchar_t buf[64];
                    swprintf(buf, _countof(buf), L"%.1f\x00B0%c", trackTemp, tempUnit);
                    rightItems.push_back({ m_iconTrackTemp.Get(), std::wstring(buf), 0.0f });
                }
                if (g_cfg.getBool(m_name, "show_laps", true)) {
                    rightItems.push_back({ m_iconLaps.Get(), toWide(std::format("{}/{}{}", laps, (irTotalLaps == 32767 ? "~" : ""), totalLaps)), 0.0f });
                }

                const float fontSize = g_cfg.getFloat("Overlay", "font_size", 16.0f);
                const float iconSize = std::max(20.0f, fontSize * 1.2f);
                const float iconPad = std::max(3.0f, fontSize * 0.25f);
                const float yText = m_height - (m_height - ybottom) / 2;

                // Left side
                if (!leftItems.empty()) {
                    float xL = 10.0f;
                    for (auto& it : leftItems) {
                        const float xStart = xL;
                        it.width = (it.icon ? iconSize + iconPad : 0.0f) + measure(it.text);
                        // Enforce a minimum width for session time equal to width of "999:99:99"
                        float minItemW = 0.0f;
                        if (it.icon == m_iconSessionTime.Get()) {
                            const float minTextW = computeTextExtent(L"999:99:99", m_dwriteFactory.Get(), m_textFormatSmall.Get(), m_fontSpacing).x;
                            minItemW = (it.icon ? iconSize + iconPad : 0.0f) + minTextW + 6.0f;
                        }
                        const float itemW = std::max(it.width + 6.0f, minItemW);
                        const float itemH = iconSize + 2.0f;
                        D2D1_RECT_F bg = { xL - 4.0f, yText - itemH * 0.5f, xL + itemW - 4.0f, yText + itemH * 0.5f };
                        D2D1_ROUNDED_RECT rrh = { bg, itemH * 0.5f, itemH * 0.5f };
                        m_brush->SetColor(float4(1,1,1,1));
                        m_renderTarget->FillRoundedRectangle(&rrh, m_brush.Get());
                        if (it.icon) {
                            D2D1_RECT_F ir = { xL, yText - iconSize * 0.5f, xL + iconSize, yText + iconSize * 0.5f };
                            m_renderTarget->DrawBitmap(it.icon, &ir);
                            xL += iconSize + iconPad;
                        }
                        m_brush->SetColor(float4(0,0,0,1));
                        m_text.render(m_renderTarget.Get(), it.text.c_str(), m_textFormatSmall.Get(), xL, xL + it.width + 64.0f, yText, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing);
                        xL = xStart + itemW + 12.0f;
                    }
                }

                // Right side
                if (!rightItems.empty()) {
                    float xR = (float)m_width - 10.0f;
                    for (int i = (int)rightItems.size() - 1; i >= 0; --i) {
                        auto& it = rightItems[i];
                        it.width = (it.icon ? iconSize + iconPad : 0.0f) + measure(it.text);
                        const float itemW = it.width + 6.0f;
                        const float itemH = iconSize + 2.0f;
                        xR -= itemW;
                        D2D1_RECT_F bg = { xR - 4.0f, yText - itemH * 0.5f, xR + itemW - 4.0f, yText + itemH * 0.5f };
                        D2D1_ROUNDED_RECT rrh = { bg, itemH * 0.5f, itemH * 0.5f };
                        m_brush->SetColor(float4(1,1,1,1));
                        m_renderTarget->FillRoundedRectangle(&rrh, m_brush.Get());
                        float itemX = xR;
                        if (it.icon) {
                            D2D1_RECT_F ir = { itemX, yText - iconSize * 0.5f, itemX + iconSize, yText + iconSize * 0.5f };
                            m_renderTarget->DrawBitmap(it.icon, &ir);
                            itemX += iconSize + iconPad;
                        }
                        m_brush->SetColor(float4(0,0,0,1));
                        m_text.render(m_renderTarget.Get(), it.text.c_str(), m_textFormatSmall.Get(), itemX, itemX + measure(it.text) + 32.0f, yText, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing);
                        xR -= 12.0f;
                    }
                }
            }

            // Minimap with header (SoF left, Incidents right)
            if( minimapEnabled )
            {
                const float y = 10;
                const float baseX = 10;
                const float h = 15;

                // Header items flanking minimap
                const float fontSizeH = g_cfg.getFloat("Overlay", "font_size", 16.0f);
                const float iconSizeH = std::max(20.0f, fontSizeH * 1.2f);
                const float iconPadH = std::max(3.0f,  fontSizeH * 0.25f);
                const float yCenter = y + h * 0.5f;

                float leftReserved  = 0.0f;
                float rightReserved = 0.0f;

                if (g_cfg.getBool(m_name, "show_SoF", true)) {
                    int sof = ir_session.sof; if (sof < 0) sof = 0;
                    std::wstring sofText = toWide(std::format("{}", sof));
                    const float textW = computeTextExtent(sofText.c_str(), m_dwriteFactory.Get(), m_textFormatSmall.Get(), m_fontSpacing).x;
                    const float minTextW = computeTextExtent(L"99999", m_dwriteFactory.Get(), m_textFormatSmall.Get(), m_fontSpacing).x;
                    const float itemH = iconSizeH + 2.0f;
                    const float itemW = (m_iconSoF ? iconSizeH + iconPadH : 0.0f) + std::max(textW, minTextW) + 6.0f;
                    float xL = baseX;
                    D2D1_RECT_F bg = { xL - 4.0f, yCenter - itemH * 0.5f, xL + itemW - 4.0f, yCenter + itemH * 0.5f };
                    D2D1_ROUNDED_RECT rrh = { bg, itemH * 0.5f, itemH * 0.5f };
                    m_brush->SetColor(float4(1,1,1,1));
                    m_renderTarget->FillRoundedRectangle(&rrh, m_brush.Get());
                    if (m_iconSoF) {
                        D2D1_RECT_F ir = { xL, yCenter - iconSizeH * 0.5f, xL + iconSizeH, yCenter + iconSizeH * 0.5f };
                        m_renderTarget->DrawBitmap(m_iconSoF.Get(), &ir);
                        xL += iconSizeH + iconPadH;
                    }
                    m_brush->SetColor(float4(0,0,0,1));
                    m_text.render(m_renderTarget.Get(), sofText.c_str(), m_textFormatSmall.Get(), xL, xL + textW + 32.0f, yCenter, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing);
                    leftReserved = itemW + 6.0f;
                }

                if (g_cfg.getBool(m_name, "show_incidents", true)) {
                    const int inc = ir_PlayerCarTeamIncidentCount.getInt();
                    const int lim = ir_session.incidentLimit;
                    std::wstring incText = toWide(lim > 0 ? std::format("{}/{}", inc, lim) : std::format("{}/--", inc));
                    const float textW = computeTextExtent(incText.c_str(), m_dwriteFactory.Get(), m_textFormatSmall.Get(), m_fontSpacing).x;
                    const float itemH = iconSizeH + 2.0f;
                    const float itemW = (m_iconIncidents ? iconSizeH + iconPadH : 0.0f) + textW + 6.0f;
                    float xR = (float)m_width - baseX - itemW;
                    D2D1_RECT_F bg = { xR - 4.0f, yCenter - itemH * 0.5f, xR + itemW - 4.0f, yCenter + itemH * 0.5f };
                    D2D1_ROUNDED_RECT rrh = { bg, itemH * 0.5f, itemH * 0.5f };
                    m_brush->SetColor(float4(1,1,1,1));
                    m_renderTarget->FillRoundedRectangle(&rrh, m_brush.Get());
                    if (m_iconIncidents) {
                        D2D1_RECT_F ir = { xR, yCenter - iconSizeH * 0.5f, xR + iconSizeH, yCenter + iconSizeH * 0.5f };
                        m_renderTarget->DrawBitmap(m_iconIncidents.Get(), &ir);
                        xR += iconSizeH + iconPadH;
                    }
                    m_brush->SetColor(float4(0,0,0,1));
                    m_text.render(m_renderTarget.Get(), incText.c_str(), m_textFormatSmall.Get(), xR, xR + textW + 32.0f, yCenter, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing);
                    rightReserved = itemW + 6.0f;
                }

                const float x = baseX + leftReserved;
                const float w = std::max(0.0f, (float)m_width - x - baseX - rightReserved);
                D2D1_RECT_F r = { x, y, x+w, y+h };
                m_brush->SetColor( minimapBgCol );
                m_renderTarget->FillRectangle( &r, m_brush.Get() );                

                // phases: lap down, same lap, lap ahead, buddies, pacecar, self
                for( int phase=0; phase<6; ++phase )
                {
                    float4 baseCol = float4(0,0,0,0);
                    switch(phase)
                    {
                        case 0: baseCol = lapBehindCol; break;
                        case 1: baseCol = sameLapCol; break;
                        case 2: baseCol = lapAheadCol; break;
                        case 3: baseCol = buddyCol; break;
                        case 4: baseCol = float4(1,1,1,1); break;
                        case 5: baseCol = selfCol; break;
                        default: break;
                    }

                    for( int i=0; i<(int)relatives.size(); ++i )
                    {
                        const CarInfo& ci     = relatives[i];
                        const Car&     car    = ir_session.cars[ci.carIdx];

                        if( phase == 0 && ci.lapDelta >= 0 )
                            continue;
                        if( phase == 1 && ci.lapDelta != 0 )
                            continue;
                        if( phase == 2 && ci.lapDelta <= 0 )
                            continue;
                        if( phase == 3 && !car.isBuddy )
                            continue;
                        if( phase == 4 && !car.isPaceCar )
                            continue;
                        if( phase == 5 && !car.isSelf )
                            continue;
                        
                        float e;
                        if (useStubData) {
                            // Use stub data minimap positions if available
                            auto relativeData = StubDataManager::getRelativeData();
                            bool foundMinimapPos = false;
                            for (const auto& rel : relativeData) {
                                if (rel.carIdx == ci.carIdx) {
                                    e = rel.minimapX * w + x;
                                    foundMinimapPos = true;
                                    break;
                                }
                            }
                            if (!foundMinimapPos) {
                                // Fallback to lap distance calculation
                                e = ir_CarIdxLapDistPct.getFloat(ci.carIdx);
                                const float eself = ir_CarIdxLapDistPct.getFloat(ir_session.driverCarIdx);
                                if( minimapIsRelative )
                                {
                                    e = e - eself + 0.5f;
                                    if( e > 1 )
                                        e -= 1;
                                    if( e < 0 )
                                        e += 1;
                                }
                                e = e * w + x;
                            }
                        } else {
                            e = ir_CarIdxLapDistPct.getFloat(ci.carIdx);
                            const float eself = ir_CarIdxLapDistPct.getFloat(ir_session.driverCarIdx);
                            if( minimapIsRelative )
                            {
                                e = e - eself + 0.5f;
                                if( e > 1 )
                                    e -= 1;
                                if( e < 0 )
                                    e += 1;
                            }
                            e = e * w + x;
                        }

                        float4 col = baseCol;
                        if( !car.isSelf && ir_CarIdxOnPitRoad.getBool(ci.carIdx) )
                            col.a *= 0.5f;

                        const float dx = 2;
                        const float dy = car.isSelf || car.isPaceCar ? 4.0f : 0.0f;
                        r = {e-dx, y+2-dy, e+dx, y+h-2+dy};
                        m_brush->SetColor( col );
                        m_renderTarget->FillRectangle( &r, m_brush.Get() );
                    }
                }
            }
            m_renderTarget->EndDraw();
        }

    protected:

        Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormat;
        Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormatSmall;

        ColumnLayout m_columns;
        TextCache    m_text;
        float m_fontSpacing = getGlobalFontSpacing();
        // Position change icons and factory
        Microsoft::WRL::ComPtr<IWICImagingFactory> m_wicFactory;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_posUpIcon;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_posDownIcon;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_posEqualIcon;

        // Footer icons
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_iconIncidents;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_iconSoF;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_iconTrackTemp;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_iconSessionTime;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> m_iconLaps;

        void loadPositionIcons()
        {
            if (!m_renderTarget.Get()) return;
            if (m_posUpIcon || m_posDownIcon || m_posEqualIcon) return;
            (void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (!m_wicFactory.Get()) {
                (void)CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wicFactory));
            }
            if (!m_wicFactory.Get()) return;
            auto loadPng = [&](const std::wstring& rel) -> Microsoft::WRL::ComPtr<ID2D1Bitmap>
            {
                Microsoft::WRL::ComPtr<ID2D1Bitmap> bmp;
                Microsoft::WRL::ComPtr<IWICBitmapDecoder> dec;
                Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
                Microsoft::WRL::ComPtr<IWICFormatConverter> conv;
                const std::wstring path = resolveAssetPathW(rel);
                if (FAILED(m_wicFactory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &dec))) return bmp;
                if (FAILED(dec->GetFrame(0, &frame))) return bmp;
                if (FAILED(m_wicFactory->CreateFormatConverter(&conv))) return bmp;
                if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut))) return bmp;
                (void)m_renderTarget->CreateBitmapFromWicBitmap(conv.Get(), nullptr, &bmp);
                return bmp;
            };
            m_posUpIcon    = loadPng(L"assets\\icons\\up.png");
            m_posDownIcon  = loadPng(L"assets\\icons\\down.png");
            m_posEqualIcon = loadPng(L"assets\\icons\\equal.png");
        }

        void releasePositionIcons()
        {
            m_posUpIcon.Reset();
            m_posDownIcon.Reset();
            m_posEqualIcon.Reset();
            m_wicFactory.Reset();
        }

        void loadFooterIcons()
        {
            if (!m_renderTarget.Get()) return;
            (void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (!m_wicFactory.Get()) {
                (void)CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wicFactory));
            }
            if (!m_wicFactory.Get()) return;
            auto loadPng = [&](const std::wstring& rel) -> Microsoft::WRL::ComPtr<ID2D1Bitmap>
            {
                Microsoft::WRL::ComPtr<ID2D1Bitmap> bmp;
                Microsoft::WRL::ComPtr<IWICBitmapDecoder> dec;
                Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
                Microsoft::WRL::ComPtr<IWICFormatConverter> conv;
                const std::wstring path = resolveAssetPathW(rel);
                if (FAILED(m_wicFactory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &dec))) return bmp;
                if (FAILED(dec->GetFrame(0, &frame))) return bmp;
                if (FAILED(m_wicFactory->CreateFormatConverter(&conv))) return bmp;
                if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut))) return bmp;
                (void)m_renderTarget->CreateBitmapFromWicBitmap(conv.Get(), nullptr, &bmp);
                return bmp;
            };
            m_iconIncidents   = loadPng(L"assets\\icons\\incidents.png");
            m_iconSoF         = loadPng(L"assets\\icons\\SoF.png");
            m_iconTrackTemp   = loadPng(L"assets\\icons\\temp_dark.png");
            m_iconSessionTime = loadPng(L"assets\\icons\\session_time.png");
            m_iconLaps        = loadPng(L"assets\\icons\\laps.png");
        }

        void releaseFooterIcons()
        {
            m_iconIncidents.Reset();
            m_iconSoF.Reset();
            m_iconTrackTemp.Reset();
            m_iconSessionTime.Reset();
            m_iconLaps.Reset();
        }
};