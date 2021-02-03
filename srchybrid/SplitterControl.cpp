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
#include "SplitterControl.h"
#include "UserMsgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CSplitterControl

HCURSOR CSplitterControl::m_hcurMoveVert = NULL;
HCURSOR CSplitterControl::m_hcurMoveHorz = NULL;

BEGIN_MESSAGE_MAP(CSplitterControl, CStatic)
	ON_WM_PAINT()
	ON_WM_MOUSEMOVE()
	ON_WM_SETCURSOR()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

CSplitterControl::CSplitterControl()
	: m_bIsPressed()
	, m_nType()
	, m_nX()
	, m_nY()
	, m_nMin(-1)
	, m_nMax(-1)
	, m_nSavePos()
	, m_bDrawBorder()
{
}

void CSplitterControl::Create(DWORD dwStyle, const CRect &rect, CWnd *pParent, UINT nID)
{
	CRect rc = rect;
	dwStyle |= SS_NOTIFY;

	// Determine default type base on its size.
	m_nType = (rc.Width() < rc.Height()) ? SPS_VERTICAL : SPS_HORIZONTAL;

	if (m_nType == SPS_VERTICAL)
		rc.right = rc.left + 4;
	else // SPS_HORIZONTAL
		rc.bottom = rc.top + 4;

	CStatic::Create(_T(""), dwStyle, rc, pParent, nID);

	if (!m_hcurMoveVert) {
		m_hcurMoveVert = AfxGetApp()->LoadStandardCursor(IDC_SIZEWE);
		m_hcurMoveHorz = AfxGetApp()->LoadStandardCursor(IDC_SIZENS);
	}

	// force the splitter not to be split.
	SetRange(0, 0, -1);
}

int CSplitterControl::SetStyle(int nStyle)
{
	int m_nOldStyle = m_nType;
	m_nType = nStyle;
	return m_nOldStyle;
}

int CSplitterControl::GetStyle()
{
	return m_nType;
}

void CSplitterControl::SetDrawBorder(bool bEnable)
{
	m_bDrawBorder = bEnable;
}

void CSplitterControl::OnPaint()
{
	if (m_bDrawBorder) {
		CPaintDC dc(this); // device context for painting
		RECT rcClient;
		GetClientRect(&rcClient);
		dc.FillSolidRect(&rcClient, ::GetSysColor(COLOR_3DFACE));
		dc.Draw3dRect(&rcClient, ::GetSysColor(COLOR_BTNHIGHLIGHT), ::GetSysColor(COLOR_BTNSHADOW));
	} else
		CStatic::OnPaint();
}

void CSplitterControl::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bIsPressed) {
		CWindowDC dc(NULL);
		DrawLine(&dc, m_nX, m_nY);

		CPoint pt = point;
		ClientToScreen(&pt);
		GetParent()->ScreenToClient(&pt);

		if (pt.x < m_nMin)
			pt.x = m_nMin;
		if (pt.y < m_nMin)
			pt.y = m_nMin;

		if (pt.x > m_nMax)
			pt.x = m_nMax;
		if (pt.y > m_nMax)
			pt.y = m_nMax;

		GetParent()->ClientToScreen(&pt);
		m_nX = pt.x;
		m_nY = pt.y;
		DrawLine(&dc, m_nX, m_nY);
	}
	CStatic::OnMouseMove(nFlags, point);
}

BOOL CSplitterControl::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message)
{
	if (nHitTest == HTCLIENT) {
		::SetCursor((m_nType == SPS_VERTICAL) ? m_hcurMoveVert : m_hcurMoveHorz);
		return 0;
	}
	return CStatic::OnSetCursor(pWnd, nHitTest, message);
}

void CSplitterControl::OnLButtonDown(UINT nFlags, CPoint point)
{
	CStatic::OnLButtonDown(nFlags, point);

	m_bIsPressed = TRUE;
	SetCapture();
	CRect rcWnd;
	GetWindowRect(rcWnd);

	if (m_nType == SPS_VERTICAL)
		m_nX = rcWnd.left + rcWnd.Width() / 2;
	else
		m_nY = rcWnd.top + rcWnd.Height() / 2;

	if (m_nType == SPS_VERTICAL)
		m_nSavePos = m_nX;
	else
		m_nSavePos = m_nY;

	CWindowDC dc(NULL);
	DrawLine(&dc, m_nX, m_nY);
}

void CSplitterControl::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bIsPressed) {
		ClientToScreen(&point);
		CWindowDC dc(NULL);

		DrawLine(&dc, m_nX, m_nY);
		CPoint pt(m_nX, m_nY);
		m_bIsPressed = FALSE;
		CWnd *pOwner = GetOwner();
		if (pOwner && ::IsWindow(pOwner->m_hWnd)) {
			pOwner->ScreenToClient(&pt);
			MoveWindowTo(pt);

			int delta = (m_nType == SPS_VERTICAL ? m_nX : m_nY) - m_nSavePos;
\
			if (delta != 0) {
				SPC_NMHDR nmsp;
				nmsp.hdr.hwndFrom = m_hWnd;
				nmsp.hdr.idFrom = GetDlgCtrlID();
				nmsp.hdr.code = UM_SPN_SIZED;
				nmsp.delta = delta;
				pOwner->SendMessage(WM_NOTIFY, nmsp.hdr.idFrom, (LPARAM)&nmsp);
			}
		}
	}

	CStatic::OnLButtonUp(nFlags, point);
	ReleaseCapture();
}

void CSplitterControl::DrawLine(CDC *pDC, int /*x*/, int /*y*/)
{
	int nRop = pDC->SetROP2(R2_NOTXORPEN);

	RECT rcWnd;
	GetWindowRect(&rcWnd);

	CPen pen;
	pen.CreatePen(0, 1, RGB(200, 200, 200));
	CPen *pOP = pDC->SelectObject(&pen);

	int d = 1;
	if (m_nType == SPS_VERTICAL) {
		pDC->MoveTo(m_nX - d, rcWnd.top);
		pDC->LineTo(m_nX - d, rcWnd.bottom);

		pDC->MoveTo(m_nX + d, rcWnd.top);
		pDC->LineTo(m_nX + d, rcWnd.bottom);
	} else // m_nType == SPS_HORIZONTAL
	{
		pDC->MoveTo(rcWnd.left, m_nY - d);
		pDC->LineTo(rcWnd.right, m_nY - d);

		pDC->MoveTo(rcWnd.left, m_nY + d);
		pDC->LineTo(rcWnd.right, m_nY + d);
	}
	pDC->SetROP2(nRop);
	pDC->SelectObject(pOP);
}

void CSplitterControl::MoveWindowTo(CPoint pt)
{
	CRect rc;
	GetWindowRect(rc);

	CWnd *pParent;
	pParent = GetParent();
	if (!pParent || !::IsWindow(pParent->m_hWnd))
		return;

	pParent->ScreenToClient(rc);
	if (m_nType == SPS_VERTICAL) {
		int nMidX = (rc.left + rc.right) / 2;
		int dx = pt.x - nMidX;
		rc.OffsetRect(dx, 0);
	} else {
		int nMidY = (rc.top + rc.bottom) / 2;
		int dy = pt.y - nMidY;
		rc.OffsetRect(0, dy);
	}
	MoveWindow(rc);
}

void CSplitterControl::ChangeWidth(CWnd *pWnd, int dx, DWORD dwFlag)
{
	CWnd *pParent = pWnd->GetParent();
	if (pParent && ::IsWindow(pParent->m_hWnd)) {
		RECT rcWnd;
		pWnd->GetWindowRect(&rcWnd);
		pParent->ScreenToClient(&rcWnd);
		if (dwFlag == CW_LEFTALIGN)
			rcWnd.right += dx;
		else if (dwFlag == CW_RIGHTALIGN)
			rcWnd.left -= dx;
		pWnd->MoveWindow(&rcWnd);
	}
}

void CSplitterControl::ChangeHeight(CWnd *pWnd, int dy, DWORD dwFlag)
{
	CWnd *pParent = pWnd->GetParent();
	if (pParent && ::IsWindow(pParent->m_hWnd)) {
		RECT rcWnd;
		pWnd->GetWindowRect(&rcWnd);
		pParent->ScreenToClient(&rcWnd);
		if (dwFlag == CW_TOPALIGN)
			rcWnd.bottom += dy;
		else if (dwFlag == CW_BOTTOMALIGN)
			rcWnd.top -= dy;
		pWnd->MoveWindow(&rcWnd);
	} else
		ASSERT(0);
}

void CSplitterControl::ChangePos(CWnd *pWnd, int dx, int dy)
{
	CWnd *pParent = pWnd->GetParent();
	if (pParent && ::IsWindow(pParent->m_hWnd)) {
		CRect rcWnd;
		pWnd->GetWindowRect(rcWnd);
		pParent->ScreenToClient(rcWnd);
		rcWnd.OffsetRect(-dx, dy);
		pWnd->MoveWindow(rcWnd);
	}
}

void CSplitterControl::SetRange(int nMin, int nMax)
{
	m_nMin = nMin;
	m_nMax = nMax;
}

// Set splitter range from (nRoot - nSubtraction) to (nRoot + nAddition)
// If (nRoot < 0)
//		nRoot = <current position of the splitter>
void CSplitterControl::SetRange(int nSubtraction, int nAddition, int nRoot)
{
	if (nRoot < 0) {
		CRect rcWnd;
		GetWindowRect(rcWnd);
		if (m_nType == SPS_VERTICAL)
			nRoot = rcWnd.left + rcWnd.Width() / 2;
		else // if m_nType == SPS_HORIZONTAL
			nRoot = rcWnd.top + rcWnd.Height() / 2;
	}
	m_nMin = nRoot - nSubtraction;
	m_nMax = nRoot + nAddition;
}