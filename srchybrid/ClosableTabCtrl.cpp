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
#include "ClosableTabCtrl.h"
#include "OtherFunctions.h"
#include "MenuCmds.h"
#include "UserMsgs.h"
#include "VisualStylesXP.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


// _WIN32_WINNT >= 0x0501 (XP only)
#define _WM_THEMECHANGED		0x031A
#define _ON_WM_THEMECHANGED()													\
	{	_WM_THEMECHANGED, 0, 0, 0, AfxSig_l,									\
		(AFX_PMSG)(AFX_PMSGW)													\
		(static_cast<LRESULT (AFX_MSG_CALL CWnd::*)(void)>(_OnThemeChanged))	\
	},

///////////////////////////////////////////////////////////////////////////////
// CClosableTabCtrl

IMPLEMENT_DYNAMIC(CClosableTabCtrl, CTabCtrl)

BEGIN_MESSAGE_MAP(CClosableTabCtrl, CTabCtrl)
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_MBUTTONUP()
	ON_WM_CREATE()
	ON_WM_SYSCOLORCHANGE()
	ON_WM_CONTEXTMENU()
	_ON_WM_THEMECHANGED()
	ON_WM_CTLCOLOR_REFLECT()
	ON_WM_CTLCOLOR()
	ON_WM_ERASEBKGND()
	ON_WM_MEASUREITEM()
	ON_WM_MEASUREITEM_REFLECT()
END_MESSAGE_MAP()

CClosableTabCtrl::CClosableTabCtrl()
	: m_bClosable(true)
	, m_iiCloseButton()
	, m_ptCtxMenu(-1, -1)
{
}

void CClosableTabCtrl::GetCloseButtonRect(int iItem, const CRect &rcItem, CRect &rcCloseButton, bool bItemSelected, bool bVistaThemeActive)
{
	rcCloseButton.top = rcItem.top + 2;
	rcCloseButton.bottom = rcCloseButton.top + (m_iiCloseButton.rcImage.bottom - m_iiCloseButton.rcImage.top);
	rcCloseButton.right = rcItem.right - 2;
	rcCloseButton.left = rcCloseButton.right - (m_iiCloseButton.rcImage.right - m_iiCloseButton.rcImage.left);
	if (bVistaThemeActive)
		rcCloseButton.left -= 1; // the close button does not look 'symmetric' with a width of 16, give it 17
	if (bItemSelected) {
		rcCloseButton.OffsetRect(-1, 0);
		if (bVistaThemeActive) {
			int iItems = GetItemCount();
			if (iItems > 1 && iItem == iItems - 1)
				rcCloseButton.OffsetRect(-2, 0);
		}
	} else {
		if (bVistaThemeActive) {
			int iItems = GetItemCount();
			if (iItems > 1 && iItem < iItems - 1)
				rcCloseButton.OffsetRect(2, 0);
		}
	}
}

int CClosableTabCtrl::GetTabUnderPoint(const CPoint &point) const
{
	CRect rcItem;
	for (int i = GetItemCount(); --i >= 0;)
		if (GetItemRect(i, rcItem)) {
			rcItem.InflateRect(2, 2); // get the real tab item rect
			if (rcItem.PtInRect(point))
				return i;
		}
	return -1;
}

int CClosableTabCtrl::GetTabUnderContextMenu() const
{
	return (m_ptCtxMenu.x == -1 || m_ptCtxMenu.y == -1) ? -1 : GetTabUnderPoint(m_ptCtxMenu);
}

bool CClosableTabCtrl::SetDefaultContextMenuPos()
{
	int iTab = GetCurSel();
	if (iTab >= 0) {
		CRect rcItem;
		if (GetItemRect(iTab, &rcItem)) {
			rcItem.InflateRect(2, 2); // get the real tab item rect
			m_ptCtxMenu.SetPoint(rcItem.left + rcItem.Width() / 2, (rcItem.top + rcItem.bottom) / 2);
			return true;
		}
	}
	return false;
}

void CClosableTabCtrl::OnMButtonUp(UINT nFlags, CPoint point)
{
	if (m_bClosable) {
		int iTab = GetTabUnderPoint(point);
		if (iTab >= 0) {
			GetParent()->SendMessage(UM_CLOSETAB, (WPARAM)iTab);
			return;
		}
	}

	CTabCtrl::OnMButtonUp(nFlags, point);
}

void CClosableTabCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bClosable) {
		int iTab = GetTabUnderPoint(point);
		if (iTab >= 0) {
			CRect rcItem;
			GetItemRect(iTab, rcItem);
			rcItem.InflateRect(2, 2); // get the real tab item rect

			bool bVistaThemeActive = theApp.IsVistaThemeActive();
			CRect rcCloseButton;
			GetCloseButtonRect(iTab, rcItem, rcCloseButton, iTab == GetCurSel(), bVistaThemeActive);

			// The visible part of our close icon is one pixel less on each side
			if (!bVistaThemeActive)
				rcCloseButton.InflateRect(-1, -1);

			if (rcCloseButton.PtInRect(point)) {
				GetParent()->SendMessage(UM_CLOSETAB, (WPARAM)iTab);
				return;
			}
		}
	}

	CTabCtrl::OnLButtonUp(nFlags, point);
}

void CClosableTabCtrl::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	int iTab = GetTabUnderPoint(point);
	if (iTab >= 0)
		GetParent()->SendMessage(UM_DBLCLICKTAB, (WPARAM)iTab);
	else
		CTabCtrl::OnLButtonDblClk(nFlags, point);
}

// It would be nice if there would the option to restrict the maximum width of a tab control.
// We would need that feature actually for almost all our tab controls. Especially for the
// search results list - those tab control labels can get quite large. But I did not yet a
// find a way to limit the width of tabs. Although MSDN says that an owner drawn
// tab control receives a WM_MEASUREITEM, I never got one.

// Vista: This gets never called for an owner drawn tab control
void CClosableTabCtrl::OnMeasureItem(int iCtlId, LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
	TRACE("CClosableTabCtrl::OnMeasureItem\n");
	__super::OnMeasureItem(iCtlId, lpMeasureItemStruct);
}

// Vista: This gets never called for an owner drawn tab control
void CClosableTabCtrl::MeasureItem(LPMEASUREITEMSTRUCT)
{
	TRACE("CClosableTabCtrl::MeasureItem\n");
}

void CClosableTabCtrl::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
	int nTabIndex = lpDIS->itemID;
	if (nTabIndex < 0)
		return;

	TCHAR szLabel[256];
	TC_ITEM tci;
	tci.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_STATE;
	tci.pszText = szLabel;
	tci.cchTextMax = _countof(szLabel);
	tci.dwStateMask = TCIS_HIGHLIGHTED;
	if (!GetItem(nTabIndex, &tci))
		return;
	szLabel[_countof(szLabel) - 1] = _T('\0');
	//TRACE("CClosableTabCtrl::DrawItem: item=%u, state=%08x, color=%08x, rc=%3d,%3d,%3dx%3d\n", nTabIndex, tci.dwState, GetTextColor(lpDIS->hDC), lpDIS->rcItem.left, lpDIS->rcItem.top, lpDIS->rcItem.right - lpDIS->rcItem.left, lpDIS->rcItem.bottom - lpDIS->rcItem.top);

	CDC *pDC = CDC::FromHandle(lpDIS->hDC);
	if (!pDC)
		return;

	CRect rcItem(lpDIS->rcItem);
	CRect rcFullItem(rcItem);
	bool bSelected = (lpDIS->itemState & ODS_SELECTED) != 0;

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
	// To complete this experience, tab controls also do not support NMCUSTOMDRAW.
	// So, the only known way to customize a tab control is by using TCS_OWNERDRAWFIXED
	// which however, does not work properly under Vista.
	//
	// The "solution" which is currently implemented to prevent Vista from drawing those
	// 3D-borders is by using "ExcludeClipRect" to reduce the drawing area which is used
	// by Windows after WM_DRAWITEM was processed. This "solution" is very sensitive to
	// the used rectangles and offsets in general. Incrementing/Decrementing one of the
	// "rcItem", "rcFullItem", etc. rectangles makes the entire "solution" flawed again
	// because some borders would become visible again.
	//
	HTHEME hTheme = NULL;
	bool bVistaHotTracked = false;
	bool bVistaThemeActive = theApp.IsVistaThemeActive();
	if (bVistaThemeActive) {
		// To determine if the current item is in 'hot tracking' mode, we need to evaluate
		// the current foreground color - there is no flag which would indicate this state
		// more safely. This applies only for Vista and for tab controls which have the
		// TCS_OWNERDRAWFIXED style.
		bVistaHotTracked = pDC->GetTextColor() == ::GetSysColor(COLOR_HOTLIGHT);

		hTheme = g_xpStyle.OpenThemeData(m_hWnd, L"TAB");
		if (hTheme) {
			if (bSelected) {
				// get the real tab item rect
				rcFullItem.left += 1;
				rcFullItem.right -= 1;
				rcFullItem.bottom -= 1;
			} else
				rcFullItem.InflateRect(2, 2); // get the real tab item rect

			int iPartId;
			int iStateId;
			RECT rcBk(rcFullItem);
			int iCnt = GetItemCount();
			if (bSelected) {
				iStateId = TTIS_SELECTED;
				if (nTabIndex == 0)
					iPartId = (iCnt == 1) ? TABP_TOPTABITEMBOTHEDGE : TABP_TOPTABITEMLEFTEDGE;
				else
					iPartId = (nTabIndex == iCnt - 1) ? TABP_TOPTABITEMRIGHTEDGE : TABP_TOPTABITEM;
			} else {
				rcBk.top += 2;
				iStateId = bVistaHotTracked ? TIS_HOT : TIS_NORMAL;
				if (nTabIndex == 0)
					iPartId = (iCnt == 1) ? TABP_TABITEMBOTHEDGE : TABP_TABITEMLEFTEDGE;
				else
					iPartId = (nTabIndex == iCnt - 1) ? TABP_TABITEMRIGHTEDGE : TABP_TABITEM;
			}
			if (g_xpStyle.IsThemeBackgroundPartiallyTransparent(hTheme, iPartId, iStateId))
				g_xpStyle.DrawThemeParentBackground(m_hWnd, *pDC, &rcFullItem);
			g_xpStyle.DrawThemeBackground(hTheme, *pDC, iPartId, iStateId, &rcBk, NULL);
		}
	}

	// Following background clearing is needed for:
	//	WinXP/Vista (when used without an application theme)
	//	Vista (when used with an application theme but without a theme for the tab control)
	if (!g_xpStyle.IsThemeActive() || !g_xpStyle.IsAppThemed() || (!hTheme && bVistaThemeActive))
		pDC->FillSolidRect(rcItem, ::GetSysColor(COLOR_BTNFACE));

	int iOldBkMode = pDC->SetBkMode(TRANSPARENT);

	// Draw image on left side
	CImageList *piml = GetImageList();
	if (tci.iImage >= 0 && piml && piml->m_hImageList) {
		IMAGEINFO ii;
		piml->GetImageInfo(0, &ii);
		rcItem.left += bSelected ? 8 : 4;
		piml->Draw(pDC, tci.iImage, POINT{ rcItem.left, rcItem.top + 2 }, ILD_TRANSPARENT);
			rcItem.left += (ii.rcImage.right - ii.rcImage.left + 1);
		if (!bSelected)
			rcItem.left += 4;
	}

	bool bClosable = m_bClosable;
	if (bClosable && GetParent()->SendMessage(UM_QUERYTAB, nTabIndex))
		bClosable = false;

	// Draw 'Close button' at right side
	if (bClosable && m_ImgLstCloseButton.m_hImageList) {
		CRect rcCloseButton;
		GetCloseButtonRect(nTabIndex, rcItem, rcCloseButton, bSelected, bVistaThemeActive);

		HTHEME hThemeNC = bVistaThemeActive ? g_xpStyle.OpenThemeData(m_hWnd, _T("WINDOW")) : NULL;
		if (hThemeNC) {
			// Possible "Close" parts: WP_CLOSEBUTTON, WP_SMALLCLOSEBUTTON, WP_MDICLOSEBUTTON
			int iPartId = WP_SMALLCLOSEBUTTON;
			int iStateId = (bSelected || bVistaHotTracked) ? CBS_NORMAL : CBS_DISABLED;
			if (g_xpStyle.IsThemeBackgroundPartiallyTransparent(hTheme, iPartId, iStateId))
				g_xpStyle.DrawThemeParentBackground(m_hWnd, *pDC, &rcCloseButton);
			g_xpStyle.DrawThemeBackground(hThemeNC, *pDC, iPartId, iStateId, rcCloseButton, NULL);
			g_xpStyle.CloseThemeData(hThemeNC);
		} else
			m_ImgLstCloseButton.Draw(pDC, static_cast<int>(!bSelected && !bVistaHotTracked), rcCloseButton.TopLeft(), ILD_TRANSPARENT);

		rcItem.right = rcCloseButton.left - 2;
		if (bSelected)
			rcItem.left += hTheme ? 4 : 2;
	}

	COLORREF crOldColor;
	if (tci.dwState & TCIS_HIGHLIGHTED)
		crOldColor = pDC->SetTextColor(RGB(192, 0, 0));
	else if (bVistaHotTracked)
		crOldColor = pDC->SetTextColor(::GetSysColor(COLOR_BTNTEXT));
	else
		crOldColor = CLR_NONE;
	rcItem.top += bSelected ? 4 : 3;
	// Vista: Tab control has troubles with determining the width of a tab if the
	// label contains one '&' character. To get around this, we use the old code which
	// replaces one '&' character with two '&' characters and we do not specify DT_NOPREFIX
	// here when drawing the text.
	//
	// Vista: "DrawThemeText" can not be used in case we need a certain foreground color. Thus we always us
	// "DrawText" to always get the same font and metrics (just for safety).
	pDC->DrawText(szLabel, rcItem, DT_SINGLELINE | DT_TOP | DT_CENTER /*| DT_NOPREFIX*/);

	if (crOldColor != CLR_NONE)
		pDC->SetTextColor(crOldColor);
	pDC->SetBkMode(iOldBkMode);

	if (hTheme) {
		RECT rcClip(rcFullItem);
		if (bSelected) {
			rcClip.left -= 2 + 1;
			rcClip.right += 2 + 1;
		} else
			rcClip.top += 2;

		pDC->ExcludeClipRect(&rcClip);
		g_xpStyle.CloseThemeData(hTheme);
	}
}

void CClosableTabCtrl::PreSubclassWindow()
{
	CTabCtrl::PreSubclassWindow();
	InternalInit();
}

int CClosableTabCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CTabCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;
	InternalInit();
	return 0;
}

void CClosableTabCtrl::InternalInit()
{
	ModifyStyle(0, TCS_OWNERDRAWFIXED);

#if 1
	// Under Vista Aero, all tab controls get by default the TCS_HOTTRACK
	// style even if it was not specified within the resource file. Though, to 'see'
	// the hot tracking effect the control also need to get initialized explicitly with
	// the WS_CLIPCHILDREN style within a *separate* function call. Yes, there is no
	// logic to all this, not at all. It simply is that way.
	//
	// So, do *not* "optimize" that code by using only one "ModifyStyle" function call.
	// The 2nd function call to "ModifyStyle" is very much by intention!
	//
	// However, the hot tracking effect which is achieved this way does not survive a
	// theme change. After the theme is changed (regardless whether we switch between
	// Vista themes or from/to a non-Vista theme), the hot tracking effect is gone even
	// if we try to modify the styles again within OnThemeChanged...
	if (theApp.IsVistaThemeActive())
		ModifyStyle(0, WS_CLIPCHILDREN);
#else
	// Remove the automatically applied hot tracking effect to avoid that the tab control
	// may use it when it also sets the WS_CLIPCHILDREN (for other reasons) later.
	ModifyStyle(TCS_HOTTRACK, 0);
#endif

	SetAllIcons();
}

void CClosableTabCtrl::OnSysColorChange()
{
	CTabCtrl::OnSysColorChange();
	SetAllIcons();
}

void CClosableTabCtrl::SetAllIcons()
{
	if (m_bClosable) {
		const int iIconWidth = 16;
		const int iIconHeight = 16;
		m_ImgLstCloseButton.DeleteImageList();
		m_ImgLstCloseButton.Create(iIconWidth, iIconHeight, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
		m_ImgLstCloseButton.Add(CTempIconLoader(_T("CloseTabSelected"), iIconWidth, iIconHeight));
		m_ImgLstCloseButton.Add(CTempIconLoader(_T("CloseTab"), iIconWidth, iIconHeight));
		m_ImgLstCloseButton.GetImageInfo(0, &m_iiCloseButton);
		Invalidate();
	}
}

void CClosableTabCtrl::OnContextMenu(CWnd*, CPoint point)
{
	if (m_bClosable) {
		if (point.x == -1 || point.y == -1) {
			if (!SetDefaultContextMenuPos())
				return;
			point = m_ptCtxMenu;
			ClientToScreen(&point);
		} else {
			m_ptCtxMenu = point;
			ScreenToClient(&m_ptCtxMenu);
		}

		int iTab = GetTabUnderPoint(m_ptCtxMenu);
		if (iTab >= 0 && GetParent()->SendMessage(UM_QUERYTAB, (WPARAM)iTab) == 0) {
			CMenu menu;
			menu.CreatePopupMenu();
			menu.AppendMenu(MF_STRING, MP_REMOVE, GetResString(IDS_FD_CLOSE));
			menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
		}
	}
}

BOOL CClosableTabCtrl::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if (wParam == MP_REMOVE) {
		if (m_ptCtxMenu.x != -1 && m_ptCtxMenu.y != -1) {
			int iTab = GetTabUnderPoint(m_ptCtxMenu);
			if (iTab >= 0) {
				GetParent()->SendMessage(UM_CLOSETAB, (WPARAM)iTab);
				return TRUE;
			}
		}
	}
	return CTabCtrl::OnCommand(wParam, lParam);
}

LRESULT CClosableTabCtrl::_OnThemeChanged()
{
	// Owner drawn tab control seems to have troubles with updating itself due to an XP theme change.
	ModifyStyle(TCS_OWNERDRAWFIXED, 0);	// Reset control style to not-owner drawn
	Default();							// Process original WM_THEMECHANGED message
	ModifyStyle(0, TCS_OWNERDRAWFIXED);	// Apply owner drawn style again
	return 0;
}

// Vista: This gets never called for an owner drawn tab control
HBRUSH CClosableTabCtrl::CtlColor(CDC*, UINT)
{
	// Change any attributes of the DC here
	// Return a non-NULL brush if the parent's handler should not be called
	return NULL;
}

// Vista: This gets never called for an owner drawn tab control
HBRUSH CClosableTabCtrl::OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CTabCtrl::OnCtlColor(pDC, pWnd, nCtlColor);
	// Change any attributes of the DC here
	// Return a different brush if the default is not desired
	return hbr;
}

// Vista: Can not be used to workaround the problems with owner drawn tab control
BOOL CClosableTabCtrl::OnEraseBkgnd(CDC *pDC)
{
	return CTabCtrl::OnEraseBkgnd(pDC);
}

BOOL CClosableTabCtrl::DeleteItem(int nItem)
{
	// if we remove a tab which would lead to scrolling back to other tabs, all those become hidden for... whatever reasons
	// its easy enough to work around by scrolling to the first visible tab _before_ we delete the other one
	SetCurSel(0);
	return __super::DeleteItem(nItem);
}