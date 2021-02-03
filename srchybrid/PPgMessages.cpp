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
#include <share.h>
#include "emule.h"
#include "PPgMessages.h"
#include "OtherFunctions.h"
#include "Preferences.h"
#include "emuledlg.h"
#include "HelpIDs.h"
#include "Log.h"
#include "ChatWnd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



IMPLEMENT_DYNAMIC(CPPgMessages, CPropertyPage)

BEGIN_MESSAGE_MAP(CPPgMessages, CPropertyPage)
	ON_EN_CHANGE(IDC_FILTER, OnSettingsChange)
	ON_EN_CHANGE(IDC_COMMENTFILTER, OnSettingsChange)
	ON_BN_CLICKED(IDC_MSGONLYFRIENDS, OnSettingsChange)
	ON_BN_CLICKED(IDC_ADVSPAMFILTER, OnSpamFilterChange)
	ON_BN_CLICKED(IDC_INDICATERATINGS, OnSettingsChange)
	ON_BN_CLICKED(IDC_MSHOWSMILEYS, OnSettingsChange)
	ON_BN_CLICKED(IDC_USECAPTCHAS, OnSettingsChange)
	ON_WM_HELPINFO()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

CPPgMessages::CPPgMessages()
	: CPropertyPage(CPPgMessages::IDD)
{
}

void CPPgMessages::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
}

void CPPgMessages::LoadSettings()
{
	CheckDlgButton(IDC_MSGONLYFRIENDS, static_cast<UINT>(thePrefs.msgonlyfriends));
	CheckDlgButton(IDC_ADVSPAMFILTER, static_cast<UINT>(thePrefs.m_bAdvancedSpamfilter));
	CheckDlgButton(IDC_INDICATERATINGS, static_cast<UINT>(thePrefs.indicateratings));
	CheckDlgButton(IDC_MSHOWSMILEYS, static_cast<UINT>(thePrefs.GetMessageEnableSmileys()));
	CheckDlgButton(IDC_USECAPTCHAS, static_cast<UINT>(thePrefs.IsChatCaptchaEnabled()));

	SetDlgItemText(IDC_FILTER, thePrefs.messageFilter);
	SetDlgItemText(IDC_COMMENTFILTER, thePrefs.commentFilter);
	OnSpamFilterChange();
}

BOOL CPPgMessages::OnInitDialog()
{
	CPropertyPage::OnInitDialog();
	InitWindowStyles(this);

	LoadSettings();
	Localize();

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPgMessages::OnApply()
{

	thePrefs.msgonlyfriends = IsDlgButtonChecked(IDC_MSGONLYFRIENDS) != 0;
	thePrefs.m_bAdvancedSpamfilter = IsDlgButtonChecked(IDC_ADVSPAMFILTER) != 0;
	thePrefs.indicateratings = IsDlgButtonChecked(IDC_INDICATERATINGS) != 0;
	thePrefs.m_bUseChatCaptchas = IsDlgButtonChecked(IDC_USECAPTCHAS) != 0;

	bool bOldSmileys = thePrefs.GetMessageEnableSmileys();
	thePrefs.m_bMessageEnableSmileys = IsDlgButtonChecked(IDC_MSHOWSMILEYS) != 0;
	if (bOldSmileys != thePrefs.GetMessageEnableSmileys())
		theApp.emuledlg->chatwnd->EnableSmileys(thePrefs.GetMessageEnableSmileys());

	GetDlgItemText(IDC_FILTER, thePrefs.messageFilter);

	CString strCommentFilters;
	GetDlgItemText(IDC_COMMENTFILTER, strCommentFilters);
	strCommentFilters.MakeLower();
	CString strNewCommentFilters;
	for (int iPos = 0; iPos >= 0;) {
		CString strFilter(strCommentFilters.Tokenize(_T("|"), iPos));
		if (!strFilter.Trim().IsEmpty()) {
			if (!strNewCommentFilters.IsEmpty())
				strNewCommentFilters += _T('|');
			strNewCommentFilters += strFilter;
		}
	}
	thePrefs.commentFilter = strNewCommentFilters;
	if (thePrefs.commentFilter != strCommentFilters)
		SetDlgItemText(IDC_COMMENTFILTER, thePrefs.commentFilter);

	LoadSettings();
	SetModified(FALSE);
	return CPropertyPage::OnApply();
}

void CPPgMessages::Localize()
{
	if (m_hWnd) {
		SetWindowText(GetResString(IDS_MESSAGESCOMMENTS));

		SetDlgItemText(IDC_FILTERCOMMENTSLABEL, GetResString(IDS_FILTERCOMMENTSLABEL));
		SetDlgItemText(IDC_STATIC_COMMENTS, GetResString(IDS_COMMENT));
		SetDlgItemText(IDC_INDICATERATINGS, GetResString(IDS_INDICATERATINGS));

		SetDlgItemText(IDC_FILTERLABEL, GetResString(IDS_FILTERLABEL));
		SetDlgItemText(IDC_MSG, GetResString(IDS_CW_MESSAGES));

		SetDlgItemText(IDC_MSGONLYFRIENDS, GetResString(IDS_MSGONLYFRIENDS));
		SetDlgItemText(IDC_USECAPTCHAS, GetResString(IDS_USECAPTCHAS));

		SetDlgItemText(IDC_ADVSPAMFILTER, GetResString(IDS_ADVSPAMFILTER));

		SetDlgItemText(IDC_MSHOWSMILEYS, GetResString(IDS_SHOWSMILEYS));
	}
}


void CPPgMessages::OnDestroy()
{
	CPropertyPage::OnDestroy();
}

BOOL CPPgMessages::PreTranslateMessage(MSG *pMsg)
{
	return CPropertyPage::PreTranslateMessage(pMsg);
}


void CPPgMessages::OnHelp()
{
	theApp.ShowHelp(eMule_FAQ_Preferences_Messages);
}

BOOL CPPgMessages::OnCommand(WPARAM wParam, LPARAM lParam)
{
	return (wParam == ID_HELP) ? OnHelpInfo(NULL) : __super::OnCommand(wParam, lParam);
}

BOOL CPPgMessages::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}

void CPPgMessages::OnSpamFilterChange()
{
	if (IsDlgButtonChecked(IDC_ADVSPAMFILTER))
		GetDlgItem(IDC_USECAPTCHAS)->EnableWindow(TRUE);
	else {
		GetDlgItem(IDC_USECAPTCHAS)->EnableWindow(FALSE);
		CheckDlgButton(IDC_USECAPTCHAS, 0);
	}

	OnSettingsChange();
}