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
/*
	Edit Control with Combined Reset and Column Selector Button (as seen on Thunderbird)
	TODO:
		- Handle font changes etc properly
		- maybe save filter settings (?)
		- maybe keyboard shortcuts
*/
#include "stdafx.h"
#include "EditDelayed.h"
#include "UserMsgs.h"
#include "emule.h"
#include "MenuCmds.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define DELAYED_EVALUATE_TIMER_ID	1
#define ICON_LEFTSPACE				20


BEGIN_MESSAGE_MAP(CEditDelayed, CEdit)
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_TIMER()
	ON_CONTROL_REFLECT(EN_CHANGE, OnEnChange)
	ON_WM_DESTROY()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_SETCURSOR()
	ON_WM_MOUSEMOVE()
	ON_WM_CTLCOLOR_REFLECT()
	ON_WM_SIZE()
END_MESSAGE_MAP()

CEditDelayed::CEditDelayed()
	: m_bShuttingDown()
	, m_uTimerResult()
	, m_dwLastModified()
	, m_hCursor()
	, m_bShowResetButton()
	, m_bShowsColumnText()
	, m_nCurrentColumnIdx()
	, m_pctrlColumnHeader()
{
}

void CEditDelayed::OnDestroy()
{
	if (m_uTimerResult != 0)
		VERIFY(KillTimer(DELAYED_EVALUATE_TIMER_ID));

	// WM_DESTROY sends another WM_SETFOCUS/WM_KILLFOCUS to the window!?
	m_bShuttingDown = true;
	CEdit::OnDestroy();
}

void CEditDelayed::OnTimer(UINT_PTR nIDEvent)
{
	//ASSERT( nIDEvent == DELAYED_EVALUATE_TIMER_ID );
	if (nIDEvent == DELAYED_EVALUATE_TIMER_ID) {
		DWORD dwTick = ::GetTickCount();
		if (dwTick >= m_dwLastModified + 400) {
			DoDelayedEvalute();
			m_dwLastModified = dwTick;
		}
	}

	CEdit::OnTimer(nIDEvent);
}

void CEditDelayed::OnSetFocus(CWnd *pOldWnd)
{
	CEdit::OnSetFocus(pOldWnd);

	if (!m_bShuttingDown) {
		// Create timer
		ASSERT(m_uTimerResult == 0);
		m_uTimerResult = SetTimer(DELAYED_EVALUATE_TIMER_ID, 100, NULL);
		ASSERT(m_uTimerResult != 0);

		ShowColumnText(false);
	}
}

void CEditDelayed::OnKillFocus(CWnd *pNewWnd)
{
	if (!m_bShuttingDown) {
		// Kill timer
		ASSERT(m_uTimerResult != 0);
		VERIFY(KillTimer(DELAYED_EVALUATE_TIMER_ID));
		m_uTimerResult = 0;

		// If there was something modified since the last evaluation.
		DoDelayedEvalute();

		if (GetWindowTextLength() == 0)
			ShowColumnText(true);
	}

	CEdit::OnKillFocus(pNewWnd);
}

void CEditDelayed::OnEnChange()
{
	if (m_uTimerResult != 0) {
		// Edit control contents were changed while the control was active (had focus)
		ASSERT(GetFocus() == this);
		m_dwLastModified = ::GetTickCount();
	} else {
		// Edit control contents were changed while the control was not active (e.g.
		// someone called 'SetWindowText' from within an other window).
		ASSERT(GetFocus() != this);
		DoDelayedEvalute();
	}
	if (GetWindowTextLength() == 0 && m_bShowResetButton) {
		m_iwReset.ShowIcon(0);
		m_bShowResetButton = false;
		SetEditRect(true);
		m_iwReset.ShowWindow(SW_HIDE);
	} else if (GetWindowTextLength() > 0 && !m_bShowResetButton) {
		m_bShowResetButton = true;
		SetEditRect(true);
		m_iwReset.ShowWindow(SW_SHOW);
	}
}

void CEditDelayed::DoDelayedEvalute(bool bForce)
{
	if (m_bShowsColumnText) {
		ASSERT(0);
		return;
	}

	// Fire 'evaluate' event only, if content really has changed.
	CString strContent;
	GetWindowText(strContent);
	if (m_strLastEvaluatedContent == strContent && !bForce)
		return;
	m_strLastEvaluatedContent = strContent;
	GetParent()->SendMessage(UM_DELAYED_EVALUATE, (WPARAM)m_nCurrentColumnIdx, (LPARAM)(LPCTSTR)m_strLastEvaluatedContent);
}

void CEditDelayed::OnInit(CHeaderCtrl *pColumnHeader, CArray<int, int> *paIgnoredColumns)
{
	SetEditRect(false);
	RECT rectWindow;
	GetClientRect(&rectWindow);

	m_pctrlColumnHeader = pColumnHeader;
	m_hCursor = ::LoadCursor(NULL, IDC_ARROW);
	m_nCurrentColumnIdx = 0;

	CImageList *pImageList = new CImageList();
	pImageList->Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	if (pColumnHeader != NULL)
		pImageList->Add(CTempIconLoader(_T("SEARCHEDIT")));
	else
		pImageList->Add(CTempIconLoader(_T("KADNODESEARCH")));
	m_iwColumn.SetImageList(pImageList);
	m_iwColumn.Create(_T(""), WS_CHILD | WS_VISIBLE, CRect(0, 0, ICON_LEFTSPACE, rectWindow.bottom), this, 1);

	pImageList = new CImageList();
	pImageList->Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	pImageList->Add(CTempIconLoader(_T("FILTERCLEAR1")));
	pImageList->Add(CTempIconLoader(_T("FILTERCLEAR2")));
	m_iwReset.SetImageList(pImageList);
	m_iwReset.Create(_T(""), WS_CHILD, CRect(0, 0, ICON_LEFTSPACE, rectWindow.bottom), this, 1);

	if (paIgnoredColumns != NULL)
		m_aIgnoredColumns.Copy(*paIgnoredColumns);
	ShowColumnText(true);
}

void CEditDelayed::SetEditRect(bool bUpdateResetButtonPos, bool bUpdateColumnButton)
{
	ASSERT(GetStyle() & ES_MULTILINE);

	RECT editRect;
	GetClientRect(&editRect);

	editRect.left += ICON_LEFTSPACE;

	if (m_bShowResetButton)
		editRect.right -= 20;
	SetRect(&editRect);

	if (m_bShowResetButton && bUpdateResetButtonPos)
		m_iwReset.MoveWindow(editRect.right + 1, 0, 16, editRect.bottom);
	if (bUpdateColumnButton)
		m_iwColumn.MoveWindow(0, 0, ICON_LEFTSPACE, editRect.bottom);
}

void CEditDelayed::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (m_pctrlColumnHeader != NULL) {
		if (point.x <= ICON_LEFTSPACE) {
			// construct a pop-up menu out of the column header for the filter setting
			CMenu menu;
			menu.CreatePopupMenu();

			TCHAR szBuffer[256];
			HDITEM hdi;
			hdi.mask = HDI_TEXT | HDI_WIDTH;
			hdi.pszText = szBuffer;
			hdi.cchTextMax = _countof(szBuffer);
			int nCount = m_pctrlColumnHeader->GetItemCount();
			for (int i = 0; i < nCount; ++i) {
				int nIdx = m_pctrlColumnHeader->OrderToIndex(i);
				m_pctrlColumnHeader->GetItem(nIdx, &hdi);
				szBuffer[_countof(szBuffer) - 1] = _T('\0');
				bool bIgnored = false;
				for (int j = 0; j < m_aIgnoredColumns.GetCount(); ++j)
					if (m_aIgnoredColumns[j] == nIdx) {
						bIgnored = true;
						break;
					}
				if (hdi.cxy > 0 && !bIgnored) // ignore hidden columns
					menu.AppendMenu(MF_STRING | ((m_nCurrentColumnIdx == nIdx) ? MF_CHECKED : MF_UNCHECKED), MP_FILTERCOLUMNS + nIdx, hdi.pszText);
			}

			// draw the menu on a fixed position so it doesn't hide the input text
			RECT editRect;
			GetClientRect(&editRect);
			POINT pointMenu = { 2, editRect.bottom };
			ClientToScreen(&pointMenu);
			menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pointMenu.x, pointMenu.y, this);
			return;
		}
	}

	RECT editRect;
	GetClientRect(&editRect);
	if (m_pointMousePos.x > editRect.right - ICON_LEFTSPACE && m_bShowResetButton) {
		m_iwReset.ShowIcon(1);
		SetCapture();
		return;
	}

	CEdit::OnLButtonDown(nFlags, point);
}

void CEditDelayed::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bShowResetButton) {
		m_iwReset.ShowIcon(0);
		ReleaseCapture();

		RECT editRect;
		GetClientRect(&editRect);
		if (m_pointMousePos.x > editRect.right - ICON_LEFTSPACE) {
			SetWindowText(_T(""));
			DoDelayedEvalute();
			m_bShowResetButton = false;
			SetEditRect(true);
			m_iwReset.ShowWindow(SW_HIDE);
			SetFocus();
			return;
		}
	}
	CEdit::OnLButtonUp(nFlags, point);
}

BOOL CEditDelayed::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message)
{
	// show an arrow when hovering over any self-made buttons instead of the caret
	if (nHitTest == HTCLIENT) {
		RECT editRect;
		GetClientRect(&editRect);
		if (m_pointMousePos.x <= ICON_LEFTSPACE || (m_pointMousePos.x > editRect.right - ICON_LEFTSPACE && m_bShowResetButton)) {
			::SetCursor(m_hCursor);
			return TRUE;
		}
	}
	return CEdit::OnSetCursor(pWnd, nHitTest, message);
}

void CEditDelayed::OnMouseMove(UINT nFlags, CPoint point)
{
	m_pointMousePos = point;
	CEdit::OnMouseMove(nFlags, point);
}

// show the title of the column selected for filtering if the control is empty and has no focus
void CEditDelayed::ShowColumnText(bool bShow)
{
	if (bShow) {
		if (GetWindowTextLength() != 0 && !m_bShowsColumnText)
			return;

		m_bShowsColumnText = true;
		if (m_pctrlColumnHeader != NULL) {
			HDITEM hdi;
			TCHAR szBuffer[256];
			hdi.mask = HDI_TEXT | HDI_WIDTH;
			hdi.pszText = szBuffer;
			hdi.cchTextMax = _countof(szBuffer);
			if (m_pctrlColumnHeader->GetItem(m_nCurrentColumnIdx, &hdi)) {
				szBuffer[_countof(szBuffer) - 1] = _T('\0');
				SetWindowText(hdi.pszText);
			}
		} else
			SetWindowText(m_strAlternateText);
	} else if (m_bShowsColumnText) {
		m_bShowsColumnText = false;
		SetWindowText(_T(""));
	}
}

HBRUSH CEditDelayed::CtlColor(CDC *pDC, UINT)
{
	// Use gray text color when showing the column text so it doesn't gets confused with typed in text
	HBRUSH hbr = ::GetSysColorBrush(COLOR_WINDOW);
	pDC->SetTextColor(::GetSysColor(m_bShowsColumnText ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT));
	pDC->SetBkColor(::GetSysColor(COLOR_WINDOW));
	return hbr;
}

BOOL CEditDelayed::OnCommand(WPARAM wParam, LPARAM)
{
	wParam = LOWORD(wParam);
	if (wParam >= MP_FILTERCOLUMNS && wParam <= MP_FILTERCOLUMNS + 50) {
		if (m_nCurrentColumnIdx != (int)wParam - MP_FILTERCOLUMNS) {
			m_nCurrentColumnIdx = (int)wParam - MP_FILTERCOLUMNS;
			if (m_bShowsColumnText)
				ShowColumnText(true);
			else if (GetWindowTextLength() != 0)
				DoDelayedEvalute(true);
		}
	}
	return TRUE;
}

void CEditDelayed::OnSize(UINT nType, int cx, int cy)
{
	CEdit::OnSize(nType, cx, cy);
	SetEditRect(true, true);
}


/////////////////////////////////////////////////////////////////////////////
// CIconWnd

BEGIN_MESSAGE_MAP(CIconWnd, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

CIconWnd::CIconWnd()
{
	m_pImageList = NULL;
	m_nCurrentIcon = 0;
}

CIconWnd::~CIconWnd()
{
	delete m_pImageList;
}

void CIconWnd::OnPaint()
{
	CPaintDC dc(this);
	RECT rect;
	GetClientRect(&rect);
	dc.FillSolidRect(&rect, ::GetSysColor(COLOR_WINDOW));
	m_pImageList->Draw(&dc, m_nCurrentIcon, POINT{ 2, (rect.bottom - 16) / 2 }, ILD_NORMAL);
}

BOOL CIconWnd::OnEraseBkgnd(CDC*)
{
	return TRUE;
}

void CIconWnd::ShowIcon(int nIconNumber)
{
	if (nIconNumber == m_nCurrentIcon)
		return;
	m_nCurrentIcon = nIconNumber;
	Invalidate();
	UpdateWindow();
}