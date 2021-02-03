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
#include "TitleMenu.h"
#include "emule.h"
#include "preferences.h"
#include "otherfunctions.h"
#include "CxImage/xImage.h"
#include <atlimage.h>
#include <winuser.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define MP_TITLE	0xFFFEu
#define ICONSIZE	16

CTitleMenu::CTitleMenu()
	: m_clRight(::GetSysColor(COLOR_GRADIENTACTIVECAPTION))
	, m_clLeft(::GetSysColor(COLOR_ACTIVECAPTION))
	, m_clText(::GetSysColor(COLOR_CAPTIONTEXT))
	, m_uEdgeFlags(BDR_SUNKENINNER)
	, m_bDrawEdge()
	, m_bIconMenu()
{
	m_mapMenuIdToIconIdx.InitHashTable(29);
}

CTitleMenu::~CTitleMenu()
{
	DeleteIcons();
}

void CTitleMenu::DeleteIcons()
{
	m_ImageList.DeleteImageList();
	m_mapIconNameToIconIdx.RemoveAll();
	m_mapMenuIdToIconIdx.RemoveAll();

	CString strKey;
	for (POSITION pos = m_mapIconNameToBitmap.GetStartPosition(); pos != NULL;) {
		void *pvBmp;
		m_mapIconNameToBitmap.GetNextAssoc(pos, strKey, pvBmp);
		VERIFY(::DeleteObject((HBITMAP)pvBmp));
	}
	m_mapIconNameToBitmap.RemoveAll();
}

BOOL CTitleMenu::CreateMenu()
{
	ASSERT(m_mapIconNameToIconIdx.IsEmpty());
	ASSERT(m_mapMenuIdToIconIdx.IsEmpty());
	ASSERT(m_ImageList.m_hImageList == NULL || m_ImageList.GetImageCount() == 0);
	ASSERT(m_mapIconNameToBitmap.IsEmpty());
	return __super::CreateMenu();
}

BOOL CTitleMenu::DestroyMenu()
{
	BOOL bResult = __super::DestroyMenu();
	DeleteIcons();
	return bResult;
}

void CTitleMenu::AddMenuTitle(LPCTSTR lpszTitle, bool bIsIconMenu)
{
	// insert an empty owner-draw item at top to serve as the title
	// note: item is not selectable (disabled) but not grayed
	//
	// Vista: Adding at least one MF_OWNERDRAW item would render the entire menu in owner drawn mode,
	// and it would be quite expensive to get the native Vista menu styles back. We would need to draw
	// the entire menu with the Vista theme API -- no way. Thus, there is no title for context menus
	// under Vista - the title doesn't fit to the native Vista menu style anyway.
	if (lpszTitle != NULL && !theApp.IsVistaThemeActive()) {
		m_strTitle = lpszTitle;
		m_strTitle.Remove(_T('&'));
		CMenu::InsertMenu(0, MF_BYPOSITION | MF_OWNERDRAW | MF_STRING | MF_DISABLED, MP_TITLE);
	}
	if (bIsIconMenu)
		EnableIcons();
}

void CTitleMenu::EnableIcons()
{
	switch (thePrefs.GetWindowsVersion()) {
	case _WINVER_95_:
	case _WINVER_NT4_:
		return;
	}
	m_bIconMenu = true;
	m_ImageList.DeleteImageList();
	m_ImageList.Create(ICONSIZE, ICONSIZE, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);

	MENUINFO mi;
	mi.cbSize = (DWORD)sizeof mi;
	mi.fMask = MIM_STYLE;
	GetMenuInfo(&mi);
	mi.dwStyle |= MNS_CHECKORBMP;
	SetMenuInfo(&mi);
}

// NOTE: This function is no longer used for Vista!
void CTitleMenu::MeasureItem(LPMEASUREITEMSTRUCT lpMIS)
{
	if (lpMIS->itemID == MP_TITLE) {
		CDC dc;
		dc.Attach(::GetDC(HWND_DESKTOP));
		HFONT hfontOld = (HFONT)::SelectObject(dc.m_hDC, (HFONT)theApp.m_fontDefaultBold);
		CSize size = dc.GetTextExtent(m_strTitle);
		::SelectObject(dc.m_hDC, hfontOld);
		size.cx += ::GetSystemMetrics(SM_CXMENUCHECK) + 8;
		::ReleaseDC(NULL, dc.Detach());

		static const int nBorderSize = 2;
		lpMIS->itemWidth = size.cx + nBorderSize;
		lpMIS->itemHeight = size.cy + nBorderSize;
	} else {
		CMenu::MeasureItem(lpMIS);
		if (m_bIconMenu) {
			lpMIS->itemHeight = max(lpMIS->itemHeight, 16);
			lpMIS->itemWidth += 18;
		}
	}
}

// NOTE: This function is no longer used for Vista!
void CTitleMenu::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
	if (lpDIS->itemID == MP_TITLE) {
		COLORREF crOldBk = ::SetBkColor(lpDIS->hDC, m_clLeft);

		if (!g_bLowColorDesktop &&/* m_pfnGradientFill &&*/ m_clLeft != m_clRight) {
			TRIVERTEX rcVertex[2];
			lpDIS->rcItem.right--; // exclude this point, like FillRect does
			lpDIS->rcItem.bottom--;
			rcVertex[0].x = lpDIS->rcItem.left;
			rcVertex[0].y = lpDIS->rcItem.top;
			rcVertex[0].Red = GetRValue(m_clLeft) << 8;	// color values from 0x0000 to 0xff00 !!!!
			rcVertex[0].Green = GetGValue(m_clLeft) << 8;
			rcVertex[0].Blue = GetBValue(m_clLeft) << 8;
			rcVertex[0].Alpha = 0;
			rcVertex[1].x = lpDIS->rcItem.right;
			rcVertex[1].y = lpDIS->rcItem.bottom;
			rcVertex[1].Red = GetRValue(m_clRight) << 8;
			rcVertex[1].Green = GetGValue(m_clRight) << 8;
			rcVertex[1].Blue = GetBValue(m_clRight) << 8;
			rcVertex[1].Alpha = 0;
			GRADIENT_RECT rect;
			rect.UpperLeft = 0;
			rect.LowerRight = 1;
			::GradientFill(lpDIS->hDC, rcVertex, 2, &rect, 1, GRADIENT_FILL_RECT_H);
		} else
			::ExtTextOut(lpDIS->hDC, 0, 0, ETO_OPAQUE, &lpDIS->rcItem, NULL, 0, NULL);

		if (m_bDrawEdge)
			::DrawEdge(lpDIS->hDC, &lpDIS->rcItem, m_uEdgeFlags, BF_RECT);

		int modeOld = ::SetBkMode(lpDIS->hDC, TRANSPARENT);
		COLORREF crOld = ::SetTextColor(lpDIS->hDC, m_clText);
		HFONT hfontOld = (HFONT)::SelectObject(lpDIS->hDC, (HFONT)theApp.m_fontDefaultBold);
		lpDIS->rcItem.left += ::GetSystemMetrics(SM_CXMENUCHECK) + 8;
		::DrawText(lpDIS->hDC, m_strTitle, -1, &lpDIS->rcItem, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
		::SelectObject(lpDIS->hDC, hfontOld);
		::SetTextColor(lpDIS->hDC, crOld);
		::SetBkMode(lpDIS->hDC, modeOld);

		::SetBkColor(lpDIS->hDC, crOldBk);
	} else {
		CDC *dc = CDC::FromHandle(lpDIS->hDC);
		int posY = lpDIS->rcItem.top + ((lpDIS->rcItem.bottom - lpDIS->rcItem.top) - ICONSIZE) / 2;
		int nIconPos;
		if (m_mapMenuIdToIconIdx.Lookup(lpDIS->itemID, nIconPos))
			if ((lpDIS->itemState & ODS_GRAYED) != 0)
				DrawMonoIcon(nIconPos, CPoint(lpDIS->rcItem.left, posY), dc);
			else
				// Draw the bitmap on the menu.
				m_ImageList.Draw(dc, nIconPos, CPoint(lpDIS->rcItem.left, posY), ILD_TRANSPARENT);
	}
}

BOOL CTitleMenu::AppendMenu(UINT nFlags, UINT_PTR nIDNewItem, LPCTSTR lpszNewItem, LPCTSTR lpszIconName)
{
	BOOL bResult = CMenu::AppendMenu(nFlags, nIDNewItem, lpszNewItem);
	if (bResult)
		SetMenuBitmap(nFlags, (UINT)nIDNewItem, lpszNewItem, lpszIconName);
	return bResult;
}

BOOL CTitleMenu::InsertMenu(UINT nPosition, UINT nFlags, UINT_PTR nIDNewItem, LPCTSTR lpszNewItem, LPCTSTR lpszIconName)
{
	BOOL bResult = CMenu::InsertMenu(nPosition, nFlags, nIDNewItem, lpszNewItem);
	if (bResult)
		SetMenuBitmap(nFlags, (UINT)nIDNewItem, lpszNewItem, lpszIconName);
	return bResult;
}

BOOL CTitleMenu::RenameMenu(UINT_PTR nIDNewItem, UINT nFlags, LPCTSTR lpszNewItem, LPCTSTR lpszIconName)
{
	MENUITEMINFO mi = {};
	mi.cbSize = (UINT)sizeof(MENUITEMINFO);
	mi.fMask = MIIM_TYPE;
	mi.fType = MFT_STRING;
	mi.dwTypeData = const_cast<LPTSTR>(lpszNewItem);
	BOOL bResult = SetMenuItemInfo((UINT)nIDNewItem, &mi, nFlags == MF_BYPOSITION);
	if (bResult)
		SetMenuBitmap(0, (UINT)nIDNewItem, lpszNewItem, lpszIconName);
	return bResult;
}

static HBITMAP Create32BitHBITMAP(HDC hdc, int cx, int cy, void **ppvBits = NULL)
{
	HBITMAP hBmp = NULL;
	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = cx;
	bmi.bmiHeader.biHeight = cy;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	HDC hdcUsed = hdc ? hdc : GetDC(NULL);
	if (hdcUsed) {
		hBmp = ::CreateDIBSection(hdcUsed, &bmi, DIB_RGB_COLORS, ppvBits, NULL, 0);
		if (hdc != hdcUsed)
			::ReleaseDC(NULL, hdcUsed);
	}
	return hBmp;
}

static HBITMAP IconToBitmap32(HICON hIcon, int cx, int cy)
{
	HBITMAP hBmp = NULL;
	HDC hdcDest = ::CreateCompatibleDC(NULL);
	if (hdcDest) {
		hBmp = ::Create32BitHBITMAP(hdcDest, cx, cy);
		if (hBmp) {
			HBITMAP hbmpOld = (HBITMAP)::SelectObject(hdcDest, hBmp);
			if (hbmpOld) {
				// "DrawIconEx" works well only for icons which do also have an XP version specified.
				// For 256 color icons the icons drawn by "DrawIconEx" are way too "bright"?
				//::DrawIconEx(hdcDest, 0, 0, hIcon, cx, cy, 0, NULL, DI_NORMAL);

				// Not as efficient as "DrawIconEx", but using an image list works for XP icons
				// as well as for 256 color icons.
				HIMAGELIST himl = ::ImageList_Create(cx, cy, theApp.m_iDfltImageListColorFlags | ILC_MASK, 1, 0);
				if (himl) {
					::ImageList_AddIcon(himl, hIcon);
					::ImageList_Draw(himl, 0, hdcDest, 0, 0, ILD_NORMAL);
					::ImageList_Destroy(himl);
				}

				::SelectObject(hdcDest, hbmpOld);
			}
		}
		::DeleteDC(hdcDest);
	}
	return hBmp;
}

void CTitleMenu::SetMenuBitmap(UINT nFlags, UINT nIDNewItem, LPCTSTR /*lpszNewItem*/, LPCTSTR lpszIconName)
{
	if (!m_bIconMenu || (nFlags & MF_SEPARATOR) != 0 || thePrefs.GetWindowsVersion() < _WINVER_2K_) {
		if (m_bIconMenu && lpszIconName != NULL)
			ASSERT(0);
		return;
	}

	// Those MFC warnings which are thrown when one opens certain context menus
	// are because of sub menu items. All the IDs shown in the warnings are sub
	// menu handles! Seems to be a bug in MFC. Look at '_AfxFindPopupMenuFromID'.
	// ---
	// Warning: unknown WM_MEASUREITEM for menu item 0x530601.
	// Warning: unknown WM_MEASUREITEM for menu item 0x4305E7.
	// ---
	//if (nFlags & MF_POPUP)
	//	TRACE(_T("TitleMenu: adding popup menu item id=%x  str=%s\n"), nIDNewItem, lpszNewItem);

	CString strIconLower(lpszIconName);
	strIconLower.MakeLower();
	if (thePrefs.GetWindowsVersion() >= _WINVER_VISTA_) {
		// Vista+: Use the Windows built-in feature for 32-bit menu item bitmaps.
		// 'MeasureItem', 'DrawItem' will not get called any longer and Vista
		// cares properly about grayed/selected menu item bitmaps.
		if (!strIconLower.IsEmpty()) {
			HBITMAP hBmp;
			void *pvBmp;
			if (m_mapIconNameToBitmap.Lookup(strIconLower, pvBmp))
				hBmp = (HBITMAP)pvBmp;
			else {
				HICON hIcon = theApp.LoadIcon(strIconLower);
				if (hIcon) {
					hBmp = IconToBitmap32(hIcon, ICONSIZE, ICONSIZE);
					VERIFY(::DestroyIcon(hIcon));
				} else
					hBmp = NULL;
			}

			if (hBmp) {
				//MENUITEMINFOEX info = {};
				MENUITEMINFO info = {};
				info.cbSize = (UINT)sizeof info;
				info.fMask = MIIM_BITMAP;
				info.hbmpItem = hBmp;
				VERIFY(SetMenuItemInfo(nIDNewItem, (MENUITEMINFO*)&info, FALSE));
				m_mapIconNameToBitmap.SetAt(strIconLower, hBmp);
			}
		}
	} else if (!strIconLower.IsEmpty()) {
		// pre-Vista: Use owner drawn menu items which are handled in 'MeasureItem' and 'DrawItem'
		int nPos;
		void *pvIndex;
		if (m_mapIconNameToIconIdx.Lookup(strIconLower, pvIndex)) {
			nPos = (int)pvIndex;
			m_mapMenuIdToIconIdx.SetAt(nIDNewItem, nPos);
		} else {
			HICON hIcon = theApp.LoadIcon(strIconLower);
			if (hIcon) {
				nPos = m_ImageList.Add(hIcon);
				if (nPos >= 0) {
					m_mapIconNameToIconIdx.SetAt(strIconLower, (void*)nPos);
					m_mapMenuIdToIconIdx.SetAt(nIDNewItem, nPos);

					// It doesn't work to use API checkmark bitmaps in a sufficient way. The size
					// of those bitmaps is limited and smaller than our menu item bitmaps.
					/*if (nFlags & MF_CHECKED) {
						HDC hdcScreen = ::GetDC(HWND_DESKTOP);
						if (hdcScreen) {
							CDC *pdcScreen = CDC::FromHandle(hdcScreen);
							CDC dcMem;
							dcMem.CreateCompatibleDC(pdcScreen);

							CBitmap bmpCheckmark;
							bmpCheckmark.CreateCompatibleBitmap(pdcScreen, ICONSIZE+4, ICONSIZE+4);
							CBitmap *pBmpOld = dcMem.SelectObject(&bmpCheckmark);
							static const RECT rc = {0, 0, ICONSIZE + 4, ICONSIZE + 4};
							dcMem.FillSolidRect(&rc, RGB(255, 255, 255));
							m_ImageList.Draw(&dcMem, nPos, CPoint(), ILD_TRANSPARENT);
							dcMem.SelectObject(pBmpOld);

							MENUITEMINFO mii = {};
							mii.cbSize = (UINT)sizeof mii;
							mii.fMask = MIIM_CHECKMARKS;
							mii.hbmpChecked = (HBITMAP)bmpCheckmark.Detach(); // resource leak
							VERIFY(SetMenuItemInfo(nIDNewItem, &mii));

							::ReleaseDC(HWND_DESKTOP, hdcScreen);
						}
					}*/
				}
				VERIFY(::DestroyIcon(hIcon));
			} else
				nPos = -1;
		}
		if (nPos != -1) {
			MENUITEMINFO info = {};
			info.cbSize = (UINT)sizeof info;
			info.fMask = MIIM_BITMAP;
			info.hbmpItem = HBMMENU_CALLBACK;
			VERIFY(SetMenuItemInfo(nIDNewItem, (MENUITEMINFO*)&info, FALSE));
		}
	}
}

// NOTE: This function is no longer used for Vista!
void CTitleMenu::DrawMonoIcon(int nIconPos, CPoint nDrawPos, CDC *dc)
{
#if 1
	CWindowDC windowDC(0);
	CDC colorDC;
	colorDC.CreateCompatibleDC(0);
	colorDC.SetLayout(dc->GetLayout());
	CBitmap bmpColor;
	bmpColor.CreateCompatibleBitmap(&windowDC, ICONSIZE, ICONSIZE);
	CBitmap *bmpOldColor = colorDC.SelectObject(&bmpColor);
	colorDC.FillSolidRect(0, 0, ICONSIZE, ICONSIZE, dc->GetBkColor());
	CxImage imgBk, imgGray;
	imgBk.CreateFromHBITMAP((HBITMAP)bmpColor);
	m_ImageList.Draw(&colorDC, nIconPos, CPoint(), ILD_TRANSPARENT);
	imgGray.CreateFromHBITMAP((HBITMAP)bmpColor);
	if (imgGray.IsValid() && imgBk.IsValid()) {
		imgGray.GrayScale();
		imgBk.GrayScale();
		imgGray.SetTransIndex(imgGray.GetNearestIndex(imgBk.GetPixelColor(0, 0)));
		// Vista: "CxImage::Draw" fails because 'SaveDC' fails (without any specific Win32 error code).
		// I don't think that it is a bug in 'CxImage'. It seems to be related to the special DC which
		// is provided by Vista for disabled menu items which are not selected. The code below which
		// is using GDI+ instead of CxImage has a very similar problem due to that special DC.
		imgGray.Draw((HDC)*dc, nDrawPos.x, nDrawPos.y);
	}
	colorDC.SelectObject(bmpOldColor);
	colorDC.DeleteDC();
	bmpColor.DeleteObject();
#else
	// The code below does not solve the problems under Vista. Though I want to keep the code
	// as it does not require any "CxImage" functions.
	ULONG_PTR gdiplusToken = 0;
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) == Gdiplus::Ok) {
		HICON hIcon = m_ImageList.ExtractIconW(nIconPos);
		if (hIcon) {
			Gdiplus::Bitmap bmp(hIcon);
			VERIFY(::DestroyIcon(hIcon));

			static const Gdiplus::ColorMatrix colorMatrix = {
				0.299f, 0.299f, 0.299f, 0.0f, 0.0f,
				0.588f, 0.588f, 0.588f, 0.0f, 0.0f,
				0.111f, 0.111f, 0.111f, 0.0f, 0.0f,
				0.0f,   0.0f,   0.0f,   1.0f, 0.0f,
				0.0f,   0.0f,   0.0f,   0.0f, 1.0f
			};
			Gdiplus::ImageAttributes  imageAttributes;
			imageAttributes.SetColorMatrix(&colorMatrix, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);

#if 0
			// This does not create any visual result if the menu item is not selected ???
			// If the menu item is selected, the grey-scaled icon is drawn correctly.
			Gdiplus::Graphics graphics(*dc);
			graphics.DrawImage(&bmp, Gdiplus::Rect(nDrawPos.x, nDrawPos.y, ICONSIZE, ICONSIZE),
				0, 0, ICONSIZE, ICONSIZE, Gdiplus::UnitPixel, &imageAttributes);
#else
			// This creates a bad looking visual result if the menu item is not selected.
			// If the menu item is selected, the grey-scaled icon is drawn correctly.
			Gdiplus::Bitmap bmp2(ICONSIZE, ICONSIZE, PixelFormat32bppARGB/*TODO: REMOVE!!*/);
			Gdiplus::Graphics graphics2(&bmp2);
			graphics2.DrawImage(&bmp, Gdiplus::Rect(nDrawPos.x, nDrawPos.y, ICONSIZE, ICONSIZE),
				0, 0, ICONSIZE, ICONSIZE, Gdiplus::UnitPixel, &imageAttributes);
			HICON ico2;
			if (bmp2.GetHICON(&ico2) == Gdiplus::Ok) {
				::DrawIconEx(*dc, nDrawPos.x, nDrawPos.y, ico2, 0, 0, 0, 0, DI_NORMAL);
				VERIFY(::DestroyIcon(ico2));
			}
#endif
		}
		Gdiplus::GdiplusShutdown(gdiplusToken);
	}
#endif
}

bool CTitleMenu::HasEnabledItems() const
{
	for (int i = GetMenuItemCount(); --i >= 0;)
		if ((GetMenuState((UINT)i, MF_BYPOSITION) & (MF_DISABLED | MF_SEPARATOR | MF_GRAYED)) == 0)
			return true;
	return false;
}