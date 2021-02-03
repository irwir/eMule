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
#include "DirectDownloadDlg.h"
#include "OtherFunctions.h"
#include "emuleDlg.h"
#include "DownloadQueue.h"
#include "ED2KLink.h"
#include "Preferences.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define	PREF_INI_SECTION	_T("DirectDownloadDlg")

IMPLEMENT_DYNAMIC(CDirectDownloadDlg, CDialog)

BEGIN_MESSAGE_MAP(CDirectDownloadDlg, CResizableDialog)
	ON_EN_KILLFOCUS(IDC_ELINK, OnEnKillfocusElink)
	ON_EN_UPDATE(IDC_ELINK, OnEnUpdateElink)
END_MESSAGE_MAP()

CDirectDownloadDlg::CDirectDownloadDlg(CWnd *pParent /*=NULL*/)
	: CResizableDialog(CDirectDownloadDlg::IDD, pParent)
	, m_icoWnd()
{
}

CDirectDownloadDlg::~CDirectDownloadDlg()
{
	if (m_icoWnd)
		VERIFY(::DestroyIcon(m_icoWnd));
}

void CDirectDownloadDlg::DoDataExchange(CDataExchange *pDX)
{
	CResizableDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DDOWN_FRM, m_ctrlDirectDlFrm);
	DDX_Control(pDX, IDC_CATS, m_cattabs);
}

void CDirectDownloadDlg::UpdateControls()
{
	GetDlgItem(IDOK)->EnableWindow(GetDlgItem(IDC_ELINK)->GetWindowTextLength() > 0);
}

void CDirectDownloadDlg::OnEnUpdateElink()
{
	UpdateControls();
}

void CDirectDownloadDlg::OnEnKillfocusElink()
{
	CString strLinks;
	GetDlgItemText(IDC_ELINK, strLinks);
	if (!strLinks.IsEmpty()) {
		strLinks.Replace(_T("\n"), _T("\r\n"));
		strLinks.Replace(_T("\r\r"), _T("\r"));
		SetDlgItemText(IDC_ELINK, strLinks);
	}
}

void CDirectDownloadDlg::OnOK()
{
	CString strLinks;
	GetDlgItemText(IDC_ELINK, strLinks);

	for (int iPos = 0; iPos >= 0;) {
		const CString &sToken(strLinks.Tokenize(_T(" \t\r\n"), iPos)); //tokenize by whitespace
		if (sToken.IsEmpty())
			break;
		bool bSlash = (sToken.Right(1) == _T("/"));
		CED2KLink *pLink = NULL;
		try {
			pLink = CED2KLink::CreateLinkFromUrl(bSlash ? sToken : sToken + _T('/'));
			if (pLink) {
				if (pLink->GetKind() != CED2KLink::kFile)
					throw CString(_T("bad link"));
				theApp.downloadqueue->AddFileLinkToDownload(pLink->GetFileLink(), thePrefs.GetCatCount() ? m_cattabs.GetCurSel() : 0);
				delete pLink;
				pLink = NULL;
			}
		} catch (const CString &error) {
			delete pLink;
			CString sBuffer;
			sBuffer.Format(GetResString(IDS_ERR_INVALIDLINK), (LPCTSTR)error);
			CString strError;
			strError.Format(GetResString(IDS_ERR_LINKERROR), (LPCTSTR)sBuffer);
			AfxMessageBox(strError);
			return;
		}
	}

	CResizableDialog::OnOK();
}

BOOL CDirectDownloadDlg::OnInitDialog()
{
	CResizableDialog::OnInitDialog();
	InitWindowStyles(this);
	SetIcon(m_icoWnd = theApp.LoadIcon(_T("PasteLink")), FALSE);

	if (theApp.IsVistaThemeActive())
		m_cattabs.ModifyStyle(0, TCS_HOTTRACK);

	AddAnchor(IDC_DDOWN_FRM, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_ELINK, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDCANCEL, BOTTOM_RIGHT);
	AddAnchor(IDOK, BOTTOM_RIGHT);
	AddAnchor(IDC_CATLABEL, BOTTOM_LEFT);
	AddAnchor(IDC_CATS, BOTTOM_LEFT, BOTTOM_RIGHT);

	EnableSaveRestore(PREF_INI_SECTION);

	SetWindowText(GetResString(IDS_SW_DIRECTDOWNLOAD));
	m_ctrlDirectDlFrm.SetIcon(_T("Download"));
	m_ctrlDirectDlFrm.SetWindowText(GetResString(IDS_SW_DIRECTDOWNLOAD));
	SetDlgItemText(IDOK, GetResString(IDS_DOWNLOAD));
	SetDlgItemText(IDC_FSTATIC2, GetResString(IDS_SW_LINK));
	SetDlgItemText(IDC_CATLABEL, GetResString(IDS_CAT) + _T(':'));

	SetDlgItemText(IDOK, GetResString(IDS_DOWNLOAD));
	SetDlgItemText(IDCANCEL, GetResString(IDS_CANCEL));

	if (thePrefs.GetCatCount() == 0) {
		GetDlgItem(IDC_CATLABEL)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_CATS)->ShowWindow(SW_HIDE);
	} else {
		UpdateCatTabs();
		if (theApp.m_fontSymbol.m_hObject) {
			GetDlgItem(IDC_CATLABEL)->SetFont(&theApp.m_fontSymbol);
			SetDlgItemText(IDC_CATLABEL, (GetExStyle() & WS_EX_LAYOUTRTL) ? _T("3") : _T("4")); // show a right-arrow
		}

	}

	UpdateControls();

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CDirectDownloadDlg::UpdateCatTabs()
{
	int oldsel = m_cattabs.GetCurSel();
	m_cattabs.DeleteAllItems();
	for (int ix = 0; ix < thePrefs.GetCatCount(); ++ix) {
		CString label = (ix == 0) ? GetResString(IDS_ALL) : thePrefs.GetCategory(ix)->strTitle;
		label.Replace(_T("&"), _T("&&"));
		m_cattabs.InsertItem(ix, label);
	}
	if (oldsel >= m_cattabs.GetItemCount() || oldsel == -1)
		oldsel = 0;

	m_cattabs.SetCurSel(oldsel);
}