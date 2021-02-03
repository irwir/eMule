///////////////////////////////////////////////////////////////////////////////
// class CProgressCtrlX
//
// Author:  Yury Goltsman
// email:   ygprg@go.to
// page:    http://go.to/ygprg
// Copyright © 2000, Yury Goltsman
//
// This code provided "AS IS," without warranty of any kind.
// You may freely use or modify this code provided this
// Copyright is included in all derived versions.
//
// version : 1.1
// Added multi-color gradient
// Added filling with brush for background and bar(overrides color settings)
// Added borders attribute
// Added vertical text support
// Added snake mode
// Added reverse mode
// Added dual color for text
// Added text formatting
// Added tied mode for text and rubber bar mode
// Added support for vertical oriented control(PBS_VERTICAL)
//
// version : 1.0
//
#include "stdafx.h"
#include "DrawGdiX.h"
#include "MemDC.h"
#include "ProgressCtrlX.h"
#include "otherfunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CProgressCtrlX

CProgressCtrlX::CProgressCtrlX()
	: /*m_pbrBar()
	, m_pbrBk()
	,*/ m_clrBk(::GetSysColor(COLOR_3DFACE))
	, m_clrTextOnBar(::GetSysColor(COLOR_CAPTIONTEXT))
	, m_clrTextOnBk(::GetSysColor(COLOR_BTNTEXT))
	, m_nTail()
	, m_nTailSize(40)
	, m_nStep(10)	// according to msdn
	, m_bEmpty()
{
	// set gradient colors
	COLORREF clrStart, clrEnd;
	clrStart = clrEnd = ::GetSysColor(COLOR_ACTIVECAPTION);
	SetGradientColors(clrStart, clrEnd);
}

BEGIN_MESSAGE_MAP(CProgressCtrlX, CProgressCtrl)
	//{{AFX_MSG_MAP(CProgressCtrlX)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_MESSAGE(PBM_SETBARCOLOR, OnSetBarColor)
	ON_MESSAGE(PBM_SETBKCOLOR, OnSetBkColor)
	ON_MESSAGE(PBM_SETPOS, OnSetPos)
	ON_MESSAGE(PBM_DELTAPOS, OnDeltaPos)
	ON_MESSAGE(PBM_STEPIT, OnStepIt)
	ON_MESSAGE(PBM_SETSTEP, OnSetStep)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CProgressCtrlX message handlers

BOOL CProgressCtrlX::OnEraseBkgnd(CDC* /*pDC*/)
{
	// TODO: Add your message handler code here and/or call default
	return TRUE; // erase in OnPaint()
}

void CProgressCtrlX::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	// TODO: Add your message handler code here
	CDrawInfo info;
	GetClientRect(&info.rcClient);

	// retrieve current position and range
	if (!m_bEmpty) {
		info.dwStyle = GetStyle();
		info.nCurPos = GetPos();
		GetRange(info.nLower, info.nUpper);
	} else {
		info.dwStyle = 0;
		info.nCurPos = 0;
		info.nLower = 0;
		info.nUpper = 0;
	}

	// Draw to memory DC
	CMemoryDC memDC(&dc);
	info.pDC = &memDC;

	// fill background
	memDC.FillSolidRect(&info.rcClient, m_clrBk);

	// apply borders
	info.rcClient.DeflateRect(m_rcBorders);

	// if current pos is out of range - return
	if (info.nCurPos < info.nLower || info.nCurPos > info.nUpper)
		return;

	bool fVert = info.dwStyle & PBS_VERTICAL;
	bool fSnake = info.dwStyle & PBS_SNAKE;
	bool fRubberBar = info.dwStyle & PBS_RUBBER_BAR;

	// calculate visible gradient width
	int range = max(info.nUpper - info.nLower, 1);
	CRect rcBar, rcMax;
	rcMax.right = fVert ? info.rcClient.Height() : info.rcClient.Width();
	rcBar.right = (LONG)((float)(info.nCurPos - info.nLower) * rcMax.right / range);
	if (fSnake)
		rcBar.left = (LONG)((float)(m_nTail - info.nLower) * rcMax.right / range);

	// draw bar
	DrawMultiGradient(info, fRubberBar ? rcBar : rcMax, rcBar);

	// Draw text
	DrawText(info, rcMax, rcBar);

	// Do not call CProgressCtrl::OnPaint() for painting messages
}

void CProgressCtrlX::DrawMultiGradient(const CDrawInfo &info, const CRect &rcGrad, const CRect &rcClip)
{
	INT_PTR nSteps = m_ardwGradColors.GetCount() - 1;
	float nWidthPerStep = (float)rcGrad.Width() / nSteps;
	RECT rcGradBand(rcGrad);
	for (INT_PTR i = 0; i < nSteps; ++i) {
		rcGradBand.left = rcGrad.left + (LONG)(nWidthPerStep * i);
		if (i < nSteps - 1)
			rcGradBand.right = rcGrad.left + (LONG)(nWidthPerStep * (i + 1));
		else //last step may have rounding errors
			rcGradBand.right = rcGrad.right;

		if (rcGradBand.right < rcClip.left)
			continue; // skip bands before theclipping rect

		RECT rcClipBand(rcGradBand);
		if (rcClipBand.left < rcClip.left)
			rcClipBand.left = rcClip.left;
		if (rcClipBand.right > rcClip.right)
			rcClipBand.right = rcClip.right;

		DrawGradient(info, rcGradBand, rcClipBand, m_ardwGradColors[i], m_ardwGradColors[i + 1]);

		if (rcClipBand.right == rcClip.right)
			break; // stop filling - next band is out of clipping rect
	}
}

void CProgressCtrlX::DrawGradient(const CDrawInfo &info, const CRect &rcGrad, const CRect &rcClip, COLORREF clrStart, COLORREF clrEnd)
{
	// Split colors to RGB channels, find channel with the maximum difference
	// between the start and end colors. This distance will determine
	// number of steps for gradient
	int r = GetRValue(clrEnd) - GetRValue(clrStart);
	int g = GetGValue(clrEnd) - GetGValue(clrStart);
	int b = GetBValue(clrEnd) - GetBValue(clrStart);
	int nSteps = maxi(abs(r), maxi(abs(g), abs(b)));
	// if number of pixels in gradient is less than number of steps -
	// use it as the number of steps
	int nPixels = rcGrad.Width();
	nSteps = min(nPixels, nSteps);
	if (nSteps == 0)
		nSteps = 1;

	float rStep = (float)r / nSteps;
	float gStep = (float)g / nSteps;
	float bStep = (float)b / nSteps;

	r = GetRValue(clrStart);
	g = GetGValue(clrStart);
	b = GetBValue(clrStart);

	float nWidthPerStep = (float)rcGrad.Width() / nSteps;
	RECT rcFill(rcGrad);
	//CBrush br;
	// Start filling
	for (int i = 0; i < nSteps; ++i) {
		rcFill.left = rcGrad.left + (LONG)(nWidthPerStep * i);
		if (i < nSteps - 1)
			rcFill.right = rcGrad.left + (LONG)(nWidthPerStep * (i + 1));
		else //last step may have rounding error
			rcFill.right = rcGrad.right;

		if (rcFill.right < rcClip.left)
			continue; // skip - band before clipping rect

		// clip it
		if (rcFill.left < rcClip.left)
			rcFill.left = rcClip.left;
		if (rcFill.right > rcClip.right)
			rcFill.right = rcClip.right;

		COLORREF clrFill = RGB(r + (int)(i * rStep), g + (int)(i * gStep), b + (int)(i * bStep));
		info.pDC->FillSolidRect(&ConvertToReal(info, rcFill), clrFill);
		if (rcFill.right >= rcClip.right)
			break; // stop filling if we reach the current position
	}
}

void CProgressCtrlX::DrawText(const CDrawInfo &info, const CRect &rcMax, const CRect &rcBar)
{
	if (!(info.dwStyle & PBS_TEXTMASK))
		return;
	bool fVert = info.dwStyle & PBS_VERTICAL;
	CDC *pDC = info.pDC;
	int nValue;
	CString sFormat;
	GetWindowText(sFormat);
	switch (info.dwStyle & PBS_TEXTMASK) {
	case PBS_SHOW_PERCENT:
		if (sFormat.IsEmpty())
			sFormat = _T("%d%%");
		// retrieve current position and range
		nValue = info.nUpper - info.nLower;
		nValue = (int)((info.nCurPos - info.nLower) * 100.0f / (nValue > 0 ? nValue : 1));
		break;
	case PBS_SHOW_POSITION:
		if (sFormat.IsEmpty())
			sFormat = _T("%d");
		// retrieve current position
		nValue = info.nCurPos;
		break;
	default:
		if (sFormat.IsEmpty())
			return;
		nValue = 0;
	}

	CFont *pFont = GetFont();
	CSelFont sf(pDC, pFont);
	CSelTextColor tc(pDC, m_clrTextOnBar);
	CSelBkMode bm(pDC, TRANSPARENT);
	CSelTextAlign ta(pDC, TA_BOTTOM | TA_CENTER);
	CPoint ptOrg = pDC->GetWindowOrg();
	CString sText;
	sText.Format(sFormat, nValue);

	LONG grad;
	if (pFont) {
		LOGFONT lf;
		pFont->GetLogFont(&lf);
		grad = lf.lfEscapement / 10;
	} else
		grad = 0;
	int x, y, dx, dy;
	CSize sizText = pDC->GetTextExtent(sText);
	switch (grad) {
	case 0:
		x = sizText.cx;
		y = sizText.cy;
		dx = 0;
		dy = y;
		break;
	case 90:
		x = sizText.cy;
		y = sizText.cx;
		dx = x;
		dy = 0;
		break;
	case 180:
		x = sizText.cx;
		y = sizText.cy;
		dx = 0;
		dy = -y;
		break;
	case 270:
		x = sizText.cy;
		y = sizText.cx;
		dx = -x;
		dy = 0;
		break;
	default:
		x = y = dx = dy = 0;
		ASSERT(0); // angle not supported
	}

	CPoint pt = pDC->GetViewportOrg();
	if (info.dwStyle & PBS_TIED_TEXT) {
		if ((fVert ? y : x) <= rcBar.Width()) {
			CRect rcFill(ConvertToReal(info, rcBar));
			pDC->SetViewportOrg(rcFill.left + (rcFill.Width() + dx) / 2,
				rcFill.top + (rcFill.Height() + dy) / 2);
			DrawClippedText(info, rcBar, sText, ptOrg);
		}
	} else {
		pDC->SetViewportOrg(info.rcClient.left + (info.rcClient.Width() + dx) / 2,
			info.rcClient.top + (info.rcClient.Height() + dy) / 2);
		if (m_clrTextOnBar == m_clrTextOnBk)
			// if the same color for bar and background draw text once
			DrawClippedText(info, rcMax, sText, ptOrg);
		else {
			// else, draw clipped parts of text

			// draw text on gradient
			if (rcBar.left != rcBar.right)
				DrawClippedText(info, rcBar, sText, ptOrg);

			// draw text out of gradient
			if (rcMax.right > rcBar.right) {
				tc.Select(m_clrTextOnBk);
				CRect rc(rcMax);
				rc.left = rcBar.right;
				DrawClippedText(info, rc, sText, ptOrg);
			}
			if (rcMax.left < rcBar.left) {
				tc.Select(m_clrTextOnBk);
				CRect rc(rcMax);
				rc.right = rcBar.left;
				DrawClippedText(info, rc, sText, ptOrg);
			}
		}
	}
	pDC->SetViewportOrg(pt);
}

void CProgressCtrlX::DrawClippedText(const CDrawInfo &info, const CRect &rcClip, CString &sText, const CPoint &ptWndOrg)
{
	CRect rc = ConvertToReal(info, rcClip);
	rc.OffsetRect(-ptWndOrg);
	CRgn rgn;
	rgn.CreateRectRgn(rc.left, rc.top, rc.right, rc.bottom);
	CDC *pDC = info.pDC;
	pDC->SelectClipRgn(&rgn);
	pDC->TextOut(0, 0, sText);
	rgn.DeleteObject();
}

LRESULT CProgressCtrlX::OnSetBarColor(WPARAM clrEnd, LPARAM clrStart)
{
	SetGradientColors((COLORREF)clrStart, (COLORREF)(clrEnd ? clrEnd : clrStart));
	return (LRESULT)CLR_DEFAULT;
}

LRESULT CProgressCtrlX::OnSetBkColor(WPARAM, LPARAM clrBk)
{
	SetBkColor((COLORREF)clrBk);
	return (LRESULT)CLR_DEFAULT;
}

LRESULT CProgressCtrlX::OnSetStep(WPARAM nStepInc, LPARAM)
{
	m_nStep = (int)nStepInc;
	return Default();
}

LRESULT CProgressCtrlX::OnSetPos(WPARAM newPos, LPARAM)
{
	int nOldPos;
	return SetSnakePos(nOldPos, (int)newPos) ? nOldPos : Default();
}

LRESULT CProgressCtrlX::OnDeltaPos(WPARAM nIncrement, LPARAM)
{
	int nOldPos;
	return SetSnakePos(nOldPos, (int)nIncrement, TRUE) ? nOldPos : Default();
}

LRESULT CProgressCtrlX::OnStepIt(WPARAM, LPARAM)
{
	int nOldPos;
	return SetSnakePos(nOldPos, m_nStep, TRUE) ? nOldPos : Default();
}

/////////////////////////////////////////////////////////////////////////////
// CProgressCtrlX implementation

BOOL CProgressCtrlX::SetSnakePos(int &nOldPos, int nNewPos, BOOL fIncrement)
{
	DWORD dwStyle = GetStyle();
	if (!(dwStyle & PBS_SNAKE))
		return FALSE;

	int nLower, nUpper;
	GetRange(nLower, nUpper);
	if (fIncrement) {
		int nCurPos = GetPos();
		if (nCurPos == nUpper && nCurPos - m_nTail < m_nTailSize)
			nCurPos = m_nTail + m_nTailSize;
		nNewPos = nCurPos + abs(nNewPos);
	}
	if (nNewPos > nUpper + m_nTailSize) {
		nNewPos -= nUpper - nLower + m_nTailSize;
		if (nNewPos > nUpper + m_nTailSize) {
			ASSERT(0); // too far - reset
			nNewPos = nUpper + m_nTailSize;
		}
		if (dwStyle & PBS_REVERSE)
			ModifyStyle(PBS_REVERSE, 0);
		else
			ModifyStyle(0, PBS_REVERSE);
	} else if (nNewPos >= nUpper)
		Invalidate();

	m_nTail = max(nNewPos - m_nTailSize, nLower);

	nOldPos = (int)DefWindowProc(PBM_SETPOS, nNewPos, 0);
	return TRUE;
}

void CProgressCtrlX::SetTextFormat(LPCTSTR szFormat, DWORD ffFormat)
{
	ASSERT(::IsWindow(m_hWnd));

	if (szFormat && *szFormat && ffFormat) {
		ModifyStyle(PBS_TEXTMASK, ffFormat);
		SetWindowText(szFormat);
	} else {
		ModifyStyle(PBS_TEXTMASK, 0);
		SetWindowText(_T(""));
	}
}

CRect CProgressCtrlX::ConvertToReal(const CDrawInfo &info, const CRect &rcVirt)
{
	bool bReverse = info.dwStyle & PBS_REVERSE;

	CRect rc(info.rcClient);
	if (info.dwStyle & PBS_VERTICAL) {
		rc.top += (bReverse ? rcVirt.left : info.rcClient.Height() - rcVirt.right);
		rc.bottom = rc.top + rcVirt.Width();
	} else {
		rc.left += (bReverse ? info.rcClient.Width() - rcVirt.right : rcVirt.left);
		rc.right = rc.left + rcVirt.Width();
	}
	return rc;
}

void CProgressCtrlX::SetGradientColorsX(int nCount, COLORREF clrFirst, COLORREF clrNext, ...)
{
	m_ardwGradColors.SetSize(nCount);

	m_ardwGradColors[0] = clrFirst;
	m_ardwGradColors[1] = clrNext;

	va_list pArgs;
	va_start(pArgs, clrNext);
	for (int i = 2; i < nCount; ++i)
		m_ardwGradColors[i] = va_arg(pArgs, COLORREF);
	va_end(pArgs);
}