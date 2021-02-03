// MeterIcon.h: interface for the CMeterIcon class.
//
// Created: 04/02/2001 {mm/dm/yyyyy}
// Written by: Anish Mistry http://am-productions.yi.org/
/* This code is licensed under the GNU GPL.  See License.txt or (http://www.gnu.org/copyleft/gpl.html). */
//////////////////////////////////////////////////////////////////////
#pragma once

class CMeterIcon
{
public:
	CMeterIcon();
	virtual	~CMeterIcon();

	bool SetColorLevels(const int *pLimits, const COLORREF *pColors, int nEntries);
	COLORREF SetBorderColor(COLORREF crColor);
	int SetNumBars(int nNum);
	int SetMaxValue(int nVal);
	int SetWidth(int nWidth);
	SIZE SetDimensions(int nWidth, int nHeight);
	bool Init(HICON hFrame, int nMaxVal, int nNumBars, int nSpacingWidth, int nWidth, int nHeight, COLORREF crColor);
	HICON Create(const int *pBarData);
	HICON SetFrame(HICON hIcon);

protected:
	SIZE m_sDimensions;
	HICON m_hFrame;
	int *m_pLimits;
	COLORREF *m_pColors;
	COLORREF m_crBorderColor;
	int m_nSpacingWidth;
	int m_nMaxVal;
	int m_nNumBars;
	int m_nEntries;
	bool m_bInit;

	bool DrawIconMeter(HDC hDestDC, HDC hDestDCMask, int nLevel, int nPos);
	HICON CreateMeterIcon(const int *pBarData);
	COLORREF GetMeterColor(int nLevel) const;
};
