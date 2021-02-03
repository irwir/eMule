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
#include "ED2kLinkDlg.h"
#include "KnownFile.h"
#include "partfile.h"
#include "preferences.h"
#include "shahashset.h"
#include "UserMsgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CED2kLinkDlg, CResizablePage)

BEGIN_MESSAGE_MAP(CED2kLinkDlg, CResizablePage)
	ON_BN_CLICKED(IDC_LD_CLIPBOARDBUT, OnBnClickedClipboard)
	ON_BN_CLICKED(IDC_LD_SOURCECHE, OnSettingsChange)
	ON_BN_CLICKED(IDC_LD_HTMLCHE, OnSettingsChange)
	ON_BN_CLICKED(IDC_LD_HOSTNAMECHE, OnSettingsChange)
	ON_BN_CLICKED(IDC_LD_HASHSETCHE, OnSettingsChange)
	ON_MESSAGE(UM_DATA_CHANGED, OnDataChanged)
END_MESSAGE_MAP()

CED2kLinkDlg::CED2kLinkDlg()
	: CResizablePage(CED2kLinkDlg::IDD)
	, m_strLinks(_T(" "))
	, m_paFiles()
	, m_bDataChanged()
	, m_bReducedDlg()
{
	m_strCaption = GetResString(IDS_SW_LINK);
	m_psp.pszTitle = m_strCaption;
	m_psp.dwFlags |= PSP_USETITLE;
}

void CED2kLinkDlg::DoDataExchange(CDataExchange *pDX)
{
	CResizablePage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LD_LINKEDI, m_ctrlLinkEdit);
}

BOOL CED2kLinkDlg::OnInitDialog()
{
	CResizablePage::OnInitDialog();
	InitWindowStyles(this);

	if (!m_bReducedDlg) {
		AddAnchor(IDC_LD_BASICGROUP, BOTTOM_LEFT, BOTTOM_RIGHT);
		AddAnchor(IDC_LD_SOURCECHE, BOTTOM_LEFT, BOTTOM_LEFT);
		AddAnchor(IDC_LD_ADVANCEDGROUP, BOTTOM_LEFT, BOTTOM_RIGHT);
		AddAnchor(IDC_LD_HTMLCHE, BOTTOM_LEFT, BOTTOM_LEFT);
		AddAnchor(IDC_LD_HASHSETCHE, BOTTOM_LEFT, BOTTOM_LEFT);
		AddAnchor(IDC_LD_HOSTNAMECHE, BOTTOM_LEFT, BOTTOM_LEFT);

		// enabled/disable checkbox depending on situation
		if (theApp.IsConnected() && !theApp.IsFirewalled())
			GetDlgItem(IDC_LD_SOURCECHE)->EnableWindow(TRUE);
		else {
			GetDlgItem(IDC_LD_SOURCECHE)->EnableWindow(FALSE);
			CheckDlgButton(IDC_LD_SOURCECHE, BST_UNCHECKED);
		}
		if (theApp.IsConnected() && !theApp.IsFirewalled() && thePrefs.GetYourHostname().Find(_T('.')) >= 0)
			GetDlgItem(IDC_LD_HOSTNAMECHE)->EnableWindow(TRUE);
		else {
			GetDlgItem(IDC_LD_HOSTNAMECHE)->EnableWindow(FALSE);
			CheckDlgButton(IDC_LD_HOSTNAMECHE, BST_UNCHECKED);
		}
	} else {
		CRect rcDefault;
		GetDlgItem(IDC_LD_LINKGROUP)->GetWindowRect(rcDefault);
		RECT rcNew;
		GetDlgItem(IDC_LD_ADVANCEDGROUP)->GetWindowRect(&rcNew);
		int nDeltaY = rcNew.bottom - rcDefault.bottom;
		GetDlgItem(IDC_LD_LINKGROUP)->SetWindowPos(NULL, 0, 0, rcDefault.Width(), rcDefault.Height() + nDeltaY, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);
		GetDlgItem(IDC_LD_LINKEDI)->GetWindowRect(rcDefault);
		GetDlgItem(IDC_LD_LINKEDI)->SetWindowPos(NULL, 0, 0, rcDefault.Width(), rcDefault.Height() + nDeltaY, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);
		GetDlgItem(IDC_LD_CLIPBOARDBUT)->GetWindowRect(rcDefault);
		ScreenToClient(rcDefault);
		GetDlgItem(IDC_LD_CLIPBOARDBUT)->SetWindowPos(NULL, rcDefault.left, rcDefault.top + nDeltaY, 0, 0, SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);

		GetDlgItem(IDC_LD_BASICGROUP)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_LD_SOURCECHE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_LD_ADVANCEDGROUP)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_LD_HTMLCHE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_LD_HASHSETCHE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_LD_HOSTNAMECHE)->ShowWindow(SW_HIDE);
	}
	AddAnchor(IDC_LD_LINKGROUP, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_LD_LINKEDI, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_LD_CLIPBOARDBUT, BOTTOM_RIGHT);
	Localize();

	return TRUE;
}

BOOL CED2kLinkDlg::OnSetActive()
{
	if (!CResizablePage::OnSetActive())
		return FALSE;

	if (m_bDataChanged) {
		//hashset link - check if at least one file has a hashset
		bool bShowHashset = false;
		bool bShowAICH = false;
		bool bShowHTML = false;
		for (int i = m_paFiles->GetSize(); --i >= 0;) {
			if (!(*m_paFiles)[i]->IsKindOf(RUNTIME_CLASS(CKnownFile)))
				continue;
			bShowHTML = true;
			const CFileIdentifier &fileid = static_cast<CKnownFile*>((*m_paFiles)[i])->GetFileIdentifier();
			if (fileid.GetAvailableMD4PartHashCount() > 0 && fileid.HasExpectedMD4HashCount())
				bShowHashset = true;
			if (fileid.HasAICHHash())
				bShowAICH = true;
			if (bShowHashset && bShowAICH)
				break;
		}
		GetDlgItem(IDC_LD_HASHSETCHE)->EnableWindow(bShowHashset);
		if (!bShowHashset)
			CheckDlgButton(IDC_LD_HASHSETCHE, BST_UNCHECKED);

		GetDlgItem(IDC_LD_HTMLCHE)->EnableWindow(bShowHTML);

		UpdateLink();
		m_bDataChanged = false;
	}

	return TRUE;
}

LRESULT CED2kLinkDlg::OnDataChanged(WPARAM, LPARAM)
{
	m_bDataChanged = true;
	return 1;
}

void CED2kLinkDlg::Localize()
{
	if (!m_hWnd)
		return;
	SetTabTitle(IDS_SW_LINK, this);

	SetDlgItemText(IDC_LD_LINKGROUP, m_strCaption);
	SetDlgItemText(IDC_LD_CLIPBOARDBUT, GetResString(IDS_LD_COPYCLIPBOARD));
	if (!m_bReducedDlg) {
		SetDlgItemText(IDC_LD_BASICGROUP, GetResString(IDS_LD_BASICOPT));
		SetDlgItemText(IDC_LD_SOURCECHE, GetResString(IDS_LD_ADDSOURCE));
		SetDlgItemText(IDC_LD_ADVANCEDGROUP, GetResString(IDS_LD_ADVANCEDOPT));
		SetDlgItemText(IDC_LD_HTMLCHE, GetResString(IDS_LD_ADDHTML));
		SetDlgItemText(IDC_LD_HASHSETCHE, GetResString(IDS_LD_ADDHASHSET));
		SetDlgItemText(IDC_LD_HOSTNAMECHE, GetResString(IDS_LD_HOSTNAME));
	}
}

void CED2kLinkDlg::UpdateLink()
{
	const bool bHashset = IsDlgButtonChecked(IDC_LD_HASHSETCHE) != 0;
	const bool bHTML = IsDlgButtonChecked(IDC_LD_HTMLCHE) != 0;
	const bool bHostname = IsDlgButtonChecked(IDC_LD_HOSTNAMECHE) != 0 && theApp.IsConnected() && !theApp.IsFirewalled() && thePrefs.GetYourHostname().Find(_T('.')) >= 0;
	const bool bSource = IsDlgButtonChecked(IDC_LD_SOURCECHE) != 0 && theApp.IsConnected() && theApp.GetPublicIP() != 0 && !theApp.IsFirewalled();

	CString strLinks;
	for (int i = 0; i != m_paFiles->GetSize(); ++i)
		if ((*m_paFiles)[i] && (*m_paFiles)[i]->IsKindOf(RUNTIME_CLASS(CKnownFile))) {
			if (!strLinks.IsEmpty())
				strLinks += _T("\r\n\r\n");
			const CKnownFile *file = static_cast<CKnownFile*>((*m_paFiles)[i]);
			strLinks += file->GetED2kLink(bHashset, bHTML, bHostname, bSource, theApp.GetPublicIP());
		}

	//skip update if nothing has changed
	if (m_strLinks != strLinks) {
		m_ctrlLinkEdit.SetWindowText(strLinks);
		m_strLinks = strLinks;
	}
}

void CED2kLinkDlg::OnBnClickedClipboard()
{
	CString strBuffer;
	m_ctrlLinkEdit.GetWindowText(strBuffer);
	theApp.CopyTextToClipboard(strBuffer);
}

void CED2kLinkDlg::OnSettingsChange()
{
	UpdateLink();
}

BOOL CED2kLinkDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if (LOWORD(wParam) == IDCANCEL)
		return ::SendMessage(::GetParent(m_hWnd), WM_COMMAND, wParam, lParam) != 0;
	return CResizablePage::OnCommand(wParam, lParam);
}