// TaskbarNotifier.cpp : implementation file
// By John O'Byrne
// 11 August 2002: - Timer precision is now determined automatically
//		   Complete change in the way the popup is showing (thanks to this,now the popup can be always on top, it shows even while watching a movie)
//		   The popup doesn't steal the focus any more (by replacing ShowWindow(SW_SHOW) by ShowWindow(SW_SHOWNOACTIVATE))
//		   Thanks to Daniel Lohmann, update in the way the taskbar pos is determined (more flexible now)
// 17 July 2002: - Another big Change in the method for determining the pos of the taskbar (using the SHAppBarMessage function)
// 16 July 2002: - Thanks to the help of Daniel Lohmann, the Show Function timings work perfectly now ;)
// 15 July 2002: - Change in the method for determining the pos of the taskbar
//		 (now handles the presence of quick launch or any other bar).
//		 Remove the Handlers for WM_CREATE and WM_DESTROY
//		 SetSkin is now called SetBitmap
// 14 July 2002: - Changed the GenerateRegion func by a new one (to correct a win98 bug)
// kei-kun modifications:
// 30 October  2002: - Added event type management (TBN_*) for eMule project
// 04 November 2002: - added skin support via .ini file
#include "stdafx.h"
#include "emule.h"
#include "ini2.h"
#include "otherfunctions.h"
#include "enbitmap.h"
#include "TaskbarNotifier.h"
#include "emuledlg.h"
#include "UserMsgs.h"
#include "opcodes.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define IDT_HIDDEN			0
#define IDT_APPEARING		1
#define IDT_WAITING			2
#define IDT_DISAPPEARING	3

#define TASKBAR_X_TOLERANCE 5
#define TASKBAR_Y_TOLERANCE 5

inline bool NearlyEqual(int a, int b, int iEpsilon)
{
	return abs(a - b) < iEpsilon;
}

// CTaskbarNotifier

IMPLEMENT_DYNAMIC(CTaskbarNotifier, CWnd)

BEGIN_MESSAGE_MAP(CTaskbarNotifier, CWnd)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_MESSAGE(WM_MOUSEHOVER, OnMouseHover)
	ON_MESSAGE(WM_MOUSELEAVE, OnMouseLeave)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_SETCURSOR()
	ON_WM_TIMER()
	ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()

CTaskbarNotifier::CTaskbarNotifier()
	: m_tConfigFileLastModified((time_t)-1)
	, m_pWndParent()
	, m_crNormalTextColor(RGB(133, 146, 181))
	, m_crSelectedTextColor(RGB(10, 36, 106))
	, m_hCursor(::LoadCursor(NULL, IDC_HAND)) // System Hand cursor
	, m_hBitmapRegion()
	, m_nAnimStatus(IDT_HIDDEN)
	, m_uTextFormat(DT_WORDBREAK | DT_PATH_ELLIPSIS | DT_END_ELLIPSIS | DT_NOPREFIX)
	, m_dwTimerPrecision(10) //was 50 ms before Windows NT - not supported any more
	, m_dwTimeToStay(SEC2MS(4))
	, m_dwShowEvents()
	, m_dwHideEvents()
	, m_dwTimeToShow(500)
	, m_dwTimeToHide(200)
	, m_bBitmapAlpha()
	, m_bMouseIsOver()
	, m_bTextSelected()
	, m_bAutoClose(true)
	, m_nBitmapWidth()
	, m_nBitmapHeight()
	, m_nTaskbarPlacement(ABE_BOTTOM)
	, m_nCurrentPosX()
	, m_nCurrentPosY()
	, m_nCurrentWidth()
	, m_nCurrentHeight()
	, m_nIncrementShow()
	, m_nIncrementHide()
	, m_nHistoryPosition()
	, m_nActiveMessageType(TBN_NULL)
	, m_pfnAlphaBlend()
{
	m_hMsImg32Dll = LoadLibrary(_T("MSIMG32.DLL"));
	if (m_hMsImg32Dll) {
		(FARPROC &)m_pfnAlphaBlend = GetProcAddress(m_hMsImg32Dll, "AlphaBlend");
		if (m_pfnAlphaBlend == NULL) {
			FreeLibrary(m_hMsImg32Dll);
			m_hMsImg32Dll = NULL;
		}
	}
}

CTaskbarNotifier::~CTaskbarNotifier()
{
	if (m_hMsImg32Dll)
		FreeLibrary(m_hMsImg32Dll);
	while (!m_MessageHistory.IsEmpty())
		delete static_cast<CTaskbarNotifierHistory*>(m_MessageHistory.RemoveHead());
}

LRESULT CALLBACK My_AfxWndProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hWnd, nMsg, wParam, lParam);
}

int CTaskbarNotifier::Create(CWnd *pWndParent)
{
	static LPCTSTR const s_szClassName = _T("eMule_TaskbarNotifierWndClass");
	ASSERT(pWndParent != NULL);
	m_pWndParent = pWndParent;

	WNDCLASSEX wcx;
	wcx.cbSize = (UINT)sizeof wcx;
	wcx.lpfnWndProc = AfxWndProc;
	wcx.style = CS_DBLCLKS | CS_SAVEBITS;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = AfxGetInstanceHandle();
	wcx.hIcon = NULL;
	wcx.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	wcx.hbrBackground = ::GetSysColorBrush(COLOR_WINDOW);
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = s_szClassName;
	wcx.hIconSm = NULL;
	::RegisterClassEx(&wcx);

	return CreateEx(WS_EX_TOPMOST, s_szClassName, NULL, WS_POPUP, 0, 0, 0, 0, pWndParent->m_hWnd, NULL);
}

void CTaskbarNotifier::OnSysColorChange()
{
	CWnd::OnSysColorChange();
	LoadConfiguration(m_strConfigFilePath);
}

bool CTaskbarNotifier::LoadConfiguration(LPCTSTR pszFilePath)
{
	struct _stat64 st;
	if (statUTC(pszFilePath, st))
		st.st_mtime = -1; // '-1' = missing file
	if (m_strConfigFilePath.CompareNoCase(pszFilePath) == 0 && (time_t)st.st_mtime <= m_tConfigFileLastModified)
		return true;

	Hide();

	TCHAR szConfigDir[MAX_PATH];
	m_strConfigFilePath = pszFilePath;
	CIni ini(pszFilePath, _T("Config"));
	_tcsncpy(szConfigDir, pszFilePath, _countof(szConfigDir));
	szConfigDir[_countof(szConfigDir) - 1] = _T('\0');
	LPTSTR pszFileName = _tcsrchr(szConfigDir, _T('\\'));
	if (pszFileName == NULL)
		return false;
	pszFileName[1] = _T('\0');

	int iTextNormalRed = ini.GetInt(_T("TextNormalRed"), 255);
	int iTextNormalGreen = ini.GetInt(_T("TextNormalGreen"), 255);
	int iTextNormalBlue = ini.GetInt(_T("TextNormalBlue"), 255);

	int iTextSelectedRed = ini.GetInt(_T("TextSelectedRed"), 255);
	int iTextSelectedGreen = ini.GetInt(_T("TextSelectedGreen"), 255);
	int iTextSelectedBlue = ini.GetInt(_T("TextSelectedBlue"), 255);

	// for backward compatibility read the old values (which had a typo) and then the new values
	int iBmpTransparentRed = ini.GetInt(_T("BmpTrasparentRed"), 255);
	iBmpTransparentRed = ini.GetInt(_T("BmpTransparentRed"), iBmpTransparentRed);
	int iBmpTransparentGreen = ini.GetInt(_T("BmpTrasparentGreen"), 0);
	iBmpTransparentGreen = ini.GetInt(_T("BmpTransparentGreen"), iBmpTransparentGreen);
	int iBmpTransparentBlue = ini.GetInt(_T("BmpTrasparentBlue"), 255);
	iBmpTransparentBlue = ini.GetInt(_T("BmpTransparentBlue"), iBmpTransparentBlue);

	const CString &strFontType = ini.GetString(_T("FontType"), theApp.GetDefaultFontFaceName());
	int iFontSize = ini.GetInt(_T("FontSize"), 8) * 10;

	m_dwTimeToStay = ini.GetInt(_T("TimeToStay"), 4000);
	m_dwTimeToShow = ini.GetInt(_T("TimeToShow"), 500);
	m_dwTimeToHide = ini.GetInt(_T("TimeToHide"), 200);

	CString strBmpFilePath;
	const CString &strBmpFileName = ini.GetString(_T("BmpFileName"), _T(""));
	if (!strBmpFileName.IsEmpty())
		if (PathIsRelative(strBmpFileName))
			strBmpFilePath.Format(_T("%s%s"), szConfigDir, (LPCTSTR)strBmpFileName);
		else
			strBmpFilePath = strBmpFileName;

	// get text rectangle coordinates
	int iLeft = ini.GetInt(_T("rcTextLeft"), 5);
	int iTop = ini.GetInt(_T("rcTextTop"), 45);
	int iRight = ini.GetInt(_T("rcTextRight"), 220);
	int iBottom = ini.GetInt(_T("rcTextBottom"), 85);
	if (iLeft <= 0)
		iLeft = 1;
	if (iTop <= 0)
		iTop = 1;
	if (iRight <= 0)
		iRight = 1;
	if (iBottom <= 0)
		iBottom = 1;
	SetTextRect(CRect(iLeft, iTop, iRight, iBottom));

	// get close button rectangle coordinates
	iLeft = ini.GetInt(_T("rcCloseBtnLeft"), 286);
	iTop = ini.GetInt(_T("rcCloseBtnTop"), 40);
	iRight = ini.GetInt(_T("rcCloseBtnRight"), 300);
	iBottom = ini.GetInt(_T("rcCloseBtnBottom"), 54);
	if (iLeft <= 0)
		iLeft = 1;
	if (iTop <= 0)
		iTop = 1;
	if (iRight <= 0)
		iRight = 1;
	if (iBottom <= 0)
		iBottom = 1;
	SetCloseBtnRect(CRect(iLeft, iTop, iRight, iBottom));

	// get history button rectangle coordinates
	iLeft = ini.GetInt(_T("rcHistoryBtnLeft"), 283);
	iTop = ini.GetInt(_T("rcHistoryBtnTop"), 14);
	iRight = ini.GetInt(_T("rcHistoryBtnRight"), 299);
	iBottom = ini.GetInt(_T("rcHistoryBtnBottom"), 39);
	if (iLeft <= 0)
		iLeft = 1;
	if (iTop <= 0)
		iTop = 1;
	if (iRight <= 0)
		iRight = 1;
	if (iBottom <= 0)
		iBottom = 1;
	SetHistoryBtnRect(CRect(iLeft, iTop, iRight, iBottom));

	if (strBmpFilePath.IsEmpty() || !SetBitmap(strBmpFilePath, iBmpTransparentRed, iBmpTransparentGreen, iBmpTransparentBlue)) {
		CEnBitmap imgTaskbar;
		if (!imgTaskbar.LoadImage(IDR_TASKBAR, _T("GIF")))
			return false;
		if (!SetBitmap(&imgTaskbar, iBmpTransparentRed, iBmpTransparentGreen, iBmpTransparentBlue))
			return false;
	}

	SetTextFont(strFontType, iFontSize, TN_TEXT_NORMAL, TN_TEXT_UNDERLINE);
	SetTextColor(RGB(iTextNormalRed, iTextNormalGreen, iTextNormalBlue)
		, RGB(iTextSelectedRed, iTextSelectedGreen, iTextSelectedBlue));

	m_tConfigFileLastModified = (time_t)st.st_mtime;
	return true;
}

void CTaskbarNotifier::SetTextFont(LPCTSTR pszFont, int nSize, int nNormalStyle, int nSelectedStyle)
{
	m_fontNormal.DeleteObject();
	CreatePointFont(m_fontNormal, nSize, pszFont);
	LOGFONT lf;
	m_fontNormal.GetLogFont(&lf);

	// We  set the Font of the unselected ITEM
	lf.lfWeight = (nNormalStyle & TN_TEXT_BOLD) ? FW_BOLD : FW_NORMAL;
	lf.lfItalic = (nNormalStyle & TN_TEXT_ITALIC) ? TRUE : FALSE;
	lf.lfUnderline = (nNormalStyle & TN_TEXT_UNDERLINE) ? TRUE : FALSE;

	m_fontNormal.DeleteObject();
	m_fontNormal.CreateFontIndirect(&lf);

	// We set the Font of the selected ITEM
	lf.lfWeight = (nSelectedStyle & TN_TEXT_BOLD) ? FW_BOLD : FW_NORMAL;
	lf.lfItalic = (nSelectedStyle & TN_TEXT_ITALIC) ? TRUE : FALSE;
	lf.lfUnderline = (nSelectedStyle & TN_TEXT_UNDERLINE) ? TRUE : FALSE;

	m_fontSelected.DeleteObject();
	m_fontSelected.CreateFontIndirect(&lf);
}

void CTaskbarNotifier::SetTextDefaultFont()
{
	LOGFONT lf;
	AfxGetMainWnd()->GetFont()->GetLogFont(&lf);
	m_fontNormal.DeleteObject();
	m_fontSelected.DeleteObject();
	m_fontNormal.CreateFontIndirect(&lf);
	lf.lfUnderline = TRUE;
	m_fontSelected.CreateFontIndirect(&lf);
}

void CTaskbarNotifier::SetTextColor(COLORREF crNormalTextColor, COLORREF crSelectedTextColor)
{
	m_crNormalTextColor = crNormalTextColor;
	m_crSelectedTextColor = crSelectedTextColor;
	RedrawWindow(&m_rcText);
}

void CTaskbarNotifier::SetTextRect(const RECT &rcText)
{
	m_rcText = rcText;
}

void CTaskbarNotifier::SetCloseBtnRect(const RECT &rcCloseBtn)
{
	m_rcCloseBtn = rcCloseBtn;
}

void CTaskbarNotifier::SetHistoryBtnRect(const RECT &rcHistoryBtn)
{
	m_rcHistoryBtn = rcHistoryBtn;
}

void CTaskbarNotifier::SetTextFormat(UINT uTextFormat)
{
	m_uTextFormat = uTextFormat;
}

void CTaskbarNotifier::SetBitmapRegion(int red, int green, int blue)
{
	BITMAP bm;
	m_bitmapBackground.GetBitmap(&bm);
	m_nBitmapWidth = bm.bmWidth;
	m_nBitmapHeight = bm.bmHeight;
	m_bBitmapAlpha = false;

	if (red != -1 && green != -1 && blue != -1) {
		// No need to delete the HRGN, SetWindowRgn() owns it after being called
		m_hBitmapRegion = CreateRgnFromBitmap(m_bitmapBackground, RGB(red, green, blue));
		SetWindowRgn(m_hBitmapRegion, TRUE);
	}
}

bool CTaskbarNotifier::SetBitmap(UINT nBitmapID, int red, int green, int blue)
{
	m_bitmapBackground.DeleteObject();
	if (!m_bitmapBackground.LoadBitmap(nBitmapID))
		return false;
	ASSERT(m_nBitmapWidth != 0 && m_nBitmapHeight != 0);

	SetBitmapRegion(red, green, blue);
	return (m_nBitmapWidth != 0 && m_nBitmapHeight != 0);
}

bool CTaskbarNotifier::SetBitmap(CBitmap *pBitmap, int red, int green, int blue)
{
	m_bitmapBackground.DeleteObject();
	if (!m_bitmapBackground.Attach(pBitmap->Detach()))
		return false;

	SetBitmapRegion(red, green, blue);
	return true;
}

bool CTaskbarNotifier::SetBitmap(LPCTSTR pszFileName, int red, int green, int blue)
{
	if (pszFileName == NULL || pszFileName[0] == _T('\0'))
		return false;
	CEnBitmap Bitmap;
	if (!Bitmap.LoadImage(pszFileName))
		return false;
	m_bitmapBackground.DeleteObject();
	if (!m_bitmapBackground.Attach(Bitmap.Detach()))
		return false;

	SetBitmapRegion(red, green, blue);
	return true;
}

void CTaskbarNotifier::SetAutoClose(bool bAutoClose)
{
	m_bAutoClose = bAutoClose;
	if (bAutoClose) {
		switch (m_nAnimStatus) {
		case IDT_APPEARING:
		case IDT_WAITING:
		case IDT_DISAPPEARING:
			KillTimer(m_nAnimStatus);
		}
		m_nAnimStatus = IDT_DISAPPEARING;
		SetTimer(IDT_DISAPPEARING, m_dwHideEvents, NULL);
	}
}

void CTaskbarNotifier::ShowLastHistoryMessage()
{
	if (m_MessageHistory.IsEmpty())
		Show(GetResString(IDS_TBN_NOMESSAGEHISTORY), TBN_NULL, NULL);
	else {
		CTaskbarNotifierHistory *pHistMsg = static_cast<CTaskbarNotifierHistory*>(m_MessageHistory.RemoveHead());
		Show(pHistMsg->m_strMessage, pHistMsg->m_nMessageType, pHistMsg->m_strLink);
		delete pHistMsg;
	}
}

void CTaskbarNotifier::Show(LPCTSTR pszCaption, TbnMsg nMsgType, LPCTSTR pszLink, BOOL bAutoClose)
{
	if (nMsgType == TBN_NONOTIFY)
		return;

	if (!LoadConfiguration(m_strConfigFilePath) || m_nBitmapHeight <= 0 || m_nBitmapWidth <= 0) {
		ASSERT(0);
		return;
	}

	m_strCaption = pszCaption;
	m_nActiveMessageType = nMsgType;
	m_strLink = pszLink;

	if (m_bAutoClose) // sets it only if already true, else wait for user action
		m_bAutoClose = bAutoClose;

	if (nMsgType != TBN_NULL && nMsgType != TBN_LOG && nMsgType != TBN_IMPORTANTEVENT) {
		// Add element into string list. Max 5 elements.
		while (m_MessageHistory.GetCount() >= 5)
			delete static_cast<CTaskbarNotifierHistory*>(m_MessageHistory.RemoveHead());
		CTaskbarNotifierHistory *pHistMsg = new CTaskbarNotifierHistory;
		pHistMsg->m_strMessage = m_strCaption;
		pHistMsg->m_nMessageType = nMsgType;
		pHistMsg->m_strLink = m_strLink;
		m_MessageHistory.AddTail(pHistMsg);
	}

	UINT nScreenWidth = ::GetSystemMetrics(SM_CXSCREEN);
	//UINT nScreenHeight = ::GetSystemMetrics(SM_CYSCREEN);
	HWND hWndTaskbar = ::FindWindow(_T("Shell_TrayWnd"), 0);
	RECT rcTaskbar;
	::GetWindowRect(hWndTaskbar, &rcTaskbar);

	// Daniel Lohmann: Calculate taskbar position from its window rect. However, on XP it may be that
	// the taskbar is slightly larger or smaller than the screen size. Therefore we allow some tolerance here.
	DWORD nBitmapSize;
	if (NearlyEqual(rcTaskbar.left, 0, TASKBAR_X_TOLERANCE) && NearlyEqual(rcTaskbar.right, nScreenWidth, TASKBAR_X_TOLERANCE)) {
		// Taskbar is on top or on bottom
		m_nTaskbarPlacement = NearlyEqual(rcTaskbar.top, 0, TASKBAR_Y_TOLERANCE) ? ABE_TOP : ABE_BOTTOM;
		nBitmapSize = m_nBitmapHeight;
	} else {
		// Taskbar is on left or on right
		m_nTaskbarPlacement = NearlyEqual(rcTaskbar.left, 0, TASKBAR_X_TOLERANCE) ? ABE_LEFT : ABE_RIGHT;
		nBitmapSize = m_nBitmapWidth;
	}

	// We calculate the pixel increment and the timer value for the showing animation
	// For transparent bitmaps, all animations are disabled.
	DWORD dwTimeToShow = m_bBitmapAlpha ? 0 : m_dwTimeToShow;
	if (dwTimeToShow > m_dwTimerPrecision) {
		UINT nEvents = max(mini((dwTimeToShow / m_dwTimerPrecision) / 2u, nBitmapSize), 1); //<<-- enkeyDEV(Ottavio84) -Reduced frames of a half-
		m_dwShowEvents = dwTimeToShow / nEvents;
		m_nIncrementShow = nBitmapSize / nEvents;
	} else {
		m_dwShowEvents = m_dwTimerPrecision;
		m_nIncrementShow = nBitmapSize;
	}

	// We calculate the pixel increment and the timer value for the hiding animation
	// For transparent bitmaps, all animations are disabled.
	DWORD dwTimeToHide = m_bBitmapAlpha ? 0 : m_dwTimeToHide;
	if (dwTimeToHide > m_dwTimerPrecision) {
		UINT nEvents = max(mini((dwTimeToHide / m_dwTimerPrecision / 2), nBitmapSize), 1); //<<-- enkeyDEV(Ottavio84) -Reduced frames of a half-
		m_dwHideEvents = dwTimeToHide / nEvents;
		m_nIncrementHide = nBitmapSize / nEvents;
	} else {
		m_dwShowEvents = m_dwTimerPrecision;
		m_nIncrementHide = nBitmapSize;
	}

	// Compute init values for the animation
	switch (m_nAnimStatus) {
	case IDT_HIDDEN:
		switch (m_nTaskbarPlacement) {
		case ABE_BOTTOM: // Taskbar is on the bottom or Invisible
		case ABE_TOP:
			m_nCurrentPosX = rcTaskbar.right - m_nBitmapWidth;
			m_nCurrentPosY = (m_nTaskbarPlacement == ABE_BOTTOM) ? rcTaskbar.top : rcTaskbar.bottom;
			m_nCurrentWidth = m_nBitmapWidth;
			m_nCurrentHeight = 0;
			break;
		case ABE_LEFT:
		case ABE_RIGHT:
			m_nCurrentPosX = (m_nTaskbarPlacement == ABE_LEFT) ? rcTaskbar.right : rcTaskbar.left;
			m_nCurrentPosY = rcTaskbar.bottom - m_nBitmapHeight;
			m_nCurrentWidth = 0;
			m_nCurrentHeight = m_nBitmapHeight;
		}
		ShowWindow(SW_SHOWNOACTIVATE);
		SetTimer(IDT_APPEARING, m_dwShowEvents, NULL);
	case IDT_APPEARING:
	case IDT_WAITING:
		RedrawWindow(&m_rcText);
		if (m_nAnimStatus == IDT_WAITING)
			SetTimer(IDT_WAITING, m_dwTimeToStay, NULL);
		break;
	case IDT_DISAPPEARING:
		KillTimer(IDT_DISAPPEARING);
		SetTimer(IDT_WAITING, m_dwTimeToStay, NULL);
		switch (m_nTaskbarPlacement) {
		case ABE_BOTTOM:
			m_nCurrentPosY = rcTaskbar.top - m_nBitmapHeight;
			m_nCurrentHeight = m_nBitmapHeight;
			break;
		case ABE_TOP:
			m_nCurrentPosY = rcTaskbar.bottom;
			m_nCurrentHeight = m_nBitmapHeight;
			break;
		case ABE_LEFT:
			m_nCurrentPosX = rcTaskbar.right;
			m_nCurrentWidth = m_nBitmapWidth;
			break;
		case ABE_RIGHT:
			m_nCurrentPosX = rcTaskbar.left - m_nBitmapWidth;
			m_nCurrentWidth = m_nBitmapWidth;
		}

		SetWindowPos(&wndTopMost, m_nCurrentPosX, m_nCurrentPosY, m_nCurrentWidth, m_nCurrentHeight, SWP_NOACTIVATE);
		RedrawWindow(&m_rcText);
	}
}

void CTaskbarNotifier::Hide()
{
	switch (m_nAnimStatus) {
	case IDT_APPEARING:
	case IDT_WAITING:
	case IDT_DISAPPEARING:
		KillTimer(m_nAnimStatus);
	}
	MoveWindow(0, 0, 0, 0);
	ShowWindow(SW_HIDE);
	m_nAnimStatus = IDT_HIDDEN;
	m_nActiveMessageType = TBN_NULL;
}

HRGN CTaskbarNotifier::CreateRgnFromBitmap(HBITMAP hBmp, COLORREF color)
{
	if (hBmp == NULL)
		return NULL;

	CDC *pDC = GetDC();
	if (pDC == NULL)
		return NULL;

	BITMAP bm;
	GetObject(hBmp, sizeof bm, &bm);

	ASSERT(!m_bBitmapAlpha);
	const BYTE *pBitmapBits = NULL;
	if (bm.bmBitsPixel == 32 && m_pfnAlphaBlend) {
		DWORD dwBitmapBitsSize = GetBitmapBits(hBmp, 0, NULL);
		if (dwBitmapBitsSize) {
			pBitmapBits = (BYTE*)malloc(dwBitmapBitsSize);
			if (pBitmapBits) {
				if (GetBitmapBits(hBmp, dwBitmapBitsSize, (LPVOID)pBitmapBits) == (LONG)dwBitmapBitsSize) {
					const BYTE *pLine = pBitmapBits;
					int iLines = bm.bmHeight;
					while (!m_bBitmapAlpha && iLines-- > 0) {
						const DWORD *pdwPixel = (DWORD*)pLine;
						for (int x = 0; x < bm.bmWidth; ++x)
							if (*pdwPixel++ & 0xFF000000) {
								m_bBitmapAlpha = true;
								break;
							}

						pLine += bm.bmWidthBytes;
					}
				}
				if (!m_bBitmapAlpha) {
					free((void*)pBitmapBits);
					pBitmapBits = NULL;
				}
			}
		}
	}

	CDC dcBmp;
	dcBmp.CreateCompatibleDC(pDC);
	HGDIOBJ hOldBmp = dcBmp.SelectObject(hBmp);
	HRGN hRgn = NULL;

	// allocate memory for region data
	const DWORD MAXBUF = 40;	// size of one block in RECTs (i.e. MAXBUF*sizeof(RECT) in bytes)
	DWORD cBlocks = 0;			// number of allocated blocks
	RGNDATAHEADER *pRgnData = (RGNDATAHEADER*)malloc(sizeof(RGNDATAHEADER) + (++cBlocks) * MAXBUF * sizeof(RECT));
	if (pRgnData) {
		// fill it by default
		pRgnData->dwSize = sizeof(RGNDATAHEADER);
		pRgnData->iType = RDH_RECTANGLES;
		pRgnData->nCount = 0;

		INT iFirstXPos = 0;		// left position of current scan line where mask was found
		bool bWasFirst = false;	// set when mask was found in current scan line

		const BYTE *pBitmapLine = (pBitmapBits != NULL) ? pBitmapBits + bm.bmWidthBytes * (bm.bmHeight - 1) : NULL;
		for (int y = 0; pRgnData != NULL && y < bm.bmHeight; ++y) {
			for (int x = 0; x < bm.bmWidth; ++x) {
				// get color
				bool bIsMask;
				if (pBitmapLine)
					bIsMask = ((((DWORD*)pBitmapLine)[x] & 0xFF000000) != 0);
				else
					bIsMask = (dcBmp.GetPixel(x, bm.bmHeight - y - 1) != color);

				// place part of scan line as RECT region if transparent color found after mask color or
				// mask color found at the end of mask image
				if (bWasFirst && (!bIsMask || x == bm.bmWidth - 1)) {
					// get offset to RECT array if RGNDATA buffer
					LPRECT pRects = (LPRECT)(pRgnData + 1);

					// save current RECT
					pRects[pRgnData->nCount++] = CRect(iFirstXPos, bm.bmHeight - y - 1, x + (x == bm.bmWidth - 1), bm.bmHeight - y);

					// if buffer full reallocate it
					if (pRgnData->nCount >= cBlocks * MAXBUF) {
						RGNDATAHEADER *pNewRgnData = (RGNDATAHEADER*)realloc(pRgnData, sizeof(RGNDATAHEADER) + (++cBlocks) * MAXBUF * sizeof(RECT));
						if (pNewRgnData == NULL) {
							free(pRgnData);
							pRgnData = NULL;
							break;
						}
						pRgnData = pNewRgnData;
					}
					bWasFirst = false;
				} else if (!bWasFirst && bIsMask) {
					iFirstXPos = x;
					bWasFirst = true;
				}
			}
			if (pBitmapBits)
				pBitmapLine -= bm.bmWidthBytes;
		}

		if (pRgnData) {
			// Create region
			// WinNT: 'ExtCreateRegion' returns NULL (by Fable@aramszu.net)
			hRgn = ::CreateRectRgn(0, 0, 0, 0);
			if (hRgn) {
				LPCRECT pRects = (LPRECT)(pRgnData + 1);
				for (DWORD i = 0; i < pRgnData->nCount; ++i) {
					HRGN hr = ::CreateRectRgn(pRects[i].left, pRects[i].top, pRects[i].right, pRects[i].bottom);
					if (hr) {
						VERIFY(CombineRgn(hRgn, hRgn, hr, RGN_OR) != ERROR);
						::DeleteObject(hr);
					}
				}
			}

			free(pRgnData);
		}
	}

	dcBmp.SelectObject(hOldBmp);
	dcBmp.DeleteDC();
	free((void*)pBitmapBits);
	ReleaseDC(pDC);
	return hRgn;
}

int CTaskbarNotifier::GetMessageType()
{
	return m_nActiveMessageType;
}

void CTaskbarNotifier::OnMouseMove(UINT nFlags, CPoint point)
{
	TRACKMOUSEEVENT tme;
	tme.cbSize = (DWORD)sizeof tme;
	tme.dwFlags = TME_LEAVE | TME_HOVER;
	tme.hwndTrack = m_hWnd;
	tme.dwHoverTime = 1;

	// We Tell Windows we want to receive WM_MOUSEHOVER and WM_MOUSELEAVE
	::_TrackMouseEvent(&tme);

	CWnd::OnMouseMove(nFlags, point);
}

void CTaskbarNotifier::OnLButtonUp(UINT nFlags, CPoint point)
{
	// close button clicked
	if (m_rcCloseBtn.PtInRect(point)) {
		m_bAutoClose = true;	// set true so next time an autoclose event arrives, the popup will autoclose
								// (when m_bAutoClose is false a "true" event will be ignored until the user
								// manually closed the windows)
		switch (m_nAnimStatus) {
		case IDT_APPEARING:
		case IDT_WAITING:
		case IDT_DISAPPEARING:
			KillTimer(m_nAnimStatus);
		}
		m_nAnimStatus = IDT_DISAPPEARING;
		SetTimer(IDT_DISAPPEARING, m_dwHideEvents, NULL);
	}

	// cycle history button clicked
	if (m_rcHistoryBtn.PtInRect(point) && !m_MessageHistory.IsEmpty()) {
		CTaskbarNotifierHistory *pHistMsg = static_cast<CTaskbarNotifierHistory*>(m_MessageHistory.RemoveHead());
		Show(pHistMsg->m_strMessage, pHistMsg->m_nMessageType, pHistMsg->m_strLink);
		delete pHistMsg;
	}

	// message clicked
	if (m_rcText.PtInRect(point)) {
		// Notify the parent window that the Notifier popup was clicked
		LPCTSTR pszLink = m_strLink.IsEmpty() ? NULL : _tcsdup(m_strLink);
		m_pWndParent->PostMessage(UM_TASKBARNOTIFIERCLICKED, 0, (LPARAM)pszLink);
	}

	CWnd::OnLButtonUp(nFlags, point);
}

LRESULT CTaskbarNotifier::OnMouseHover(WPARAM, LPARAM lParam)
{
	if (m_nAnimStatus == IDT_WAITING)
		KillTimer(IDT_WAITING);

	POINTS mp = MAKEPOINTS(lParam);
	m_ptMousePosition.SetPoint(mp.x, mp.y);

	if (!m_bMouseIsOver) {
		m_bMouseIsOver = true;
		RedrawWindow(&m_rcText);
	} else if (m_rcText.PtInRect(m_ptMousePosition)) {
		if (!m_bTextSelected)
			RedrawWindow(&m_rcText);
	} else if (m_bTextSelected)
		RedrawWindow(&m_rcText);

	return 0;
}

LRESULT CTaskbarNotifier::OnMouseLeave(WPARAM, LPARAM)
{
	if (m_bMouseIsOver) {
		m_bMouseIsOver = false;
		RedrawWindow(&m_rcText);
		if (m_nAnimStatus == IDT_WAITING)
			SetTimer(IDT_WAITING, m_dwTimeToStay, NULL);
	}
	return 0;
}

BOOL CTaskbarNotifier::OnEraseBkgnd(CDC *pDC)
{
	CDC memDC;
	memDC.CreateCompatibleDC(pDC);
	CBitmap *pOldBitmap = memDC.SelectObject(&m_bitmapBackground);
	if (m_bBitmapAlpha && m_pfnAlphaBlend) {
		static const BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
		(*m_pfnAlphaBlend)(pDC->m_hDC, 0, 0, m_nCurrentWidth, m_nCurrentHeight,
			memDC.m_hDC, 0, 0, m_nCurrentWidth, m_nCurrentHeight, bf);
	} else
		pDC->BitBlt(0, 0, m_nCurrentWidth, m_nCurrentHeight, &memDC, 0, 0, SRCCOPY);

	memDC.SelectObject(pOldBitmap);
	return TRUE;
}

void CTaskbarNotifier::OnPaint()
{
	CPaintDC dc(this);
	CFont *pOldFont;
	if (m_bMouseIsOver) {
		if (m_rcText.PtInRect(m_ptMousePosition)) {
			m_bTextSelected = true;
			dc.SetTextColor(m_crSelectedTextColor);
			pOldFont = dc.SelectObject(&m_fontSelected);
		} else {
			m_bTextSelected = false;
			dc.SetTextColor(m_crNormalTextColor);
			pOldFont = dc.SelectObject(&m_fontNormal);
		}
	} else {
		dc.SetTextColor(m_crNormalTextColor);
		pOldFont = dc.SelectObject(&m_fontNormal);
	}

	dc.SetBkMode(TRANSPARENT);
	dc.DrawText(m_strCaption, m_rcText, m_uTextFormat);

	dc.SelectObject(pOldFont);
}

BOOL CTaskbarNotifier::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message)
{
	if (nHitTest == HTCLIENT) {
		if (m_rcCloseBtn.PtInRect(m_ptMousePosition)
			|| m_rcHistoryBtn.PtInRect(m_ptMousePosition)
			|| m_rcText.PtInRect(m_ptMousePosition))
		{
			::SetCursor(m_hCursor);
			return TRUE;
		}
	}
	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

void CTaskbarNotifier::OnTimer(UINT_PTR nIDEvent)
{
	bool bKillTimer = false;
	switch (nIDEvent) {
	case IDT_APPEARING:
		m_nAnimStatus = IDT_APPEARING;
		switch (m_nTaskbarPlacement) {
		case ABE_BOTTOM:
		case ABE_TOP:
			bKillTimer = (m_nCurrentHeight >= m_nBitmapHeight);
			if (!bKillTimer) {
				m_nCurrentHeight += m_nIncrementShow;
				if (m_nTaskbarPlacement == ABE_BOTTOM)
					m_nCurrentPosY -= m_nIncrementShow;
			}
			break;
		case ABE_LEFT:
		case ABE_RIGHT:
			bKillTimer = (m_nCurrentWidth >= m_nBitmapWidth);
			if (!bKillTimer) {
				m_nCurrentWidth += m_nIncrementShow;
				if (m_nTaskbarPlacement == ABE_RIGHT)
					m_nCurrentPosX -= m_nIncrementShow;
			}
		}
		if (bKillTimer) {
			KillTimer(IDT_APPEARING);
			SetTimer(IDT_WAITING, m_dwTimeToStay, NULL);
			m_nAnimStatus = IDT_WAITING;
		}
		break;
	case IDT_WAITING:
		KillTimer(IDT_WAITING);
		if (m_bAutoClose)
			SetTimer(IDT_DISAPPEARING, m_dwHideEvents, NULL);
		break;
	case IDT_DISAPPEARING:
		m_nAnimStatus = IDT_DISAPPEARING;
		switch (m_nTaskbarPlacement) {
		case ABE_BOTTOM:
		case ABE_TOP:
			bKillTimer = (m_nCurrentHeight <= 0);
			if (!bKillTimer) {
				m_nCurrentHeight -= m_nIncrementHide;
				if (m_nTaskbarPlacement == ABE_BOTTOM)
					m_nCurrentPosY += m_nIncrementHide;
			}
			break;
		case ABE_LEFT:
		case ABE_RIGHT:
			bKillTimer = (m_nCurrentWidth <= 0);
			if (!bKillTimer) {
				m_nCurrentWidth -= m_nIncrementHide;
				if (m_nTaskbarPlacement == ABE_RIGHT)
					m_nCurrentPosX += m_nIncrementHide;
			}
		}
		if (bKillTimer) {
			KillTimer(IDT_DISAPPEARING);
			Hide();
		}
	}

	if (nIDEvent == IDT_APPEARING || nIDEvent == IDT_DISAPPEARING) {
		HRGN hRgn = ::CreateRectRgn(0, 0, 0, 0);
		GetWindowRgn(hRgn);
		SetWindowPos(&wndTopMost, m_nCurrentPosX, m_nCurrentPosY, m_nCurrentWidth, m_nCurrentHeight, SWP_NOACTIVATE);
		VERIFY(SetWindowRgn(hRgn, TRUE) != 0);
	}
	CWnd::OnTimer(nIDEvent);
}