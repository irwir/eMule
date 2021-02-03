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
#include "ComboBoxEx2.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

///////////////////////////////////////////////////////////////////////////////
// CComboBoxEx2

IMPLEMENT_DYNAMIC(CComboBoxEx2, CComboBoxEx)

BEGIN_MESSAGE_MAP(CComboBoxEx2, CComboBoxEx)
END_MESSAGE_MAP()

int CComboBoxEx2::AddItem(LPCTSTR pszText, int iImage)
{
	COMBOBOXEXITEM cbi = {};
	cbi.mask = CBEIF_TEXT;
	cbi.iItem = -1;
	cbi.pszText = (LPTSTR)pszText;
	if (iImage >= 0) {
		cbi.mask |= CBEIF_IMAGE | CBEIF_SELECTEDIMAGE;
		cbi.iImage = iImage;
		cbi.iSelectedImage = iImage;
	}
	return InsertItem(&cbi);
}

BOOL CComboBoxEx2::PreTranslateMessage(MSG *pMsg)
{
	// there seems to be no way that we get the WM_CHARTOITEM for this control
	ASSERT(pMsg->message != WM_CHARTOITEM);

	if (pMsg->message == WM_KEYDOWN) {
		TCHAR uChar = (TCHAR)::MapVirtualKey((UINT)pMsg->wParam, MAPVK_VK_TO_CHAR);
		if (uChar != 0) {
			const TCHAR str[2] = { uChar, _T('\0') };
			if (SelectString(-1, str) != CB_ERR)
				return TRUE;
		}
	}
	return CComboBoxEx::PreTranslateMessage(pMsg);
}

BOOL CComboBoxEx2::SelectItemDataString(LPCTSTR pszText)
{
	CComboBox *pctrlCB = GetComboBoxCtrl();
	if (pctrlCB != NULL)
		for (int i = pctrlCB->GetCount(); --i >= 0;) {
			void *pvItemData = GetItemDataPtr(i);
			if (pvItemData && _tcscmp((LPCTSTR)pvItemData, pszText) == 0) {
				SetCurSel(i);
				GetParent()->SendMessage(WM_COMMAND, MAKELONG((WORD)::GetWindowLongPtr(m_hWnd, GWLP_ID), CBN_SELCHANGE), (LPARAM)m_hWnd);
				return TRUE;
			}
		}

	return FALSE;
}

void UpdateHorzExtent(CComboBox &rctlComboBox, int iIconWidth)
{
	int iItemCount = rctlComboBox.GetCount();
	if (iItemCount > 0) {
		CDC *pDC = rctlComboBox.GetDC();
		if (pDC != NULL) {
			// *** To get *ACCURATE* results from 'GetOutputTextExtent' one *MUST*
			// *** explicitly set the font!
			CFont *pOldFont = pDC->SelectObject(rctlComboBox.GetFont());

			CString strItem;
			int iMaxWidth = 0;
			while (--iItemCount >= 0) {
				rctlComboBox.GetLBText(iItemCount, strItem);
				int iItemWidth = pDC->GetOutputTextExtent(strItem, strItem.GetLength()).cx;
				if (iItemWidth > iMaxWidth)
					iMaxWidth = iItemWidth;
			}

			pDC->SelectObject(pOldFont);
			rctlComboBox.ReleaseDC(pDC);

			// Depending on the string (lot of "M" or lot of "i") sometime the
			// width is just a few pixels too small!
			iMaxWidth += 4;
			if (iIconWidth)
				iMaxWidth += 2 + iIconWidth + 2;
			rctlComboBox.SetHorizontalExtent(iMaxWidth);
			if (rctlComboBox.GetDroppedWidth() < iMaxWidth)
				rctlComboBox.SetDroppedWidth(iMaxWidth);
		}
	} else
		rctlComboBox.SetHorizontalExtent(0);
}

HWND GetComboBoxEditCtrl(const CComboBox &cb)
{
	for (CWnd *pWnd = cb.GetWindow(GW_CHILD); pWnd; pWnd = pWnd->GetNextWindow()) {
		TCHAR szClassName[MAX_PATH];
		if (GetClassName(*pWnd, szClassName, _countof(szClassName)))
			if (_tcsicmp(szClassName, _T("EDIT")) == 0)
				return pWnd->m_hWnd;
	}
	return NULL;
}