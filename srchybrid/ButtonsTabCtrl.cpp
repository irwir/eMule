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
#include "ButtonsTabCtrl.h"
#include "MenuCmds.h"
#include "UserMsgs.h"
#include "VisualStylesXP.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


// _WIN32_WINNT >= 0x0501 (XP only)
#define _WM_THEMECHANGED                0x031A
#define _ON_WM_THEMECHANGED()													\
	{	_WM_THEMECHANGED, 0, 0, 0, AfxSig_l,									\
		(AFX_PMSG)(AFX_PMSGW)													\
		(static_cast<LRESULT (AFX_MSG_CALL CWnd::*)(void)>(_OnThemeChanged))	\
	},

///////////////////////////////////////////////////////////////////////////////
// CButtonsTabCtrl

IMPLEMENT_DYNAMIC(CButtonsTabCtrl, CTabCtrl)

BEGIN_MESSAGE_MAP(CButtonsTabCtrl, CTabCtrl)
	ON_WM_CREATE()
	_ON_WM_THEMECHANGED()
END_MESSAGE_MAP()

void CButtonsTabCtrl::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
	int nTabIndex = lpDIS->itemID;
	if (nTabIndex < 0)
		return;

	TCHAR szLabel[256];
	TC_ITEM tci;
	tci.mask = TCIF_TEXT;
	tci.pszText = szLabel;
	tci.cchTextMax = _countof(szLabel);
	if (!GetItem(nTabIndex, &tci))
		return;
	szLabel[_countof(szLabel) - 1] = _T('\0');

	CDC *pDC = CDC::FromHandle(lpDIS->hDC);
	if (!pDC)
		return;

	///////////////////////////////////////////////////////////////////////////////////////
	// Adding support for XP Styles (Vista Themes) for owner drawn tab controls simply
	// does *not* work under Vista. Maybe it works under XP (did not try), but that is
	// meaningless because under XP an owner drawn tab control is already rendered *with*
	// the proper XP Styles. So, for XP there is no need to care about the theme API at all.
	//
	// However, under Vista, a tab control which has the TCS_OWNERDRAWFIXED
	// style gets additional 3D-borders which are applied by Vista *after* WM_DRAWITEM
	// was processed. Thus, there is no known workaround available to prevent Vista from
	// adding those old fashioned 3D-borders. We can render the tab control items within
	// the WM_DRAWITEM handler in whatever style we want, but Vista will in each case
	// overwrite the borders of each tab control item with old fashioned 3D-borders...
	//
	// To complete this experience, tab controls also do not support NMCUSTOMDRAW. So, the
	// only known way to customize a tab control is by using TCS_OWNERDRAWFIXED which does
	// however not work properly under Vista.
	//
	// The "solution" which is currently implemented to prevent Vista from drawing those
	// 3D-borders is by using "ExcludeClipRect" to reduce the drawing area which is used
	// by Windows after WM_DRAWITEM was processed. This "solution" is very sensitive to
	// the used rectangles and offsets in general. Incrementing/Decrementing one of the
	// "rcItem", "rcFullItem", etc. rectangles makes the entire "solution" flawed again
	// because some borders would become visible again.
	//
	RECT rcItem(lpDIS->rcItem);
	CRect rcFullItem(rcItem);
	bool bSelected = (lpDIS->itemState & ODS_SELECTED) != 0;
	HTHEME hTheme = NULL;
	int iPartId = BP_PUSHBUTTON;
	int iStateId = PBS_NORMAL;
	bool bVistaHotTracked;
	bool bVistaThemeActive = theApp.IsVistaThemeActive();
	if (bVistaThemeActive) {
		// To determine if the current item is in 'hot tracking' mode, we need to evaluate
		// the current foreground color - there is no flag which would indicate this state
		// more safely. This applies only for Vista and for tab controls which have the
		// TCS_OWNERDRAWFIXED style.
		bVistaHotTracked = pDC->GetTextColor() == ::GetSysColor(COLOR_HOTLIGHT);

		hTheme = g_xpStyle.OpenThemeData(m_hWnd, L"BUTTON");
		if (hTheme) {
			rcFullItem.InflateRect(2, 2); // get the real tab item rect

			if (bSelected)
				iStateId = PBS_PRESSED;
			else
				iStateId = bVistaHotTracked ? PBS_HOT : PBS_NORMAL;

			// Not very smart, but this is needed (in addition to the DrawThemeBackground and the clipping)
			// to fix a minor glitch in both of the bottom side corners.
			RECT rcTopBorder(rcFullItem);
			rcTopBorder.bottom = rcTopBorder.top + 2;
			pDC->FillSolidRect(&rcTopBorder, ::GetSysColor(COLOR_BTNFACE));

			if (g_xpStyle.IsThemeBackgroundPartiallyTransparent(hTheme, iPartId, iStateId))
				g_xpStyle.DrawThemeParentBackground(m_hWnd, *pDC, rcFullItem);
			g_xpStyle.DrawThemeBackground(hTheme, *pDC, iPartId, iStateId, rcFullItem, NULL);
		}
	} else
		bVistaHotTracked = false;

	// Following background clearing is needed for:
	//	WinXP/Vista (when used without an application theme)
	//	Vista (when used with an application theme but without a theme for the tab control)
	if (!g_xpStyle.IsThemeActive() || !g_xpStyle.IsAppThemed() || (hTheme == NULL && bVistaThemeActive))
		pDC->FillSolidRect(&lpDIS->rcItem, ::GetSysColor(COLOR_BTNFACE));

	int iOldBkMode = pDC->SetBkMode(TRANSPARENT);

	COLORREF crOldColor = bVistaHotTracked ? pDC->SetTextColor(::GetSysColor(COLOR_BTNTEXT)) : CLR_NONE;

	rcItem.top += 2;
	// Vista: Tab control has troubles with determining the width of a tab if the
	// label contains one '&' character. To get around this, we use the old code which
	// replaces one '&' character with two '&' characters and we do not specify DT_NOPREFIX
	// here when drawing the text.
	//
	// Vista: "DrawThemeText" can not be used in case we need a certain foreground color. Thus we always us
	// "DrawText" to always get the same font and metrics (just for safety).
	pDC->DrawText(szLabel, &rcItem, DT_SINGLELINE | DT_TOP | DT_CENTER /*| DT_NOPREFIX*/);

	if (crOldColor != CLR_NONE)
		pDC->SetTextColor(crOldColor);
	pDC->SetBkMode(iOldBkMode);

	if (hTheme) {
		pDC->ExcludeClipRect(&rcFullItem);
		g_xpStyle.CloseThemeData(hTheme);
	}
}

void CButtonsTabCtrl::PreSubclassWindow()
{
	CTabCtrl::PreSubclassWindow();
	InternalInit();
}

int CButtonsTabCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CTabCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;
	InternalInit();
	return 0;
}

void CButtonsTabCtrl::InternalInit()
{
	if (theApp.IsVistaThemeActive()) {
		ModifyStyle(0, TCS_OWNERDRAWFIXED);
		ModifyStyle(0, TCS_HOTTRACK);
	}
}

LRESULT CButtonsTabCtrl::_OnThemeChanged()
{
	// Owner drawn tab control seems to have troubles with updating itself due to an XP theme change.
	bool bIsOwnerDrawn = (GetStyle() & TCS_OWNERDRAWFIXED) != 0;
	if (bIsOwnerDrawn)
		ModifyStyle(TCS_OWNERDRAWFIXED, 0);	// Reset control style to not-owner drawn
	Default();								// Process original WM_THEMECHANGED message
	if (bIsOwnerDrawn)
		ModifyStyle(0, TCS_OWNERDRAWFIXED);	// Apply owner drawn style again
	return 0;
}