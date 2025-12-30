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

#include <algorithm>
#include <string>
#include <vector>
#include "Overlay.h"
#include "iracing.h"
#include "Config.h"
#include "irsdk/irsdk_defines.h"
#include "preview_mode.h"

class OverlayFlags : public Overlay
{
public:
	OverlayFlags()
		: Overlay("OverlayFlags")
	{}

protected:

	virtual float2 getDefaultSize()
	{
		// Wide but compact (Kapps-like "card")
		return float2(640, 220);
	}

	virtual void onEnable()
	{
		onConfigChanged();
		m_text.reset(m_dwriteFactory.Get());
		m_bgBrush.Reset();
		m_panelBrush.Reset();
		m_bannerClipLayer.Reset();
	}

	virtual void onConfigChanged()
	{
		// Per-overlay FPS (configurable; default 10)
		setTargetFPS(g_cfg.getInt(m_name, "target_fps", 10));

		// Centralized fonts
		m_text.reset(m_dwriteFactory.Get());
		createGlobalTextFormat(1.05f, (int)DWRITE_FONT_WEIGHT_BOLD, "", m_textFormatTop);
		createGlobalTextFormat(2.10f, (int)DWRITE_FONT_WEIGHT_BOLD, "", m_textFormatMain);

		// Recreate D2D brushes (render target may change)
		m_bgBrush.Reset();
		m_panelBrush.Reset();
		m_bannerClipLayer.Reset();

		// Per-overlay FPS (configurable; default 10)
		// (already set above)
	}

	virtual void onUpdate()
	{
		const float globalOpacity = getGlobalOpacity();

		wchar_t sTop[256] = L"";
		wchar_t sBottom[256] = L"";

		// Resolve active flag info from iracing bitfield
		FlagInfo info = resolveActiveFlag();
		if( !info.active )
		{
			// Nothing to show – clear and early out
			m_renderTarget->BeginDraw();
			m_renderTarget->Clear( float4(0,0,0,0) );
			m_renderTarget->EndDraw();
			return;
		}

		float4 flagCol = info.color;
		flagCol.w *= globalOpacity;

		// Contrast helpers
		auto luminance = [](const float4& c)->float { return 0.2126f*c.x + 0.7152f*c.y + 0.0722f*c.z; };
		const bool flagIsDark = luminance(info.color) < 0.35f;

		// Build strings
		swprintf(sTop, _countof(sTop), L"%S", info.topText.c_str());
		swprintf(sBottom, _countof(sBottom), L"%S", info.bottomText.c_str());

		m_renderTarget->BeginDraw();
		m_renderTarget->Clear( float4(0,0,0,0) );

		ensureStyleBrushes();

		// Layout (Kapps-like card)
		{
			const float W = (float)m_width;
			const float H = (float)m_height;
			const float minDim = std::max(1.0f, std::min(W, H));
			const float pad = std::clamp(minDim * 0.045f, 8.0f, 18.0f);
			const float innerPad = std::clamp(minDim * 0.045f, 10.0f, 20.0f);
			const float corner = std::clamp(minDim * 0.070f, 10.0f, 26.0f);

			D2D1_RECT_F rCard = { pad, pad, W - pad, H - pad };
			const float cardW = std::max(1.0f, rCard.right - rCard.left);
			const float cardH = std::max(1.0f, rCard.bottom - rCard.top);

			// Card background gradient
			{
				D2D1_ROUNDED_RECT rr = { rCard, corner, corner };
				if (m_bgBrush) {
					m_bgBrush->SetStartPoint(D2D1_POINT_2F{ rCard.left, rCard.top });
					m_bgBrush->SetEndPoint(D2D1_POINT_2F{ rCard.left, rCard.bottom });
					m_renderTarget->FillRoundedRectangle(&rr, m_bgBrush.Get());
				} else {
					m_brush->SetColor(float4(0.05f, 0.05f, 0.06f, 0.92f * globalOpacity));
					m_renderTarget->FillRoundedRectangle(&rr, m_brush.Get());
				}
			}

			// Banner (dark panel + colored accent strip)
			const float bannerH = std::clamp(cardH * 0.22f, 34.0f, 60.0f);
			D2D1_RECT_F rBanner = {
				rCard.left + innerPad,
				rCard.top + innerPad,
				rCard.right - innerPad,
				rCard.top + innerPad + bannerH
			};
			const float bannerRadius = bannerH * 0.22f;

			{
				const float panelCorner = std::clamp(corner * 0.75f, 8.0f, 22.0f);
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

				// Banner text: flag name (top text)
				if (m_textFormatTop) {
					m_textFormatTop->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
					m_textFormatTop->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
				}
				float4 topTextCol = flagIsDark ? float4(1, 1, 1, 0.95f * globalOpacity)
					: float4(info.color.x, info.color.y, info.color.z, 0.95f * globalOpacity);
				m_brush->SetColor(topTextCol);
				m_text.render(
					m_renderTarget.Get(),
					sTop,
					m_textFormatTop.Get(),
					rBanner.left + innerPad,
					rBanner.right - innerPad,
					(rBanner.top + rBanner.bottom) * 0.5f,
					m_brush.Get(),
					DWRITE_TEXT_ALIGNMENT_CENTER,
					getGlobalFontSpacing()
				);
			}

			// Main panel: filled with flag color (flat), with subtle border like Kapps cards
			const float gap = std::clamp(cardH * 0.035f, 8.0f, 14.0f);
			D2D1_RECT_F rPanel = {
				rCard.left + innerPad,
				rBanner.bottom + gap,
				rCard.right - innerPad,
				rCard.bottom - innerPad
			};
			if (rPanel.bottom > rPanel.top + 20.0f)
			{
				// Slightly larger radius for the bottom "flag" panel (requested)
				const float panelW = std::max(1.0f, rPanel.right - rPanel.left);
				const float panelH = std::max(1.0f, rPanel.bottom - rPanel.top);
				float panelCorner = std::clamp(corner * 0.95f, 20.0f, 30.0f);
				panelCorner = std::min(panelCorner, std::min(panelW, panelH) * 0.5f);
				D2D1_ROUNDED_RECT rrPanel = { rPanel, panelCorner, panelCorner };
				m_brush->SetColor(flagCol);
				m_renderTarget->FillRoundedRectangle(&rrPanel, m_brush.Get());

				float4 borderCol = flagCol;
				borderCol.x *= 0.55f;
				borderCol.y *= 0.55f;
				borderCol.z *= 0.55f;
				borderCol.w = std::min(borderCol.w, 0.85f * globalOpacity);
				m_brush->SetColor(borderCol);
				m_renderTarget->DrawRoundedRectangle(&rrPanel, m_brush.Get(), 1.5f);

				// Main text: message (bottom text)
				if (m_textFormatMain) {
					m_textFormatMain->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
					m_textFormatMain->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
				}
				float4 bottomTextCol = flagIsDark ? float4(1, 1, 1, 0.95f * globalOpacity)
					: float4(0, 0, 0, 0.95f * globalOpacity);
				m_brush->SetColor(bottomTextCol);
				m_text.render(
					m_renderTarget.Get(),
					sBottom,
					m_textFormatMain.Get(),
					rPanel.left + innerPad,
					rPanel.right - innerPad,
					(rPanel.top + rPanel.bottom) * 0.5f,
					m_brush.Get(),
					DWRITE_TEXT_ALIGNMENT_CENTER,
					getGlobalFontSpacing()
				);
			}
		}

		m_renderTarget->EndDraw();
	}

private:

	void ensureStyleBrushes()
	{
		if (!m_renderTarget) return;
		if (m_bgBrush && m_panelBrush) return;

		// Card background gradient (same palette as OverlayPit)
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

		// Inner panel gradient (same palette as OverlayPit)
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

	struct FlagInfo
	{
		bool active = false;
		std::string topText;
		std::string bottomText;
		float4 color = float4(1,1,1,1);
	};

	static inline float4 col(float r,float g,float b,float a=1.0f){ return float4(r,g,b,a); }

	FlagInfo resolveActiveFlag()
	{
		FlagInfo out;
		// Preview override
		if (preview_mode_get())
		{
			auto set = [&](const char* top, const char* bottom, const float4& c)->FlagInfo{ FlagInfo f; f.active=true; f.topText=top; f.bottomText=bottom; f.color=c; return f; };
			return set("GO!","Green Green!!", col(0.1f,0.9f,0.1f));
		}

		const int flags = ir_SessionFlags.getInt();
		const int sessionState = ir_SessionState.getInt();
		
		// Helper lambda to set and return
		auto set = [&](const char* top, const char* bottom, const float4& c)->FlagInfo{
			FlagInfo f; f.active=true; f.topText=top; f.bottomText=bottom; f.color=c; return f;
		};
		
		// Helper to check if we're in a race session (not practice/qualify)
		auto isRaceSession = [&]() -> bool {
			return ir_session.sessionType == SessionType::RACE;
		};
		
		// Helper to check if we're in a starting sequence
		auto isStartingSequence = [&]() -> bool {
			return sessionState == irsdk_StateWarmup || 
			       sessionState == irsdk_StateParadeLaps ||
			       sessionState == irsdk_StateGetInCar;
		};

		// Priority order following iRacing SDK categories:
		// 1. Driver Black Flags (highest priority - individual penalties)
		if( flags & irsdk_disqualify ) return set("DISQUALIFIED","You are disqualified", col(0,0,0));
		if( flags & irsdk_black )      return set("PENALTY","Black Flag", col(0,0,0));
		if( flags & irsdk_repair )     return set("REQUIRED REPAIR","Meatball Flag", col(1.0f,0.4f,0.0f));
		if( flags & irsdk_furled )     return set("CUTTING TRACK","Furled Flag", col(1.0f,0.6f,0.0f));
		
		// 2. Critical Session Flags (session-stopping conditions)
		if( flags & irsdk_red )        return set("SESSION SUSPENDED","Red Flag", col(1,0,0));
		
		// 3. Active Racing Condition Flags (immediate track conditions)
		if( flags & irsdk_yellowWaving ) return set("ACCIDENT AHEAD","Yellow Waving", col(1,1,0));
		if( flags & irsdk_cautionWaving ) return set("CAUTION","Caution Waving", col(1,1,0));
		if( flags & irsdk_yellow )      return set("CAUTION","Yellow Flag", col(1,1,0));
		if( flags & irsdk_caution )    return set("CAUTION","Caution Flag", col(1,1,0));
		if( flags & irsdk_debris )     return set("DEBRIS ON TRACK","Debris Flag", col(1.0f,0.5f,0.0f));
		if( flags & irsdk_blue )       return set("LET OTHERS BY","Blue Flag", col(0.1f,0.4f,1.0f));
		
		// 4. Session Status Flags (race progress indicators)
		if( flags & irsdk_checkered )  return set("SESSION FINISHED","Checkered Flag", col(1,1,1));
		if( flags & irsdk_white )      return set("FINAL LAP","White Flag", col(1,1,1));
		if( flags & irsdk_green )      return set("RACING","Green Flag", col(0.1f,0.9f,0.1f));
		if( flags & irsdk_greenHeld )  return set("GREEN HELD","Green Flag Held", col(0.1f,0.9f,0.1f));
		
		// 5. Start Light Sequence (during race start only)
		if( isStartingSequence() ) {
			if( flags & irsdk_startGo )    return set("GO!","Green Green!!", col(0.1f,0.9f,0.1f));
			if( flags & irsdk_startSet )   return set("SET","Start Lights ", col(1.0f,0.9f,0.0f));
			if( flags & irsdk_startReady ) return set("GET READY","Start Lights", col(1,0,0));
		}
		
		// 6. Session Information Flags (context-sensitive)
		if( isRaceSession() ) {
			// Race-specific flags
			if( flags & irsdk_oneLapToGreen ) return set("ONE LAP TO GREEN","Session Info", col(1,1,1));
			if( flags & irsdk_tenToGo )    return set("10 LAPS TO GO","Session Info", col(1,1,1));
			if( flags & irsdk_fiveToGo )   return set("5 LAPS TO GO","Session Info", col(1,1,1));
		}
		
		// General flags (all session types)
		if( flags & irsdk_randomWaving ) return set("RANDOM WAVING","Random Waving", col(1,1,1));
		if( flags & irsdk_crossed )    return set("CROSSED","Crossed Flag", col(0.7f,0.7f,0.7f));

		// No flag condition
		out.active = false;
		return out;
	}

protected:

	Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormatTop;
	Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormatMain;
	TextCache m_text;
	float m_fontSpacing = getGlobalFontSpacing();

	// Styling brushes (cached; recreated on config change / enable)
	Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_bgBrush;
	Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_panelBrush;
	Microsoft::WRL::ComPtr<ID2D1Layer> m_bannerClipLayer;
};