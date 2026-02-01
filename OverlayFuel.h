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
#include "ClassColors.h"

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
	virtual float2 getDefaultSize() { return float2(350, 300); }

	virtual void onEnable()
	{
		onConfigChanged();
		m_text.reset(m_dwriteFactory.Get());
		m_bgBrush.Reset();
		m_panelBrush.Reset();
	}

	virtual void onDisable()
	{
		m_text.reset();
		m_bgBrush.Reset();
		m_panelBrush.Reset();
	}

	virtual void onConfigChanged()
	{
		std::string fontStyle = g_cfg.getString(m_name, "font_style", "");
		int fontWeight = g_cfg.getInt(m_name, "font_weight", g_cfg.getInt("Overlay", "font_weight", 500));

		m_text.reset(m_dwriteFactory.Get());

		float unused_fontSpacing = g_cfg.getFloat(m_name, "font_spacing", g_cfg.getFloat("Overlay", "font_spacing", 0.30f));

		if (!fontStyle.empty() || fontWeight != 500) {
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

		m_bgBrush.Reset();
		m_panelBrush.Reset();
	}

	virtual void onSessionChanged()
	{
		m_isValidFuelLap = false;
		const bool useStub = StubDataManager::shouldUseStubData();
		m_lapStartRemainingFuel = useStub ? StubDataManager::getStubFuelLevel() : ir_FuelLevel.getFloat();
		m_prevRemainingFuel = m_lapStartRemainingFuel;
		m_prevOnPitRoad = false;
		m_greenLapsSincePit = 0;
		m_maxFuelUsedLapSession = 0.0f;
		m_maxFuelUsedLapStint = 0.0f;
		m_pitHistory.clear();

		std::string newCacheKey = buildFuelCacheKey();
		m_cacheSavedThisSession = false;

		if (newCacheKey != m_cacheKey && !m_cacheKey.empty())
		{
			m_fuelUsedLastLaps.clear();
		}

		m_cacheKey = newCacheKey;

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

		if (m_fuelUsedLastLaps.empty() && StubDataManager::shouldUseStubData())
		{
			const int numLapsToAvg = g_cfg.getInt(m_name, "fuel_estimate_avg_green_laps", 4);
			const float stubFuelPerLap = StubDataManager::getStubFuelPerLap();
			for (int i = 0; i < numLapsToAvg; ++i)
				m_fuelUsedLastLaps.push_back(stubFuelPerLap);
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
		const float pushEstimateFactor = g_cfg.getFloat(m_name, "fuel_push_estimate_factor", 1.0f);
		const float reserve = g_cfg.getFloat(m_name, "fuel_reserve_margin", 0.25f);
		const int targetLap = useStub ? StubDataManager::getStubTargetLap() : g_cfg.getInt(m_name, "fuel_target_lap", 0);
		const int carIdx = useStub ? 0 : ir_session.driverCarIdx;
		const int currentLap = useStub ? StubDataManager::getStubLap() : (ir_isPreStart() ? 0 : std::max(0, ir_CarIdxLap.getInt(carIdx)));
		const int remainingLaps = useStub ? StubDataManager::getStubLapsRemaining() : ir_getLapsRemaining();

		const float remainingFuel = useStub ? StubDataManager::getStubFuelLevel() : ir_FuelLevel.getFloat();
		const float fuelCapacity = ir_session.fuelMaxLtr;
		const bool onPitRoadNow = useStub ? false : ir_CarIdxOnPitRoad.getBool(carIdx);

		const int prevLap = m_prevCurrentLap;
		m_prevCurrentLap = currentLap;
		if (currentLap != prevLap)
		{
			const float usedLastLap = std::max(0.0f, m_lapStartRemainingFuel - remainingFuel);
			m_lapStartRemainingFuel = remainingFuel;
			if (m_isValidFuelLap && usedLastLap > 0.0f)
			{
				m_fuelUsedLastLaps.push_back(usedLastLap);
				m_maxFuelUsedLapSession = std::max(m_maxFuelUsedLapSession, usedLastLap);
				m_maxFuelUsedLapStint = std::max(m_maxFuelUsedLapStint, usedLastLap);
				m_greenLapsSincePit = std::max(0, m_greenLapsSincePit + 1);
			}

			const int numLapsToAvg = g_cfg.getInt(m_name, "fuel_estimate_avg_green_laps", 4);
			while ((int)m_fuelUsedLastLaps.size() > numLapsToAvg)
				m_fuelUsedLastLaps.pop_front();

			m_isValidFuelLap = true;
		}

		// Pit history: record pit-road entry and reset stint counter/max.
		if (!m_prevOnPitRoad && onPitRoadNow)
		{
			PitEntry e;
			e.pitLap = currentLap;
			e.greenLaps = m_greenLapsSincePit;
			m_pitHistory.push_back(e);
			while ((int)m_pitHistory.size() > 6)
				m_pitHistory.pop_front();

			m_greenLapsSincePit = 0;
			m_maxFuelUsedLapStint = 0.0f;
		}
		m_prevOnPitRoad = onPitRoadNow;
		m_prevRemainingFuel = remainingFuel;

		// Invalidate lap if on pit road or under flags (same spirit as DDU)
		const int flagStatus = (ir_SessionFlags.getInt() & ((((int)ir_session.sessionType != 0) ? irsdk_oneLapToGreen : 0) | irsdk_yellow | irsdk_yellowWaving | irsdk_red | irsdk_checkered | irsdk_crossed | irsdk_caution | irsdk_cautionWaving | irsdk_disqualify | irsdk_repair));
		if (flagStatus != 0 || onPitRoadNow)
			m_isValidFuelLap = false;

		float avgPerLap = 0.0f;
		for (float v : m_fuelUsedLastLaps) avgPerLap += v;
		if (!m_fuelUsedLastLaps.empty()) avgPerLap /= (float)m_fuelUsedLastLaps.size();
		const float perLapConsEst = avgPerLap * estimateFactor;
		// "Max per lap" (and push calcs) should be based on the same data source as Avg per lap.
		// Note: m_fuelUsedLastLaps may be seeded from cache before any valid lap is recorded this session,
		// so we also take the max of the history deque to avoid maxPerLap being 0.0 early on.
		float maxPerLapFromHistory = 0.0f;
		for (float v : m_fuelUsedLastLaps) maxPerLapFromHistory = std::max(maxPerLapFromHistory, v);
		const float maxPerLap = std::max(m_maxFuelUsedLapSession, maxPerLapFromHistory);
		const float pushPerLapConsEst = maxPerLap * pushEstimateFactor;

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

		ensureStyleBrushes();

		const float W = (float)m_width;
		const float H = (float)m_height;
		const float minDim = std::max(1.0f, std::min(W, H));
		const float pad = std::clamp(minDim * 0.045f, 8.0f, 18.0f);
		const float innerPad = std::clamp(minDim * 0.045f, 10.0f, 20.0f);
		float corner = std::clamp(minDim * 0.070f, 10.0f, 26.0f);

		// Optional override (legacy config)
		{
			const float cfgCorner = g_cfg.getFloat(m_name, "corner_radius", -1.0f);
			if (cfgCorner > 0.0f) corner = std::clamp(cfgCorner, 3.0f, minDim * 0.5f);
		}

		D2D1_RECT_F rCard = { pad, pad, W - pad, H - pad };
		const float cardW = std::max(1.0f, rCard.right - rCard.left);
		const float cardH = std::max(1.0f, rCard.bottom - rCard.top);

		// Card background gradient
		{
			D2D1_ROUNDED_RECT rrCard = { rCard, corner, corner };
			if (m_bgBrush) {
				m_bgBrush->SetStartPoint(D2D1_POINT_2F{ rCard.left, rCard.top });
				m_bgBrush->SetEndPoint(D2D1_POINT_2F{ rCard.left, rCard.bottom });
				m_renderTarget->FillRoundedRectangle(&rrCard, m_bgBrush.Get());
			} else {
				m_brush->SetColor(float4(0.05f, 0.05f, 0.06f, 0.92f * globalOpacity));
				m_renderTarget->FillRoundedRectangle(&rrCard, m_brush.Get());
			}
		}

		const float bannerH = std::clamp(cardH * 0.075f, 18.0f, 26.0f);
		D2D1_RECT_F rBanner = {
			rCard.left + innerPad,
			rCard.top + innerPad,
			rCard.right - innerPad,
			rCard.top + innerPad + bannerH
		};
		const float bannerRadius = bannerH * 0.22f;

		// Determine warning state for accent color
		const float fuelPct = std::clamp(useStub ? StubDataManager::getStubFuelLevelPct() : ir_FuelLevelPct.getFloat(), 0.0f, 1.0f);
		const bool fuelWarn = fuelPct < 0.10f;
		float4 accentCol = fuelWarn ? warnCol : ClassColors::self();
		accentCol.w *= globalOpacity;

		{
			D2D1_ROUNDED_RECT rrBan = { rBanner, bannerRadius, bannerRadius };
			if (m_panelBrush) {
				m_panelBrush->SetStartPoint(D2D1_POINT_2F{ rBanner.left, rBanner.top });
				m_panelBrush->SetEndPoint(D2D1_POINT_2F{ rBanner.left, rBanner.bottom });
				m_renderTarget->FillRoundedRectangle(&rrBan, m_panelBrush.Get());
			} else {
				m_brush->SetColor(float4(0.03f, 0.03f, 0.04f, 0.88f * globalOpacity));
				m_renderTarget->FillRoundedRectangle(&rrBan, m_brush.Get());
			}

			// Subtle border
			m_brush->SetColor(float4(0.9f, 0.9f, 0.95f, 0.18f * globalOpacity));
			m_renderTarget->DrawRoundedRectangle(&rrBan, m_brush.Get(), 1.5f);

			// Banner text
			if (m_textFormatLarge) {
				m_textFormatLarge->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
				m_textFormatLarge->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
			}
			m_brush->SetColor(float4(0.95f, 0.95f, 0.98f, 0.92f * globalOpacity));
			m_text.render(m_renderTarget.Get(), L"FUEL", m_textFormatLarge.Get(), rBanner.left + innerPad, rBanner.right - innerPad, (rBanner.top + rBanner.bottom) * 0.5f, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
		} // end banner

		// Fuel bar (inset)
		const float gap = std::clamp(cardH * 0.035f, 8.0f, 14.0f);
		const float barH = std::clamp(cardH * 0.11f, 22.0f, 34.0f);
		D2D1_RECT_F rBar = {
			rCard.left + innerPad,
			rBanner.bottom + gap,
			rCard.right - innerPad,
			rBanner.bottom + gap + barH
		};

		wchar_t s[128];

		if (rBar.bottom > rBar.top + 8.0f)
		{
			const float barCorner = std::clamp(barH * 0.22f, 4.0f, 10.0f);
			D2D1_ROUNDED_RECT rrBg = { rBar, barCorner, barCorner };

			// Bar background + border
			m_brush->SetColor(float4(0.04f, 0.05f, 0.06f, 0.70f * globalOpacity));
			m_renderTarget->FillRoundedRectangle(&rrBg, m_brush.Get());
			m_brush->SetColor(float4(0.80f, 0.82f, 0.86f, 0.28f * globalOpacity));
			m_renderTarget->DrawRoundedRectangle(&rrBg, m_brush.Get(), 1.5f);

			// Bar fill
			const float fillW = (rBar.right - rBar.left) * fuelPct;
			if (fillW > 0.0f) {
				D2D1_RECT_F rFill = { rBar.left, rBar.top, rBar.left + fillW, rBar.bottom };
				D2D1_ROUNDED_RECT rrFill = { rFill, barCorner, barCorner };
				float4 fillCol = fuelWarn ? warnCol : goodCol;
				fillCol.w *= globalOpacity;
				m_brush->SetColor(fillCol);
				m_renderTarget->FillRoundedRectangle(&rrFill, m_brush.Get());
			}

			// Center text (remaining fuel)
			if (remainingFuel >= 0.0f) {
				float val = remainingFuel; if (imperial) val *= 0.264172f;
				swprintf(s, _countof(s), imperial ? L"%.1f GAL" : L"%.1f L", val);
				if (m_textFormat) {
					m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
					m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
				}
				m_brush->SetColor(float4(1, 1, 1, 0.92f * globalOpacity));
				m_text.render(m_renderTarget.Get(), s, m_textFormat.Get(), rBar.left, rBar.right, (rBar.top + rBar.bottom) * 0.5f, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);
			}
		}

		// Capacity + "E" footer under the bar (small, subtle)
		const float yLabels = rBar.bottom + std::max(18.0f, gap * 1.45f);
		{
			const float yLbl = yLabels;
			m_brush->SetColor(float4(textCol.x, textCol.y, textCol.z, textCol.w * globalOpacity));
			if (m_textFormatSmall) {
				m_textFormatSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
				m_textFormatSmall->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
			}
			m_text.render(m_renderTarget.Get(), L"E", m_textFormatSmall.Get(), rBar.left, rBar.left + 28.0f, yLbl, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing);

			if (fuelCapacity > 0.0f) {
				float val = fuelCapacity; if (imperial) val *= 0.264172f;
				swprintf(s, _countof(s), imperial ? L"%.1f GAL" : L"%.1f L", val);
				m_text.render(m_renderTarget.Get(), s, m_textFormatSmall.Get(), rBar.right - 110.0f, rBar.right, yLbl, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
			}
		}

		// Data panel (rows)
		// Extra breathing room between the labels row and the first data row
		const float rowsTop = yLabels + std::max(26.0f, gap * 1.85f);
		D2D1_RECT_F rPanel = {
			rCard.left + innerPad,
			rowsTop,
			rCard.right - innerPad,
			rCard.bottom - innerPad
		};

		if (rPanel.bottom > rPanel.top + 20.0f)
		{
			const float panelCorner = std::clamp(corner * 0.75f, 8.0f, 22.0f);
			D2D1_ROUNDED_RECT rrPanel = { rPanel, panelCorner, panelCorner };
			if (m_panelBrush) {
				m_panelBrush->SetStartPoint(D2D1_POINT_2F{ rPanel.left, rPanel.top });
				m_panelBrush->SetEndPoint(D2D1_POINT_2F{ rPanel.left, rPanel.bottom });
				m_renderTarget->FillRoundedRectangle(&rrPanel, m_panelBrush.Get());
			} else {
				m_brush->SetColor(float4(0.03f, 0.03f, 0.04f, 0.88f * globalOpacity));
				m_renderTarget->FillRoundedRectangle(&rrPanel, m_brush.Get());
			}
			m_brush->SetColor(float4(0.9f, 0.9f, 0.95f, 0.18f * globalOpacity));
			m_renderTarget->DrawRoundedRectangle(&rrPanel, m_brush.Get(), 1.5f);
		}

		const float xPad = rPanel.left + std::max(8.0f, innerPad * 0.75f);
		const float xRight = rPanel.right - std::max(8.0f, innerPad * 0.75f);

		// Column data (same values/logic as before)
		const float baseFontSize = g_cfg.getFloat(m_name, "font_size", g_cfg.getFloat("Overlay", "font_size", 16.0f));
		const float lineHeight = baseFontSize * 1.75f;
		float y = rPanel.top + std::max(14.0f, lineHeight * 0.70f);
		int rowCnt = 0;

		auto drawRowBg = [&](float yCenter, bool isAlt)
		{
			if (!isAlt) return;
			if (alternateLineBgCol.a <= 0) return;
			float4 c = alternateLineBgCol;
			c.w *= globalOpacity;
			m_brush->SetColor(c);
			D2D1_RECT_F r = { rPanel.left + 2.0f, yCenter - lineHeight * 0.5f, rPanel.right - 2.0f, yCenter + lineHeight * 0.5f };
			m_renderTarget->FillRectangle(&r, m_brush.Get());
		};

		auto drawLabel = [&](const wchar_t* label, float yCenter, const float4& col)
		{
			m_brush->SetColor(float4(col.x, col.y, col.z, col.w * globalOpacity));
			m_text.render(m_renderTarget.Get(), label, m_textFormatSmall.Get(), xPad, (xPad + xRight) * 0.5f, yCenter, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_fontSpacing);
		};

		auto drawValue = [&](const wchar_t* value, float yCenter, const float4& col)
		{
			m_brush->SetColor(float4(col.x, col.y, col.z, col.w * globalOpacity));
			m_text.render(m_renderTarget.Get(), value, m_textFormatSmall.Get(), (xPad + xRight) * 0.5f, xRight, yCenter, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_fontSpacing);
		};

		{
			drawRowBg(y, (rowCnt & 1) != 0);
			drawLabel(L"Avg per lap", y, textCol);

			if (avgPerLap > 0.0f)
			{
				float val = avgPerLap; if (imperial) val *= 0.264172f;
				swprintf(s, _countof(s), imperial ? L"%.2f G" : L"%.2f L", val);
				drawValue(s, y, textCol);
			}
			rowCnt++;
			y += lineHeight;
		}

		{
			drawRowBg(y, (rowCnt & 1) != 0);
			drawLabel(L"Max per lap", y, textCol);

			if (maxPerLap > 0.0f)
			{
				float val = maxPerLap; if (imperial) val *= 0.264172f;
				swprintf(s, _countof(s), imperial ? L"%.2f G" : L"%.2f L", val);
				drawValue(s, y, textCol);
			}
			rowCnt++;
			y += lineHeight;
		}

		{
			drawRowBg(y, (rowCnt & 1) != 0);
			drawLabel(L"Refuel to finish", y, textCol);

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
				const bool warn = (value > (useStub ? StubDataManager::getStubPitServiceFuel() : ir_PitSvFuel.getFloat())) || (value > 0 && !(useStub ? StubDataManager::getStubFuelFillAvailable() : ir_dpFuelFill.getFloat()));
				const float4 valCol = warn ? warnCol : goodCol;
				float val = value; if (imperial) val *= 0.264172f;
				swprintf(s, _countof(s), imperial ? L"%3.2f G" : L"%3.2f L", val);
				drawValue(s, y, valCol);
			}
			rowCnt++;
			y += lineHeight;
		}

		{
			drawRowBg(y, (rowCnt & 1) != 0);
			drawLabel(L"Push refuel", y, textCol);

			if (pushPerLapConsEst > 0.0f)
			{
				float value;
				if (targetLap == 0)
				{
					value = std::max(0.0f, (float)remainingLaps * pushPerLapConsEst - (remainingFuel - reserve));
				}
				else
				{
					value = (targetLap + 1 - currentLap) * pushPerLapConsEst - (m_lapStartRemainingFuel - reserve);
				}
				const bool warn = (value > (useStub ? StubDataManager::getStubPitServiceFuel() : ir_PitSvFuel.getFloat())) || (value > 0 && !(useStub ? StubDataManager::getStubFuelFillAvailable() : ir_dpFuelFill.getFloat()));
				const float4 valCol = warn ? warnCol : goodCol;
				float val = value; if (imperial) val *= 0.264172f;
				swprintf(s, _countof(s), imperial ? L"%3.2f G" : L"%3.2f L", val);
				drawValue(s, y, valCol);
			}
			rowCnt++;
			y += lineHeight;
		}

		{
			drawRowBg(y, (rowCnt & 1) != 0);
			drawLabel((targetLap == 0 ? L"Add" : L"Target"), y, textCol);

			if (targetLap != 0) {
				float targetFuel = (m_lapStartRemainingFuel - reserve) / (targetLap + 1 - currentLap);

				if (imperial)
					targetFuel *= 0.264172f;
				swprintf(s, _countof(s), imperial ? L"%3.2f G" : L"%3.2f L", targetFuel);
				drawValue(s, y, textCol);
			}
			else {
				float add = useStub ? StubDataManager::getStubPitServiceFuel() : ir_PitSvFuel.getFloat();
				if (add >= 0)
				{
					const bool canFill = (useStub ? StubDataManager::getStubFuelFillAvailable() : ir_dpFuelFill.getFloat()) != 0.0f;
					const float4 addCol = canFill ? goodCol : warnCol;
					if (imperial)
						add *= 0.264172f;
					swprintf(s, _countof(s), imperial ? L"%3.2f G" : L"%3.2f L", add);
					drawValue(s, y, addCol);
				}
			}
			rowCnt++;
			y += lineHeight;
		}

		{
			drawRowBg(y, (rowCnt & 1) != 0);

			const float4 goldCol = float4(1.0f, 0.84f, 0.0f, textCol.w);
			drawLabel(L"Laps left", y, goldCol);

			if (perLapConsEst > 0.0f)
			{
				const float estLaps = (remainingFuel - reserve) / perLapConsEst;
				const bool lowFuelWarning = (estLaps <= 2.0f);
				const float4 lapsCol = lowFuelWarning ? warnCol : goldCol;
				swprintf(s, _countof(s), L"%.*f", g_cfg.getInt(m_name, "fuel_decimal_places", 2), estLaps);
				drawValue(s, y, lapsCol);
			}
			rowCnt++;
			y += lineHeight;
		}

		{
			drawRowBg(y, (rowCnt & 1) != 0);

			const float4 goldCol = float4(1.0f, 0.84f, 0.0f, textCol.w);
			drawLabel(L"Push laps left", y, goldCol);

			if (pushPerLapConsEst > 0.0f)
			{
				const float estLaps = (remainingFuel - reserve) / pushPerLapConsEst;
				const bool lowFuelWarning = (estLaps <= 2.0f);
				const float4 lapsCol = lowFuelWarning ? warnCol : goldCol;
				swprintf(s, _countof(s), L"%.*f", g_cfg.getInt(m_name, "fuel_decimal_places", 2), estLaps);
				drawValue(s, y, lapsCol);
			}
			rowCnt++;
			y += lineHeight;
		}

		{
			drawRowBg(y, (rowCnt & 1) != 0);
			drawLabel(L"Pits", y, textCol);

			// Show a compact pit history like: "L12(9G) L31(8G) ..."
			std::wstring pitStr;
			const int maxShow = 3;
			const int n = (int)m_pitHistory.size();
			const int start = std::max(0, n - maxShow);
			for (int i = start; i < n; ++i)
			{
				wchar_t b[64];
				swprintf(b, _countof(b), L"L%d(%dG)", m_pitHistory[i].pitLap, m_pitHistory[i].greenLaps);
				if (!pitStr.empty()) pitStr += L" ";
				pitStr += b;
			}
			if (!pitStr.empty())
				drawValue(pitStr.c_str(), y, textCol);

			rowCnt++;
			y += lineHeight;
		}

		m_renderTarget->EndDraw();
	}

	virtual bool hasCustomBackground() { return true; }

protected:
	Microsoft::WRL::ComPtr<IDWriteTextFormat>	m_textFormat;
	Microsoft::WRL::ComPtr<IDWriteTextFormat>	m_textFormatSmall;
	Microsoft::WRL::ComPtr<IDWriteTextFormat>	m_textFormatLarge;
	TextCache	m_text;

	int			m_prevCurrentLap = 0;
	float		m_prevRemainingFuel = 0.0f;
	float		m_lapStartRemainingFuel = 0.0f;
	std::deque<float>	m_fuelUsedLastLaps;
	bool		m_isValidFuelLap = false;
	float		m_fontSpacing = getGlobalFontSpacing();

	// Worst-case tracking (valid green laps only)
	float		m_maxFuelUsedLapSession = 0.0f;
	float		m_maxFuelUsedLapStint = 0.0f;

	// Pit history (pit lap + green-flag laps in-between)
	struct PitEntry { int pitLap = 0; int greenLaps = 0; };
	std::deque<PitEntry> m_pitHistory;
	bool		m_prevOnPitRoad = false;
	int			m_greenLapsSincePit = 0;

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

private:
	void ensureStyleBrushes()
	{
		if (!m_renderTarget) return;
		if (m_bgBrush && m_panelBrush) return;

		// Card background gradient (same palette as OverlayPit/OverlayFlags)
		{
			D2D1_GRADIENT_STOP stops[3] = {};
			stops[0].position = 0.0f;  stops[0].color = D2D1::ColorF(0.16f, 0.18f, 0.22f, 0.95f);
			stops[1].position = 0.45f; stops[1].color = D2D1::ColorF(0.06f, 0.07f, 0.09f, 0.95f);
			stops[2].position = 1.0f;  stops[2].color = D2D1::ColorF(0.02f, 0.02f, 0.03f, 0.95f);

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
			stops[0].position = 0.0f;  stops[0].color = D2D1::ColorF(0.08f, 0.09f, 0.11f, 0.92f);
			stops[1].position = 0.55f; stops[1].color = D2D1::ColorF(0.04f, 0.045f, 0.055f, 0.92f);
			stops[2].position = 1.0f;  stops[2].color = D2D1::ColorF(0.02f, 0.02f, 0.03f, 0.92f);

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

	// Styling brushes (cached; recreated on config change / enable)
	Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_bgBrush;
	Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_panelBrush;
};