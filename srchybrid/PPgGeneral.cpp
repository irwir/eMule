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
#include "langids.h"
#include "emule.h"
#include "SearchDlg.h"
#include "PreferencesDlg.h"
#include "ppggeneral.h"
#include "HttpDownloadDlg.h"
#include "Preferences.h"
#include "emuledlg.h"
#include "StatisticsDlg.h"
#include "ServerWnd.h"
#include "TransferDlg.h"
#include "ChatWnd.h"
#include "SharedFilesWnd.h"
#include "KademliaWnd.h"
#include "IrcWnd.h"
#include "WebServices.h"
#include "HelpIDs.h"
#include "StringConversion.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CPPgGeneral, CPropertyPage)

BEGIN_MESSAGE_MAP(CPPgGeneral, CPropertyPage)
	ON_BN_CLICKED(IDC_STARTMIN, OnSettingsChange)
	ON_BN_CLICKED(IDC_STARTWIN, OnSettingsChange)
	ON_EN_CHANGE(IDC_NICK, OnSettingsChange)
	ON_BN_CLICKED(IDC_EXIT, OnSettingsChange)
	ON_BN_CLICKED(IDC_SPLASHON, OnSettingsChange)
	ON_BN_CLICKED(IDC_BRINGTOFOREGROUND, OnSettingsChange)
	ON_CBN_SELCHANGE(IDC_LANGS, OnLangChange)
	ON_BN_CLICKED(IDC_ED2KFIX, OnBnClickedEd2kfix)
	ON_BN_CLICKED(IDC_WEBSVEDIT, OnBnClickedEditWebservices)
	ON_BN_CLICKED(IDC_ONLINESIG, OnSettingsChange)
	ON_BN_CLICKED(IDC_CHECK4UPDATE, OnBnClickedCheck4Update)
	ON_BN_CLICKED(IDC_MINIMULE, OnSettingsChange)
	ON_BN_CLICKED(IDC_PREVENTSTANDBY, OnSettingsChange)
	ON_WM_HSCROLL()
	ON_WM_HELPINFO()
END_MESSAGE_MAP()

void CPPgGeneral::SetLangSel()
{
	for (int i = m_language.GetCount(); --i >= 0;)
		if (m_language.GetItemData(i) == thePrefs.GetLanguageID()) {
			m_language.SetCurSel(i);
			break;
		}
}

CPPgGeneral::CPPgGeneral()
	: CPropertyPage(CPPgGeneral::IDD)
{
}

void CPPgGeneral::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LANGS, m_language);
}

void CPPgGeneral::LoadSettings()
{
	SetLangSel();
	SetDlgItemText(IDC_NICK, thePrefs.GetUserNick());
	CheckDlgButton(IDC_STARTWIN, static_cast<UINT>(thePrefs.m_bAutoStart));
	CheckDlgButton(IDC_STARTMIN, static_cast<UINT>(thePrefs.startMinimized));
	CheckDlgButton(IDC_ONLINESIG, static_cast<UINT>(thePrefs.onlineSig));
	CheckDlgButton(IDC_EXIT, static_cast<UINT>(thePrefs.confirmExit));
	CheckDlgButton(IDC_SPLASHON, static_cast<UINT>(thePrefs.splashscreen));
	CheckDlgButton(IDC_BRINGTOFOREGROUND, static_cast<UINT>(thePrefs.bringtoforeground));
	CheckDlgButton(IDC_CHECK4UPDATE, static_cast<UINT>(thePrefs.updatenotify));
	CheckDlgButton(IDC_MINIMULE, static_cast<UINT>(thePrefs.m_bEnableMiniMule));

	if (thePrefs.GetWindowsVersion() != _WINVER_95_)
		CheckDlgButton(IDC_PREVENTSTANDBY, static_cast<UINT>(thePrefs.GetPreventStandby()));
	else {
		CheckDlgButton(IDC_PREVENTSTANDBY, 0);
		GetDlgItem(IDC_PREVENTSTANDBY)->EnableWindow(FALSE);
	}

	CString strBuffer;
	strBuffer.Format(_T("%u %s"), thePrefs.versioncheckdays, (LPCTSTR)GetResString(IDS_DAYS2));
	SetDlgItemText(IDC_DAYS, strBuffer);
}

BOOL CPPgGeneral::OnInitDialog()
{
	CPropertyPage::OnInitDialog();
	InitWindowStyles(this);

	static_cast<CEdit*>(GetDlgItem(IDC_NICK))->SetLimitText(thePrefs.GetMaxUserNickLength());

	CWordArray aLanguageIDs;
	thePrefs.GetLanguages(aLanguageIDs);
	for (int i = 0; i < aLanguageIDs.GetCount(); ++i) {
		TCHAR szLang[128];
		TCHAR *pLang = szLang;
		int ret = GetLocaleInfo(aLanguageIDs[i], LOCALE_SLANGUAGE, szLang, _countof(szLang));

		if (ret == 0)
			switch (aLanguageIDs[i]) {
			case LANGID_UG_CN:
				pLang = _T("Uyghur");
				break;
			case LANGID_GL_ES:
				pLang = _T("Galician");
				break;
			case LANGID_FR_BR:
				pLang = _T("Breton (Brezhoneg)");
				break;
			case LANGID_MT_MT:
				pLang = _T("Maltese");
				break;
			case LANGID_ES_AS:
				pLang = _T("Asturian");
				break;
			case LANGID_VA_ES:
				pLang = _T("Valencian");
				break;
			case LANGID_VA_ES_RACV:
				pLang = _T("Valencian (RACV)");
				break;
			default:
				ASSERT(0);
				pLang = _T("?(unknown language)?");
			}

		m_language.SetItemData(m_language.AddString(pLang), aLanguageIDs[i]);
	}

	UpdateEd2kLinkFixCtrl();

	CSliderCtrl *sliderUpdate = static_cast<CSliderCtrl*>(GetDlgItem(IDC_CHECKDAYS));
	sliderUpdate->SetRange(2, 7, true);
	sliderUpdate->SetPos(thePrefs.GetUpdateDays());

	LoadSettings();
	Localize();
	GetDlgItem(IDC_CHECKDAYS)->ShowWindow(IsDlgButtonChecked(IDC_CHECK4UPDATE) ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_DAYS)->ShowWindow(IsDlgButtonChecked(IDC_CHECK4UPDATE) ? SW_SHOW : SW_HIDE);

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void ModifyAllWindowStyles(CWnd *pWnd, DWORD dwRemove, DWORD dwAdd)
{
	CWnd *pWndChild = pWnd->GetWindow(GW_CHILD);
	while (pWndChild) {
		ModifyAllWindowStyles(pWndChild, dwRemove, dwAdd);
		pWndChild = pWndChild->GetNextWindow();
	}

	if (pWnd->ModifyStyleEx(dwRemove, dwAdd, SWP_FRAMECHANGED)) {
		pWnd->Invalidate();
		//		pWnd->UpdateWindow();
	}
}

BOOL CPPgGeneral::OnApply()
{
	CString strNick;
	GetDlgItemText(IDC_NICK, strNick);
	if (!IsValidEd2kString(strNick.Trim()) || strNick.IsEmpty()) {
		strNick = DEFAULT_NICK;
		SetDlgItemText(IDC_NICK, strNick);
	}
	thePrefs.SetUserNick(strNick);

	if (m_language.GetCurSel() != CB_ERR) {
		LANGID wNewLang = (LANGID)m_language.GetItemData(m_language.GetCurSel());
		if (thePrefs.GetLanguageID() != wNewLang) {
			thePrefs.SetLanguageID(wNewLang);
			thePrefs.SetLanguage();

#ifdef _DEBUG
			// Can't yet be switched on-the-fly, too many unresolved issues.
			if (thePrefs.GetRTLWindowsLayout()) {
				ModifyAllWindowStyles(theApp.emuledlg, WS_EX_LAYOUTRTL | WS_EX_RTLREADING | WS_EX_RIGHT | WS_EX_LEFTSCROLLBAR, 0);
				ModifyAllWindowStyles(theApp.emuledlg->preferenceswnd, WS_EX_LAYOUTRTL | WS_EX_RTLREADING | WS_EX_RIGHT | WS_EX_LEFTSCROLLBAR, 0);
				theApp.DisableRTLWindowsLayout();
				thePrefs.m_bRTLWindowsLayout = false;
			}
#endif
			theApp.emuledlg->preferenceswnd->Localize();
			theApp.emuledlg->statisticswnd->CreateMyTree();
			theApp.emuledlg->statisticswnd->Localize();
			theApp.emuledlg->statisticswnd->ShowStatistics(true);
			theApp.emuledlg->serverwnd->Localize();
			theApp.emuledlg->transferwnd->Localize();
			theApp.emuledlg->transferwnd->UpdateCatTabTitles();
			theApp.emuledlg->searchwnd->Localize();
			theApp.emuledlg->sharedfileswnd->Localize();
			theApp.emuledlg->chatwnd->Localize();
			theApp.emuledlg->Localize();
			theApp.emuledlg->ircwnd->Localize();
			theApp.emuledlg->kademliawnd->Localize();
		}
	}

	SetAutoStart(thePrefs.m_bAutoStart);
	thePrefs.startMinimized = thePrefs.m_bAutoStart = (IsDlgButtonChecked(IDC_STARTWIN) != 0);
	thePrefs.confirmExit = IsDlgButtonChecked(IDC_EXIT) != 0;
	thePrefs.splashscreen = IsDlgButtonChecked(IDC_SPLASHON) != 0;
	thePrefs.bringtoforeground = IsDlgButtonChecked(IDC_BRINGTOFOREGROUND) != 0;
	thePrefs.updatenotify = IsDlgButtonChecked(IDC_CHECK4UPDATE) != 0;
	thePrefs.onlineSig = IsDlgButtonChecked(IDC_ONLINESIG) != 0;
	thePrefs.versioncheckdays = static_cast<CSliderCtrl*>(GetDlgItem(IDC_CHECKDAYS))->GetPos();
	thePrefs.m_bEnableMiniMule = IsDlgButtonChecked(IDC_MINIMULE) != 0;
	thePrefs.m_bPreventStandby = IsDlgButtonChecked(IDC_PREVENTSTANDBY) != 0;

	LoadSettings();

	SetModified(FALSE);
	return CPropertyPage::OnApply();
}

void CPPgGeneral::UpdateEd2kLinkFixCtrl()
{
	GetDlgItem(IDC_ED2KFIX)->EnableWindow(Ask4RegFix(true, false, true));
}

BOOL CPPgGeneral::OnSetActive()
{
	UpdateEd2kLinkFixCtrl();
	return __super::OnSetActive();
}

void CPPgGeneral::OnBnClickedEd2kfix()
{
	Ask4RegFix(false, false, true);
	GetDlgItem(IDC_ED2KFIX)->EnableWindow(Ask4RegFix(true));
}

void CPPgGeneral::Localize()
{
	if (m_hWnd) {
		SetWindowText(GetResString(IDS_PW_GENERAL));
		SetDlgItemText(IDC_NICK_FRM, GetResString(IDS_QL_USERNAME));
		SetDlgItemText(IDC_LANG_FRM, GetResString(IDS_PW_LANG));
		SetDlgItemText(IDC_MISC_FRM, GetResString(IDS_PW_MISC));
		SetDlgItemText(IDC_EXIT, GetResString(IDS_PW_PROMPT));
		SetDlgItemText(IDC_SPLASHON, GetResString(IDS_PW_SPLASH));
		SetDlgItemText(IDC_BRINGTOFOREGROUND, GetResString(IDS_PW_FRONT));
		SetDlgItemText(IDC_ONLINESIG, GetResString(IDS_PREF_ONLINESIG));
		SetDlgItemText(IDC_STARTMIN, GetResString(IDS_PREF_STARTMIN));
		SetDlgItemText(IDC_WEBSVEDIT, GetResString(IDS_WEBSVEDIT));
		SetDlgItemText(IDC_ED2KFIX, GetResString(IDS_ED2KLINKFIX));
		SetDlgItemText(IDC_CHECK4UPDATE, GetResString(IDS_CHECK4UPDATE));
		SetDlgItemText(IDC_STARTUP, GetResString(IDS_STARTUP));
		SetDlgItemText(IDC_STARTWIN, GetResString(IDS_STARTWITHWINDOWS));
		SetDlgItemText(IDC_MINIMULE, GetResString(IDS_ENABLEMINIMULE));
		SetDlgItemText(IDC_PREVENTSTANDBY, GetResString(IDS_PREVENTSTANDBY));
	}
}

void CPPgGeneral::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
	SetModified(TRUE);

	if (pScrollBar == GetDlgItem(IDC_CHECKDAYS)) {
		CString text;
		text.Format(_T("%i %s"), reinterpret_cast<CSliderCtrl*>(pScrollBar)->GetPos(), (LPCTSTR)GetResString(IDS_DAYS2));
		SetDlgItemText(IDC_DAYS, text);
	}

	UpdateData(FALSE);
	CPropertyPage::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CPPgGeneral::OnBnClickedEditWebservices()
{
	theWebServices.Edit();
}

void CPPgGeneral::OnLangChange()
{
// Official version mirrors
//#define MIRRORS_URL	_T("http://langmirror%u.emule-project.org/lang/%u%u%u%u/")

// Community version mirrors
#if defined _M_IX86
#define SBITS _T("32/")
#elif defined _M_X64
#define SBITS _T("64/")
#else
#define SBITS
#endif
#define MIRRORS_URL	_T("https://langmirror%u.emule-project.org/lang/fox/%u%u%u%u/") SBITS

	LANGID newLangId = (LANGID)m_language.GetItemData(m_language.GetCurSel());
	if (thePrefs.GetLanguageID() != newLangId) {
		if (!thePrefs.IsLanguageSupported(newLangId)) {
			CString sAsk;
			sAsk.Format(_T("%s\r\n\r\n%s"), (LPCTSTR)GetResString(IDS_ASKDOWNLOADLANGCAP), (LPCTSTR)GetResString(IDS_ASKDOWNLOADLANG));
			if (AfxMessageBox(sAsk, MB_ICONQUESTION | MB_YESNO) == IDYES) {
				// download file
				const CString &strFilename = thePrefs.GetLangDLLNameByID(newLangId);
				// create url, use random mirror for load balancing
				UINT nRand = (rand() / (RAND_MAX / 3)) + 1;
				CString strUrl;
				strUrl.Format(MIRRORS_URL _T("%s")
					, nRand
					, CemuleApp::m_nVersionMjr, CemuleApp::m_nVersionMin
					, CemuleApp::m_nVersionUpd, CemuleApp::m_nVersionBld
					, (LPCTSTR)strFilename);

				// start download
				CHttpDownloadDlg dlgDownload;
				dlgDownload.m_strTitle = GetResString(IDS_DOWNLOAD_LANGFILE);
				dlgDownload.m_sURLToDownload = strUrl;
				dlgDownload.m_sFileToDownloadInto = thePrefs.GetMuleDirectory(EMULE_ADDLANGDIR, true) + strFilename; // save to
				if (dlgDownload.DoModal() == IDOK && thePrefs.IsLanguageSupported(newLangId)) {
					// everything OK, new language downloaded and working
					OnSettingsChange();
					return;
				}
				CString strErr;
				strErr.Format(GetResString(IDS_ERR_FAILEDDOWNLOADLANG), (LPCTSTR)strUrl);
				LogError(LOG_STATUSBAR, _T("%s"), (LPCTSTR)strErr);
				AfxMessageBox(strErr, MB_ICONERROR | MB_OK);
			}
			// undo change selection
			SetLangSel();
		} else
			OnSettingsChange();
	}
}

void CPPgGeneral::OnBnClickedCheck4Update()
{
	SetModified();
	int nCmd = IsDlgButtonChecked(IDC_CHECK4UPDATE) ? SW_SHOW : SW_HIDE;
	GetDlgItem(IDC_CHECKDAYS)->ShowWindow(nCmd);
	GetDlgItem(IDC_DAYS)->ShowWindow(nCmd);
}

void CPPgGeneral::OnHelp()
{
	theApp.ShowHelp(eMule_FAQ_Preferences_General);
}

BOOL CPPgGeneral::OnCommand(WPARAM wParam, LPARAM lParam)
{
	return (wParam == ID_HELP) ? OnHelpInfo(NULL) : __super::OnCommand(wParam, lParam);
}

BOOL CPPgGeneral::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}