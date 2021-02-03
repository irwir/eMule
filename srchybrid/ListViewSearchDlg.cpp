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
#include "ListViewSearchDlg.h"
#include "OtherFunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CListViewSearchDlg, CDialog)

BEGIN_MESSAGE_MAP(CListViewSearchDlg, CDialog)
	ON_EN_CHANGE(IDC_LISTVIEW_SEARCH_TEXT, OnEnChangeSearchText)
END_MESSAGE_MAP()

CListViewSearchDlg::CListViewSearchDlg(CWnd *pParent /*=NULL*/)
	: CDialog(CListViewSearchDlg::IDD, pParent)
	, m_pListView()
	, m_bCanSearchInAllColumns(true)
	, m_iSearchColumn()
	, m_icoWnd()
{
}

CListViewSearchDlg::~CListViewSearchDlg()
{
	if (m_icoWnd)
		VERIFY(::DestroyIcon(m_icoWnd));
}

void CListViewSearchDlg::DoDataExchange(CDataExchange *pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LISTVIEW_SEARCH_COLUMN, m_ctlSearchCol);
	DDX_CBIndex(pDX, IDC_LISTVIEW_SEARCH_COLUMN, m_iSearchColumn);
	DDX_Text(pDX, IDC_LISTVIEW_SEARCH_TEXT, m_strFindText);
}

void CListViewSearchDlg::OnEnChangeSearchText()
{
	UpdateControls();
}

void CListViewSearchDlg::UpdateControls()
{
	GetDlgItem(IDOK)->EnableWindow(GetDlgItem(IDC_LISTVIEW_SEARCH_TEXT)->GetWindowTextLength() > 0);
}

BOOL CListViewSearchDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetIcon(m_icoWnd = theApp.LoadIcon(_T("Search")), FALSE);
	InitWindowStyles(this);

	SetWindowText(GetResString(IDS_SW_SEARCHBOX));
	SetDlgItemText(IDC_LISTVIEW_SEARCH_TEXT_LBL, GetResString(IDS_SEARCH_TEXT) + _T(':'));
	SetDlgItemText(IDC_LISTVIEW_SEARCH_COLUMN_LBL, GetResString(IDS_SEARCH_COLUMN) + _T(':'));
	SetDlgItemText(IDCANCEL, GetResString(IDS_CANCEL));
	SetDlgItemText(IDOK, GetResString(IDS_TREEOPTIONS_OK));


	if (!m_bCanSearchInAllColumns)
		m_iSearchColumn = 0;

	if (m_pListView != NULL) {
		TCHAR szColTitle[256];
		LVCOLUMN lvc;
		lvc.mask = LVCF_TEXT;
		lvc.cchTextMax = _countof(szColTitle);
		lvc.pszText = szColTitle;
		int iCol = 0;
		while (m_pListView->GetColumn(iCol++, &lvc)) {
			szColTitle[_countof(szColTitle) - 1] = _T('\0');
			m_ctlSearchCol.AddString(lvc.pszText);
			if (!m_bCanSearchInAllColumns)
				break;
		}
		if ((UINT)m_iSearchColumn >= (UINT)m_ctlSearchCol.GetCount())
			m_iSearchColumn = 0;
	} else {
		m_ctlSearchCol.EnableWindow(FALSE);
		m_ctlSearchCol.ShowWindow(SW_HIDE);

		m_iSearchColumn = 0;
	}
	m_ctlSearchCol.SetCurSel(m_iSearchColumn);

	UpdateControls();
	return TRUE;
}