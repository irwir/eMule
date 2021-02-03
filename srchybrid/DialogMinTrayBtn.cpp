// ------------------------------------------------------------
//  CDialogMinTrayBtn template class
//  MFC CDialog with minimize to system tray button (0.04)
//  Supports WinXP styles (thanks to David Yuheng Zhao for CVisualStylesXP - yuheng_zhao@yahoo.com)
// ------------------------------------------------------------
//  DialogMinTrayBtn.hpp
//  zegzav - 2002,2003 - eMule project (http://www.emule-project.net)
// ------------------------------------------------------------
#include "stdafx.h"
#include "DialogMinTrayBtn.h"
#include "VisualStylesXP.h"
#include "ResizableLib\ResizableDialog.h"
#include "OtherFunctions.h"
#include "MenuCmds.h"
#undef SCHEMADEF_H
#undef SCHEMA_VERIFY_VSSYM32
#include "SchemaDef.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static BOOL (WINAPI *s_pfnTransparentBlt)(HDC, int, int, int, int, HDC, int, int, int, int, UINT) = NULL;

#if 0
// define this to use that source file as template
#define	TEMPLATE	template <class BASE>
#else
// define this to instantiate functions for class 'BASE' right in this CPP module
#if _MSC_VER >= 1310
#define	TEMPLATE	template <>
#else
#define	TEMPLATE
#endif
#define BASE		CResizableDialog
#endif

// ------------------------------
//  constants
// ------------------------------

#define CAPTION_BUTTONSPACE      (2)
#define CAPTION_MINHEIGHT        (8)

#define TIMERMINTRAYBTN_ID       0x76617a67
#define TIMERMINTRAYBTN_PERIOD   200    // ms

#define WP_TRAYBUTTON WP_MINBUTTON

BEGIN_TM_PART_STATES(TRAYBUTTON)
	TM_STATE(1, TRAYBS, NORMAL)
	TM_STATE(2, TRAYBS, HOT)
	TM_STATE(3, TRAYBS, PUSHED)
	TM_STATE(4, TRAYBS, DISABLED)
	// Inactive
	TM_STATE(5, TRAYBS, INORMAL)
	TM_STATE(6, TRAYBS, IHOT)
	TM_STATE(7, TRAYBS, IPUSHED)
	TM_STATE(8, TRAYBS, IDISABLED)
END_TM_PART_STATES()

#define BMP_TRAYBTN_WIDTH		(21)
#define BMP_TRAYBTN_HEIGHT		(21)
#define BMP_TRAYBTN_BLUE		_T("TRAYBTN_BLUE")
#define BMP_TRAYBTN_METALLIC	_T("TRAYBTN_METALLIC")
#define BMP_TRAYBTN_HOMESTEAD	_T("TRAYBTN_HOMESTEAD")
#define BMP_TRAYBTN_TRANSCOLOR	(RGB(255,0,255))

TEMPLATE const TCHAR *CDialogMinTrayBtn<BASE>::m_pszMinTrayBtnBmpName[] = { BMP_TRAYBTN_BLUE, BMP_TRAYBTN_METALLIC, BMP_TRAYBTN_HOMESTEAD };

#define VISUALSTYLESXP_DEFAULTFILE		L"LUNA.MSSTYLES"
#define VISUALSTYLESXP_BLUE				0
#define VISUALSTYLESXP_METALLIC			1
#define VISUALSTYLESXP_HOMESTEAD		2
#define VISUALSTYLESXP_NAMEBLUE			L"NORMALCOLOR"
#define VISUALSTYLESXP_NAMEMETALLIC		L"METALLIC"
#define VISUALSTYLESXP_NAMEHOMESTEAD	L"HOMESTEAD"

// _WIN32_WINNT >= 0x0501 (XP only)
#define _WM_THEMECHANGED                0x031A

#define _ON_WM_THEMECHANGED() \
	{ _WM_THEMECHANGED, 0, 0, 0, AfxSig_l, \
		(AFX_PMSG)(AFX_PMSGW) \
		(static_cast<LRESULT (AFX_MSG_CALL CWnd::*)(void)>(&ThisClass::_OnThemeChanged)) },

BEGIN_TEMPLATE_MESSAGE_MAP(CDialogMinTrayBtn, BASE, BASE)
	ON_WM_NCPAINT()
	ON_WM_NCACTIVATE()
	ON_WM_NCHITTEST()
	ON_WM_NCLBUTTONDOWN()
	ON_WM_NCRBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_TIMER()
	_ON_WM_THEMECHANGED()
END_MESSAGE_MAP()


TEMPLATE CDialogMinTrayBtn<BASE>::CDialogMinTrayBtn()
	: BASE()
{
	MinTrayBtnInit();
}

TEMPLATE CDialogMinTrayBtn<BASE>::CDialogMinTrayBtn(LPCTSTR lpszTemplateName, CWnd *pParentWnd)
	: BASE(lpszTemplateName, pParentWnd)
{
	MinTrayBtnInit();
}

TEMPLATE CDialogMinTrayBtn<BASE>::CDialogMinTrayBtn(UINT nIDTemplate, CWnd *pParentWnd)
	: BASE(nIDTemplate, pParentWnd)
{
	MinTrayBtnInit();
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnInit()
{
	m_bMinTrayBtnVisible = TRUE;
	m_bMinTrayBtnEnabled = TRUE;
	m_bMinTrayBtnUp = TRUE;
	m_bMinTrayBtnCapture = FALSE;
	m_bMinTrayBtnActive = FALSE;
	m_bMinTrayBtnHitTest = FALSE;
	m_nMinTrayBtnTimerId = 0;

	// - Load the 'MSIMG32.DLL' only, if it's really needed.
	if (MinTrayBtnInitBitmap() && !s_pfnTransparentBlt) {
		HMODULE hMsImg32 = LoadLibrary(_T("MSIMG32.DLL"));
		if (hMsImg32) {
			(FARPROC &)s_pfnTransparentBlt = GetProcAddress(hMsImg32, "TransparentBlt");
			if (!s_pfnTransparentBlt)
				FreeLibrary(hMsImg32);
		}
	}
}

TEMPLATE BOOL CDialogMinTrayBtn<BASE>::OnInitDialog()
{
	BOOL bReturn = BASE::OnInitDialog();
	InitWindowStyles(this);
	m_nMinTrayBtnTimerId = SetTimer(TIMERMINTRAYBTN_ID, TIMERMINTRAYBTN_PERIOD, NULL);
	return bReturn;
}

TEMPLATE void CDialogMinTrayBtn<BASE>::OnNcPaint()
{
	BASE::OnNcPaint();
	MinTrayBtnUpdatePosAndSize();
	MinTrayBtnDraw();
}

TEMPLATE BOOL CDialogMinTrayBtn<BASE>::OnNcActivate(BOOL bActive)
{
	MinTrayBtnUpdatePosAndSize();
	BOOL bResult = BASE::OnNcActivate(bActive);
	m_bMinTrayBtnActive = bActive;
	MinTrayBtnDraw();
	return bResult;
}

TEMPLATE LRESULT CDialogMinTrayBtn<BASE>::OnNcHitTest(CPoint point)
{
	BOOL bPreviousHitTest = m_bMinTrayBtnHitTest;
	m_bMinTrayBtnHitTest = MinTrayBtnHitTest(point);
	if (!IsWindowsClassicStyle() && m_bMinTrayBtnHitTest != bPreviousHitTest)
		MinTrayBtnDraw(); // Windows XP Style (hot button)
	return m_bMinTrayBtnHitTest ? HTMINTRAYBUTTON : BASE::OnNcHitTest(point);
}

TEMPLATE void CDialogMinTrayBtn<BASE>::OnNcLButtonDown(UINT nHitTest, CPoint point)
{
	if ((GetStyle() & WS_DISABLED) || !MinTrayBtnIsEnabled() || !MinTrayBtnIsVisible() || !MinTrayBtnHitTest(point)) {
		BASE::OnNcLButtonDown(nHitTest, point);
		return;
	}
	SetCapture();
	m_bMinTrayBtnCapture = TRUE;
	MinTrayBtnSetDown();
}

TEMPLATE void CDialogMinTrayBtn<BASE>::OnNcRButtonDown(UINT nHitTest, CPoint point)
{
	if ((GetStyle() & WS_DISABLED) || !MinTrayBtnIsVisible() || !MinTrayBtnHitTest(point))
		BASE::OnNcRButtonDown(nHitTest, point);
}

TEMPLATE void CDialogMinTrayBtn<BASE>::OnMouseMove(UINT nFlags, CPoint point)
{
	if ((GetStyle() & WS_DISABLED) || !m_bMinTrayBtnCapture) {
		BASE::OnMouseMove(nFlags, point);
		return;
	}

	ClientToScreen(&point);
	m_bMinTrayBtnHitTest = MinTrayBtnHitTest(point);
	if (m_bMinTrayBtnHitTest) {
		if (m_bMinTrayBtnUp)
			MinTrayBtnSetDown();
	} else if (!m_bMinTrayBtnUp)
		MinTrayBtnSetUp();
}

TEMPLATE void CDialogMinTrayBtn<BASE>::OnLButtonUp(UINT nFlags, CPoint point)
{
	if ((GetStyle() & WS_DISABLED) || !m_bMinTrayBtnCapture) {
		BASE::OnLButtonUp(nFlags, point);
		return;
	}

	ReleaseCapture();
	m_bMinTrayBtnCapture = FALSE;
	MinTrayBtnSetUp();

	ClientToScreen(&point);
	if (MinTrayBtnHitTest(point))
		SendMessage(WM_SYSCOMMAND, MP_MINIMIZETOTRAY, MAKELONG(point.x, point.y));
}

TEMPLATE void CDialogMinTrayBtn<BASE>::OnTimer(UINT_PTR nIDEvent)
{
	if (!IsWindowsClassicStyle() && nIDEvent == m_nMinTrayBtnTimerId) {
		// Visual XP Style (hot button)
		CPoint point;
		if (::GetCursorPos(&point)) {
			BOOL bHitTest = MinTrayBtnHitTest(point);
			if (m_bMinTrayBtnHitTest != bHitTest) {
				m_bMinTrayBtnHitTest = bHitTest;
				MinTrayBtnDraw();
			}
		}
	}
}

TEMPLATE LRESULT CDialogMinTrayBtn<BASE>::_OnThemeChanged()
{
	// BASE::OnThemeChanged();
	MinTrayBtnInitBitmap();
	return 0;
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnUpdatePosAndSize()
{
	DWORD dwStyle = GetStyle();
	DWORD dwExStyle = GetExStyle();

	INT cyCaption = ((dwExStyle & WS_EX_TOOLWINDOW) == 0) ? ::GetSystemMetrics(SM_CYCAPTION) - 1 : ::GetSystemMetrics(SM_CYSMCAPTION) - 1;
	if (cyCaption < CAPTION_MINHEIGHT)
		cyCaption = CAPTION_MINHEIGHT;

	CRect rcWnd;
	GetWindowRect(&rcWnd);

	// get Windows' frame window button width/height (this may not always be a square!)
	CSize szBtn;
	szBtn.cy = cyCaption - (CAPTION_BUTTONSPACE * 2);
	szBtn.cx = ::GetSystemMetrics(SM_CXSIZE) - (IsWindowsClassicStyle() ? 2 : 4);

	// set our frame window button width/height...
	if (IsWindowsClassicStyle()) {
		// ...this is same as Windows' buttons for non WinXP
		m_MinTrayBtnSize = szBtn;
	} else {
		// ...this is a square for WinXP
		m_MinTrayBtnSize.cx = szBtn.cy;
		m_MinTrayBtnSize.cy = szBtn.cy;
	}

	m_MinTrayBtnPos.x = rcWnd.Width() - (CAPTION_BUTTONSPACE + m_MinTrayBtnSize.cx + CAPTION_BUTTONSPACE + szBtn.cx);
	m_MinTrayBtnPos.y = CAPTION_BUTTONSPACE;

	if ((dwStyle & WS_THICKFRAME) != 0) {
		// resizable window
		CSize bordersize(-::GetSystemMetrics(SM_CXSIZEFRAME), ::GetSystemMetrics(SM_CYSIZEFRAME));
		m_MinTrayBtnPos += bordersize;
	} else {
		// fixed window
		CSize borderfixed(-::GetSystemMetrics(SM_CXFIXEDFRAME), ::GetSystemMetrics(SM_CYFIXEDFRAME));
		m_MinTrayBtnPos += borderfixed;
	}

	if (((dwExStyle & WS_EX_TOOLWINDOW) == 0) && (((dwStyle & WS_MINIMIZEBOX) != 0) || ((dwStyle & WS_MAXIMIZEBOX) != 0)))
		if (IsWindowsClassicStyle())
			m_MinTrayBtnPos.x -= (szBtn.cx * 2) + CAPTION_BUTTONSPACE;
		else
			m_MinTrayBtnPos.x -= (szBtn.cx + CAPTION_BUTTONSPACE) * 2;
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnShow()
{
	if (MinTrayBtnIsVisible())
		return;

	m_bMinTrayBtnVisible = TRUE;
	if (IsWindowVisible())
		RedrawWindow(NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnHide()
{
	if (!MinTrayBtnIsVisible())
		return;

	m_bMinTrayBtnVisible = FALSE;
	if (IsWindowVisible())
		RedrawWindow(NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnEnable()
{
	if (MinTrayBtnIsEnabled())
		return;

	m_bMinTrayBtnEnabled = TRUE;
	MinTrayBtnSetUp();
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnDisable()
{
	if (!MinTrayBtnIsEnabled())
		return;

	m_bMinTrayBtnEnabled = FALSE;
	if (m_bMinTrayBtnCapture) {
		ReleaseCapture();
		m_bMinTrayBtnCapture = FALSE;
	}
	MinTrayBtnSetUp();
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnDraw()
{
	if (!MinTrayBtnIsVisible())
		return;

	CDC *pDC = GetWindowDC();
	if (!pDC)
		return; // panic!

	if (IsWindowsClassicStyle()) {
		COLORREF black(::GetSysColor(COLOR_BTNTEXT));
		COLORREF gray(::GetSysColor(COLOR_GRAYTEXT));
		COLORREF gray2(::GetSysColor(COLOR_BTNHILIGHT));

		// button
		if (m_bMinTrayBtnUp)
			pDC->DrawFrameControl(MinTrayBtnGetRect(), DFC_BUTTON, DFCS_BUTTONPUSH);
		else
			pDC->DrawFrameControl(MinTrayBtnGetRect(), DFC_BUTTON, DFCS_BUTTONPUSH | DFCS_PUSHED);

		 // dot
		CRect btn = MinTrayBtnGetRect();
		btn.DeflateRect(2, 2);
		UINT caption = MinTrayBtnGetSize().cy + (CAPTION_BUTTONSPACE * 2);
		UINT pixratio = (caption >= 14) ? ((caption >= 20) ? 2 + ((caption - 20) / 8) : 2) : 1;
		UINT pixratio2 = (caption >= 12) ? 1 + (caption - 12) / 8 : 0;
		UINT dotwidth = (1 + pixratio * 3) >> 1;
		UINT dotheight = pixratio;
		CRect dot(0, 0, dotwidth, dotheight);
		CSize spc((1 + pixratio2 * 3) >> 1, pixratio2);
		dot -= dot.Size();
		dot += btn.BottomRight();
		dot -= spc;
		if (!m_bMinTrayBtnUp)
			dot += SIZE{ 1, 1 };
		if (m_bMinTrayBtnEnabled)
			pDC->FillSolidRect(dot, black);
		else {
			pDC->FillSolidRect(dot + SIZE{ 1, 1 }, gray2);
			pDC->FillSolidRect(dot, gray);
		}
	} else {
		// VisualStylesXP
		CRect btn(MinTrayBtnGetRect());
		int iState;
		if (!m_bMinTrayBtnEnabled)
			iState = TRAYBS_DISABLED;
		else if (GetStyle() & WS_DISABLED)
			iState = MINBS_NORMAL;
		else if (m_bMinTrayBtnHitTest)
			iState = (m_bMinTrayBtnCapture) ? MINBS_PUSHED : MINBS_HOT;
		else
			iState = MINBS_NORMAL;
		// inactive
		if (!m_bMinTrayBtnActive)
			iState += 4; // inactive state TRAYBS_Ixxx

		if (m_bmMinTrayBtnBitmap.m_hObject && s_pfnTransparentBlt) {
			// known theme (bitmap)
			CBitmap *pBmpOld;
			CDC dcMem;
			if (dcMem.CreateCompatibleDC(pDC) && (pBmpOld = dcMem.SelectObject(&m_bmMinTrayBtnBitmap)) != NULL) {
				s_pfnTransparentBlt(pDC->m_hDC, btn.left, btn.top, btn.Width(), btn.Height(), dcMem.m_hDC, 0, BMP_TRAYBTN_HEIGHT * (iState - 1), BMP_TRAYBTN_WIDTH, BMP_TRAYBTN_HEIGHT, BMP_TRAYBTN_TRANSCOLOR);
				dcMem.SelectObject(pBmpOld);
			}
		} else {
			// unknown theme (ThemeData)
			HTHEME hTheme = g_xpStyle.OpenThemeData(m_hWnd, L"Window");
			if (hTheme) {
				btn.top += btn.Height() / 8;
				g_xpStyle.DrawThemeBackground(hTheme, pDC->m_hDC, WP_TRAYBUTTON, iState, &btn, NULL);
				g_xpStyle.CloseThemeData(hTheme);
			}
		}
	}

	ReleaseDC(pDC);
}

TEMPLATE BOOL CDialogMinTrayBtn<BASE>::MinTrayBtnHitTest(CPoint ptScreen) const
{
	CRect rcWnd;
	GetWindowRect(&rcWnd);
	// adjust 'ptScreen' with possible RTL window layout
	if (::MapWindowPoints(HWND_DESKTOP, m_hWnd, &rcWnd.TopLeft(), 2) == 0
		|| ::MapWindowPoints(HWND_DESKTOP, m_hWnd, &ptScreen, 1) == 0)
	{
		// several bug reports about not working on NT SP6 (?). in case of any problems with
		// 'MapWindowPoints' we fall back to old code (does not work for RTL window layout though)
		CRect rcWndOrg(rcWnd);
		CPoint ptScreenOrg(ptScreen);
		rcWnd = rcWndOrg;
		ptScreen = ptScreenOrg;
	}
	ptScreen.Offset(-rcWnd.TopLeft());

	CRect rcBtn(MinTrayBtnGetRect());
	rcBtn.InflateRect(0, CAPTION_BUTTONSPACE);
	return rcBtn.PtInRect(ptScreen);
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnSetUp()
{
	m_bMinTrayBtnUp = TRUE;
	MinTrayBtnDraw();
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnSetDown()
{
	m_bMinTrayBtnUp = FALSE;
	MinTrayBtnDraw();
}

TEMPLATE BOOL CDialogMinTrayBtn<BASE>::IsWindowsClassicStyle() const
{
	return m_bMinTrayBtnWindowsClassicStyle;
}

TEMPLATE void CDialogMinTrayBtn<BASE>::SetWindowText(LPCTSTR lpszString)
{
	BASE::SetWindowText(lpszString);
	MinTrayBtnDraw();
}

TEMPLATE INT CDialogMinTrayBtn<BASE>::GetVisualStylesXPColor() const
{
	if (IsWindowsClassicStyle())
		return -1;

	WCHAR szwThemeFile[MAX_PATH];
	WCHAR szwThemeColor[256];
	if (g_xpStyle.GetCurrentThemeName(szwThemeFile, MAX_PATH, szwThemeColor, _countof(szwThemeColor), NULL, 0) != S_OK)
		return -1;
	WCHAR *p = wcsrchr(szwThemeFile, _T('\\'));
	if (p == NULL)
		return -1;
	++p;
	if (_wcsicmp(p, VISUALSTYLESXP_DEFAULTFILE) == 0) {
		if (_wcsicmp(szwThemeColor, VISUALSTYLESXP_NAMEBLUE) == 0)
			return VISUALSTYLESXP_BLUE;
		if (_wcsicmp(szwThemeColor, VISUALSTYLESXP_NAMEMETALLIC) == 0)
			return VISUALSTYLESXP_METALLIC;
		if (_wcsicmp(szwThemeColor, VISUALSTYLESXP_NAMEHOMESTEAD) == 0)
			return VISUALSTYLESXP_HOMESTEAD;
	}
	return -1;
}

TEMPLATE BOOL CDialogMinTrayBtn<BASE>::MinTrayBtnInitBitmap()
{
	m_bMinTrayBtnWindowsClassicStyle = !(g_xpStyle.IsThemeActive() && g_xpStyle.IsAppThemed());

	m_bmMinTrayBtnBitmap.DeleteObject();
	INT iColor = GetVisualStylesXPColor();
	return (iColor >= 0) && m_bmMinTrayBtnBitmap.LoadBitmap(m_pszMinTrayBtnBmpName[iColor]);
}