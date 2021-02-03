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
#include "emuleDlg.h"
#include "PPgNotify.h"
#include "SMTPdialog.h"
#include "Preferences.h"
#include "OtherFunctions.h"
#include "HelpIDs.h"
#include "TextToSpeech.h"
#include "TaskbarNotifier.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CPPgNotify, CPropertyPage)

BEGIN_MESSAGE_MAP(CPPgNotify, CPropertyPage)
	ON_WM_HELPINFO()
	ON_BN_CLICKED(IDC_CB_TBN_NOSOUND, OnBnClickedNoSound)
	ON_BN_CLICKED(IDC_CB_TBN_USESOUND, OnBnClickedUseSound)
	ON_BN_CLICKED(IDC_CB_TBN_USESPEECH, OnBnClickedUseSpeech)
	ON_EN_CHANGE(IDC_EDIT_TBN_WAVFILE, OnSettingsChange)
	ON_BN_CLICKED(IDC_BTN_BROWSE_WAV, OnBnClickedBrowseAudioFile)
	ON_BN_CLICKED(IDC_TEST_NOTIFICATION, OnBnClickedTestNotification)
	ON_BN_CLICKED(IDC_CB_TBN_ONNEWDOWNLOAD, OnSettingsChange)
	ON_BN_CLICKED(IDC_CB_TBN_ONDOWNLOAD, OnSettingsChange)
	ON_BN_CLICKED(IDC_CB_TBN_ONLOG, OnSettingsChange)
	ON_BN_CLICKED(IDC_CB_TBN_ONCHAT, OnBnClickedOnChat)
	ON_BN_CLICKED(IDC_CB_TBN_IMPORTATNT, OnSettingsChange)
	ON_BN_CLICKED(IDC_CB_TBN_POP_ALWAYS, OnSettingsChange)
	ON_BN_CLICKED(IDC_CB_TBN_ONNEWVERSION, OnSettingsChange)
	ON_BN_CLICKED(IDC_SMTPSERVER, OnBnClickedSMTPserver)
	ON_EN_CHANGE(IDC_EDIT_SENDER, OnSettingsChange)
	ON_EN_CHANGE(IDC_EDIT_RECEIVER, OnSettingsChange)
	ON_BN_CLICKED(IDC_CB_ENABLENOTIFICATIONS, OnBnClickedCbEnablenotifications)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

CPPgNotify::CPPgNotify()
	: CPropertyPage(CPPgNotify::IDD)
	, m_mail(thePrefs.GetEmailSettings())
	, m_icoBrowse()
{
}

void CPPgNotify::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
}

BOOL CPPgNotify::OnInitDialog()
{
	CPropertyPage::OnInitDialog();
	InitWindowStyles(this);

	AddBuddyButton(GetDlgItem(IDC_EDIT_TBN_WAVFILE)->m_hWnd, ::GetDlgItem(m_hWnd, IDC_BTN_BROWSE_WAV));
	InitAttachedBrowseButton(::GetDlgItem(m_hWnd, IDC_BTN_BROWSE_WAV), m_icoBrowse);

	ASSERT(IDC_CB_TBN_NOSOUND < IDC_CB_TBN_USESOUND && IDC_CB_TBN_USESOUND < IDC_CB_TBN_USESPEECH);
	int iBtnID;
	switch (thePrefs.notifierSoundType) {
	case ntfstSoundFile:
		iBtnID = IDC_CB_TBN_USESOUND;
		break;
	case ntfstSpeech:
		iBtnID = IDC_CB_TBN_USESPEECH;
		break;
	default:
		ASSERT(thePrefs.notifierSoundType == ntfstNoSound);
		iBtnID = IDC_CB_TBN_NOSOUND;
	}
	CheckRadioButton(IDC_CB_TBN_NOSOUND, IDC_CB_TBN_USESPEECH, iBtnID);

	CheckDlgButton(IDC_CB_TBN_ONDOWNLOAD, thePrefs.notifierOnDownloadFinished ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CB_TBN_ONNEWDOWNLOAD, thePrefs.notifierOnNewDownload ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CB_TBN_ONCHAT, thePrefs.notifierOnChat ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CB_TBN_ONLOG, thePrefs.notifierOnLog ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CB_TBN_IMPORTATNT, thePrefs.notifierOnImportantError ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CB_TBN_POP_ALWAYS, thePrefs.notifierOnEveryChatMsg ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CB_TBN_ONNEWVERSION, thePrefs.notifierOnNewVersion ? BST_CHECKED : BST_UNCHECKED);

	GetDlgItem(IDC_CB_TBN_POP_ALWAYS)->EnableWindow(IsDlgButtonChecked(IDC_CB_TBN_ONCHAT));

	SetDlgItemText(IDC_EDIT_TBN_WAVFILE, thePrefs.notifierSoundFile);

	bool b = IsRunningXPSP2OrHigher();
	m_mail.bSendMail &= b;
	CheckDlgButton(IDC_CB_ENABLENOTIFICATIONS, m_mail.bSendMail ? BST_CHECKED : BST_UNCHECKED);

	UpdateControls();
	Localize();

	GetDlgItem(IDC_CB_TBN_USESPEECH)->EnableWindow(IsSpeechEngineAvailable());

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CPPgNotify::UpdateControls()
{
	UINT b = IsDlgButtonChecked(IDC_CB_TBN_USESOUND);
	GetDlgItem(IDC_EDIT_TBN_WAVFILE)->EnableWindow(b);
	GetDlgItem(IDC_BTN_BROWSE_WAV)->EnableWindow(b);

	b = IsDlgButtonChecked(IDC_CB_ENABLENOTIFICATIONS);
	GetDlgItem(IDC_SMTPSERVER)->EnableWindow(b);
	GetDlgItem(IDC_EDIT_RECEIVER)->EnableWindow(b);
	GetDlgItem(IDC_EDIT_SENDER)->EnableWindow(b);
}

void CPPgNotify::Localize()
{
	if (m_hWnd) {
		SetWindowText(GetResString(IDS_PW_EKDEV_OPTIONS));
		SetDlgItemText(IDC_CB_TBN_USESOUND, GetResString(IDS_PW_TBN_USESOUND));
		SetDlgItemText(IDC_CB_TBN_NOSOUND, GetResString(IDS_NOSOUND));
		SetDlgItemText(IDC_CB_TBN_ONLOG, GetResString(IDS_PW_TBN_ONLOG));
		SetDlgItemText(IDC_CB_TBN_ONCHAT, GetResString(IDS_PW_TBN_ONCHAT));
		SetDlgItemText(IDC_CB_TBN_POP_ALWAYS, GetResString(IDS_PW_TBN_POP_ALWAYS));
		SetDlgItemText(IDC_CB_TBN_ONDOWNLOAD, GetResString(IDS_PW_TBN_ONDOWNLOAD) + _T(" (*)"));
		SetDlgItemText(IDC_CB_TBN_ONNEWDOWNLOAD, GetResString(IDS_TBN_ONNEWDOWNLOAD));
		SetDlgItemText(IDC_TASKBARNOTIFIER, GetResString(IDS_PW_TASKBARNOTIFIER));
		SetDlgItemText(IDC_CB_TBN_IMPORTATNT, GetResString(IDS_PS_TBN_IMPORTANT) + _T(" (*)"));
		SetDlgItemText(IDC_CB_TBN_ONNEWVERSION, GetResString(IDS_CB_TBN_ONNEWVERSION));
		SetDlgItemText(IDC_TBN_OPTIONS, GetResString(IDS_PW_TBN_OPTIONS));
		SetDlgItemText(IDC_CB_TBN_USESPEECH, GetResString(IDS_USESPEECH));
		SetDlgItemText(IDC_EMAILNOT_GROUP, _T("(*) ") + GetResString(IDS_PW_EMAILNOTIFICATIONS));
		SetDlgItemText(IDC_SMTPSERVER, GetResString(IDS_SMTPSERVER) + _T("..."));
		SetDlgItemText(IDC_TXT_RECEIVER, GetResString(IDS_PW_RECEIVERADDRESS));
		SetDlgItemText(IDC_TXT_SENDER, GetResString(IDS_PW_SENDERADDRESS));
		SetDlgItemText(IDC_CB_ENABLENOTIFICATIONS, GetResString(IDS_PW_ENABLEEMAIL));
		SetDlgItemText(IDC_TEST_NOTIFICATION, GetResString(IDS_TEST));
	}
}

BOOL CPPgNotify::OnApply()
{
	m_mail.bSendMail = IsDlgButtonChecked(IDC_CB_ENABLENOTIFICATIONS) != 0;
	if (m_mail.bSendMail
		&& (m_mail.sServer.IsEmpty()
			|| !m_mail.uPort
			|| (m_mail.uAuth != AUTH_NONE && m_mail.sUser.IsEmpty())))
	{
		CheckDlgButton(IDC_CB_ENABLENOTIFICATIONS, BST_UNCHECKED);
		UpdateControls();
		return FALSE;
	}

	thePrefs.notifierOnDownloadFinished = IsDlgButtonChecked(IDC_CB_TBN_ONDOWNLOAD) != 0;
	thePrefs.notifierOnNewDownload = IsDlgButtonChecked(IDC_CB_TBN_ONNEWDOWNLOAD) != 0;
	thePrefs.notifierOnChat = IsDlgButtonChecked(IDC_CB_TBN_ONCHAT) != 0;
	thePrefs.notifierOnLog = IsDlgButtonChecked(IDC_CB_TBN_ONLOG) != 0;
	thePrefs.notifierOnImportantError = IsDlgButtonChecked(IDC_CB_TBN_IMPORTATNT) != 0;
	thePrefs.notifierOnEveryChatMsg = IsDlgButtonChecked(IDC_CB_TBN_POP_ALWAYS) != 0;
	thePrefs.notifierOnNewVersion = IsDlgButtonChecked(IDC_CB_TBN_ONNEWVERSION) != 0;

	GetDlgItemText(IDC_EDIT_SENDER, m_mail.sFrom);
	GetDlgItemText(IDC_EDIT_RECEIVER, m_mail.sTo);
	thePrefs.SetEmailSettings(m_mail);

	ApplyNotifierSoundType();
	if (thePrefs.notifierSoundType != ntfstSpeech)
		ReleaseTTS();

	SetModified(FALSE);
	return CPropertyPage::OnApply();
}

void CPPgNotify::ApplyNotifierSoundType()
{
	GetDlgItemText(IDC_EDIT_TBN_WAVFILE, thePrefs.notifierSoundFile);
	if (IsDlgButtonChecked(IDC_CB_TBN_USESOUND))
		thePrefs.notifierSoundType = ntfstSoundFile;
	else if (IsDlgButtonChecked(IDC_CB_TBN_USESPEECH))
		thePrefs.notifierSoundType = IsSpeechEngineAvailable() ? ntfstSpeech : ntfstNoSound;
	else {
		ASSERT(IsDlgButtonChecked(IDC_CB_TBN_NOSOUND));
		thePrefs.notifierSoundType = ntfstNoSound;
	}
}

void CPPgNotify::OnBnClickedOnChat()
{
	GetDlgItem(IDC_CB_TBN_POP_ALWAYS)->EnableWindow(IsDlgButtonChecked(IDC_CB_TBN_ONCHAT));
	SetModified();
}

void CPPgNotify::OnBnClickedBrowseAudioFile()
{
	CString strWavPath;
	GetDlgItemText(IDC_EDIT_TBN_WAVFILE, strWavPath);
	CString buffer;
	if (DialogBrowseFile(buffer, _T("Audio-Files (*.wav)|*.wav||"), strWavPath)) {
		SetDlgItemText(IDC_EDIT_TBN_WAVFILE, buffer);
		SetModified();
	}
}

void CPPgNotify::OnBnClickedNoSound()
{
	UpdateControls();
	SetModified();
}

void CPPgNotify::OnBnClickedUseSound()
{
	UpdateControls();
	SetModified();
}

void CPPgNotify::OnBnClickedUseSpeech()
{
	UpdateControls();
	SetModified();
}

void CPPgNotify::OnBnClickedTestNotification()
{
	// save current pref settings
	bool bCurNotifyOnImportantError = thePrefs.notifierOnImportantError;
	ENotifierSoundType iCurSoundType = thePrefs.notifierSoundType;
	CString strSoundFile = thePrefs.notifierSoundFile;

	// temporary apply current settings from dialog
	thePrefs.notifierOnImportantError = true;
	ApplyNotifierSoundType();

	// play test notification
	CString strTest;
	strTest.Format(GetResString(IDS_MAIN_READY), (LPCTSTR)theApp.m_strCurVersionLong);
	theApp.emuledlg->ShowNotifier(strTest, TBN_IMPORTANTEVENT);

	// restore pref settings
	thePrefs.notifierSoundFile = strSoundFile;
	thePrefs.notifierSoundType = iCurSoundType;
	thePrefs.notifierOnImportantError = bCurNotifyOnImportantError;
}

void CPPgNotify::OnHelp()
{
	theApp.ShowHelp(eMule_FAQ_Preferences_Notifications);
}

BOOL CPPgNotify::OnCommand(WPARAM wParam, LPARAM lParam)
{
	return (wParam == ID_HELP) ? OnHelpInfo(NULL) : __super::OnCommand(wParam, lParam);
}

BOOL CPPgNotify::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}

void CPPgNotify::OnBnClickedCbEnablenotifications()
{
	UpdateControls();
	SetModified();
}

void CPPgNotify::OnBnClickedSMTPserver()
{
	CSMTPserverDlg serverDlg;
	if (serverDlg.DoModal() == IDOK) {
		SetModified();
		m_mail = thePrefs.GetEmailSettings(); //reload
		UpdateControls();
	}
}

void CPPgNotify::OnDestroy()
{
	CPropertyPage::OnDestroy();
	if (m_icoBrowse) {
		VERIFY(::DestroyIcon(m_icoBrowse));
		m_icoBrowse = NULL;
	}
}