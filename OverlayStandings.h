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

#include <assert.h>
#include <set>
#include <format>
#include <string>
#include <map>
#include <algorithm>
#include <wincodec.h>
#include "Overlay.h"
#include "Config.h"
#include "Units.h"
#include "OverlayDebug.h"
#include "stub_data.h"
#include "ClassColors.h"

class OverlayStandings : public Overlay
{
public:

    virtual bool canEnableWhileDisconnected() const { return StubDataManager::shouldUseStubData(); }
    
    const int defaultNumTopDrivers = 3;
    const int defaultNumAheadDrivers = 5;
    const int defaultNumBehindDrivers = 5;

    enum class Columns { POSITION, CAR_NUMBER, NAME, GAP, BEST, LAST, LICENSE, IRATING, CAR_BRAND, PIT, DELTA, L5, POSITIONS_GAINED, TIRE_COMPOUND };

    OverlayStandings()
        : Overlay("OverlayStandings")
    {
        m_avgL5Times.reserve(IR_MAX_CARS);

        for (int i = 0; i < IR_MAX_CARS; ++i) {
            m_avgL5Times.emplace_back();
            m_avgL5Times[i].reserve(5);

            for (int j = 0; j < 5; ++j) {
                m_avgL5Times[i].emplace_back(0.0f);
            }
        }

        this->m_carBrandIconsLoaded = false;
    }

    void setCarBrandIcons(const std::map<std::string, IWICFormatConverter*>& carBrandIconsMap, bool loaded)
    {
        // Drop any cached bitmaps (they are tied to this overlay's render target)
        m_carIdToIconMap.clear();
        m_brandConvToBitmap.clear();
        m_carBrandIconsMap = carBrandIconsMap;
        m_carBrandIconsLoaded = loaded;
    }

    std::string tireCompoundToString(int compound) const
    {
        switch (compound)
        {
        case 0: return "Dry";
        case 1: {
            // In some series, 1 maps to Wet (wet/dry only). In others, 1 is Primary.
            // Disambiguate using current weather conditions.
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

protected:

    virtual void onEnable()
    {
        onConfigChanged();  // trigger font load
        loadPositionIcons();
        loadFooterIcons();
    }

    virtual void onDisable()
    {
        m_text.reset();

        // Clear car brand bitmap caches on disable
        m_carIdToIconMap.clear();
        m_brandConvToBitmap.clear();

        // Release positions gained icons/factory
        releasePositionIcons();
        releaseFooterIcons();
    }

    virtual void onConfigChanged()
    {
        m_text.reset( m_dwriteFactory.Get() );

        // Centralized fonts
        createGlobalTextFormat(1.0f, m_textFormat);
        createGlobalTextFormat(0.8f, m_textFormatSmall);

        // Determine widths of text columns (use base font size from global settings)
        m_columns.reset();
        const float baseFontSize = g_cfg.getFloat("Overlay", "font_size", 16.0f);
        m_columns.add( (int)Columns::POSITION,   computeTextExtent( L"P99", m_dwriteFactory.Get(), m_textFormat.Get(), m_fontSpacing ).x, baseFontSize/2 );
        m_columns.add( (int)Columns::CAR_NUMBER, computeTextExtent( L"#999", m_dwriteFactory.Get(), m_textFormat.Get(), m_fontSpacing ).x, baseFontSize/2 );
        m_columns.add( (int)Columns::NAME,       0, baseFontSize/2 );

        if (g_cfg.getBool(m_name, "show_pit", true))
            m_columns.add( (int)Columns::PIT,        computeTextExtent( L"P.Age", m_dwriteFactory.Get(), m_textFormat.Get(), m_fontSpacing ).x, baseFontSize/2 );

        if (g_cfg.getBool(m_name, "show_license", true))
            m_columns.add( (int)Columns::LICENSE,    computeTextExtent( L"A 4.44", m_dwriteFactory.Get(), m_textFormatSmall.Get(), m_fontSpacing ).x, baseFontSize/6 );

        if (g_cfg.getBool(m_name, "show_irating", true))
            m_columns.add( (int)Columns::IRATING,    computeTextExtent( L" 9.9k ", m_dwriteFactory.Get(), m_textFormatSmall.Get(), m_fontSpacing ).x, baseFontSize/6 );

        if (g_cfg.getBool(m_name, "show_car_brand", true))
            m_columns.add( (int)Columns::CAR_BRAND,  30, baseFontSize / 2);

        if (g_cfg.getBool(m_name, "show_positions_gained", true))
        {
            const float posTextW = computeTextExtent( L"99", m_dwriteFactory.Get(), m_textFormat.Get(), m_fontSpacing ).x;
            // Add extra width for icon space
            m_columns.add( (int)Columns::POSITIONS_GAINED, posTextW + baseFontSize * 1.8f, baseFontSize / 2);
        }

        if (g_cfg.getBool(m_name, "show_tire_compound", false))
            m_columns.add( (int)Columns::TIRE_COMPOUND, computeTextExtent( L"Comp 00", m_dwriteFactory.Get(), m_textFormatSmall.Get(), m_fontSpacing ).x, baseFontSize / 2 );

        if (g_cfg.getBool(m_name, "show_gap", true))
            m_columns.add( (int)Columns::GAP,        computeTextExtent(L"999.9", m_dwriteFactory.Get(), m_textFormat.Get(), m_fontSpacing ).x, baseFontSize / 2 );

        if (g_cfg.getBool(m_name, "show_best", true))
            m_columns.add( (int)Columns::BEST,       computeTextExtent( L"99:99.999", m_dwriteFactory.Get(), m_textFormat.Get(), m_fontSpacing ).x, baseFontSize/2 );

        if (g_cfg.getBool(m_name, "show_lap_time", true))
            m_columns.add( (int)Columns::LAST,   computeTextExtent( L"99:99.999", m_dwriteFactory.Get(), m_textFormat.Get(), m_fontSpacing ).x, baseFontSize/2 );

        if (g_cfg.getBool(m_name, "show_delta", true))
            m_columns.add( (int)Columns::DELTA,  computeTextExtent( L"99.99", m_dwriteFactory.Get(), m_textFormat.Get(), m_fontSpacing ).x, baseFontSize/2 );

        if (g_cfg.getBool(m_name, "show_L5", true))
            m_columns.add( (int)Columns::L5,     computeTextExtent(L"99.99.999", m_dwriteFactory.Get(), m_textFormat.Get(), m_fontSpacing ).x, baseFontSize / 2 );
    }

    virtual void onUpdate()
    {
        if (!StubDataManager::shouldUseStubData() && !ir_hasValidDriver()) {
            return;
        }
        struct CarInfo {
            int     carIdx = 0;
            int     classIdx = 0;
            int     lapCount = 0;
            float   pctAroundLap = 0;
            int     lapGap = 0;
            float   gap = 0;
            float   delta = 0;
            int     position = 0;
            float   best = 0;
            float   last = 0;
            float   l5 = 0;
            bool    hasFastestLap = false;
            int     pitAge = 0;
            int     positionsChanged = 0;
            int     tireCompound = -1;
        };

        struct classBestLap {
            int     carIdx = -1;
            float   best = FLT_MAX;
        };

        std::vector<CarInfo> carInfo;
        carInfo.reserve( IR_MAX_CARS );
        
        // Use stub data in preview mode
        const bool useStubData = StubDataManager::shouldUseStubData();
        if (useStubData) {
            StubDataManager::populateSessionCars();
        }
        const int talkerCarIdx = (!useStubData && ir_RadioTransmitCarIdx.isValid()) ? ir_RadioTransmitCarIdx.getInt() : -1;
        
        // Apply global opacity to colors
        const float globalOpacity = getGlobalOpacity();

        // Init array
        std::map<int, classBestLap> bestLapClass;
        std::set<int> activeClasses;
        int selfPosition = ir_getPosition(ir_session.driverCarIdx);
        bool hasPacecar = false;
        
        if (useStubData) {
            const auto& stubCars = StubDataManager::getStubCars();
            
            for (size_t i = 0; i < stubCars.size(); ++i) {
                const auto& stubCar = stubCars[i];
                CarInfo ci;
                ci.carIdx = (int)i;
                ci.classIdx = stubCar.classId;
                ci.lapCount = stubCar.lapCount;
                ci.position = stubCar.position;
                ci.pctAroundLap = 0.1f + (i * 0.08f);
                ci.lapGap = stubCar.position > 1 ? -(stubCar.position - 1) : 0;
                ci.gap = stubCar.position == 1 ? 0.0f : (stubCar.position * 0.523f + 0.234f);
                ci.delta = stubCar.position == 1 ? 0.0f : (stubCar.position * 0.234f + 0.123f);
                ci.best = stubCar.bestLapTime;
                ci.last = stubCar.lastLapTime;
                ci.l5 = stubCar.bestLapTime + 0.2f;
                ci.pitAge = stubCar.pitAge;
                ci.hasFastestLap = (stubCar.bestLapTime < 84.4f);
                ci.positionsChanged = (int)((i % 3) - 1);
                ci.tireCompound = stubCar.tireCompound;

                activeClasses.insert(ci.classIdx);
                carInfo.push_back(ci);
            }
        } else {
        for( int i=0; i<IR_MAX_CARS; ++i )
        {
            const Car& car = ir_session.cars[i];

            if (car.isPaceCar || car.isSpectator || car.userName.empty()) {
                hasPacecar = true;
                continue;
            }

            CarInfo ci;
            ci.carIdx       = i;
            ci.lapCount     = std::max( ir_CarIdxLap.getInt(i), ir_CarIdxLapCompleted.getInt(i) );
            ci.position     = ir_getPosition(i);
            ci.pctAroundLap = ir_CarIdxLapDistPct.getFloat(i);
            ci.gap          = ir_session.sessionType!=SessionType::RACE ? 0 : -ir_CarIdxF2Time.getFloat(i);
            ci.last         = ir_CarIdxLastLapTime.getFloat(i);
            ci.pitAge       = ir_CarIdxLap.getInt(i) - car.lastLapInPits;
            ci.positionsChanged = ir_getPositionsChanged(i);
            ci.classIdx     = ir_getClassId(ci.carIdx);
            ci.tireCompound = ir_CarIdxTireCompound.isValid() ? ir_CarIdxTireCompound.getInt(i) : -1;
            if (ci.tireCompound < 0 && car.tireCompound >= 0)
                ci.tireCompound = car.tireCompound;

            ci.best         = ir_CarIdxBestLapTime.getFloat(i);
            if (ir_session.sessionType == SessionType::RACE && ir_SessionState.getInt() <= irsdk_StateWarmup || ir_session.sessionType == SessionType::QUALIFY && ci.best <= 0) {
                ci.best = car.qualy.fastestTime;
                for (int j = 0; j < 5; ++j) {
                    m_avgL5Times[ci.carIdx][j] = 0.0;
                }
            }
                
            if (ir_CarIdxTrackSurface.getInt(ci.carIdx) == irsdk_NotInWorld) {
                switch (ir_session.sessionType) {
                    case SessionType::QUALIFY:
                        ci.best = car.qualy.fastestTime;
                        ci.last = car.qualy.lastTime;
                        break;
                    case SessionType::PRACTICE:
                        ci.best = car.practice.fastestTime;
                        ci.last = car.practice.lastTime;
                        break;
                    case SessionType::RACE:
                        ci.best = car.race.fastestTime;
                        ci.last = car.race.lastTime;
                        break;
                    default:
                        break;
                }               
            }

            if (!bestLapClass.contains(ci.classIdx)) {
                classBestLap classBest;
                bestLapClass.insert_or_assign(ci.classIdx, classBest);
            }

            if( ci.best > 0 && ci.best < bestLapClass[ci.classIdx].best) {
                bestLapClass[ci.classIdx].best = ci.best;
                bestLapClass[ci.classIdx].carIdx = hasPacecar ? ci.carIdx - 1 : ci.carIdx;               
            }
            
            if(ci.lapCount > 0)
                m_avgL5Times[ci.carIdx][ci.lapCount % 5] = ci.last;

            float total = 0;
            int conteo = 0;
            for (float time : m_avgL5Times[ci.carIdx]) {
                if (time > 0.0) {
                    total += time;
                    conteo++;
                }
            }

            ci.l5 = conteo ? total / conteo : 0.0F;

            activeClasses.insert(ci.classIdx);
            carInfo.push_back(ci);
        }
        }

        for (const auto& pair : bestLapClass)
        {
            if (pair.second.best > 0 && pair.second.carIdx >= 0) {
                carInfo[pair.second.carIdx].hasFastestLap = true;
                std::string str = formatLaptime(pair.second.best);
            }
        }

        //const CarInfo ciSelf = carInfo[ir_PlayerCarIdx.getInt() > 0 ? hasPacecar ? ir_PlayerCarIdx.getInt() - 1 : ir_PlayerCarIdx.getInt() : 0];
        // Sometimes the offset is not necessary. In a free practice session it didn't need it, but in a qualifying it did
        const CarInfo ciSelf = carInfo[ir_session.driverCarIdx];
        
        // Sort by position
        std::sort( carInfo.begin(), carInfo.end(),
            []( const CarInfo& a, const CarInfo& b ) {
                const int ap = a.position<=0 ? INT_MAX : a.position;
                const int bp = b.position<=0 ? INT_MAX : b.position;
                return ap < bp;
            } );

        // Compute lap gap to leader and compute delta
        const bool isMultiClassSession = activeClasses.size() > 1;
        const bool showSingleClassHeader = g_cfg.getBool(m_name, "show_class_header_single", false);
        const bool useMultiClassLayout = isMultiClassSession || showSingleClassHeader;

        int classLeader = -1;
        int carsInClass = 0;
        float classLeaderGapToOverall = 0.0f;
        
        if (!useStubData) {
            for( int i=0; i<(int)carInfo.size(); ++i )
            {
                CarInfo&       ci       = carInfo[i];
                if (ci.classIdx != ciSelf.classIdx)
                    continue;

                carsInClass++;

                if (ci.position == 1) {
                    classLeader = ci.carIdx;
                    classLeaderGapToOverall = ci.gap;
                }

                ci.lapGap = ir_getLapDeltaToLeader( ci.carIdx, classLeader);
                ci.delta = ir_getDeltaTime( ci.carIdx, ir_session.driverCarIdx );

                if (ir_session.sessionType != SessionType::RACE) {
                    if(classLeader != -1) {
                        ci.gap -= classLeaderGapToOverall;
                        ci.gap = ci.gap < 0 ? 0 : ci.gap;
                    }
                    else {
                        ci.gap = 0;
                    }
                }
                else {
                    ci.gap -= classLeaderGapToOverall;
                }
            }
        } else {
            for( int i=0; i<(int)carInfo.size(); ++i )
            {
                CarInfo&       ci       = carInfo[i];
                if (ci.classIdx == ciSelf.classIdx) {
                    if (ci.position == 1) {
                        classLeader = ci.carIdx;
                    }
                }
            }
            for( int i=0; i<(int)carInfo.size(); ++i )
            {
                if (carInfo[i].classIdx == ciSelf.classIdx)
                    carsInClass++;
            }
        }

        const float  fontSize           = g_cfg.getFloat("Overlay", "font_size", 16.0f);
        const float  lineSpacing        = g_cfg.getFloat( m_name, "line_spacing", 8 );
        const float  lineHeight         = fontSize + lineSpacing;
        const float4 selfCol            = g_cfg.getFloat4( m_name, "self_col", float4(0.94f,0.67f,0.13f,1) );
        const float4 buddyCol           = g_cfg.getFloat4( m_name, "buddy_col", float4(0.2f,0.75f,0,1) );
        const float4 flaggedCol         = g_cfg.getFloat4( m_name, "flagged_col", float4(0.68f,0.42f,0.2f,1) );
        const float4 otherCarCol        = g_cfg.getFloat4( m_name, "other_car_col", float4(1,1,1,0.9f) );
        const float4 headerCol          = g_cfg.getFloat4( m_name, "header_col", float4(0.7f,0.7f,0.7f,0.9f) );
        const float4 carNumberTextCol   = g_cfg.getFloat4( m_name, "car_number_text_col", float4(0,0,0,0.9f) );
        const float4 alternateLineBgCol = g_cfg.getFloat4( m_name, "alternate_line_background_col", float4(0.5f,0.5f,0.5f,0.1f) );
        const float4 iratingTextCol     = g_cfg.getFloat4( m_name, "irating_text_col", float4(0,0,0,0.9f) );
        const float4 iratingBgCol       = g_cfg.getFloat4( m_name, "irating_background_col", float4(1,1,1,0.85f) );
        const float4 licenseTextCol     = g_cfg.getFloat4( m_name, "license_text_col", float4(1,1,1,0.9f) );
        const float4 fastestLapCol      = g_cfg.getFloat4( m_name, "fastest_lap_col", float4(1,0,1,1) );
        const float4 pitCol             = g_cfg.getFloat4( m_name, "pit_col", float4(0.94f,0.8f,0.13f,1) );
        const float4 deltaPosCol        = g_cfg.getFloat4( m_name, "delta_positive_col", float4(0.0f, 1.0f, 0.0f, 1.0f));
        const float4 deltaNegCol        = g_cfg.getFloat4( m_name, "delta_negative_col", float4(1.0f, 0.0f, 0.0f, 1.0f));
        const float  licenseBgAlpha     = g_cfg.getFloat( m_name, "license_background_alpha", 0.8f );
        int  numTopDrivers        = g_cfg.getInt(m_name, "num_top_drivers", defaultNumTopDrivers);
        int  numAheadDrivers      = g_cfg.getInt(m_name, "num_ahead_drivers", defaultNumAheadDrivers);
        int  numBehindDrivers     = g_cfg.getInt(m_name, "num_behind_drivers", defaultNumBehindDrivers);
        const bool   imperial           = isImperialUnits();

        const float xoff = 10.0f;
        const float yoff = 10;
        m_columns.layout( (float)m_width - 2*xoff );
        float y = yoff + lineHeight/2;
        const float ybottom = m_height - lineHeight * 1.5f;

        const ColumnLayout::Column* clm = nullptr;
        wchar_t s[512];
        std::string str;
        D2D1_RECT_F r = {};
        D2D1_ROUNDED_RECT rr = {};

        m_renderTarget->BeginDraw();

        // Header row above column labels: SoF (left) and Incidents (right) with divider
        {
            const float xMargin = 10.0f;
            const float yTop = 6.0f;
            const float fontSizeH = g_cfg.getFloat("Overlay", "font_size", 16.0f);
            const float iconSizeH = std::max(20.0f, fontSizeH * 1.2f);
            const float iconPadH = std::max(3.0f,  fontSizeH * 0.25f);
            const float itemH = iconSizeH + 2.0f;
            const float yCenter = yTop + itemH * 0.5f;

            // Divider under header (like footer divider)
            const float headerDividerY = yTop + itemH + 6.0f;
            m_brush->SetColor(float4(1,1,1,0.4f));
            m_renderTarget->DrawLine(float2(0, headerDividerY), float2((float)m_width, headerDividerY), m_brush.Get());

            // Left: SoF
            if (g_cfg.getBool(m_name, "show_SoF", true)) {
                int sof = ir_session.sof; if (sof < 0) sof = 0;
                std::wstring sofText = toWide(std::format("{}", sof));
                const float textW = computeTextExtent(sofText.c_str(), m_dwriteFactory.Get(), m_textFormatSmall.Get(), m_fontSpacing).x;
                const float minTextW = computeTextExtent(L"99999", m_dwriteFactory.Get(), m_textFormatSmall.Get(), m_fontSpacing).x;
                const float itemW = (m_iconSoF ? iconSizeH + iconPadH : 0.0f) + std::max(textW, minTextW) + 6.0f;
                float x = xMargin;
                D2D1_RECT_F bg = { x - 4.0f, yCenter - itemH * 0.5f, x + itemW - 4.0f, yCenter + itemH * 0.5f };
                D2D1_ROUNDED_RECT rrh = { bg, itemH * 0.5f, itemH * 0.5f };
                m_brush->SetColor(float4(1,1,1,1));
                m_renderTarget->FillRoundedRectangle(&rrh, m_brush.Get());
                if (m_iconSoF) {
                    D2D1_RECT_F ir = { x, yCenter - iconSizeH * 0.5f, x + iconSizeH, yCenter + iconSizeH * 0.5f };
                    m_renderTarget->DrawBitmap(m_iconSoF.Get(), &ir);
                    x += iconSizeH + iconPadH;
                }
                m_brush->SetColor(float4(0,0,0,1));
                m_text.render(m_renderTarget.Get(), sofText.c_str(), m_textFormatSmall.Get(), x, x + textW + 32.0f, yCenter, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing);
            }

            // Right: Incidents
            if (g_cfg.getBool(m_name, "show_incidents", true)) {
                const int inc = ir_PlayerCarTeamIncidentCount.getInt();
                const int lim = ir_session.incidentLimit;
                std::wstring incText = toWide(lim > 0 ? std::format("{}/{}", inc, lim) : std::format("{}/--", inc));
                const float textW = computeTextExtent(incText.c_str(), m_dwriteFactory.Get(), m_textFormatSmall.Get(), m_fontSpacing).x;
                const float itemW = (m_iconIncidents ? iconSizeH + iconPadH : 0.0f) + textW + 6.0f;
                float x = (float)m_width - xMargin - itemW;
                D2D1_RECT_F bg = { x - 4.0f, yCenter - itemH * 0.5f, x + itemW - 4.0f, yCenter + itemH * 0.5f };
                D2D1_ROUNDED_RECT rrh = { bg, itemH * 0.5f, itemH * 0.5f };
                m_brush->SetColor(float4(1,1,1,1));
                m_renderTarget->FillRoundedRectangle(&rrh, m_brush.Get());
                if (m_iconIncidents) {
                    D2D1_RECT_F ir = { x, yCenter - iconSizeH * 0.5f, x + iconSizeH, yCenter + iconSizeH * 0.5f };
                    m_renderTarget->DrawBitmap(m_iconIncidents.Get(), &ir);
                    x += iconSizeH + iconPadH;
                }
                m_brush->SetColor(float4(0,0,0,1));
                m_text.render(m_renderTarget.Get(), incText.c_str(), m_textFormatSmall.Get(), x, x + textW + 32.0f, yCenter, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing);
            }

            // Move y below header divider for column labels with extra top margin
            y = headerDividerY + 12.0f;
        }

        m_brush->SetColor( headerCol );

        // Headers
        clm = m_columns.get( (int)Columns::POSITION );
        swprintf( s, _countof(s), L"Po." );
        m_text.render( m_renderTarget.Get(), s, m_textFormat.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );

        clm = m_columns.get( (int)Columns::CAR_NUMBER );
        swprintf( s, _countof(s), L"No." );
        m_text.render( m_renderTarget.Get(), s, m_textFormat.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );

        clm = m_columns.get( (int)Columns::NAME );
        swprintf( s, _countof(s), L"Driver" );
        m_text.render( m_renderTarget.Get(), s, m_textFormat.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing );

        if (clm = m_columns.get( (int)Columns::PIT )) {
            swprintf( s, _countof(s), L"P.Age" );
            m_text.render( m_renderTarget.Get(), s, m_textFormat.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );
        }

        if (clm = m_columns.get( (int)Columns::LICENSE )) {
            swprintf( s, _countof(s), L"SR" );
            m_text.render( m_renderTarget.Get(), s, m_textFormat.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );
        }

        if (clm = m_columns.get( (int)Columns::IRATING )) {
            swprintf( s, _countof(s), L"IR" );
            m_text.render( m_renderTarget.Get(), s, m_textFormat.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );
        }

        if (clm = m_columns.get((int)Columns::CAR_BRAND)) {
            swprintf(s, _countof(s), L"  ");
            m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
        }

        if (clm = m_columns.get((int)Columns::POSITIONS_GAINED)) {
            swprintf(s, _countof(s), L"+/-");
            m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
        }

        if (clm = m_columns.get((int)Columns::GAP)) {
            swprintf(s, _countof(s), L"Gap");
            m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
        }

        if (clm = m_columns.get((int)Columns::BEST )) {
            swprintf( s, _countof(s), L"Best" );
            m_text.render( m_renderTarget.Get(), s, m_textFormat.Get(), xoff+clm->textL, xoff+clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing );
        }

        if (clm = m_columns.get((int)Columns::LAST ) ) {
            swprintf(s, _countof(s), L"Last");
            m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
        }

        if (clm = m_columns.get((int)Columns::DELTA)) {
            swprintf(s, _countof(s), L"Delta");
            m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
        }

        if (clm = m_columns.get((int)Columns::L5)) {
            swprintf(s, _countof(s), L"Last 5");
            m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
        }

        if (clm = m_columns.get((int)Columns::TIRE_COMPOUND)) {
            swprintf(s, _countof(s), L"Comp");
            m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
        }

        // Content
        const float contentStartY = y + lineHeight + 6.0f;

        if (useMultiClassLayout)
        {
            // Multi-class layout: stack one list per class, each with its own header row.
            struct ClassSummary
            {
                int                 classId = 0;
                std::wstring        name;
                int                 participants = 0;
                int                 sofSum = 0;
                int                 sofCount = 0;
                float               leaderBest = 0.0f;
                std::vector<int>    carIndices;     // indices into carInfo
            };

            std::vector<ClassSummary> classSummaries;
            std::map<int, int> classIdToIndex;

            // Build per-class aggregates
            for (int i = 0; i < (int)carInfo.size(); ++i)
            {
                const CarInfo& ci = carInfo[i];
                const Car&     car = ir_session.cars[ci.carIdx];
                const int      classId = ci.classIdx;

                int summaryIdx;
                auto it = classIdToIndex.find(classId);
                if (it == classIdToIndex.end())
                {
                    ClassSummary summary;
                    summary.classId = classId;

                    std::string classNameStr = car.carClassShortName;
                    if (classNameStr.empty())
                        classNameStr = std::format("Class {}", classId);

                    summary.name = toWide(classNameStr);
                    classSummaries.push_back(summary);
                    summaryIdx = (int)classSummaries.size() - 1;
                    classIdToIndex.emplace(classId, summaryIdx);
                }
                else
                {
                    summaryIdx = it->second;
                }

                ClassSummary& summary = classSummaries[summaryIdx];
                summary.participants++;
                if (car.irating > 0)
                {
                    summary.sofSum += car.irating;
                    summary.sofCount++;
                }
                if (ci.best > 0.0f && (summary.leaderBest <= 0.0f || ci.best < summary.leaderBest))
                {
                    summary.leaderBest = ci.best;
                }
                summary.carIndices.push_back(i);
            }

            // Finalize per-class data: average SoF and sort cars by class position
            for (auto& summary : classSummaries)
            {
                if (summary.sofCount > 0)
                    summary.sofSum = summary.sofSum / summary.sofCount; // reuse sofSum as averaged SoF

                std::sort(summary.carIndices.begin(), summary.carIndices.end(),
                    [&](int aIndex, int bIndex)
                    {
                        const CarInfo& a = carInfo[aIndex];
                        const CarInfo& b = carInfo[bIndex];
                        const int ap = a.position <= 0 ? INT_MAX : a.position;
                        const int bp = b.position <= 0 ? INT_MAX : b.position;
                        return ap < bp;
                    });
            }

            // Sort classes: self class first, then by leader lap time (fastest first)
            const int selfClassId = ciSelf.classIdx;
            std::sort(classSummaries.begin(), classSummaries.end(),
                [&](const ClassSummary& a, const ClassSummary& b)
                {
                    if (a.classId == selfClassId && b.classId != selfClassId) return true;
                    if (b.classId == selfClassId && a.classId != selfClassId) return false;
                    const float aBest = (a.leaderBest > 0.0f) ? a.leaderBest : FLT_MAX;
                    const float bBest = (b.leaderBest > 0.0f) ? b.leaderBest : FLT_MAX;
                    return aBest < bBest;
                });

            struct RenderRow
            {
                bool    isHeader = false;
                int     classSummaryIndex = -1;
                int     carInfoIndex = -1;    // -1 for header or spacer
                int     rowIndexInClass = 0;  // 1-based for driver rows, 0 for headers, -1 for spacer rows
            };

            std::vector<RenderRow> rows;
            rows.reserve(carInfo.size() + (int)classSummaries.size() * 2);

            // Build a flattened row list: [Header, drivers..., spacer] per class
            for (int c = 0; c < (int)classSummaries.size(); ++c)
            {
                const ClassSummary& summary = classSummaries[c];
                if (summary.participants <= 0)
                    continue;

                RenderRow header;
                header.isHeader = true;
                header.classSummaryIndex = c;
                rows.push_back(header);

                int rowIdxInClass = 0;
                for (int carIndex : summary.carIndices)
                {
                    RenderRow row;
                    row.isHeader = false;
                    row.classSummaryIndex = c;
                    row.carInfoIndex = carIndex;
                    row.rowIndexInClass = ++rowIdxInClass;
                    rows.push_back(row);
                }

                // Spacer row between classes (blank line)
                RenderRow spacer;
                spacer.isHeader = false;
                spacer.classSummaryIndex = c;
                spacer.carInfoIndex = -1;
                spacer.rowIndexInClass = -1;
                rows.push_back(spacer);
            }

            // Drop trailing spacer, if any
            while (!rows.empty() && rows.back().carInfoIndex == -1 && !rows.back().isHeader)
                rows.pop_back();

            const float availableH = ybottom - contentStartY;
            const int visibleRows = std::max(0, (int)(availableH / lineHeight));
            const int totalRows = (int)rows.size();

            m_maxScrollRow = std::max(0, totalRows - visibleRows);
            if (m_scrollRow > m_maxScrollRow) m_scrollRow = m_maxScrollRow;
            if (m_scrollRow < 0) m_scrollRow = 0;

            const int firstRow = std::min(std::max(0, m_scrollRow), std::max(0, totalRows));
            const int lastRow = std::min(totalRows, firstRow + visibleRows);

            int drawnRows = 0;
            float extraHeaderPadY = 4.0f;
            for (int ri = firstRow; ri < lastRow; ++ri)
            {
                const RenderRow& row = rows[ri];
                const float rowY = contentStartY + lineHeight * 0.5f + (float)drawnRows * lineHeight + extraHeaderPadY;
                ++drawnRows;

                // Stop when we run out of vertical space (accounts for extra header padding)
                if (rowY + lineHeight * 0.5f > ybottom)
                    break;

                if (row.isHeader)
                {
                    const ClassSummary& summary = classSummaries[row.classSummaryIndex];

                    // Header background tinted with class color (shared across overlays)
                    D2D1_RECT_F hr = { 0.0f, rowY - lineHeight / 2, (float)m_width, rowY + lineHeight / 2 };
                    float4 bgCol = ClassColors::get(summary.classId);
                    if (bgCol.a <= 0.0f) bgCol.a = 1.0f;
                    bgCol.a *= globalOpacity;
                    m_brush->SetColor(bgCol);
                    m_renderTarget->FillRectangle(&hr, m_brush.Get());

                    // Class name label with lighter variant of class color
                    {
                        const float4 pillBgCol = ClassColors::getLight(summary.classId);
                        const float  pillPadX  = 15.0f; 

                        const float textW = computeTextExtent(summary.name.c_str(), m_dwriteFactory.Get(),
                                                              m_textFormat.Get(), m_fontSpacing).x;
                        const float pillW = textW + pillPadX * 2.0f;
                        const float pillH = lineHeight; 

                        // Align pill with the left border of the overlay (x = 0)
                        D2D1_RECT_F nameRect = {
                            0.0f,
                            rowY - pillH * 0.5f,
                            pillW,
                            rowY + pillH * 0.5f
                        };

                        // Shape: square left edge + pill-shaped right edge
                        m_brush->SetColor(pillBgCol);
                        const float rCap = pillH * 0.5f; // pill radius on the right side

                        // If the label is too narrow for a pill cap, fall back to small rounded corners.
                        if (pillW <= (rCap * 2.0f + 1.0f))
                        {
                            rr.rect = nameRect;
                            rr.radiusX = 3.0f;
                            rr.radiusY = 3.0f;
                            m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
                        }
                        else
                        {
                            // Left/center rectangle (no rounding on the left edge, and no fill into the right rounding region)
                            D2D1_RECT_F baseRect = nameRect;
                            baseRect.right = nameRect.right - rCap;
                            m_renderTarget->FillRectangle(&baseRect, m_brush.Get());

                            // Right cap: a rounded rectangle of width 2*rCap, gives a pill-shaped right end
                            D2D1_ROUNDED_RECT capRR = {};
                            capRR.rect = { nameRect.right - 2.0f * rCap, nameRect.top, nameRect.right, nameRect.bottom };
                            capRR.radiusX = rCap;
                            capRR.radiusY = rCap;
                            m_renderTarget->FillRoundedRectangle(&capRR, m_brush.Get());
                        }

                        // Text centered within symmetric padding, using dark class color
                        m_brush->SetColor(ClassColors::getDark(summary.classId));
                        m_text.render(m_renderTarget.Get(), summary.name.c_str(), m_textFormat.Get(),
                                      nameRect.left + pillPadX, nameRect.right - pillPadX, rowY,
                                      m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
                    }

                    // SoF (center)
                    if (g_cfg.getBool(m_name, "show_SoF", true))
                    {
                        const int sof = summary.sofSum > 0 ? summary.sofSum : 0;
                        wchar_t sofBuf[64];
                        swprintf(sofBuf, _countof(sofBuf), L"SoF %d", sof);
                        m_brush->SetColor(float4(1, 1, 1, 1));
                        m_text.render(m_renderTarget.Get(), sofBuf, m_textFormatSmall.Get(),
                                      xoff + (float)m_width * 0.35f, xoff + (float)m_width * 0.7f,
                                      rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
                    }

                    // Participant count (right)
                    wchar_t cntBuf[64];
                    swprintf(cntBuf, _countof(cntBuf), L"%d cars", summary.participants);
                    m_brush->SetColor(float4(1, 1, 1, 1));
                    m_text.render(m_renderTarget.Get(), cntBuf, m_textFormatSmall.Get(),
                                  (float)m_width - 160.0f, (float)m_width - 10.0f,
                                  rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);

                    // Extra padding below the class header (small, not a whole extra row)
                    extraHeaderPadY += 4.0f;
                    continue;
                }

                // Spacer row: skip drawing
                if (row.carInfoIndex < 0 || row.rowIndexInClass <= 0)
                    continue;

                const CarInfo&  ci  = carInfo[row.carInfoIndex];
                const Car&      car = ir_session.cars[ci.carIdx];
                const bool      isTalking = (talkerCarIdx >= 0 && talkerCarIdx == ci.carIdx);

                // Alternating line backgrounds (within class)
                if ((row.rowIndexInClass & 1) && alternateLineBgCol.a > 0)
                {
                    D2D1_RECT_F br = { 0, rowY - lineHeight / 2, (float)m_width, rowY + lineHeight / 2 };
                    m_brush->SetColor(alternateLineBgCol);
                    m_renderTarget->FillRectangle(&br, m_brush.Get());
                }

                // Dim color if player is disconnected.
                const bool isGone = !car.isSelf && ir_CarIdxTrackSurface.getInt(ci.carIdx) == irsdk_NotInWorld;
                float4 textCol = car.isSelf ? selfCol : (car.isBuddy ? buddyCol : (car.isFlagged ? flaggedCol : otherCarCol));
                if (isGone)
                    textCol.a *= 0.5f;

                // Position
                if (ci.position > 0)
                {
                    clm = m_columns.get((int)Columns::POSITION);
                    // White label: square left edge + pill-shaped right edge (same as class-name label)
                    {
                        const float inset = 1.0f;
                        // Use full column width (text + borders) to make the white background wider
                        const float prL = xoff + (clm->textL - clm->borderL);
                        const float prR = xoff + (clm->textR + clm->borderR);
                        D2D1_RECT_F pr = { prL, rowY - lineHeight / 2, prR, rowY + lineHeight / 2 };
                        D2D1_RECT_F rrRect = { pr.left + inset, pr.top + inset, pr.right - inset, pr.bottom - inset };
                        const float h = rrRect.bottom - rrRect.top;
                        const float rCap = h * 0.5f;

                        // Fully opaque white (do not apply globalOpacity) to avoid seeing the shape beneath
                        float4 posBg = float4(1, 1, 1, 1);
                        m_brush->SetColor(posBg);

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
                    m_brush->SetColor(float4(0, 0, 0, 1));
                    swprintf(s, _countof(s), L"P%d", ci.position);
                    m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
                }

                // Car number
                {
                    clm = m_columns.get((int)Columns::CAR_NUMBER);
                    // While driver is transmitting on voice, replace the car-number badge with the push-to-talk icon.
                    if (isTalking && m_pushToTalkIcon)
                    {
                        const float cellL = xoff + clm->textL;
                        const float cellR = xoff + clm->textR;
                        const float cellW = cellR - cellL;
                        const float iconSize = std::max(0.0f, std::min(lineHeight - 6.0f, cellW));
                        const float cx = (cellL + cellR) * 0.5f;
                        D2D1_RECT_F ir = { cx - iconSize * 0.5f, rowY - iconSize * 0.5f, cx + iconSize * 0.5f, rowY + iconSize * 0.5f };
                        m_renderTarget->DrawBitmap(m_pushToTalkIcon.Get(), &ir);
                    }
                    else
                    {
                        swprintf(s, _countof(s), L"#%S", car.carNumberStr.c_str());
                        r = { xoff + clm->textL, rowY - lineHeight / 2, xoff + clm->textR, rowY + lineHeight / 2 };
                        rr.rect = { r.left - 2, r.top + 1, r.right + 2, r.bottom - 1 };
                        rr.radiusX = 3;
                        rr.radiusY = 3;
                        // Use class base color for number background (matches header base color)
                        float4 numBg = ClassColors::get(ci.classIdx);
                        numBg.a *= globalOpacity;
                        if (isGone) numBg.a *= 0.5f;
                        m_brush->SetColor(numBg);
                        m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
                        // Left accent strip using class light color
                        {
                            float4 stripCol = ClassColors::getLight(ci.classIdx);
                            stripCol.a = numBg.a;
                            m_brush->SetColor(stripCol);
                            const float stripW = 3.0f;
                            D2D1_RECT_F strip = { rr.rect.left + 1.0f, rr.rect.top + 1.0f, rr.rect.left + 1.0f + stripW, rr.rect.bottom - 1.0f };
                            m_renderTarget->FillRectangle(&strip, m_brush.Get());
                        }
                        m_brush->SetColor(float4(1, 1, 1, 1));
                        m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
                    }
                }

                // Name
                {
                    clm = m_columns.get((int)Columns::NAME);
                    m_brush->SetColor(textCol);
                    std::string displayName = car.teamName;
                    if (!g_cfg.getBool(m_name, "show_full_name", true)) {
                        // Show only first name
                        size_t spacePos = displayName.find(' ');
                        if (spacePos != std::string::npos) {
                            displayName = displayName.substr(0, spacePos);
                        }
                    }
                    swprintf(s, _countof(s), L"%S", displayName.c_str());
                    m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing);
                }

                // Pit age
                if (!ir_isPreStart() && (ci.pitAge >= 0 || ir_CarIdxOnPitRoad.getBool(ci.carIdx)))
                {
                    if (clm = m_columns.get((int)Columns::PIT)) {
                        m_brush->SetColor(pitCol);
                        swprintf(s, _countof(s), L"%d", ci.pitAge);
                        r = { xoff + clm->textL, rowY - lineHeight / 2 + 2, xoff + clm->textR, rowY + lineHeight / 2 - 2 };
                        if (ir_CarIdxOnPitRoad.getBool(ci.carIdx)) {
                            swprintf(s, _countof(s), L"PIT");
                            m_renderTarget->FillRectangle(&r, m_brush.Get());
                            m_brush->SetColor(float4(0, 0, 0, 1));
                        }
                        else {
                            swprintf(s, _countof(s), L"%d", ci.pitAge);
                            m_renderTarget->DrawRectangle(&r, m_brush.Get());
                        }
                        m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), xoff + clm->textL, xoff + clm->textR, rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
                    }
                }

                // License/SR
                if (clm = m_columns.get((int)Columns::LICENSE)) {
                    swprintf(s, _countof(s), L"%C %.1f", car.licenseChar, car.licenseSR);
                    r = { xoff + clm->textL, rowY - lineHeight / 2, xoff + clm->textR, rowY + lineHeight / 2 };
                    rr.rect = { r.left + 1, r.top + 1, r.right - 1, r.bottom - 1 };
                    rr.radiusX = 3;
                    rr.radiusY = 3;
                    float4 c = car.licenseCol;
                    c.a = licenseBgAlpha;
                    m_brush->SetColor(c);
                    m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
                    m_brush->SetColor(licenseTextCol);
                    m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), xoff + clm->textL, xoff + clm->textR, rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
                }

                // Irating
                if (clm = m_columns.get((int)Columns::IRATING)) {
                    swprintf(s, _countof(s), L"%.1fk", (float)car.irating / 1000.0f);
                    r = { xoff + clm->textL, rowY - lineHeight / 2, xoff + clm->textR, rowY + lineHeight / 2 };
                    rr.rect = { r.left + 1, r.top + 1, r.right - 1, r.bottom - 1 };
                    rr.radiusX = 3;
                    rr.radiusY = 3;
                    m_brush->SetColor(iratingBgCol);
                    m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
                    m_brush->SetColor(iratingTextCol);
                    m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), xoff + clm->textL, xoff + clm->textR, rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
                }

                // Car brand
                if ((clm = m_columns.get((int)Columns::CAR_BRAND)) && m_carBrandIconsLoaded)
                {
                    if (m_carIdToIconMap.find(car.carID) == m_carIdToIconMap.end())
                    {
                        // Cache by *brand converter* first (dedupe across different carIDs using same manufacturer icon),
                        // then map carID -> bitmap for fast lookup during rendering.
                        IWICFormatConverter* conv = findCarBrandIcon(car.carName, m_carBrandIconsMap);
                        if (conv)
                        {
                            auto& brandBmp = m_brandConvToBitmap[conv];
                            if (!brandBmp)
                            {
                                (void)m_renderTarget->CreateBitmapFromWicBitmap(conv, nullptr, &brandBmp);
                            }
                            if (brandBmp)
                            {
                                m_carIdToIconMap[car.carID] = brandBmp;
                            }
                        }
                    }

                    const auto itBmp = m_carIdToIconMap.find(car.carID);
                    if (itBmp != m_carIdToIconMap.end() && itBmp->second) {
                        D2D1_RECT_F br = { xoff + clm->textL, rowY - lineHeight / 2, xoff + clm->textL + lineHeight, rowY + lineHeight / 2 };
                        m_renderTarget->DrawBitmap(itBmp->second.Get(), br);
                    }
                }

                // Positions gained
                if (clm = m_columns.get((int)Columns::POSITIONS_GAINED)) {
                    r = { xoff + clm->textL, rowY - lineHeight / 2, xoff + clm->textR, rowY + lineHeight / 2 };
                    rr.rect = { r.left + 1, r.top + 1, r.right - 1, r.bottom - 1 };
                    rr.radiusX = 3;
                    rr.radiusY = 3;
                    m_brush->SetColor(float4(1, 1, 1, 1));
                    m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());

                    const int delta = ci.positionsChanged;
                    ID2D1Bitmap* icon = nullptr;
                    if (delta > 0)      icon = m_posUpIcon.Get();
                    else if (delta < 0) icon = m_posDownIcon.Get();
                    else                icon = m_posEqualIcon.Get();

                    const float iconPad = 4.0f;
                    const float iconSize = std::max(0.0f, lineHeight - 6.0f);
                    if (icon) {
                        D2D1_RECT_F ir = { r.left + iconPad, rowY - iconSize * 0.5f, r.left + iconPad + iconSize, rowY + iconSize * 0.5f };
                        m_renderTarget->DrawBitmap(icon, &ir);
                    }

                    m_brush->SetColor(float4(0, 0, 0, 1));
                    swprintf(s, _countof(s), L"%d", abs(delta));
                    const float textL = r.left + iconPad + (icon ? iconSize + 2.0f : 0.0f);
                    m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), textL, r.right - 15.0f, rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
                }

                // Tire compound
                if (clm = m_columns.get((int)Columns::TIRE_COMPOUND))
                {
                    int compound = ci.tireCompound;
                    if (compound < 0 && car.tireCompound >= 0)
                        compound = car.tireCompound;
                    if (compound < 0 && ir_CarIdxTireCompound.isValid())
                        compound = ir_CarIdxTireCompound.getInt(ci.carIdx);
                    const std::string compoundText = tireCompoundToString(compound);
                    m_brush->SetColor(textCol);
                    m_text.render(m_renderTarget.Get(), toWide(compoundText).c_str(), m_textFormatSmall.Get(), xoff + clm->textL, xoff + clm->textR, rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
                }

                // Gap
                if (ci.lapGap || ci.gap)
                {
                    if (clm = m_columns.get((int)Columns::GAP)) {
                        if (ci.lapGap < 0)
                            swprintf(s, _countof(s), L"%d L", ci.lapGap);
                        else
                            swprintf(s, _countof(s), L"%.01f", ci.gap);
                        m_brush->SetColor(textCol);
                        m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
                    }
                }

                // Best
                if (clm = m_columns.get((int)Columns::BEST)) {
                    str.clear();
                    if (ci.best > 0)
                        str = formatLaptime(ci.best);
                    m_brush->SetColor(ci.hasFastestLap ? fastestLapCol : textCol);
                    m_text.render(m_renderTarget.Get(), toWide(str).c_str(), m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
                }

                // Last
                if (clm = m_columns.get((int)Columns::LAST))
                {
                    str.clear();
                    if (ci.last > 0)
                        str = formatLaptime(ci.last);
                    m_brush->SetColor(textCol);
                    m_text.render(m_renderTarget.Get(), toWide(str).c_str(), m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
                }

                // Delta
                if (clm = m_columns.get((int)Columns::DELTA))
                {
                    if (ci.delta)
                    {
                        swprintf(s, _countof(s), L"%.01f", abs(ci.delta));
                        if (ci.delta > 0)
                            m_brush->SetColor(deltaPosCol);
                        else
                            m_brush->SetColor(deltaNegCol);
                        m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
                    }
                }

                // Average 5 laps
                if (clm = m_columns.get((int)Columns::L5))
                {
                    str.clear();
                    if (ci.l5 > 0 && selfPosition > 0) {
                        str = formatLaptime(ci.l5);
                        if (ci.l5 >= ciSelf.l5)
                            m_brush->SetColor(deltaPosCol);
                        else
                            m_brush->SetColor(deltaNegCol);
                    }
                    else
                        m_brush->SetColor(textCol);

                    m_text.render(m_renderTarget.Get(), toWide(str).c_str(), m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, rowY, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
                }
            }

            // Scrollbar for multi-class layout
            if (totalRows > visibleRows && visibleRows > 0)
            {
                const float trackLeft  = (float)m_width - 6.0f;
                const float trackRight = (float)m_width - 3.0f;
                const float trackTop   = 2 * yoff + lineHeight;
                const float trackBot   = ybottom;
                const float trackH     = std::max(0.0f, trackBot - trackTop);
                const float ratio      = (float)visibleRows / (float)totalRows;
                const float thumbH     = std::max(12.0f, trackH * ratio);
                const float maxThumbTravel = std::max(0.0f, trackH - thumbH);
                const float scrollRatio = (m_maxScrollRow > 0) ? ((float)m_scrollRow / (float)m_maxScrollRow) : 0.0f;
                const float thumbTop  = trackTop + maxThumbTravel * scrollRatio;
                const float thumbBot  = thumbTop + thumbH;

                float4 trackCol = headerCol; trackCol.a *= 0.20f * globalOpacity;
                float4 thumbCol = headerCol; thumbCol.a *= 0.45f * globalOpacity;

                D2D1_RECT_F track = { trackLeft, trackTop, trackRight, trackBot };
                D2D1_RECT_F thumb = { trackLeft, thumbTop, trackRight, thumbBot };
                m_brush->SetColor(trackCol);
                m_renderTarget->FillRectangle(&track, m_brush.Get());
                m_brush->SetColor(thumbCol);
                m_renderTarget->FillRectangle(&thumb, m_brush.Get());
            }
        }
        else
        {
            // Original single-class layout (only the driver's class)
            int carsToDraw = static_cast<int>((ybottom - contentStartY) / lineHeight) - 1;
            int carsToSkip;
            if (carsToDraw >= carsInClass) {
                numTopDrivers = carsToDraw;
                carsToSkip = 0;
            }
            else {
                // cars to add ahead = total cars - position
                numAheadDrivers += std::max((ciSelf.position - carsInClass + numBehindDrivers), 0);
                numBehindDrivers -= std::min(std::max((ciSelf.position - carsInClass + numBehindDrivers), 0), 2);
                numTopDrivers += std::max(carsToDraw - (numTopDrivers + numAheadDrivers + numBehindDrivers + 2), 0);
                numBehindDrivers += std::max(carsToDraw - (ciSelf.position + numBehindDrivers), 0);

                if (ciSelf.position < numTopDrivers + numAheadDrivers) {
                    carsToSkip = 0;
                }
                else if (ciSelf.position > carsInClass - numBehindDrivers) {
                    carsToSkip = carsInClass - numTopDrivers - numBehindDrivers - numAheadDrivers - 1;
                }
                else carsToSkip = 0;
            }
            // Compute scroll limits and clamp current scroll position
            m_maxScrollRow = std::max(0, carsInClass - carsToDraw);
            if (m_scrollRow > m_maxScrollRow) m_scrollRow = m_maxScrollRow;
            // Apply scroll offset to the number of cars to skip for rendering
            {
                const int maxSkip = std::max(0, carsInClass - carsToDraw);
                carsToSkip = std::clamp(carsToSkip + m_scrollRow, 0, maxSkip);
            }
            int drawnCars = 0;
            int ownClass = useStubData ? ciSelf.classIdx : ir_PlayerCarClass.getInt();
            int selfClassDrivers = 0;
            bool skippedCars = false;
            int numSkippedCars = 0;
            for (int i = 0; i < (int)carInfo.size(); ++i)
            {
                if (drawnCars > carsToDraw) break;

                y = contentStartY + lineHeight / 2 + drawnCars * lineHeight;

                if (carInfo[i].classIdx != ownClass) {
                    continue;
                }

                selfClassDrivers++;

                // Apply scroll offset: skip the first 'carsToSkip' rows in-class
                if (selfClassDrivers <= carsToSkip) {
                    continue;
                }

                if (y + lineHeight / 2 > ybottom)
                    break;

                // Focus on the driver
                if (selfPosition > 0 && selfClassDrivers > numTopDrivers) {

                    if (selfClassDrivers > carsToSkip && selfClassDrivers < selfPosition - numAheadDrivers) {
                        if (!skippedCars) {
                            skippedCars = true;
                            drawnCars++;
                        }
                        continue;
                    }
                }

                drawnCars++;

                // Alternating line backgrounds
                if (selfClassDrivers & 1 && alternateLineBgCol.a > 0)
                {
                    D2D1_RECT_F r = { 0, y - lineHeight / 2, (float)m_width,  y + lineHeight / 2 };
                    m_brush->SetColor(alternateLineBgCol);
                    m_renderTarget->FillRectangle(&r, m_brush.Get());
                }

                const CarInfo&  ci = carInfo[i];
                const Car&      car = ir_session.cars[ci.carIdx];
                const bool      isTalking = (talkerCarIdx >= 0 && talkerCarIdx == ci.carIdx);

                // Dim color if player is disconnected.
                const bool isGone = !car.isSelf && ir_CarIdxTrackSurface.getInt(ci.carIdx) == irsdk_NotInWorld;
                float4 textCol = car.isSelf ? selfCol : (car.isBuddy ? buddyCol : (car.isFlagged ? flaggedCol : otherCarCol));
                if (isGone)
                    textCol.a *= 0.5f;

                // Position
                if (ci.position > 0)
                {
                    clm = m_columns.get((int)Columns::POSITION);
                    // White label: square left edge + pill-shaped right edge (same as class-name label)
                    {
                        const float inset = 1.0f;
                        // Use full column width (text + borders) to make the white background wider
                        const float prL = xoff + (clm->textL - clm->borderL);
                        const float prR = xoff + (clm->textR + clm->borderR);
                        D2D1_RECT_F pr = { prL, y - lineHeight / 2, prR, y + lineHeight / 2 };
                        D2D1_RECT_F rrRect = { pr.left + inset, pr.top + inset, pr.right - inset, pr.bottom - inset };
                        const float h = rrRect.bottom - rrRect.top;
                        const float rCap = h * 0.5f;

                        // Fully opaque white (do not apply globalOpacity) to avoid seeing the shape beneath
                        float4 posBg = float4(1, 1, 1, 1);
                        m_brush->SetColor(posBg);

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
                    m_brush->SetColor(float4(0, 0, 0, 1));
                    swprintf(s, _countof(s), L"P%d", ci.position);
                    m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
                }

                // Car number
                {
                    clm = m_columns.get((int)Columns::CAR_NUMBER);
                    // While driver is transmitting on voice, replace the car-number badge with the push-to-talk icon.
                    if (isTalking && m_pushToTalkIcon)
                    {
                        const float cellL = xoff + clm->textL;
                        const float cellR = xoff + clm->textR;
                        const float cellW = cellR - cellL;
                        const float iconSize = std::max(0.0f, std::min(lineHeight - 6.0f, cellW));
                        const float cx = (cellL + cellR) * 0.5f;
                        D2D1_RECT_F ir = { cx - iconSize * 0.5f, y - iconSize * 0.5f, cx + iconSize * 0.5f, y + iconSize * 0.5f };
                        m_renderTarget->DrawBitmap(m_pushToTalkIcon.Get(), &ir);
                    }
                    else
                    {
                        swprintf(s, _countof(s), L"#%S", car.carNumberStr.c_str());
                        r = { xoff + clm->textL, y - lineHeight / 2, xoff + clm->textR, y + lineHeight / 2 };
                        rr.rect = { r.left - 2, r.top + 1, r.right + 2, r.bottom - 1 };
                        rr.radiusX = 3;
                        rr.radiusY = 3;
                        // Use class base color for number background (matches header base color)
                        float4 numBg = ClassColors::get(ci.classIdx);
                        numBg.a *= globalOpacity;
                        if (isGone) numBg.a *= 0.5f;
                        m_brush->SetColor(numBg);
                        m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
                        // Left accent strip using class light color
                        {
                            float4 stripCol = ClassColors::getLight(ci.classIdx);
                            stripCol.a = numBg.a;
                            m_brush->SetColor(stripCol);
                            const float stripW = 3.0f;
                            D2D1_RECT_F strip = { rr.rect.left + 1.0f, rr.rect.top + 1.0f, rr.rect.left + 1.0f + stripW, rr.rect.bottom - 1.0f };
                            m_renderTarget->FillRectangle(&strip, m_brush.Get());
                        }
                        m_brush->SetColor(float4(1, 1, 1, 1));
                        m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
                    }
                }

                // Name
                {
                    clm = m_columns.get((int)Columns::NAME);
                    m_brush->SetColor(textCol);
                    std::string displayName = car.teamName;
                    if (!g_cfg.getBool(m_name, "show_full_name", true)) {
                        // Show only first name
                        size_t spacePos = displayName.find(' ');
                        if (spacePos != std::string::npos) {
                            displayName = displayName.substr(0, spacePos);
                        }
                    }
                    swprintf(s, _countof(s), L"%S", displayName.c_str());
                    m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing);
                }

                // Pit age
                if (!ir_isPreStart() && (ci.pitAge >= 0 || ir_CarIdxOnPitRoad.getBool(ci.carIdx)))
                {
                    if (clm = m_columns.get((int)Columns::PIT)) {
                        m_brush->SetColor(pitCol);
                        swprintf(s, _countof(s), L"%d", ci.pitAge);
                        r = { xoff + clm->textL, y - lineHeight / 2 + 2, xoff + clm->textR, y + lineHeight / 2 - 2 };
                        if (ir_CarIdxOnPitRoad.getBool(ci.carIdx)) {
                            swprintf(s, _countof(s), L"PIT");
                            m_renderTarget->FillRectangle(&r, m_brush.Get());
                            m_brush->SetColor(float4(0, 0, 0, 1));
                        }
                        else {
                            swprintf(s, _countof(s), L"%d", ci.pitAge);
                            m_renderTarget->DrawRectangle(&r, m_brush.Get());
                        }
                        m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
                    }
                }

                // License/SR
                if (clm = m_columns.get((int)Columns::LICENSE)) {
                    swprintf(s, _countof(s), L"%C %.1f", car.licenseChar, car.licenseSR);
                    r = { xoff + clm->textL, y - lineHeight / 2, xoff + clm->textR, y + lineHeight / 2 };
                    rr.rect = { r.left + 1, r.top + 1, r.right - 1, r.bottom - 1 };
                    rr.radiusX = 3;
                    rr.radiusY = 3;
                    float4 c = car.licenseCol;
                    c.a = licenseBgAlpha;
                    m_brush->SetColor(c);
                    m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
                    m_brush->SetColor(licenseTextCol);
                    m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
                }

                // Irating
                if (clm = m_columns.get((int)Columns::IRATING)) {
                    swprintf(s, _countof(s), L"%.1fk", (float)car.irating / 1000.0f);
                    r = { xoff + clm->textL, y - lineHeight / 2, xoff + clm->textR, y + lineHeight / 2 };
                    rr.rect = { r.left + 1, r.top + 1, r.right - 1, r.bottom - 1 };
                    rr.radiusX = 3;
                    rr.radiusY = 3;
                    m_brush->SetColor(iratingBgCol);
                    m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
                    m_brush->SetColor(iratingTextCol);
                    m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
                }

                // Car brand
                if ((clm = m_columns.get((int)Columns::CAR_BRAND)) && m_carBrandIconsLoaded)
                {
                    if (m_carIdToIconMap.find(car.carID) == m_carIdToIconMap.end())
                    {
                        IWICFormatConverter* conv = findCarBrandIcon(car.carName, m_carBrandIconsMap);
                        if (conv)
                        {
                            auto& brandBmp = m_brandConvToBitmap[conv];
                            if (!brandBmp)
                            {
                                (void)m_renderTarget->CreateBitmapFromWicBitmap(conv, nullptr, &brandBmp);
                            }
                            if (brandBmp)
                            {
                                m_carIdToIconMap[car.carID] = brandBmp;
                            }
                        }
                    }

                    const auto itBmp = m_carIdToIconMap.find(car.carID);
                    if (itBmp != m_carIdToIconMap.end() && itBmp->second) {
                        D2D1_RECT_F br = { xoff + clm->textL, y - lineHeight / 2, xoff + clm->textL + lineHeight, y + lineHeight / 2 };
                        m_renderTarget->DrawBitmap(itBmp->second.Get(), br);
                    }
                }

                // Positions gained
                if (clm = m_columns.get((int)Columns::POSITIONS_GAINED)) {
                    r = { xoff + clm->textL, y - lineHeight / 2, xoff + clm->textR, y + lineHeight / 2 };
                    rr.rect = { r.left + 1, r.top + 1, r.right - 1, r.bottom - 1 };
                    rr.radiusX = 3;
                    rr.radiusY = 3;
                    m_brush->SetColor(float4(1, 1, 1, 1));
                    m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());

                    const int delta = ci.positionsChanged;
                    ID2D1Bitmap* icon = nullptr;
                    if (delta > 0)      icon = m_posUpIcon.Get();
                    else if (delta < 0) icon = m_posDownIcon.Get();
                    else                icon = m_posEqualIcon.Get();

                    const float iconPad = 4.0f;
                    const float iconSize = std::max(0.0f, lineHeight - 6.0f);
                    if (icon) {
                        D2D1_RECT_F ir = { r.left + iconPad, y - iconSize * 0.5f, r.left + iconPad + iconSize, y + iconSize * 0.5f };
                        m_renderTarget->DrawBitmap(icon, &ir);
                    }

                    m_brush->SetColor(float4(0, 0, 0, 1));
                    swprintf(s, _countof(s), L"%d", abs(delta));
                    const float textL = r.left + iconPad + (icon ? iconSize + 2.0f : 0.0f);
                    m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), textL, r.right - 15.0f, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
                }

                // Tire compound
                if (clm = m_columns.get((int)Columns::TIRE_COMPOUND))
                {
                    int compound = ci.tireCompound;
                    if (compound < 0 && car.tireCompound >= 0)
                        compound = car.tireCompound;
                    if (compound < 0 && ir_CarIdxTireCompound.isValid())
                        compound = ir_CarIdxTireCompound.getInt(ci.carIdx);
                    const std::string compoundText = tireCompoundToString(compound);
                    m_brush->SetColor(textCol);
                    m_text.render(m_renderTarget.Get(), toWide(compoundText).c_str(), m_textFormatSmall.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
                }

                // Gap
                if (ci.lapGap || ci.gap)
                {
                    if (clm = m_columns.get((int)Columns::GAP)) {
                        if (ci.lapGap < 0)
                            swprintf(s, _countof(s), L"%d L", ci.lapGap);
                        else
                            swprintf(s, _countof(s), L"%.01f", ci.gap);
                        m_brush->SetColor(textCol);
                        m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING);
                    }
                }

                // Best
                if (clm = m_columns.get((int)Columns::BEST)) {
                    str.clear();
                    if (ci.best > 0)
                        str = formatLaptime(ci.best);
                    m_brush->SetColor(ci.hasFastestLap ? fastestLapCol : textCol);
                    m_text.render(m_renderTarget.Get(), toWide(str).c_str(), m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
                }

                // Last
                if (clm = m_columns.get((int)Columns::LAST))
                {
                    str.clear();
                    if (ci.last > 0)
                        str = formatLaptime(ci.last);
                    m_brush->SetColor(textCol);
                    m_text.render(m_renderTarget.Get(), toWide(str).c_str(), m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
                }

                // Delta
                if (clm = m_columns.get((int)Columns::DELTA))
                {
                    if (ci.delta)
                    {
                        swprintf(s, _countof(s), L"%.01f", abs(ci.delta));
                        if (ci.delta > 0)
                            m_brush->SetColor(deltaPosCol);
                        else
                            m_brush->SetColor(deltaNegCol);
                        m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
                    }
                }

                // Average 5 laps
                if (clm = m_columns.get((int)Columns::L5))
                {
                    str.clear();
                    if (ci.l5 > 0 && selfPosition > 0) {
                        str = formatLaptime(ci.l5);
                        if (ci.l5 >= ciSelf.l5)
                            m_brush->SetColor(deltaPosCol);
                        else
                            m_brush->SetColor(deltaNegCol);
                    }
                    else
                        m_brush->SetColor(textCol);

                    m_text.render(m_renderTarget.Get(), toWide(str).c_str(), m_textFormat.Get(), xoff + clm->textL, xoff + clm->textR, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
                }
            }

            // Scrollbar as before for single-class layout
            const int totalRows = carsInClass;
            const int visibleRows = carsToDraw;

            if (totalRows > visibleRows && visibleRows > 0)
            {
                const float trackLeft = (float)m_width - 6.0f;
                const float trackRight = (float)m_width - 3.0f;
                const float trackTop = 2 * yoff + lineHeight;
                const float trackBot = ybottom;
                const float trackH = std::max(0.0f, trackBot - trackTop);
                const float ratio = (float)visibleRows / (float)totalRows;
                const float thumbH = std::max(12.0f, trackH * ratio);
                const float maxThumbTravel = std::max(0.0f, trackH - thumbH);
                const float scrollRatio = (m_maxScrollRow > 0) ? ((float)m_scrollRow / (float)m_maxScrollRow) : 0.0f;
                const float thumbTop = trackTop + maxThumbTravel * scrollRatio;
                const float thumbBot = thumbTop + thumbH;

                float4 trackCol = headerCol; trackCol.a *= 0.20f * globalOpacity;
                float4 thumbCol = headerCol; thumbCol.a *= 0.45f * globalOpacity;

                D2D1_RECT_F track = { trackLeft, trackTop, trackRight, trackBot };
                D2D1_RECT_F thumb = { trackLeft, thumbTop, trackRight, thumbBot };
                m_brush->SetColor(trackCol);
                m_renderTarget->FillRectangle(&track, m_brush.Get());
                m_brush->SetColor(thumbCol);
                m_renderTarget->FillRectangle(&thumb, m_brush.Get());
            }
        }
        
        // Footer: left (session time), right (track temp + laps)
        {
            float trackTemp = ir_TrackTempCrew.getFloat();
            char  tempUnit  = 'C';

            if( imperial ) {
                trackTemp = celsiusToFahrenheit( trackTemp );
                tempUnit  = 'F';
            }

            int hours, mins, secs;
            ir_getSessionTimeRemaining(hours, mins, secs);
            const int laps = std::max(ir_CarIdxLap.getInt(ir_session.driverCarIdx), ir_CarIdxLapCompleted.getInt(ir_session.driverCarIdx));
            const int remainingLaps = ir_getLapsRemaining();
            const int irTotalLaps = ir_SessionLapsTotal.getInt();
            const int totalLaps = (irTotalLaps == 32767) ? (laps + remainingLaps) : irTotalLaps;

            // Divider line
            m_brush->SetColor(float4(1,1,1,0.4f));
            m_renderTarget->DrawLine( float2(0,ybottom),float2((float)m_width,ybottom),m_brush.Get() );

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

        m_renderTarget->EndDraw();
    }

    virtual void onMouseWheel( int delta, int /*x*/, int /*y*/ ) override
    {
        // delta is typically +1 or -1 from the caller (Overlay::WndProc)
        if (m_maxScrollRow <= 0)
            return;
        // Invert so positive wheel (towards user) scrolls down the list
        m_scrollRow -= delta;
        if (m_scrollRow < 0) m_scrollRow = 0;
        if (m_scrollRow > m_maxScrollRow) m_scrollRow = m_maxScrollRow;
    }

    virtual bool canEnableWhileNotDriving() const
    {
        return true;
    }

protected:

    // Loads position change icons (up/down/equal) once
    void loadPositionIcons()
    {
        if (!m_renderTarget.Get()) return;
        if (m_posUpIcon || m_posDownIcon || m_posEqualIcon || m_pushToTalkIcon) return;

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
        m_pushToTalkIcon = loadPng(L"assets\\icons\\pushtotalk.png");
    }

    void releasePositionIcons()
    {
        m_posUpIcon.Reset();
        m_posDownIcon.Reset();
        m_posEqualIcon.Reset();
        m_pushToTalkIcon.Reset();
        m_wicFactory.Reset();
    }

    // Footer icons
    void loadFooterIcons()
    {
        if (!m_renderTarget.Get()) return;
        if (m_iconIncidents || m_iconSoF || m_iconTrackTemp || m_iconSessionTime || m_iconLaps) return;
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

    Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormatSmall;
    std::vector<std::vector<float>> m_avgL5Times;
    bool m_carBrandIconsLoaded;
    std::map<std::string, IWICFormatConverter*> m_carBrandIconsMap;
    std::map<IWICFormatConverter*, Microsoft::WRL::ComPtr<ID2D1Bitmap>> m_brandConvToBitmap;
    std::map<int, Microsoft::WRL::ComPtr<ID2D1Bitmap>> m_carIdToIconMap;
    std::set<std::string> notFoundBrands;

    // Position change icons
    Microsoft::WRL::ComPtr<IWICImagingFactory> m_wicFactory;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> m_posUpIcon;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> m_posDownIcon;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> m_posEqualIcon;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> m_pushToTalkIcon;

    // Footer icons bitmaps
    Microsoft::WRL::ComPtr<ID2D1Bitmap> m_iconIncidents;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> m_iconSoF;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> m_iconTrackTemp;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> m_iconSessionTime;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> m_iconLaps;

    ColumnLayout m_columns;
    TextCache m_text;
    int m_scrollRow = 0;
    int m_maxScrollRow = 0;
    float m_fontSpacing = getGlobalFontSpacing();
};