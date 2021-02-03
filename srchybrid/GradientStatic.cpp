// GradientStatic.cpp : implementation file
//

#include "stdafx.h"
#include "GradientStatic.h"
#define _USE_MATH_DEFINES
#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGradientStatic

CGradientStatic::CGradientStatic()
	: m_bInit(TRUE)
	, m_bHorizontal(TRUE)
	, m_bInvert()
	, m_crColorLB(RGB(0, 0, 0))
	, m_crColorRT(RGB(255, 255, 255))
	, m_crTextColor(RGB(127, 127, 127))
	, m_Mem()
{
}

CGradientStatic::~CGradientStatic()
{
	if (m_Mem.dc.GetSafeHdc() && m_Mem.pold)
		m_Mem.dc.SelectObject(m_Mem.pold);
	if (m_Mem.bmp.GetSafeHandle())
		m_Mem.bmp.DeleteObject();
	if (m_Mem.dc.GetSafeHdc())
		m_Mem.dc.DeleteDC();
}


BEGIN_MESSAGE_MAP(CGradientStatic, CStatic)
	//{{AFX_MSG_MAP(CGradientStatic)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGradientStatic message handlers

void CGradientStatic::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	CRect rClient;
	GetClientRect(rClient);

	if (m_bInit) {
		CreateGradient(&dc, &rClient);
		m_bInit = false;
	}

	dc.BitBlt(0, 0, m_Mem.cx, m_Mem.cy, &m_Mem.dc, 0, 0, SRCCOPY);
}

void CGradientStatic::CreateGradient(CDC *pDC, CRect *pRect)
{
	m_Mem.cx = pRect->Width();
	m_Mem.cy = pRect->Height();

	if (m_Mem.dc.GetSafeHdc()) {
		if (m_Mem.bmp.GetSafeHandle() && m_Mem.pold)
			m_Mem.dc.SelectObject(m_Mem.pold);
		m_Mem.dc.DeleteDC();
	}
	m_Mem.dc.CreateCompatibleDC(pDC);

	if (m_Mem.bmp.GetSafeHandle())
		m_Mem.bmp.DeleteObject();
	m_Mem.bmp.CreateCompatibleBitmap(pDC, m_Mem.cx, m_Mem.cy);

	m_Mem.pold = m_Mem.dc.SelectObject(&m_Mem.bmp);

//-----------------------------------------------------------------

	if (m_bHorizontal) {
		DrawHorizontalGradient();
		DrawHorizontalText(pRect);
	} else {
		DrawVerticalGradient();
		DrawVerticalText(pRect);
	}
}

void CGradientStatic::DrawHorizontalGradient()
{
	double dblRstep = (GetRValue(m_crColorRT) - GetRValue(m_crColorLB)) / static_cast<double>(m_Mem.cx);
	double dblGstep = (GetGValue(m_crColorRT) - GetGValue(m_crColorLB)) / static_cast<double>(m_Mem.cx);
	double dblBstep = (GetBValue(m_crColorRT) - GetBValue(m_crColorLB)) / static_cast<double>(m_Mem.cx);
	double r = GetRValue(m_crColorLB);
	double g = GetGValue(m_crColorLB);
	double b = GetBValue(m_crColorLB);

	for (int x = 0; x < m_Mem.cx; ++x) {
		CPen Pen(PS_SOLID, 1, RGB(r, g, b));
		CPen *pOld = m_Mem.dc.SelectObject(&Pen);
		m_Mem.dc.MoveTo(x, 0);
		m_Mem.dc.LineTo(x, m_Mem.cy);
		m_Mem.dc.SelectObject(pOld);

		r += dblRstep;
		g += dblGstep;
		b += dblBstep;
	}
}

void CGradientStatic::DrawVerticalGradient()
{
	double dblRstep = (GetRValue(m_crColorLB) - GetRValue(m_crColorRT)) / static_cast<double>(m_Mem.cy);
	double dblGstep = (GetGValue(m_crColorLB) - GetGValue(m_crColorRT)) / static_cast<double>(m_Mem.cy);
	double dblBstep = (GetBValue(m_crColorLB) - GetBValue(m_crColorRT)) / static_cast<double>(m_Mem.cy);
	double r = GetRValue(m_crColorRT);
	double g = GetGValue(m_crColorRT);
	double b = GetBValue(m_crColorRT);

	for (int y = 0; y < m_Mem.cy; ++y) {
		CPen Pen(PS_SOLID, 1, RGB(r, g, b));
		CPen *pOld = m_Mem.dc.SelectObject(&Pen);
		m_Mem.dc.MoveTo(0, y);
		m_Mem.dc.LineTo(m_Mem.cx, y);
		m_Mem.dc.SelectObject(pOld);

		r += dblRstep;
		g += dblGstep;
		b += dblBstep;
	}
}

void CGradientStatic::DrawHorizontalText(CRect *pRect)
{
	CFont *pOldFont = m_cfFont.GetSafeHandle() ? m_Mem.dc.SelectObject(&m_cfFont) : NULL;
	CString strText;
	GetWindowText(strText);

	m_Mem.dc.SetTextColor(m_crTextColor);
	m_Mem.dc.SetBkMode(TRANSPARENT);
	m_Mem.dc.DrawText(strText, pRect, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
	if (pOldFont)
		m_Mem.dc.SelectObject(pOldFont);
}

void DrawRotatedText(HDC hdc, LPCTSTR str, LPRECT rect, double angle, UINT nOptions = 0)
{
	// convert angle to radian
	double radian = M_PI * 2 / 360 * angle;

	// get the center of a not-rotated text
	SIZE TextSize;
	GetTextExtentPoint32(hdc, str, (int)_tcslen(str), &TextSize);

	POINT center;
	center.x = TextSize.cx / 2;
	center.y = TextSize.cy / 2;

	// now calculate the center of the rotated text
	POINT rcenter;
	rcenter.x = long(cos(radian) * center.x - sin(radian) * center.y);
	rcenter.y = long(sin(radian) * center.x + cos(radian) * center.y);

	// finally draw the text and move it to the center of the rectangle
	::SetTextAlign(hdc, TA_BOTTOM);
	::SetBkMode(hdc, TRANSPARENT);
	::ExtTextOut(hdc, rect->left + (rect->right - rect->left) / 2 - rcenter.x,
		rect->bottom, nOptions, rect, str, (UINT)_tcslen(str), NULL);
}

void CGradientStatic::DrawVerticalText(CRect *pRect)
{
	LOGFONT lfFont;
	if (m_cfFont.GetSafeHandle())
		m_cfFont.GetLogFont(&lfFont);
	else {
		CFont *pFont = GetFont();
		pFont->GetLogFont(&lfFont);
		_tcscpy(lfFont.lfFaceName, _T("Arial"));	// some fonts won't turn :(
	}
	lfFont.lfEscapement = 900;

	CFont Font;
	Font.CreateFontIndirect(&lfFont);
	CFont *pOldFont = m_Mem.dc.SelectObject(&Font);

	CString strText;
	GetWindowText(strText);

	m_Mem.dc.SetTextColor(m_crTextColor);
	m_Mem.dc.SetBkColor(TRANSPARENT);
	CRect rText = pRect;
	rText.bottom -= 5;
	DrawRotatedText(m_Mem.dc.m_hDC, strText, rText, 90);

	m_Mem.dc.SelectObject(pOldFont);
}

void CGradientStatic::SetFont(CFont *pFont)
{
	LOGFONT lfFont;
	pFont->GetLogFont(&lfFont);

	if (m_cfFont.GetSafeHandle())
		m_cfFont.DeleteObject();
	m_cfFont.CreateFontIndirect(&lfFont);
}