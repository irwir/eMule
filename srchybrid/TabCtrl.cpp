#include "stdafx.h"
#include "tabctrl.hpp"
#include <algorithm>
#include "UserMsgs.h"
#include "emule.h"
#include "VisualStylesXP.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/************************************************
*
*                   TAB CONTROL
*
************************************************/

#define INDICATOR_WIDTH  4
#define INDICATOR_COLOR  COLOR_HOTLIGHT
#define METHOD           DSTINVERT

BEGIN_MESSAGE_MAP(TabControl, CTabCtrl)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

//
// 'TabControl::TabControl'
//
TabControl::TabControl()
	: m_bDragging()
	, m_nSrcTab()
	, m_nDstTab()
	, m_bHotTracking()
	, m_pSpinCtrl()
{
}

//
// 'TabControl::~TabControl'
//
TabControl::~TabControl()
{
	if (m_pSpinCtrl) {
		m_pSpinCtrl->Detach();
		delete m_pSpinCtrl;
	}
}

//
// 'TabControl::OnLButtonDown'
//
// @mfunc Handler that is called when the left mouse button is activated.
//        The handler examines whether we have initiated a drag 'n drop
//        process.
//
void TabControl::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (DragDetectPlus(this, point)) {
		// Yes, we're beginning to drag, so capture the mouse...
		m_bDragging = true;

		// Find and remember the source tab (the one we're going to move/drag 'n drop)
		TCHITTESTINFO hitinfo;
		hitinfo.pt = point;
		m_nSrcTab = HitTest(&hitinfo);
		m_nDstTab = m_nSrcTab;

		// Reset insert indicator
		m_InsertPosRect.SetRectEmpty();
		DrawIndicator(point);
		SetCapture();
	} else
		CTabCtrl::OnLButtonDown(nFlags, point);

	// Note: We're not calling the base classes CTabCtrl::OnLButtonDown
	//       every time, because we want to be able to drag a tab without
	//       actually selecting it first (so that it gets the focus).
}

//
// 'TabControl::OnLButtonUp'
//
// @mfunc Handler that is called when the left mouse button is released.
//        Is used to stop the drag 'n drop process, releases the mouse
//        capture and reorders the tabs accordingly to insertion (drop)
//        position.
//
void TabControl::OnLButtonUp(UINT nFlags, CPoint point)
{
	CTabCtrl::OnLButtonUp(nFlags, point);

	if (m_bDragging) {
		// We're going to drop something now...

		// Stop the dragging process and release the mouse capture
		// This will eventually call our OnCaptureChanged which stops the dragging
		if (GetCapture() == this)
			ReleaseCapture();

		// Modify the tab control style so that Hot Tracking is re-enabled
		if (m_bHotTracking)
			ModifyStyle(0, TCS_HOTTRACK);

		if (m_nSrcTab == m_nDstTab)
			return;

		// Inform Parent about Dragrequest
		NMHDR nmh;
		nmh.code = UM_TABMOVED;
		nmh.hwndFrom = GetSafeHwnd();
		nmh.idFrom = GetDlgCtrlID();
		GetParent()->SendMessage(WM_NOTIFY, nmh.idFrom, (LPARAM)&nmh);
	}
}

//
// 'TabControl::OnMouseMove'
//
// @mfunc Handler that is called when the mouse is moved. This is used
//        to, when in a drag 'n drop process, to:
//
//        1) Draw the drop indicator of where the tab can be inserted.
//        2) Possible scroll the tab so more tabs is viewed.
//
void TabControl::OnMouseMove(UINT nFlags, CPoint point)
{
	CTabCtrl::OnMouseMove(nFlags, point);

	// This code added to do extra check - shouldn't be strictly necessary!
	if (!(nFlags & MK_LBUTTON))
		m_bDragging = false;

	if (m_bDragging) {
		// Draw the indicator
		DrawIndicator(point);

		// Get the up-down (spin) control that is associated with the tab control
		// and which contains scroll position information.
		if (!m_pSpinCtrl) {
			CWnd *pWnd = FindWindowEx(GetSafeHwnd(), 0, _T("msctls_updown32"), 0);
			if (pWnd) {
				// DevNote: It may be somewhat of an overkill to use the MFC version
				//          of the CSpinButtonCtrl since were actually only using it
				//          for retrieving the current scroll position (GetPos). A simple
				//          HWND could have been enough.
				m_pSpinCtrl = new CSpinButtonCtrl;
				m_pSpinCtrl->Attach(pWnd->GetSafeHwnd());
			}
		}

		RECT rcClient;
		GetClientRect(&rcClient);

		if (m_pSpinCtrl) {
		// Examine whether we should scroll left...
			if (point.x < rcClient.left) {
				int nPos = LOWORD(m_pSpinCtrl->GetPos());
				if (nPos > 0) {
					InvalidateRect(&m_InsertPosRect, FALSE);
					m_InsertPosRect.SetRectEmpty();
					SendMessage(WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, nPos - 1), 0);
				}
			}

			// Examine whether we should scroll right...
			if (point.x > rcClient.right && m_pSpinCtrl->IsWindowVisible()) {
				InvalidateRect(&m_InsertPosRect, FALSE);
				m_InsertPosRect.SetRectEmpty();
				int nPos = LOWORD(m_pSpinCtrl->GetPos());
				SendMessage(WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, nPos + 1), 0);
			}
		}
	}
}

//
// 'TabControl::OnCaptureChanged'
//
// @mfunc Handler that is called when the WM_CAPTURECHANGED message is received. It notifies
//        us that we do not longer capture the mouse. Therefore we must stop or drag 'n drop
//        process. Clean up code etc.
//
void TabControl::OnCaptureChanged(CWnd*)
{
	if (m_bDragging) {
		m_bDragging = false;

		// Remove the indicator by invalidate the rectangle (forces repaint)
		InvalidateRect(&m_InsertPosRect);

		// If a drag image is in play this probably should be cleaned up here.
		// ...
	}
}

//
// 'TabControl::DrawIndicator'
//
// @mfunc Utility member function to draw the (drop) indicator of where the
//        tab will be inserted.
//
bool TabControl::DrawIndicator(CPoint point		// @parm Specifies a position (e.g. the mouse pointer position) which
												//  will be used to determine whether the indicator should be
												//   painted to the left or right of the indicator.
)
{
	TCHITTESTINFO hitinfo;
	hitinfo.pt = point;

	// Adjust position to top of tab control (allow the mouse the actually
	// be outside the top of tab control and still be able to find the right
	// tab index
	CRect rcItem;
	if (GetItemRect(0, &rcItem))
		hitinfo.pt.y = rcItem.top;

	// If the position is inside the rectangle where tabs are visible we
	// can safely draw the insert indicator...
	unsigned int nTab = HitTest(&hitinfo);
	if (hitinfo.flags != TCHT_NOWHERE)
		m_nDstTab = nTab;
	else if (m_nDstTab == (UINT)GetItemCount())
		--m_nDstTab;

	GetItemRect(m_nDstTab, &rcItem);
	CRect newInsertPosRect(rcItem.left - 1, rcItem.top, rcItem.left - 1 + INDICATOR_WIDTH, rcItem.bottom);

	// Determine whether the indicator should be painted at the right of
	// the tab - in which case we update the indicator position and the
	// destination tab...
	if (point.x >= rcItem.right - rcItem.Width() / 2) {
		newInsertPosRect.MoveToX(rcItem.right - 1);
		++m_nDstTab;
	}

	if (newInsertPosRect != m_InsertPosRect) {
		// Remove the current indicator by invalidate the rectangle (forces repaint)
		InvalidateRect(&m_InsertPosRect);

		// Update to new insert indicator position...
		m_InsertPosRect = newInsertPosRect;
	}

	// Create a simple device context in which we initialize the pen and brush
	// that we will use for drawing the new indicator...
	CClientDC dc(this);

	CBrush brush(::GetSysColor(INDICATOR_COLOR));
	CPen pen(PS_SOLID, 1, ::GetSysColor(INDICATOR_COLOR));

	CBrush *pOldBrush = dc.SelectObject(&brush);
	CPen *pOldPen = dc.SelectObject(&pen);

	// Draw the insert indicator
	dc.Rectangle(m_InsertPosRect);

	dc.SelectObject(pOldPen);
	dc.SelectObject(pOldBrush);

	return true; // success
}

//
// 'TabControl::ReorderTab'
//
// @mfunc Reorders the tab by moving the source tab to the position of the
//        destination tab.
//
BOOL TabControl::ReorderTab(unsigned int nSrcTab, unsigned int nDstTab)
{
	if (nSrcTab == nDstTab)
		return TRUE; // Return success (we have nothing to do)

	// Remember the current selected tab
	unsigned int nSelectedTab = GetCurSel();

	// Get information from the tab to move (to be deleted)
	TCHAR sBuffer[256];
	TCITEM item;
	item.mask = TCIF_IMAGE | TCIF_PARAM | TCIF_TEXT; //| TCIF_STATE;
	item.pszText = sBuffer;
	item.cchTextMax = _countof(sBuffer);
	BOOL bOK = GetItem(nSrcTab, &item);
	sBuffer[_countof(sBuffer) - 1] = _T('\0');
	ASSERT(bOK);

	bOK = DeleteItem(nSrcTab);
	ASSERT(bOK);

	// Insert it at new location
	bOK = InsertItem(nDstTab - static_cast<unsigned>(m_nDstTab > m_nSrcTab), &item);

	// Setup new selected tab
	if (nSelectedTab == nSrcTab)
		SetCurSel(nDstTab - static_cast<unsigned>(m_nDstTab > m_nSrcTab));
	else {
		if (nSelectedTab > nSrcTab && nSelectedTab < nDstTab)
			SetCurSel(nSelectedTab - 1);

		if (nSelectedTab < nSrcTab && nSelectedTab > nDstTab)
			SetCurSel(nSelectedTab + 1);
	}

	// Force update of tab control
	// Necessary to do so that notified clients ('users') - by selection change call
	// below - can draw the tab contents in correct tab.
	UpdateWindow();

	NMHDR nmh;
	nmh.hwndFrom = GetSafeHwnd();
	nmh.idFrom = GetDlgCtrlID();
	nmh.code = TCN_SELCHANGE;
	GetParent()->SendMessage(WM_NOTIFY, nmh.idFrom, (LPARAM)&nmh);

	return bOK;
}

BOOL TabControl::DragDetectPlus(CWnd *Handle, CPoint p)
{
	BOOL bResult = FALSE;
	Handle->ClientToScreen(&p);
	CRect DragRect(p, p);
	InflateRect(DragRect, ::GetSystemMetrics(SM_CXDRAG), ::GetSystemMetrics(SM_CYDRAG));
	BOOL bDispatch = TRUE;
	Handle->SetCapture();
	while (!bResult && bDispatch) {
		MSG Msg;
		if (PeekMessage(&Msg, *Handle, 0, 0, PM_REMOVE)) {
			switch (Msg.message) {
			case WM_MOUSEMOVE:
				bResult = !DragRect.PtInRect(Msg.pt);
				break;
			case WM_RBUTTONUP:
			case WM_LBUTTONUP:
			case WM_CANCELMODE:
				bDispatch = FALSE;
				break;
			case WM_QUIT:
				ReleaseCapture();
				return FALSE;
			default:
				TranslateMessage(&Msg);
				DispatchMessage(&Msg);
			}
		} else
			::Sleep(0);
	}
	ReleaseCapture();
	return bResult;
}

void TabControl::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
	int nTabIndex = lpDIS->itemID;
	if (nTabIndex < 0)
		return;

	TCHAR szLabel[256];
	TC_ITEM tci;
	tci.mask = TCIF_TEXT | TCIF_PARAM;
	tci.pszText = szLabel;
	tci.cchTextMax = _countof(szLabel);
	if (!GetItem(nTabIndex, &tci))
		return;
	szLabel[_countof(szLabel) - 1] = _T('\0');

	CDC *pDC = CDC::FromHandle(lpDIS->hDC);
	if (!pDC)
		return;

	RECT rcItem(lpDIS->rcItem);
	CRect rcFullItem(rcItem);
	bool bSelected = (lpDIS->itemState & ODS_SELECTED) != 0;

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
			if (bSelected) {
				iStateId = TTIS_SELECTED;
				if (nTabIndex == 0) {
					// First item
					if (nTabIndex == GetItemCount() - 1)
						iPartId = TABP_TOPTABITEMBOTHEDGE; // First & Last item
					else
						iPartId = TABP_TOPTABITEMLEFTEDGE;
				} else if (nTabIndex == GetItemCount() - 1)
					// Last item
					iPartId = TABP_TOPTABITEMRIGHTEDGE;
				else
					iPartId = TABP_TOPTABITEM;
			} else {
				rcBk.top += 2;
				iStateId = bVistaHotTracked ? TIS_HOT : TIS_NORMAL;
				if (nTabIndex == 0) {
					// First item
					if (nTabIndex == GetItemCount() - 1)
						iPartId = TABP_TABITEMBOTHEDGE; // First & Last item
					else
						iPartId = TABP_TABITEMLEFTEDGE;
				} else if (nTabIndex == GetItemCount() - 1)
					// Last item
					iPartId = TABP_TABITEMRIGHTEDGE;
				else
					iPartId = TABP_TABITEM;
			}
			if (g_xpStyle.IsThemeBackgroundPartiallyTransparent(hTheme, iPartId, iStateId))
				g_xpStyle.DrawThemeParentBackground(m_hWnd, *pDC, &rcFullItem);
			g_xpStyle.DrawThemeBackground(hTheme, *pDC, iPartId, iStateId, &rcBk, NULL);
		}
	}

	// Vista: Need to clear the background explicitly to avoid glitches with
	//	*) a changed icon
	//	*) hot tracking effect
	if (hTheme == NULL && bVistaThemeActive)
		pDC->FillSolidRect(&rcItem, ::GetSysColor(COLOR_BTNFACE));

	int iOldBkMode = pDC->SetBkMode(TRANSPARENT);

	COLORREF crOldColor;
	if (tci.lParam != -1)
		crOldColor = pDC->SetTextColor((COLORREF)tci.lParam);
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
	pDC->DrawText(szLabel, &rcItem, DT_SINGLELINE | DT_TOP | DT_CENTER /*| DT_NOPREFIX*/);

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

void TabControl::SetTabTextColor(int index, DWORD color)
{
	TCITEM tab;
	tab.mask = TCIF_PARAM;
	if (GetItem(index, &tab)) {
		tab.lParam = color;
		SetItem(index, &tab);
	}
}