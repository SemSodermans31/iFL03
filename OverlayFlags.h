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

	const float DefaultFontSize = 28.0f;

	OverlayFlags()
		: Overlay("OverlayFlags")
	{}

protected:

	virtual float2 getDefaultSize()
	{
		// Wide and not too tall – two stacked bands
		return float2(640, 200);
	}

	virtual void onConfigChanged()
	{
		// Compute scale factors from current window size
		const float2 ref = getDefaultSize();
		if( m_width <= 0 || m_height <= 0 || ref.x <= 0 || ref.y <= 0 )
		{
			m_scaleFactorX = m_scaleFactorY = m_scaleFactor = 1.0f;
		}
		else
		{
			m_scaleFactorX = (float)m_width / ref.x;
			m_scaleFactorY = (float)m_height / ref.y;
			m_scaleFactor = std::min(m_scaleFactorX, m_scaleFactorY);
			m_scaleFactor = std::max(0.5f, std::min(6.0f, m_scaleFactor));
		}

		// Fonts
		m_text.reset( m_dwriteFactory.Get() );
		const std::string font = g_cfg.getString( m_name, "font", "Poppins" );
		const float baseSize = g_cfg.getFloat( m_name, "font_size", DefaultFontSize );
		const int fontWeight = g_cfg.getInt( m_name, "font_weight", 700 );
		const std::string fontStyleStr = g_cfg.getString( m_name, "font_style", "normal");
		m_fontSpacing = g_cfg.getFloat( m_name, "font_spacing", 0.0f );
		DWRITE_FONT_STYLE fontStyle = DWRITE_FONT_STYLE_NORMAL;
		if (fontStyleStr == "italic") fontStyle = DWRITE_FONT_STYLE_ITALIC;
		else if (fontStyleStr == "oblique") fontStyle = DWRITE_FONT_STYLE_OBLIQUE;
		const float size = std::max(8.0f, std::min(220.0f, baseSize * m_scaleFactor));

		HRCHECK(m_dwriteFactory->CreateTextFormat( toWide(font).c_str(), NULL, (DWRITE_FONT_WEIGHT)fontWeight, fontStyle, DWRITE_FONT_STRETCH_EXTRA_EXPANDED, size*1.5f, L"en-us", &m_textFormatBold ));
		m_textFormatBold->SetParagraphAlignment( DWRITE_PARAGRAPH_ALIGNMENT_CENTER );
		m_textFormatBold->SetWordWrapping( DWRITE_WORD_WRAPPING_NO_WRAP );


		HRCHECK(m_dwriteFactory->CreateTextFormat( toWide(font).c_str(), NULL, (DWRITE_FONT_WEIGHT)fontWeight, fontStyle, DWRITE_FONT_STRETCH_EXTRA_EXPANDED, size*1.5f, L"en-us", &m_textFormatLarge ));
		m_textFormatLarge->SetParagraphAlignment( DWRITE_PARAGRAPH_ALIGNMENT_CENTER );
		m_textFormatLarge->SetWordWrapping( DWRITE_WORD_WRAPPING_NO_WRAP );
	}

	virtual void onUpdate()
	{
		const float4 white = float4(1,1,1,1);
		const float4 black = float4(0,0,0,1);
		const float4 darkBg = float4(0,0,0,0.7f * getGlobalOpacity());
		const float globalOpacity = getGlobalOpacity();

		wchar_t sTop[256];
		wchar_t sBottom[256];

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

		const float padding = std::max(6.0f, 16.0f * m_scaleFactor);
		const float topH = (float)m_height * 0.45f;
		const float bottomH = (float)m_height - topH;

		// Contrast helpers
		auto luminance = [](const float4& c)->float { return 0.2126f*c.x + 0.7152f*c.y + 0.0722f*c.z; };
		const bool flagIsDark = luminance(info.color) < 0.35f;

		// Build strings
		swprintf(sTop, _countof(sTop), L"%S", info.topText.c_str());
		swprintf(sBottom, _countof(sBottom), L"%S", info.bottomText.c_str());

		m_renderTarget->BeginDraw();
		m_renderTarget->Clear( float4(0,0,0,0) );

		// TOP BAND: dark background, text in flag color (or white if flag too dark)
		{
			m_brush->SetColor( float4(darkBg.x,darkBg.y,darkBg.z, darkBg.w) );
			D2D1_RECT_F r = { padding, padding, (float)m_width - padding, padding + topH };
			D2D1_ROUNDED_RECT rr = { r, topH/2, topH/2 };
			m_renderTarget->FillRoundedRectangle( &rr, m_brush.Get() );

			float4 topTextCol = flagIsDark ? white : float4(info.color.x, info.color.y, info.color.z, 1.0f);
			topTextCol.w = globalOpacity;
			m_brush->SetColor( topTextCol );
			const float yTop = padding + topH * 0.52f;
			m_text.render( m_renderTarget.Get(), sTop, m_textFormatBold.Get(), r.left + padding, r.right - padding, yTop, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );
		}

		// BOTTOM BAND: background = flag color, text = black or white for contrast
		{
			D2D1_RECT_F r = { padding, padding + topH + padding*0.5f, (float)m_width - padding, (float)m_height - padding };
			m_brush->SetColor( flagCol );
			D2D1_ROUNDED_RECT rr = { r, topH/2, topH/2 };
			m_renderTarget->FillRoundedRectangle( &rr, m_brush.Get() );

			

			float4 bottomTextCol = flagIsDark ? white : black;
			bottomTextCol.w = globalOpacity;
			m_brush->SetColor( bottomTextCol );
			const float yBottom = r.top + (r.bottom - r.top) * 0.52f;
			m_text.render( m_renderTarget.Get(), sBottom, m_textFormatLarge.Get(), r.left + padding, r.right - padding, yBottom, m_brush.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_fontSpacing );
		}

		m_renderTarget->EndDraw();
	}

private:

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
			const std::string sel = g_cfg.getString("OverlayFlags", "preview_flag", "none");
			auto set = [&](const char* top, const char* bottom, const float4& c)->FlagInfo{ FlagInfo f; f.active=true; f.topText=top; f.bottomText=bottom; f.color=c; return f; };
			if (sel == "checkered")   return set("SESSION IS OVER","Checkered Flag", col(1,1,1));
			if (sel == "white")       return set("ONE MORE LAP","White Flag", col(1,1,1));
			if (sel == "green")       return set("RESUME RACING","Green Flag", col(0.1f,0.9f,0.1f));
			if (sel == "yellow")      return set("CAUTION","Yellow Flag", col(1,1,0));
			if (sel == "yellowWaving") return set("ACCIDENT AHEAD","Yellow Waving", col(1,1,0));
			if (sel == "red")         return set("SESSION IS SUSPENDED","Red Flag", col(1,0,0));
			if (sel == "blue")        return set("LET OTHERS BY","Blue Flag", col(0.1f,0.4f,1.0f));
			if (sel == "debris")      return set("DEBRIS ON TRACK","Debris Flag", col(1.0f,0.5f,0.0f));
			if (sel == "crossed")     return set("CROSSED","Crossed Flag", col(0.7f,0.7f,0.7f));
			if (sel == "oneLapToGreen") return set("ONE LAP TO GREEN","Session Flag", col(1,1,1));
			if (sel == "greenHeld")   return set("GREEN HELD","Green Held", col(0.1f,0.9f,0.1f));
			if (sel == "tenToGo")     return set("10 LAPS TO GO","Session Flag", col(1,1,1));
			if (sel == "fiveToGo")    return set("5 LAPS TO GO","Session Flag", col(1,1,1));
			if (sel == "randomWaving") return set("RANDOM WAVING","Waving Flag", col(1,1,1));
			if (sel == "caution")     return set("CAUTION","Caution Flag", col(1,1,0));
			if (sel == "cautionWaving") return set("CAUTION","Caution Waving", col(1,1,0));
			if (sel == "black")       return set("PENALTY","Black Flag", col(0,0,0));
			if (sel == "disqualify")  return set("DISQUALIFIED","Disqualified", col(0,0,0));
			if (sel == "furled")      return set("CUTTING TRACK","Furled Flag", col(1.0f,0.6f,0.0f));
			if (sel == "repair")      return set("REQUIRED REPAIR","Meatball Flag", col(1.0f,0.4f,0.0f));
			if (sel == "startReady")  return set("GET READY","Start Ready", col(1,0,0));
			if (sel == "startSet")    return set("SET","Start Set", col(1.0f,0.9f,0.0f));
			if (sel == "startGo")     return set("GO!","Start Go", col(0.1f,0.9f,0.1f));
			out.active = false; return out;
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
			if( flags & irsdk_startGo )    return set("GO!","Start Go", col(0.1f,0.9f,0.1f));
			if( flags & irsdk_startSet )   return set("SET","Start Set", col(1.0f,0.9f,0.0f));
			if( flags & irsdk_startReady ) return set("GET READY","Start Ready", col(1,0,0));
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

	// Scaling factors
	float m_scaleFactorX = 1.0f;
	float m_scaleFactorY = 1.0f;
	float m_scaleFactor = 1.0f;

	Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormatBold;
	Microsoft::WRL::ComPtr<IDWriteTextFormat>  m_textFormatLarge;
	TextCache m_text;
	float m_fontSpacing = 0.0f;
};