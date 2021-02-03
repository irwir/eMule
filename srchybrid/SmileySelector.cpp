
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
#include "emule.h"
#include "SmileySelector.h"
#include "VisualStylesXP.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


typedef struct {
	LPCTSTR	pszResource;
	LPCTSTR pszSmileys;
} SSmiley;

SSmiley g_aSmileys[] =
{
#define ADD_SMILEY(res, sml)	{ _T( res ), _T( sml ) }
	ADD_SMILEY("SMILEY_SMILE",		":-)"),
	ADD_SMILEY("SMILEY_HAPPY",		":-))"),
	ADD_SMILEY("SMILEY_LAUGH",		":-D"),
	ADD_SMILEY("SMILEY_WINK",		";-)"),
	ADD_SMILEY("SMILEY_TONGUE",		":-P"),
	ADD_SMILEY("SMILEY_INTEREST",	"=-)"),
	ADD_SMILEY("SMILEY_SAD",		":-("),
	ADD_SMILEY("SMILEY_CRY",		":'("),
	ADD_SMILEY("SMILEY_DISGUST",	":-|"),
	ADD_SMILEY("SMILEY_OMG",		":-O"),
	ADD_SMILEY("SMILEY_SKEPTIC",	":-/"),
	ADD_SMILEY("SMILEY_LOVE",		":-*"),
	ADD_SMILEY("SMILEY_SMILEQ",		":-]"),
	ADD_SMILEY("SMILEY_SADQ",		":-["),
	ADD_SMILEY("SMILEY_PH34R",		":ph34r:"),
	ADD_SMILEY("SMILEY_LOOKSIDE",	">_>"),
	ADD_SMILEY("SMILEY_SEALED",		":-X"),
#undef ADD_SMILEY
};

//#define	SHOW_SMILEY_SHORTCUTS	// could be used for Noob mode

#define IDC_SMILEY_TOOLBAR	1

#define SMSEL_CMD_START	100

IMPLEMENT_DYNAMIC(CSmileySelector, CWnd)

BEGIN_MESSAGE_MAP(CSmileySelector, CWnd)
	ON_WM_ACTIVATEAPP()
	ON_WM_KILLFOCUS()
#ifndef SHOW_SMILEY_SHORTCUTS
	ON_NOTIFY(TBN_GETINFOTIP, IDC_SMILEY_TOOLBAR, OnTbnSmileyGetInfoTip)
#endif
	ON_WM_DESTROY()
END_MESSAGE_MAP()

CSmileySelector::CSmileySelector()
	: m_iBitmaps()
	, m_pwndEdit()
{
}

void CSmileySelector::OnDestroy()
{
	CImageList *piml = m_tb.GetImageList();
	if (piml)
		piml->DeleteImageList();
	CWnd::OnDestroy();
}

BOOL CSmileySelector::Create(CWnd *pWndParent, LPCRECT pRect, CEdit *pwndEdit)
{
	m_pwndEdit = pwndEdit;

	//////////////////////////////////////////////////////////////////////////
	// Create the popup window
	//
	COLORREF crBackground = (g_xpStyle.IsAppThemed() && g_xpStyle.IsThemeActive()) ? COLOR_WINDOW : COLOR_BTNFACE;
	static const CString &strClassName = AfxRegisterWndClass(
			CS_CLASSDC | CS_SAVEBITS | CS_HREDRAW | CS_VREDRAW,
			AfxGetApp()->LoadStandardCursor(IDC_ARROW), (HBRUSH)(crBackground + 1), 0);
	if (!CWnd::CreateEx(0, strClassName, _T(""),
						WS_POPUP
						| WS_BORDER
						| WS_CLIPCHILDREN
						| WS_CLIPSIBLINGS,
						CRect(pRect->left, pRect->top, pRect->left, pRect->top),
						pWndParent, 0, NULL))
	{
		return FALSE;
	}
	//////////////////////////////////////////////////////////////////////////
	// Create the toolbar window
	//
	if (!m_tb.Create(WS_CHILD
					| WS_VISIBLE
					| WS_CLIPCHILDREN
					| WS_CLIPSIBLINGS
					| TBSTYLE_FLAT
					| TBSTYLE_TRANSPARENT
					| TBSTYLE_WRAPABLE
#ifndef SHOW_SMILEY_SHORTCUTS
					| TBSTYLE_TOOLTIPS
#endif
					| CCS_NOPARENTALIGN
					| CCS_NODIVIDER,
					CRect(), this, IDC_SMILEY_TOOLBAR))
	{
		return FALSE;
	}
	// Win98: Explicitly set to Unicode to receive Unicode notifications.
	m_tb.SendMessage(CCM_SETUNICODEFORMAT, TRUE);

	//////////////////////////////////////////////////////////////////////////
	// Create toolbar image list with smileys
	//
	CSize sizSmiley(18, 18);
	m_iBitmaps = 0;
	CImageList iml;
	iml.Create(sizSmiley.cx, sizSmiley.cy, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	for (unsigned i = 0; i < _countof(g_aSmileys); ++i) {
		iml.Add(CTempIconLoader(g_aSmileys[i].pszResource, sizSmiley.cx, sizSmiley.cy));
		++m_iBitmaps;
	}
	CImageList *pimlOld = m_tb.SetImageList(&iml);
	iml.Detach();
	if (pimlOld)
		pimlOld->DeleteImageList();

	//////////////////////////////////////////////////////////////////////////
	// Create toolbar button strings
	//
#ifdef SHOW_SMILEY_SHORTCUTS
	CString strStrings;
	for (int i = 0; i < _countof(g_aSmileys); ++i)
		strStrings.AppendFormat(_T("%s\001"), g_aSmileys[i].pszSmileys);
	strStrings += _T('\001');
	strStrings.Replace('\001', '\000');
	m_tb.AddStrings(strStrings);
#endif

	//////////////////////////////////////////////////////////////////////////
	// Create toolbar buttons
	//
	TBBUTTON *tb = new TBBUTTON[m_iBitmaps];
	for (int i = m_iBitmaps; --i >= 0;) {
		tb[i].iBitmap = i;
		tb[i].idCommand = SMSEL_CMD_START + i;
		tb[i].fsState = TBSTATE_ENABLED;
		tb[i].fsStyle = BTNS_BUTTON;
		tb[i].dwData = (DWORD_PTR)&g_aSmileys[i];
#ifdef SHOW_SMILEY_SHORTCUTS
		tb[i].iString = i;	// Looks interesting!! -> Smileys with shortcuts below
#else
		tb[i].iString = -1;
#endif
	}
	m_tb.AddButtons(m_iBitmaps, tb);
	delete[] tb;

	// Set toolbar rows
	m_tb.SetRows(3, TRUE, CRect());

	//////////////////////////////////////////////////////////////////////////
	// Get toolbar size and adjust for borders and some strange offset at bottom
	//
	CRect rcToolBar;
	m_tb.GetWindowRect(&rcToolBar);
	int iToolBarWidth = rcToolBar.Width();
	int iToolBarHeight = rcToolBar.Height();

	if (GetStyle() & WS_BORDER)
		iToolBarWidth += ::GetSystemMetrics(SM_CXBORDER) * 2;
	iToolBarHeight -= 2; // ??? = ::GetSystemMetrics(SM_CYBORDER)*2;
	MoveWindow(pRect->left, pRect->top - iToolBarHeight, iToolBarWidth, iToolBarHeight);
	ShowWindow(SW_SHOW);

	return TRUE;
}

void CSmileySelector::OnActivateApp(BOOL bActive, DWORD dwThreadID)
{
	CWnd::OnActivateApp(bActive, dwThreadID);
	if (!bActive)
		DestroyWindow();
}

void CSmileySelector::OnKillFocus(CWnd *pNewWnd)
{
	CWnd::OnKillFocus(pNewWnd);
	DestroyWindow();
}

BOOL CSmileySelector::PreTranslateMessage(MSG *pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam) {
		DestroyWindow();
		return TRUE;
	}

	return CWnd::PreTranslateMessage(pMsg);
}

BOOL CSmileySelector::OnCommand(WPARAM wParam, LPARAM lParam)
{
	int iButtons = m_tb.GetButtonCount();
	int iCmd = LOWORD(wParam);
	if (m_pwndEdit != NULL && iCmd >= SMSEL_CMD_START && iCmd < SMSEL_CMD_START + iButtons) {
		int iStart, iEnd;
		m_pwndEdit->GetSel(iStart, iEnd);

		CString strEdit;
		m_pwndEdit->GetWindowText(strEdit);
		int iEditLen = strEdit.GetLength();

		CString strInsert;
		if ((iStart > 0 && iStart < iEditLen - 1 && strEdit[iStart] != _T(' '))
			|| (iStart == iEnd && iEnd >= iEditLen && iEditLen > 0 && strEdit[iEditLen - 1] != _T(' ')))
		{
			strInsert += _T(' ');
		}
		strInsert += g_aSmileys[iCmd - SMSEL_CMD_START].pszSmileys;

		if (iEnd >= 0 && iEnd < iEditLen - 1 && strEdit[iEnd] != _T(' '))
			strInsert += _T(' ');
		m_pwndEdit->ReplaceSel(strInsert);
	} else
		ASSERT(0);

	return CWnd::OnCommand(wParam, lParam);
}

#ifndef SHOW_SMILEY_SHORTCUTS
void CSmileySelector::OnTbnSmileyGetInfoTip(LPNMHDR pNMHDR, LRESULT *pResult)
{
	LPNMTBGETINFOTIP pNMTBGIT = reinterpret_cast<LPNMTBGETINFOTIP>(pNMHDR);
	if (pNMTBGIT->cchTextMax) {
		int iButtons = m_tb.GetButtonCount();
		if (pNMTBGIT->iItem >= SMSEL_CMD_START && pNMTBGIT->iItem < SMSEL_CMD_START + iButtons) {
			_sntprintf(pNMTBGIT->pszText, pNMTBGIT->cchTextMax, _T(" %s "), g_aSmileys[pNMTBGIT->iItem - SMSEL_CMD_START].pszSmileys);
			pNMTBGIT->pszText[pNMTBGIT->cchTextMax - 1] = _T('\0');
		}
	}
	*pResult = 0;
}
#endif