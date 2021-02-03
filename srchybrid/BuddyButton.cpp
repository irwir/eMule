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
//
// Based on idea from http://www.gipsysoft.com/articles/BuddyButton/
#include "stdafx.h"
#include "emule.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static LPCTSTR const s_szPropOldWndProc = _T("PropBuddyButtonOldWndProc");
static LPCTSTR const s_szPropBuddyData = _T("PropBuddyButtonData");

struct SBuddyData
{
	UINT m_uButtonWidth;
	HWND m_hwndButton;
};

static LRESULT CALLBACK BuddyButtonSubClassedProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	WNDPROC	pfnOldWndProc = (WNDPROC)::GetProp(hWnd, s_szPropOldWndProc);
	ASSERT(pfnOldWndProc != NULL);

	SBuddyData *pBuddyData = static_cast<SBuddyData*>(::GetProp(hWnd, s_szPropBuddyData));
	ASSERT(pBuddyData != NULL);

	switch (uMessage) {
	case WM_NCDESTROY:
		::SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)pfnOldWndProc);
		VERIFY(RemoveProp(hWnd, s_szPropOldWndProc) != NULL);
		VERIFY(RemoveProp(hWnd, s_szPropBuddyData) != NULL);
		delete pBuddyData;
		break;
	case WM_NCHITTEST:
		{
			LRESULT lResult = CallWindowProc(pfnOldWndProc, hWnd, uMessage, wParam, lParam);
			return (lResult == HTNOWHERE) ? HTTRANSPARENT : lResult;
		}
	case WM_NCCALCSIZE:
		{
			LRESULT lResult = CallWindowProc(pfnOldWndProc, hWnd, uMessage, wParam, lParam);
			LPNCCALCSIZE_PARAMS lpNCCS = (LPNCCALCSIZE_PARAMS)lParam;
			lpNCCS->rgrc[0].right -= pBuddyData->m_uButtonWidth;
			return lResult;
		}
	case WM_SIZE:
		{
			CRect rc;
			GetClientRect(hWnd, rc);
			rc.left = rc.right;
			rc.right += pBuddyData->m_uButtonWidth;
			::MapWindowPoints(hWnd, GetParent(hWnd), &rc.TopLeft(), 2);
			::SetWindowPos(pBuddyData->m_hwndButton, NULL, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOZORDER);
		}
	}
	return CallWindowProc(pfnOldWndProc, hWnd, uMessage, wParam, lParam);
}

void AddBuddyButton(HWND hwndEdit, HWND hwndButton)
{
	FARPROC lpfnOldWndProc = (FARPROC)::SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)BuddyButtonSubClassedProc);
	ASSERT(lpfnOldWndProc != NULL);
	VERIFY(::SetProp(hwndEdit, s_szPropOldWndProc, (HANDLE)lpfnOldWndProc));

	// Remove the 'flat' style which my have been set by 'InitWindowStyles'
	LONG_PTR dwButtonStyle = ::GetWindowLongPtr(hwndButton, GWL_STYLE);
	if (dwButtonStyle & BS_FLAT)
		::SetWindowLongPtr(hwndButton, GWL_STYLE, dwButtonStyle & ~BS_FLAT);

	CRect rcButton;
	::GetWindowRect(hwndButton, rcButton);

	SBuddyData *pBuddyData = new SBuddyData;
	pBuddyData->m_uButtonWidth = rcButton.Width();
	pBuddyData->m_hwndButton = hwndButton;
	VERIFY(::SetProp(hwndEdit, s_szPropBuddyData, (HANDLE)pBuddyData));

	::SetWindowPos(hwndEdit, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

bool InitAttachedBrowseButton(HWND hwndButton, HICON &ricoBrowse)
{
	// Showing an icon button works for all Windows versions *except* Windows XP w/ active styles
	if (theApp.IsXPThemeActive())
		return false;

	if (ricoBrowse == NULL) {
		ricoBrowse = theApp.LoadIcon(_T("BrowseFolderSmall"));
		if (ricoBrowse == NULL)
			return false;
	}

	LONG_PTR dwStyle = ::GetWindowLongPtr(hwndButton, GWL_STYLE);
	::SetWindowLongPtr(hwndButton, GWL_STYLE, dwStyle | BS_ICON);
	::SendMessage(hwndButton, BM_SETIMAGE, IMAGE_ICON, (LPARAM)ricoBrowse);
	return true;
}