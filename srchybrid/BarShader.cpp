//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "stdafx.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include "barshader.h"
#include "Preferences.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define HALF(X) (((X) + 1) / 2)

CBarShader::CBarShader(uint32 height, uint32 width)
	: m_dPixelsPerByte()
	, m_dBytesPerPixel()
	, m_uFileSize(EMFileSize(1ull))
	, m_iWidth(width)
	, m_iHeight(height)
	, m_bIsPreview()
	, m_Modifiers()
	, m_used3dlevel()
{
	m_Spans.SetAt(0, 0);	// SLUGFILLER: speedBarShader
}

CBarShader::~CBarShader()
{
	delete[] m_Modifiers;
}

void CBarShader::Reset()
{
	Fill(0);
}

void CBarShader::BuildModifiers()
{
	delete[] m_Modifiers;
	m_Modifiers = NULL; // 'new' may throw an exception

	if (!m_bIsPreview)
		m_used3dlevel = thePrefs.Get3DDepth();

	// Barry - New property page slider to control depth of gradient

	// Depth must be at least 2
	// 2 gives greatest depth, the higher the value, the flatter the appearance
	// m_Modifiers[count-1] will always be 1, m_Modifiers[0] depends on the value of depth

	int depth = (7 - m_used3dlevel);
	int count = HALF(m_iHeight);
	double piOverDepth = M_PI / depth;
	double base = piOverDepth * ((depth / 2.0) - 1);
	double increment = piOverDepth / (count - 1);

	m_Modifiers = new float[count];
	for (int i = 0; i < count; ++i)
		m_Modifiers[i] = (float)(sin(base + i * increment));
}

void CBarShader::SetWidth(int width)
{
	if (m_iWidth != width) {
		m_iWidth = width;
		m_dPixelsPerByte = ((uint64)m_uFileSize > 0) ? (double)m_iWidth / (uint64)m_uFileSize : 0.0;
		m_dBytesPerPixel = (m_iWidth > 0) ? (uint64)m_uFileSize / (double)m_iWidth : 0.0;
	}
}

void CBarShader::SetFileSize(EMFileSize fileSize)
{
	if (m_uFileSize != fileSize) {
		m_uFileSize = fileSize;
		m_dPixelsPerByte = ((uint64)m_uFileSize > 0) ? (double)m_iWidth / (uint64)m_uFileSize : 0.0;
		m_dBytesPerPixel = (m_iWidth > 0) ? (uint64)m_uFileSize / (double)m_iWidth : 0.0;
	}
}

void CBarShader::SetHeight(int height)
{
	if (m_iHeight != height) {
		m_iHeight = height;
		BuildModifiers();
	}
}

void CBarShader::FillRange(uint64 start, uint64 end, COLORREF color)
{
	if (end > (uint64)m_uFileSize)
		end = (uint64)m_uFileSize;

	if (start >= end)
		return;

	// SLUGFILLER: speedBarShader
	POSITION endpos = m_Spans.FindFirstKeyAfter(end + 1);

	if (endpos)
		m_Spans.GetPrev(endpos);
	else
		endpos = m_Spans.GetTailPosition();

	ASSERT(endpos != NULL);

	COLORREF endcolor = m_Spans.GetValueAt(endpos);
	endpos = m_Spans.SetAt(end, endcolor);

	for (POSITION pos = m_Spans.FindFirstKeyAfter(start + 1); pos != NULL && pos != endpos;) {
		POSITION pos1 = pos;
		m_Spans.GetNext(pos);
		m_Spans.RemoveAt(pos1);
	}

	m_Spans.GetPrev(endpos);

	if (m_Spans.GetValueAt(endpos) != color)
		m_Spans.SetAt(start, color);
	// SLUGFILLER: speedBarShader
}

void CBarShader::Fill(COLORREF color)
{
// SLUGFILLER: speedBarShader
	m_Spans.RemoveAll();
	m_Spans.SetAt(0, color);
	m_Spans.SetAt(m_uFileSize, 0);
	// SLUGFILLER: speedBarShader
}

void CBarShader::Draw(CDC *dc, int iLeft, int iTop, bool bFlat)
{
	//FillSolidRect() is simpler and faster, though FillRect() did not alter background colour
	//minor additional trouble: to save and restore background colour for CMuleListCtrl
	COLORREF cBk = dc->GetBkColor();
	RECT rectSpan = { 0, iTop, iLeft, iTop + m_iHeight };
	uint64 uBytesInOnePixel = (uint64)(m_dBytesPerPixel + 0.5);
	uint64 start = 0; //bsCurrent->start;

	POSITION pos = m_Spans.GetHeadPosition();	// SLUGFILLER: speedBarShader
	COLORREF color = m_Spans.GetNextValue(pos);	// SLUGFILLER: speedBarShader
	// SLUGFILLER: speedBarShader
	while (pos != NULL && rectSpan.right < (iLeft + m_iWidth)) {	// SLUGFILLER: speedBarShader
		uint64 uSpan = m_Spans.GetKeyAt(pos) - start;	// SLUGFILLER: speedBarShader
		uint64 uPixels = (uint64)(uSpan * m_dPixelsPerByte + 0.5);
		if (uPixels > 0) {
			rectSpan.left = rectSpan.right;
			rectSpan.right += (int)uPixels;
			FillBarRect(dc, &rectSpan, color, bFlat);	// SLUGFILLER: speedBarShader
			start += (uint64)(uPixels * m_dBytesPerPixel + 0.5);
		} else {
			float fRed = 0;
			float fGreen = 0;
			float fBlue = 0;
			uint64 iEnd = start + uBytesInOnePixel;
			uint64 iLast = start;
			// SLUGFILLER: speedBarShader
			do {
				uint64 uKey = m_Spans.GetKeyAt(pos);
				float fWeight = (float)((min(uKey, iEnd) - iLast) * m_dPixelsPerByte);
				fRed += GetRValue(color) * fWeight;
				fGreen += GetGValue(color) * fWeight;
				fBlue += GetBValue(color) * fWeight;
				if (uKey > iEnd)
					break;
				iLast = uKey;
				color = m_Spans.GetNextValue(pos);
			} while (pos != NULL);
			// SLUGFILLER: speedBarShader
			rectSpan.left = rectSpan.right;
			rectSpan.right++;
			if (g_bLowColorDesktop)
				FillBarRect(dc, &rectSpan, color, bFlat);
			else
				FillBarRect(dc, &rectSpan, fRed, fGreen, fBlue, bFlat);
			start += uBytesInOnePixel;
		}
		// SLUGFILLER: speedBarShader
		while (pos != NULL && m_Spans.GetKeyAt(pos) < start)
			color = m_Spans.GetNextValue(pos);
		// SLUGFILLER: speedBarShader
	}
	dc->SetBkColor(cBk); //restore background colour
}

void CBarShader::FillBarRect(CDC *dc, LPRECT rectSpan, COLORREF color, bool bFlat)
{
	if (!color || bFlat)
		dc->FillSolidRect(rectSpan, color);
	else
		FillBarRect(dc, rectSpan, GetRValue(color), GetGValue(color), GetBValue(color), false);
}

void CBarShader::FillBarRect(CDC *dc, LPRECT rectSpan, float fRed, float fGreen, float fBlue, bool bFlat)
{
	if (bFlat)
		dc->FillSolidRect(rectSpan, RGB((int)(fRed + .5f), (int)(fGreen + .5f), (int)(fBlue + .5f)));
	else {
		if (m_Modifiers == NULL || (m_used3dlevel != thePrefs.Get3DDepth() && !m_bIsPreview))
			BuildModifiers();
		RECT rect(*rectSpan);
		int iTop = rect.top;
		int iBot = rect.bottom;
		int iMax = HALF(m_iHeight);
		for (int i = 0; i < iMax; ++i) {
			COLORREF cbNew(RGB((int)(fRed * m_Modifiers[i] + .5f), (int)(fGreen * m_Modifiers[i] + .5f), (int)(fBlue * m_Modifiers[i] + .5f)));

			rect.top = iTop + i;
			rect.bottom = iTop + i + 1;
			dc->FillSolidRect(&rect, cbNew);

			rect.top = iBot - i - 1;
			rect.bottom = iBot - i;
			dc->FillSolidRect(&rect, cbNew);
		}
	}
}

void CBarShader::DrawPreview(CDC *dc, int iLeft, int iTop, UINT previewLevel)		//Cax2 aqua bar
{
	m_bIsPreview = true;
	m_used3dlevel = previewLevel;
	BuildModifiers();
	Draw(dc, iLeft, iTop, (previewLevel == 0));
	m_bIsPreview = false;
}