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
#include "PPgFiles.h"
#include "Inputbox.h"
#include "OtherFunctions.h"
#include "TransferDlg.h"
#include "emuledlg.h"
#include "Preferences.h"
#include "HelpIDs.h"
#include "ppgfiles.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CPPgFiles, CPropertyPage)

BEGIN_MESSAGE_MAP(CPPgFiles, CPropertyPage)
	ON_BN_CLICKED(IDC_PF_TIMECALC, OnSettingsChange)
	ON_BN_CLICKED(IDC_UAP, OnSettingsChange)
	ON_BN_CLICKED(IDC_DAP, OnSettingsChange)
	ON_BN_CLICKED(IDC_PREVIEWPRIO, OnSettingsChange)
	ON_BN_CLICKED(IDC_ADDNEWFILESPAUSED, OnSettingsChange)
	ON_BN_CLICKED(IDC_FULLCHUNKTRANS, OnSettingsChange)
	ON_BN_CLICKED(IDC_STARTNEXTFILE, OnSettingsChange)
	ON_BN_CLICKED(IDC_WATCHCB, OnSettingsChange)
	ON_BN_CLICKED(IDC_STARTNEXTFILECAT, OnSettingsChangeCat1)
	ON_BN_CLICKED(IDC_STARTNEXTFILECAT2, OnSettingsChangeCat2)
	ON_BN_CLICKED(IDC_FNCLEANUP, OnSettingsChange)
	ON_BN_CLICKED(IDC_FNC, OnSetCleanupFilter)
	ON_EN_CHANGE(IDC_VIDEOPLAYER, OnSettingsChange)
	ON_EN_CHANGE(IDC_VIDEOPLAYER_ARGS, OnSettingsChange)
	ON_BN_CLICKED(IDC_VIDEOBACKUP, OnSettingsChange)
	ON_BN_CLICKED(IDC_REMEMBERDOWNLOADED, OnSettingsChange)
	ON_BN_CLICKED(IDC_REMEMBERCANCELLED, OnSettingsChange)
	ON_BN_CLICKED(IDC_BROWSEV, BrowseVideoplayer)
	ON_WM_HELPINFO()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

CPPgFiles::CPPgFiles()
	: CPropertyPage(CPPgFiles::IDD)
	, m_icoBrowse()
{
}

void CPPgFiles::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
}

BOOL CPPgFiles::OnInitDialog()
{
	CPropertyPage::OnInitDialog();
	InitWindowStyles(this);

	AddBuddyButton(GetDlgItem(IDC_VIDEOPLAYER)->m_hWnd, ::GetDlgItem(m_hWnd, IDC_BROWSEV));
	InitAttachedBrowseButton(::GetDlgItem(m_hWnd, IDC_BROWSEV), m_icoBrowse);

	LoadSettings();
	Localize();

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CPPgFiles::LoadSettings()
{
	CheckDlgButton(IDC_ADDNEWFILESPAUSED, static_cast<UINT>(thePrefs.addnewfilespaused));
	CheckDlgButton(IDC_PF_TIMECALC, static_cast<UINT>(!thePrefs.m_bUseOldTimeRemaining));
	CheckDlgButton(IDC_PREVIEWPRIO, static_cast<UINT>(thePrefs.m_bpreviewprio));
	CheckDlgButton(IDC_DAP, static_cast<UINT>(thePrefs.m_bDAP));
	CheckDlgButton(IDC_UAP, static_cast<UINT>(thePrefs.m_bUAP));
	CheckDlgButton(IDC_FULLCHUNKTRANS, static_cast<UINT>(thePrefs.m_btransferfullchunks));

	CheckDlgButton(IDC_STARTNEXTFILECAT, FALSE);
	CheckDlgButton(IDC_STARTNEXTFILECAT2, FALSE);
	if (thePrefs.m_istartnextfile) {
		CheckDlgButton(IDC_STARTNEXTFILE, 1);
		if (thePrefs.m_istartnextfile == 2)
			CheckDlgButton(IDC_STARTNEXTFILECAT, TRUE);
		else if (thePrefs.m_istartnextfile == 3)
			CheckDlgButton(IDC_STARTNEXTFILECAT2, TRUE);
	} else
		CheckDlgButton(IDC_STARTNEXTFILE, 0);

	SetDlgItemText(IDC_VIDEOPLAYER, thePrefs.m_strVideoPlayer);
	SetDlgItemText(IDC_VIDEOPLAYER_ARGS, thePrefs.m_strVideoPlayerArgs);

	CheckDlgButton(IDC_VIDEOBACKUP, static_cast<UINT>(thePrefs.moviePreviewBackup));
	CheckDlgButton(IDC_FNCLEANUP, static_cast<UINT>(thePrefs.AutoFilenameCleanup()));
	CheckDlgButton(IDC_WATCHCB, static_cast<UINT>(thePrefs.watchclipboard));
	CheckDlgButton(IDC_REMEMBERDOWNLOADED, static_cast<UINT>(thePrefs.IsRememberingDownloadedFiles()));
	CheckDlgButton(IDC_REMEMBERCANCELLED, static_cast<UINT>(thePrefs.IsRememberingCancelledFiles()));

	GetDlgItem(IDC_STARTNEXTFILECAT)->EnableWindow(IsDlgButtonChecked(IDC_STARTNEXTFILE));
}

BOOL CPPgFiles::OnApply()
{
	bool bOldPreviewPrio = thePrefs.m_bpreviewprio;
	thePrefs.m_bpreviewprio = IsDlgButtonChecked(IDC_PREVIEWPRIO) != 0;
	if (bOldPreviewPrio != thePrefs.m_bpreviewprio)
		theApp.emuledlg->transferwnd->GetDownloadList()->CreateMenus();

	thePrefs.m_bDAP = IsDlgButtonChecked(IDC_DAP) != 0;

	thePrefs.m_bUAP = IsDlgButtonChecked(IDC_UAP) != 0;

	if (IsDlgButtonChecked(IDC_STARTNEXTFILE)) {
		thePrefs.m_istartnextfile = 1;
		if (IsDlgButtonChecked(IDC_STARTNEXTFILECAT))
			thePrefs.m_istartnextfile = 2;
		else if (IsDlgButtonChecked(IDC_STARTNEXTFILECAT2))
			thePrefs.m_istartnextfile = 3;
	} else
		thePrefs.m_istartnextfile = 0;

	thePrefs.m_btransferfullchunks = IsDlgButtonChecked(IDC_FULLCHUNKTRANS) != 0;

	thePrefs.watchclipboard = IsDlgButtonChecked(IDC_WATCHCB) != 0;

	thePrefs.SetRememberDownloadedFiles(IsDlgButtonChecked(IDC_REMEMBERDOWNLOADED) != 0);

	thePrefs.SetRememberCancelledFiles(IsDlgButtonChecked(IDC_REMEMBERCANCELLED) != 0);

	thePrefs.addnewfilespaused = IsDlgButtonChecked(IDC_ADDNEWFILESPAUSED) != 0;
	thePrefs.autofilenamecleanup = IsDlgButtonChecked(IDC_FNCLEANUP) != 0;
	thePrefs.m_bUseOldTimeRemaining = !IsDlgButtonChecked(IDC_PF_TIMECALC);

	GetDlgItemText(IDC_VIDEOPLAYER, thePrefs.m_strVideoPlayer);
	thePrefs.m_strVideoPlayer.Trim();
	GetDlgItemText(IDC_VIDEOPLAYER_ARGS, thePrefs.m_strVideoPlayerArgs);
	thePrefs.m_strVideoPlayerArgs.Trim();
	thePrefs.moviePreviewBackup = IsDlgButtonChecked(IDC_VIDEOBACKUP) != 0;

	LoadSettings();
	SetModified(FALSE);
	return CPropertyPage::OnApply();
}

void CPPgFiles::Localize()
{
	if (m_hWnd) {
		SetWindowText(GetResString(IDS_PW_FILES));
		SetDlgItemText(IDC_LBL_MISC, GetResString(IDS_PW_MISC));
		SetDlgItemText(IDC_PF_TIMECALC, GetResString(IDS_PF_ADVANCEDCALC));
		SetDlgItemText(IDC_UAP, GetResString(IDS_PW_UAP));
		SetDlgItemText(IDC_DAP, GetResString(IDS_PW_DAP));
		SetDlgItemText(IDC_PREVIEWPRIO, GetResString(IDS_DOWNLOADMOVIECHUNKS));
		SetDlgItemText(IDC_ADDNEWFILESPAUSED, GetResString(IDS_ADDNEWFILESPAUSED));
		SetDlgItemText(IDC_WATCHCB, GetResString(IDS_PF_WATCHCB));
		SetDlgItemText(IDC_FULLCHUNKTRANS, GetResString(IDS_FULLCHUNKTRANS));
		SetDlgItemText(IDC_STARTNEXTFILE, GetResString(IDS_STARTNEXTFILE));
		SetDlgItemText(IDC_STARTNEXTFILECAT, GetResString(IDS_PREF_STARTNEXTFILECAT));
		SetDlgItemText(IDC_STARTNEXTFILECAT2, GetResString(IDS_PREF_STARTNEXTFILECATONLY));
		SetDlgItemText(IDC_FNC, GetResString(IDS_EDIT));
		SetDlgItemText(IDC_ONND, GetResString(IDS_ONNEWDOWNLOAD));
		SetDlgItemText(IDC_FNCLEANUP, GetResString(IDS_AUTOCLEANUPFN));
		SetDlgItemText(IDC_STATICVIDEOPLAYER, GetResString(IDS_PW_VIDEOPLAYER));
		SetDlgItemText(IDC_VIDEOPLAYER_CMD_LBL, GetResString(IDS_COMMAND));
		SetDlgItemText(IDC_VIDEOPLAYER_ARGS_LBL, GetResString(IDS_ARGUMENTS));
		SetDlgItemText(IDC_VIDEOBACKUP, GetResString(IDS_VIDEOBACKUP));
		SetDlgItemText(IDC_REMEMBERDOWNLOADED, GetResString(IDS_PW_REMEMBERDOWNLOADED));
		SetDlgItemText(IDC_REMEMBERCANCELLED, GetResString(IDS_PW_REMEMBERCANCELLED));
	}
}

void CPPgFiles::OnSetCleanupFilter()
{
	InputBox inputbox;
	inputbox.SetLabels(GetResString(IDS_FNFILTERTITLE), GetResString(IDS_FILTERFILENAMEWORD), thePrefs.GetFilenameCleanups());
	inputbox.DoModal();
	if (!inputbox.WasCancelled())
		thePrefs.SetFilenameCleanups(inputbox.GetInput());
}

void CPPgFiles::BrowseVideoplayer()
{
	CString strPlayerPath;
	GetDlgItemText(IDC_VIDEOPLAYER, strPlayerPath);
	CFileDialog dlgFile(TRUE, _T("exe"), strPlayerPath, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY, _T("Executable (*.exe)|*.exe||"), NULL, 0);
	if (dlgFile.DoModal() == IDOK) {
		SetDlgItemText(IDC_VIDEOPLAYER, dlgFile.GetPathName());
		SetModified();
	}
}

void CPPgFiles::OnHelp()
{
	theApp.ShowHelp(eMule_FAQ_Preferences_Files);
}

BOOL CPPgFiles::OnCommand(WPARAM wParam, LPARAM lParam)
{
	return (wParam == ID_HELP) ? OnHelpInfo(NULL) : __super::OnCommand(wParam, lParam);
}

BOOL CPPgFiles::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}

void CPPgFiles::OnSettingsChange()
{
	SetModified();
	GetDlgItem(IDC_STARTNEXTFILECAT)->EnableWindow(IsDlgButtonChecked(IDC_STARTNEXTFILE));
	GetDlgItem(IDC_STARTNEXTFILECAT2)->EnableWindow(IsDlgButtonChecked(IDC_STARTNEXTFILE));
}

void CPPgFiles::OnSettingsChangeCat(uint8 index)
{
	bool on = IsDlgButtonChecked(index == 1 ? IDC_STARTNEXTFILECAT : IDC_STARTNEXTFILECAT2) != 0;
	if (on)
		CheckDlgButton(index == 1 ? IDC_STARTNEXTFILECAT2 : IDC_STARTNEXTFILECAT, FALSE);
	OnSettingsChange();
}

void CPPgFiles::OnDestroy()
{
	CPropertyPage::OnDestroy();
	if (m_icoBrowse) {
		VERIFY(::DestroyIcon(m_icoBrowse));
		m_icoBrowse = NULL;
	}
}