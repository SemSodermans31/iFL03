/*
MIT License

Copyright (c) 2021-2025 L. E. Spalt & Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"); to deal
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

#include <deque>
#include <algorithm>
#include <cctype>
#include "Overlay.h"
#include "iracing.h"
#include "Units.h"
#include "Config.h"
#include "OverlayDebug.h"
#include "stub_data.h"

// Lightweight overlay showing the same fuel calculator values as DDU
class OverlayFuel : public Overlay
{
public:
	OverlayFuel() : Overlay("OverlayFuel") {}

#ifdef _DEBUG
	virtual bool	canEnableWhileNotDriving() const { return true; }
	virtual bool	canEnableWhileDisconnected() const { return true; }
#else
	virtual bool	canEnableWhileDisconnected() const { return StubDataManager::shouldUseStubData(); }
#endif

protected:
	virtual float2 getDefaultSize() { return float2(350, 160); }

	virtual void onEnable() { onConfigChanged(); }

	virtual void onDisable() { m_text.reset(); }

	virtual void onConfigChanged()
	{
		std::string fontStyle = g_cfg.getString(m_name, "font_style", "");
		int fontWeight = g_cfg.getInt(m_name, "font_weight", g_cfg.getInt("Overlay", "font_weight", 500));

		m_text.reset(m_dwriteFactory.Get());

		float unused_fontSpacing = g_cfg.getFloat(m_name, "font_spacing", g_cfg.getFloat("Overlay", "font_spacing", 0.30f));

		if (!fontStyle.empty() || fontWeight != 500) {
			// Use the override version for style and weight
			int dwriteWeight = fontWeight;
			std::string dwriteStyle = fontStyle.empty() ? "normal" : fontStyle;

			createGlobalTextFormat(1.0f, dwriteWeight, dwriteStyle, m_textFormat);
			createGlobalTextFormat(0.85f, dwriteWeight, dwriteStyle, m_textFormatSmall);
			createGlobalTextFormat(1.2f, dwriteWeight, dwriteStyle, m_textFormatLarge);
		} else {
			createGlobalTextFormat(1.0f, m_textFormat);
			createGlobalTextFormat(0.85f, m_textFormatSmall);
			createGlobalTextFormat(1.2f, m_textFormatLarge);
		}

		setTargetFPS(g_cfg.getInt(m_name, "target_fps", 10));
	}

	virtual void onSessionChanged()
	{
		m_isValidFuelLap = false;
		m_lapStartRemainingFuel = ir_FuelLevel.getFloat();

		// Build cache key based on car+track combination
		std::string newCacheKey = buildFuelCacheKey();
		m_cacheSavedThisSession = false;

		// Only clear fuel data if car or track changed, otherwise preserve as filler values
		if (newCacheKey != m_cacheKey && !m_cacheKey.empty())
		{
			m_fuelUsedLastLaps.clear();
		}

		m_cacheKey = newCacheKey;

		// If we don't have fuel data, try to seed from cache
		if (m_fuelUsedLastLaps.empty() && !m_cacheKey.empty())
		{
			const float cachedAvgPerLap = g_cfg.getFloat("FuelCache", m_cacheKey, -1.0f);
			if (cachedAvgPerLap > 0.0f)
			{
				const int numLapsToAvg = g_cfg.getInt(m_name, "fuel_estimate_avg_green_laps", 4);
				for (int i = 0; i < numLapsToAvg; ++i)
					m_fuelUsedLastLaps.push_back(cachedAvgPerLap);
			}
		}
	}

	virtual void onUpdate()
	{
		const bool useStub = StubDataManager::shouldUseStubData();
		if (!useStub && !ir_hasValidDriver()) {
			return;
		}
		if (useStub) StubDataManager::populateSessionCars();

		const bool imperial = isImperialUnits();
		const float estimateFactor = g_cfg.getFloat(m_name, "fuel_estimate_factor", 1.1f);
		const float reserve = g_cfg.getFloat(m_name, "fuel_reserve_margin", 0.25f);
		const int targetLap = useStub ? StubDataManager::getStubTargetLap() : g_cfg.getInt(m_name, "fuel_target_lap", 0);
		const int carIdx = useStub ? 0 : ir_session.driverCarIdx;
		const int currentLap = useStub ? StubDataManager::getStubLap() : (ir_isPreStart() ? 0 : std::max(0, ir_CarIdxLap.getInt(carIdx)));
		const int remainingLaps = useStub ? StubDataManager::getStubLapsRemaining() : ir_getLapsRemaining();

		const float remainingFuel = ir_FuelLevel.getFloat();
		const float fuelCapacity = ir_session.fuelMaxLtr;

		const int prevLap = m_prevCurrentLap;
		m_prevCurrentLap = currentLap;
		if (currentLap != prevLap)
		{
			const float usedLastLap = std::max(0.0f, m_lapStartRemainingFuel - remainingFuel);
			m_lapStartRemainingFuel = remainingFuel;
			if (m_isValidFuelLap && usedLastLap > 0.0f)
				m_fuelUsedLastLaps.push_back(usedLastLap);

			const int numLapsToAvg = g_cfg.getInt(m_name, "fuel_estimate_avg_green_laps", 4);
			while ((int)m_fuelUsedLastLaps.size() > numLapsToAvg)
				m_fuelUsedLastLaps.pop_front();

			m_isValidFuelLap = true;
		}

		// Invalidate lap if on pit road or under flags (same spirit as DDU)
		const int flagStatus = (ir_SessionFlags.getInt() & ((((int)ir_session.sessionType != 0) ? irsdk_oneLapToGreen : 0) | irsdk_yellow | irsdk_yellowWaving | irsdk_red | irsdk_checkered | irsdk_crossed | irsdk_caution | irsdk_cautionWaving | irsdk_disqualify | irsdk_repair));
		if (flagStatus != 0 || ir_CarIdxOnPitRoad.getBool(carIdx))
			m_isValidFuelLap = false;

		float avgPerLap = 0.0f;
		for (float v : m_fuelUsedLastLaps) avgPerLap += v;
		if (!m_fuelUsedLastLaps.empty()) avgPerLap /= (float)m_fuelUsedLastLaps.size();
		const float perLapConsEst = avgPerLap * estimateFactor;

		// Persist a fresh average for this car/track combo once we have enough valid laps
		{
			const int numLapsToAvg = g_cfg.getInt(m_name, "fuel_estimate_avg_green_laps", 4);
			if (!m_cacheSavedThisSession && (int)m_fuelUsedLastLaps.size() >= numLapsToAvg && avgPerLap > 0.0f)
			{
				if (m_cacheKey.empty()) m_cacheKey = buildFuelCacheKey();
				if (!m_cacheKey.empty())
				{
					g_cfg.setFloat("FuelCache", m_cacheKey, avgPerLap);
					m_cacheSavedThisSession = true;
				}
			}
		}

		// Colors - changed goodCol to white, warnCol to orange
		const float4 textCol = g_cfg.getFloat4(m_name, "text_col", float4(1,1,1,0.9f));
		const float4 goodCol = float4(1,1,1,0.9f);
		const float4 warnCol = float4(1,0.6f,0,1);
		const float4 bgCol   = g_cfg.getFloat4(m_name, "background_col", float4(0,0,0,1));
		const float4 alternateLineBgCol = g_cfg.getFloat4(m_name, "alternate_line_background_col", float4(0.5f,0.5f,0.5f,0.15f));
		const float globalOpacity = getGlobalOpacity();

		// Draw
		m_renderTarget->BeginDraw();
		m_renderTarget->Clear(float4(0,0,0,0));

		// Background rounded rect
		const float w = (float)m_width, h = (float)m_height;
		const float cornerRadius = g_cfg.getFloat(m_name, "corner_radius", 6.0f);
		D2D1_ROUNDED_RECT rr;
		rr.rect = { 0.5f, 0.5f, w-0.5f, h-0.5f };
		rr.radiusX = cornerRadius; rr.radiusY = cornerRadius;
		float4 bg = bgCol; bg.w *= globalOpacity;
		m_brush->SetColor(bg);
		m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());

		// Title
		m_brush->SetColor(float4(textCol.x, textCol.y, textCol.z, textCol.w * globalOpacity));
		m_text.render(m_renderTarget.Get(), L"Fuel", m_textFormat.Get(), 10, w-10, 15, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);

		// Bars and labels
		wchar_t s[128];
		const float xPad = 12.0f;
		const float barTop = 35.0f;
		const float barH = 14.0f;
		const float barL = xPad, barR = w - xPad;

		// Fuel bar with stroke
		{
			const float pct = std::clamp(ir_FuelLevelPct.getFloat(), 0.0f, 1.0f);
			const float radius = barH / 2.0f;
			D2D1_ROUNDED_RECT rBg = { { barL, barTop, barR, barTop + barH }, radius, radius };
			m_brush->SetColor(float4(0.5f, 0.5f, 0.5f, 0.5f));
			m_renderTarget->FillRoundedRectangle(&rBg, m_brush.Get());
			D2D1_ROUNDED_RECT rFg = { { barL, barTop, barL + pct * (barR - barL), barTop + barH }, radius, radius };
			m_brush->SetColor(pct < 0.1f ? warnCol : goodCol);
			m_renderTarget->FillRoundedRectangle(&rFg, m_brush.Get());

			// White stroke around the bar
			m_brush->SetColor(float4(1,1,1,0.9f));
			m_renderTarget->DrawRoundedRectangle(&rBg, m_brush.Get(), 2.0f);
		}

		// Fuel bar labels
		m_brush->SetColor(float4(textCol.x, textCol.y, textCol.z, textCol.w * globalOpacity));
		// "E" on the left
		m_text.render(m_renderTarget.Get(), L"E", m_textFormatSmall.Get(), barL, barL + 20, barTop + barH + 12, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);

		// Remaining fuel in the middle
		if (remainingFuel >= 0.0f)
		{
			float val = remainingFuel; if (imperial) val *= 0.264172f;
			swprintf(s, _countof(s), imperial ? L"%.1f gl" : L"%.1f lt", val);
			m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), barL + (barR - barL)/2 - 30, barL + (barR - barL)/2 + 30, barTop + barH + 12, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
		}

		// Full tank capacity on the right
		if (fuelCapacity > 0.0f)
		{
			float val = fuelCapacity; if (imperial) val *= 0.264172f;
			swprintf(s, _countof(s), imperial ? L"%.1f gl" : L"%.1f lt", val);
			m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), barR - 50, barR, barTop + barH + 12, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
		}

		// Column data
		const float baseFontSize = g_cfg.getFloat(m_name, "font_size", g_cfg.getFloat("Overlay", "font_size", 16.0f));
		const float lineHeight = baseFontSize * 1.75f;
		const float colYStart = 100.0f;
		int rowCnt = 0;

		{
			float y = colYStart + rowCnt * lineHeight;
			// Alternating background
			if (rowCnt & 1 && alternateLineBgCol.a > 0)
			{
				D2D1_RECT_F r = { 4.0f, y - lineHeight/2, w - 4.0f, y + lineHeight/2 };
				m_brush->SetColor(alternateLineBgCol);
				m_renderTarget->FillRectangle(&r, m_brush.Get());
			}

			m_brush->SetColor(float4(textCol.x, textCol.y, textCol.z, textCol.w * globalOpacity));
			m_text.render(m_renderTarget.Get(), L"Avg per lap", m_textFormatSmall.Get(), xPad, w/2, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing);

			if (avgPerLap > 0.0f)
			{
				float val = avgPerLap; if (imperial) val *= 0.264172f;
				swprintf(s, _countof(s), imperial ? L"%.2f gl" : L"%.2f lt", val);
				m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), w/2, w-xPad, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
			}
			rowCnt++;
		}

		{
			float y = colYStart + rowCnt * lineHeight;
			if (rowCnt & 1 && alternateLineBgCol.a > 0)
			{
				D2D1_RECT_F r = { 4.0f, y - lineHeight/2, w - 4.0f, y + lineHeight/2 };
				m_brush->SetColor(alternateLineBgCol);
				m_renderTarget->FillRectangle(&r, m_brush.Get());
			}

			m_brush->SetColor(float4(textCol.x, textCol.y, textCol.z, textCol.w * globalOpacity));
			m_text.render(m_renderTarget.Get(), L"Laps left", m_textFormatSmall.Get(), xPad, w/2, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing);

			if (perLapConsEst > 0.0f)
			{
				const float estLaps = (remainingFuel - reserve) / perLapConsEst;	
				swprintf(s, _countof(s), L"%.*f", g_cfg.getInt(m_name, "fuel_decimal_places", 2), estLaps);
				m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), w/2, w-xPad, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
			}
			rowCnt++;
		}

		{
			float y = colYStart + rowCnt * lineHeight;
			if (rowCnt & 1 && alternateLineBgCol.a > 0)
			{
				D2D1_RECT_F r = { 4.0f, y - lineHeight/2, w - 4.0f, y + lineHeight/2 };
				m_brush->SetColor(alternateLineBgCol);
				m_renderTarget->FillRectangle(&r, m_brush.Get());
			}

			m_brush->SetColor(float4(textCol.x, textCol.y, textCol.z, textCol.w * globalOpacity));
			m_text.render(m_renderTarget.Get(), L"Refuel to finish", m_textFormatSmall.Get(), xPad, w/2, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing);

			if (perLapConsEst > 0.0f)
			{
				float value;
				if (targetLap == 0)
				{
					value = std::max(0.0f, (float)remainingLaps * perLapConsEst - (remainingFuel - reserve));
				}
				else
				{
					value = (targetLap + 1 - currentLap) * perLapConsEst - (m_lapStartRemainingFuel - reserve);
				}
				const bool warn = (value > ir_PitSvFuel.getFloat()) || (value > 0 && !ir_dpFuelFill.getFloat());
				m_brush->SetColor(warn ? warnCol : goodCol);
				float val = value; if (imperial) val *= 0.264172f;
				swprintf(s, _countof(s), imperial ? L"%3.2f gl" : L"%3.2f lt", val);
				m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), w/2, w-xPad, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
			}
			rowCnt++;
		}

		{
			float y = colYStart + rowCnt * lineHeight;
			if (rowCnt & 1 && alternateLineBgCol.a > 0)
			{
				D2D1_RECT_F r = { 4.0f, y - lineHeight/2, w - 4.0f, y + lineHeight/2 };
				m_brush->SetColor(alternateLineBgCol);
				m_renderTarget->FillRectangle(&r, m_brush.Get());
			}

			m_brush->SetColor(float4(textCol.x, textCol.y, textCol.z, textCol.w * globalOpacity));
			m_text.render(m_renderTarget.Get(), (targetLap==0?L"Add":L"Tgt"), m_textFormatSmall.Get(), xPad, w/2, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing);

			if (targetLap != 0) {
				float targetFuel = (m_lapStartRemainingFuel - reserve) / (targetLap + 1 - currentLap);

				if (imperial)
					targetFuel *= 0.264172f;
				swprintf(s, _countof(s), imperial ? L"%3.2f gl" : L"%3.2f lt", targetFuel);
				m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), w/2, w-xPad, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
			}
			else {
				float add = ir_PitSvFuel.getFloat();
				if (add >= 0)
				{
					if (ir_dpFuelFill.getFloat())
						m_brush->SetColor(goodCol);

					if (imperial)
						add *= 0.264172f;
					swprintf(s, _countof(s), imperial ? L"%3.2f gl" : L"%3.2f lt", add);
					m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), w/2, w-xPad, y, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
					m_brush->SetColor(float4(textCol.x, textCol.y, textCol.z, textCol.w * globalOpacity));
				}
			}
			rowCnt++;
		}

		// Draw white border at the top of rows
		{
			const float borderY = colYStart - lineHeight/2;
			D2D1_POINT_2F startPoint = { 4.0f, borderY };
			D2D1_POINT_2F endPoint = { w - 4.0f, borderY };
			m_brush->SetColor(float4(1.0f, 1.0f, 1.0f, 1.0f));
			m_renderTarget->DrawLine(startPoint, endPoint, m_brush.Get(), 3.0f);
		}

		m_renderTarget->EndDraw();
	}

	virtual bool hasCustomBackground() { return false; }

protected:
	Microsoft::WRL::ComPtr<IDWriteTextFormat>	m_textFormat;
	Microsoft::WRL::ComPtr<IDWriteTextFormat>	m_textFormatSmall;
	Microsoft::WRL::ComPtr<IDWriteTextFormat>	m_textFormatLarge;
	TextCache	m_text;

	int			m_prevCurrentLap = 0;
	float		m_lapStartRemainingFuel = 0.0f;
	std::deque<float>	m_fuelUsedLastLaps;
	bool		m_isValidFuelLap = false;
	float		m_fontSpacing = getGlobalFontSpacing();

		// Simple per-car+track fuel average cache
		std::string	m_cacheKey;
		bool		m_cacheSavedThisSession = false;

		// Build a stable key like: t<trackId>_<sanitizedTrackCfg>_c<carId>
		std::string buildFuelCacheKey() const
		{
			int trackId = ir_session.trackId;
			std::string cfg = ir_session.trackConfigName;
			int carId = 0;
			if (ir_session.driverCarIdx >= 0)
				carId = ir_session.cars[ir_session.driverCarIdx].carID;

			if (trackId <= 0 || carId <= 0)
				return std::string();

			for (char& c : cfg)
			{
				if (!( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ))
					c = '_';
			}
			char buf[256];
			_snprintf_s(buf, _countof(buf), _TRUNCATE, "t%d_%s_c%d", trackId, cfg.c_str(), carId);
			return std::string(buf);
		}
};


