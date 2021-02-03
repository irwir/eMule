//this file is part of eMule
//Copyright (C)2002-2010 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
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
#include "ListViewWalkerPropertySheet.h"
#include "UserMsgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define IDC_PREV	100
#define IDC_NEXT	101

// CListViewPropertySheet
IMPLEMENT_DYNAMIC(CListViewPropertySheet, CResizableSheet)

BEGIN_MESSAGE_MAP(CListViewPropertySheet, CResizableSheet)
END_MESSAGE_MAP()

CListViewPropertySheet::CListViewPropertySheet(UINT nIDCaption, CWnd *pParentWnd, UINT iSelectPage)
	:CResizableSheet(nIDCaption, pParentWnd, iSelectPage)
{
}

CListViewPropertySheet::CListViewPropertySheet(LPCTSTR pszCaption, CWnd *pParentWnd, UINT iSelectPage)
	: CResizableSheet(pszCaption, pParentWnd, iSelectPage)
{
}

void CListViewPropertySheet::InsertPage(int iIndex, CPropertyPage *pPage)
{
	ASSERT_VALID(this);
	ASSERT(pPage != NULL);
	ASSERT_KINDOF(CPropertyPage, pPage);
	ASSERT_VALID(pPage);

	m_pages.InsertAt(iIndex, pPage);
	BuildPropPageArray();

	if (m_hWnd != NULL) {
		PROPSHEETPAGE *ppsp = const_cast<PROPSHEETPAGE*>(m_psh.ppsp);
		for (UINT i = 0; i < m_psh.nPages; ++i) {
			if (i == (UINT)iIndex)
				break;
			(BYTE *&)ppsp += ppsp->dwSize;
		}

		HPROPSHEETPAGE hPSP = CreatePropertySheetPage(ppsp);
		if (hPSP == NULL)
			AfxThrowMemoryException();

		if (!SendMessage(PSM_INSERTPAGE, iIndex, (LPARAM)hPSP)) {
			DestroyPropertySheetPage(hPSP);
			AfxThrowMemoryException();
		}
	}
}

void CListViewPropertySheet::ChangedData()
{
	SendMessage(UM_DATA_CHANGED);

	for (int iPage = GetPageCount(); --iPage >= 0;) {
		CPropertyPage *pPage = GetPage(iPage);
		if (pPage && pPage->m_hWnd) {
			pPage->SendMessage(UM_DATA_CHANGED);
			pPage->SetModified(FALSE);
		}
	}
	GetActivePage()->OnSetActive();
}

// CListViewWalkerPropertySheet

IMPLEMENT_DYNAMIC(CListViewWalkerPropertySheet, CListViewPropertySheet)

BEGIN_MESSAGE_MAP(CListViewWalkerPropertySheet, CListViewPropertySheet)
	ON_BN_CLICKED(IDC_NEXT, OnNext)
	ON_BN_CLICKED(IDC_PREV, OnPrev)
END_MESSAGE_MAP()

CListViewWalkerPropertySheet::CListViewWalkerPropertySheet(UINT nIDCaption, CWnd *pParentWnd, UINT iSelectPage)
	: CListViewPropertySheet(nIDCaption, pParentWnd, iSelectPage)
	, m_pListCtrl()
{
}

CListViewWalkerPropertySheet::CListViewWalkerPropertySheet(LPCTSTR pszCaption, CWnd *pParentWnd, UINT iSelectPage)
	: CListViewPropertySheet(pszCaption, pParentWnd, iSelectPage)
	, m_pListCtrl()
{
}

BOOL CListViewWalkerPropertySheet::OnInitDialog()
{
	BOOL bResult = CListViewPropertySheet::OnInitDialog();

	//////////////////////////////////////////////////////////////////////////
	// Add additional controls
	//
	if (m_pListCtrl != NULL) {
		// switching from multi-selection to single selection is currently not supported -> disable Up/Down controls
		DWORD dwCtrlStyle = (m_aItems.GetSize() > 1) ? WS_DISABLED : 0;

		const struct
		{
			CButton	*pCtlBtn;
			LPCTSTR	pszLabel;
			LPCTSTR	pszSymbol;
			DWORD	dwStyle;
			UINT	uCtlId;
		} aCtrls[] =
		{
			{&m_ctlPrev, _T("&Prev"), _T("5"), WS_CHILD | WS_VISIBLE | WS_GROUP | WS_TABSTOP,  IDC_PREV},
			{&m_ctlNext, _T("&Next"), _T("6"), WS_CHILD | WS_VISIBLE | WS_GROUP | WS_TABSTOP,  IDC_NEXT}
		};

		int iLeftMostButtonId = IDOK;
		int iMax = 32767;
		static const int _aiPropSheetButtons[] = {IDOK, IDCANCEL, ID_APPLY_NOW, IDHELP};
		for (unsigned i = 0; i < _countof(_aiPropSheetButtons); ++i) {
			const CWnd *pBtn = GetDlgItem(_aiPropSheetButtons[i]);
			if (pBtn /*&& pBtn->IsWindowVisible()*/) {
				RECT rcBtn;
				pBtn->GetWindowRect(&rcBtn);
				ScreenToClient(&rcBtn);
				if (rcBtn.left < iMax) {
					iMax = rcBtn.left;
					iLeftMostButtonId = _aiPropSheetButtons[i];
				}
			}
		}

		CWnd *pctlOk = GetDlgItem(iLeftMostButtonId);
		CRect rcOk;
		pctlOk->GetWindowRect(rcOk);
		ScreenToClient(rcOk);
		CFont *pDefCtrlFont = pctlOk->GetFont();
		const int iNaviBtnWidth = rcOk.Width() / 2;

		for (unsigned i = 0; i < _countof(aCtrls); ++i) {
			RECT rc;
			rc.left = rcOk.left - (8 + iNaviBtnWidth) * (_countof(aCtrls) - i);
			rc.top = rcOk.top;
			rc.right = rc.left + iNaviBtnWidth;
			rc.bottom = rc.top + rcOk.Height();
			VERIFY(aCtrls[i].pCtlBtn->Create(aCtrls[i].pszLabel, dwCtrlStyle | aCtrls[i].dwStyle, rc, this, aCtrls[i].uCtlId));

			if (theApp.m_fontSymbol.m_hObject) {
				aCtrls[i].pCtlBtn->SetFont(&theApp.m_fontSymbol);
				aCtrls[i].pCtlBtn->SetWindowText(aCtrls[i].pszSymbol); // show down-arrow
			} else
				aCtrls[i].pCtlBtn->SetFont(pDefCtrlFont);
			AddAnchor(*aCtrls[i].pCtlBtn, BOTTOM_RIGHT);
		}
	}

	return bResult;
}

void CListViewWalkerPropertySheet::OnPrev()
{
	ASSERT(m_pListCtrl != NULL);
	if (m_pListCtrl == NULL)
		return;

	CObject *pObj = m_pListCtrl->GetPrevSelectableItem();
	if (pObj) {
		m_aItems.RemoveAll();
		m_aItems.Add(pObj);
		ChangedData();
	} else
		MessageBeep(MB_OK);
}

void CListViewWalkerPropertySheet::OnNext()
{
	ASSERT(m_pListCtrl != NULL);
	if (m_pListCtrl == NULL)
		return;

	CObject *pObj = m_pListCtrl->GetNextSelectableItem();
	if (pObj) {
		m_aItems.RemoveAll();
		m_aItems.Add(pObj);
		ChangedData();
	} else
		MessageBeep(MB_OK);
}

CObject* CListCtrlItemWalk::GetPrevSelectableItem()
{
	ASSERT(m_pListCtrl != NULL);
	if (m_pListCtrl == NULL)
		return NULL;

	int iItemCount = m_pListCtrl->GetItemCount();
	if (iItemCount >= 2) {
		POSITION pos = m_pListCtrl->GetFirstSelectedItemPosition();
		if (pos) {
			int iItem = m_pListCtrl->GetNextSelectedItem(pos);
			if (iItem - 1 >= 0) {
				m_pListCtrl->SetItemState(iItem--, 0, LVIS_SELECTED | LVIS_FOCUSED);
				m_pListCtrl->SetItemState(iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				m_pListCtrl->SetSelectionMark(iItem);
				m_pListCtrl->EnsureVisible(iItem, FALSE);

				return reinterpret_cast<CObject*>(m_pListCtrl->GetItemData(iItem));
			}
		}
	}
	return NULL;
}

CObject* CListCtrlItemWalk::GetNextSelectableItem()
{
	ASSERT(m_pListCtrl != NULL);
	if (m_pListCtrl == NULL)
		return NULL;

	int iItemCount = m_pListCtrl->GetItemCount();
	if (iItemCount >= 2) {
		POSITION pos = m_pListCtrl->GetFirstSelectedItemPosition();
		if (pos) {
			int iItem = m_pListCtrl->GetNextSelectedItem(pos);
			if (iItem + 1 < iItemCount) {
				m_pListCtrl->SetItemState(iItem++, 0, LVIS_SELECTED | LVIS_FOCUSED);
				m_pListCtrl->SetItemState(iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				m_pListCtrl->SetSelectionMark(iItem);
				m_pListCtrl->EnsureVisible(iItem, FALSE);

				return reinterpret_cast<CObject*>(m_pListCtrl->GetItemData(iItem));
			}
		}
	}
	return NULL;
}