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
#include <math.h>
#include "emule.h"
#include "PPgConnection.h"
#include "wizard.h"
#include "Scheduler.h"
#include "OtherFunctions.h"
#include "emuledlg.h"
#include "Preferences.h"
#include "Opcodes.h"
#include "StatisticsDlg.h"
#include "Kademlia/Kademlia/Kademlia.h"
#include "HelpIDs.h"
#include "Statistics.h"
#include "Firewallopener.h"
#include "ListenSocket.h"
#include "ClientUDPSocket.h"
#include "LastCommonRouteFinder.h"
#include "PreferencesDlg.h"
#include "PPgWebServer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CPPgConnection, CPropertyPage)

BEGIN_MESSAGE_MAP(CPPgConnection, CPropertyPage)
	ON_BN_CLICKED(IDC_STARTTEST, OnStartPortTest)
	ON_EN_CHANGE(IDC_DOWNLOAD_CAP, OnSettingsChange)
	ON_EN_CHANGE(IDC_UPLOAD_CAP, OnSettingsChange)
	ON_BN_CLICKED(IDC_UDPDISABLE, OnEnChangeUDPDisable)
	ON_EN_CHANGE(IDC_UDPPORT, OnSettingsChange)
	ON_EN_CHANGE(IDC_PORT, OnSettingsChange)
	ON_EN_KILLFOCUS(IDC_UDPPORT, OnEnKillFocusUDP)
	ON_EN_KILLFOCUS(IDC_PORT, OnEnKillFocusTCP)
	ON_EN_CHANGE(IDC_MAXCON, OnSettingsChange)
	ON_EN_CHANGE(IDC_MAXSOURCEPERFILE, OnSettingsChange)
	ON_BN_CLICKED(IDC_AUTOCONNECT, OnSettingsChange)
	ON_BN_CLICKED(IDC_RECONN, OnSettingsChange)
	ON_BN_CLICKED(IDC_WIZARD, OnBnClickedWizard)
	ON_BN_CLICKED(IDC_NETWORK_ED2K, OnSettingsChange)
	ON_BN_CLICKED(IDC_SHOWOVERHEAD, OnSettingsChange)
	ON_BN_CLICKED(IDC_ULIMIT_LBL, OnLimiterChange)
	ON_BN_CLICKED(IDC_DLIMIT_LBL, OnLimiterChange)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_NETWORK_KADEMLIA, OnSettingsChange)
	ON_WM_HELPINFO()
	ON_BN_CLICKED(IDC_OPENPORTS, OnBnClickedOpenports)
	ON_BN_CLICKED(IDC_PREF_UPNPONSTART, OnSettingsChange)
END_MESSAGE_MAP()

CPPgConnection::CPPgConnection()
	: CPropertyPage(CPPgConnection::IDD)
	, m_lastudp()
{
}

void CPPgConnection::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MAXDOWN_SLIDER, m_ctlMaxDown);
	DDX_Control(pDX, IDC_MAXUP_SLIDER, m_ctlMaxUp);
}

void CPPgConnection::OnEnKillFocusTCP()
{
	ChangePorts(1);
}

void CPPgConnection::OnEnKillFocusUDP()
{
	ChangePorts(0);
}

void CPPgConnection::ChangePorts(uint8 iWhat)
{
	UINT tcp = GetDlgItemInt(IDC_PORT, NULL, FALSE);
	UINT udp = GetDlgItemInt(IDC_UDPPORT, NULL, FALSE);

	GetDlgItem(IDC_STARTTEST)->EnableWindow(
		GetDlgItemInt(IDC_PORT, NULL, FALSE) == theApp.listensocket->GetConnectedPort()
		&& GetDlgItemInt(IDC_UDPPORT, NULL, FALSE) == theApp.clientudp->GetConnectedPort()
	);

	if (iWhat == 0) //UDP
		ChangeUDP();
	else if (iWhat == 1) //TCP
		if (tcp != thePrefs.port || udp != thePrefs.udpport)
			OnSettingsChange();
	//else if (iWhat == 2) "Test ports" button enable/disable - done already
}

bool CPPgConnection::ChangeUDP()
{
	bool bDisabled = IsDlgButtonChecked(IDC_UDPDISABLE) != 0;
	GetDlgItem(IDC_UDPPORT)->EnableWindow(!bDisabled);

	uint16 newVal, oldVal = (uint16)GetDlgItemInt(IDC_UDPPORT, NULL, FALSE);
	if (oldVal)
		m_lastudp = oldVal;
	if (bDisabled)
		newVal = 0;
	else
		newVal = m_lastudp ? m_lastudp : (10ui16 + thePrefs.port);
	if (newVal != oldVal)
		SetDlgItemInt(IDC_UDPPORT, newVal, FALSE);
	return bDisabled;
}

void CPPgConnection::OnEnChangeUDPDisable()
{
	SetModified();
	bool bDisabled = ChangeUDP();
	CheckDlgButton(IDC_NETWORK_KADEMLIA, static_cast<UINT>(thePrefs.networkkademlia && !bDisabled)); // don't use GetNetworkKademlia here
	GetDlgItem(IDC_NETWORK_KADEMLIA)->EnableWindow(!bDisabled);
}

BOOL CPPgConnection::OnInitDialog()
{
	CPropertyPage::OnInitDialog();
	InitWindowStyles(this);

	static_cast<CEdit*>(GetDlgItem(IDC_PORT))->SetLimitText(5);
	static_cast<CEdit*>(GetDlgItem(IDC_UDPPORT))->SetLimitText(5);

	LoadSettings();
	Localize();

	ChangePorts(2); //"Test ports" button enable/disable

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CPPgConnection::LoadSettings()
{
	if (m_hWnd) {
		if (thePrefs.m_maxupload != 0)
			thePrefs.m_maxdownload = thePrefs.GetMaxDownload();
		m_lastudp = thePrefs.udpport;
		CheckDlgButton(IDC_UDPDISABLE, !m_lastudp); //before the port number!
		SetDlgItemInt(IDC_UDPPORT, m_lastudp, FALSE);

		SetDlgItemInt(IDC_DOWNLOAD_CAP, thePrefs.maxGraphDownloadRate);

		m_ctlMaxDown.SetRange(1, thePrefs.maxGraphDownloadRate);
		SetRateSliderTicks(m_ctlMaxDown);

		SetDlgItemInt(IDC_UPLOAD_CAP, (thePrefs.maxGraphUploadRate != UNLIMITED ? thePrefs.maxGraphUploadRate : 0));

		m_ctlMaxUp.SetRange(1, thePrefs.GetMaxGraphUploadRate(true));
		SetRateSliderTicks(m_ctlMaxUp);

		uint32 up = thePrefs.m_maxupload;
		uint32 dn = thePrefs.m_maxdownload;
		CheckDlgButton(IDC_DLIMIT_LBL, (dn != UNLIMITED));
		CheckDlgButton(IDC_ULIMIT_LBL, (up != UNLIMITED));
		if (dn == UNLIMITED)
			dn = thePrefs.maxGraphDownloadRate;
		if (up == UNLIMITED)
			up = thePrefs.GetMaxGraphUploadRate(true);
		CheckUp(up, dn);
		CheckDown(up, dn);
		m_ctlMaxDown.SetPos(dn);
		m_ctlMaxUp.SetPos(up);

		SetDlgItemInt(IDC_PORT, thePrefs.port, FALSE);
		SetDlgItemInt(IDC_MAXCON, thePrefs.maxconnections);
		SetDlgItemInt(IDC_MAXSOURCEPERFILE, (thePrefs.maxsourceperfile == 0xffff ? 0 : thePrefs.maxsourceperfile));

		CheckDlgButton(IDC_RECONN, static_cast<UINT>(thePrefs.reconnect));
		CheckDlgButton(IDC_SHOWOVERHEAD, static_cast<UINT>(thePrefs.m_bshowoverhead));
		CheckDlgButton(IDC_AUTOCONNECT, static_cast<UINT>(thePrefs.autoconnect));
		CheckDlgButton(IDC_NETWORK_KADEMLIA, static_cast<UINT>(thePrefs.GetNetworkKademlia()));
		GetDlgItem(IDC_NETWORK_KADEMLIA)->EnableWindow(thePrefs.GetUDPPort() > 0);
		CheckDlgButton(IDC_NETWORK_ED2K, static_cast<UINT>(thePrefs.networked2k));

		WORD wv = thePrefs.GetWindowsVersion();
		// don't try on XP SP2 or higher, not needed there any more
		GetDlgItem(IDC_OPENPORTS)->ShowWindow(
			(wv == _WINVER_XP_ && !IsRunningXPSP2() && theApp.m_pFirewallOpener->DoesFWConnectionExist())
			? SW_SHOW : SW_HIDE);

		GetDlgItem(IDC_PREF_UPNPONSTART)->EnableWindow(wv != _WINVER_95_ && wv != _WINVER_98_ && wv != _WINVER_NT4_);

		CheckDlgButton(IDC_PREF_UPNPONSTART, static_cast<UINT>(thePrefs.IsUPnPEnabled()));

		//ShowLimitValues(); - will be called in OnLimiterChange()
		OnLimiterChange();
	}
}

BOOL CPPgConnection::OnApply()
{
	UINT v = GetDlgItemInt(IDC_DOWNLOAD_CAP, NULL, FALSE);
	if (v >= UNLIMITED) {
		GetDlgItem(IDC_DOWNLOAD_CAP)->SetFocus();
		return FALSE;
	}
	UINT u = GetDlgItemInt(IDC_UPLOAD_CAP, NULL, FALSE);
	if (u >= UNLIMITED) {
		GetDlgItem(IDC_UPLOAD_CAP)->SetFocus();
		return FALSE;
	}

	int lastmaxgu = thePrefs.maxGraphUploadRate; //save the values
	int lastmaxgd = thePrefs.maxGraphDownloadRate;

	thePrefs.SetMaxGraphDownloadRate(v);
	m_ctlMaxDown.SetRange(1, thePrefs.GetMaxGraphDownloadRate(), TRUE);
	SetRateSliderTicks(m_ctlMaxDown);

	thePrefs.SetMaxGraphUploadRate(u);
	m_ctlMaxUp.SetRange(1, thePrefs.GetMaxGraphUploadRate(true), TRUE);
	SetRateSliderTicks(m_ctlMaxUp);

	if (IsDlgButtonChecked(IDC_ULIMIT_LBL)) {
		u = (uint32)m_ctlMaxUp.GetPos();
		v = (uint32)thePrefs.GetMaxGraphUploadRate(true);
		if (u > v)
			u = v * 4 / 5; //80%
	} else
		u = UNLIMITED;

	if (u > thePrefs.GetMaxUpload())
		// make USS go up to higher ul limit faster
		theApp.lastCommonRouteFinder->InitiateFastReactionPeriod();

	thePrefs.SetMaxUpload(u);

	if (thePrefs.GetMaxUpload() != UNLIMITED)
		m_ctlMaxUp.SetPos(thePrefs.GetMaxUpload());

	thePrefs.SetMaxDownload(IsDlgButtonChecked(IDC_DLIMIT_LBL) ? m_ctlMaxDown.GetPos() : UNLIMITED);

	if (thePrefs.GetMaxDownload() != UNLIMITED) {
		u = (uint32)thePrefs.GetMaxGraphDownloadRate();
		if (thePrefs.GetMaxDownload() > u)
			thePrefs.SetMaxDownload(u * 4 / 5); //80%
		m_ctlMaxDown.SetPos(thePrefs.GetMaxDownload());
	}

	u = GetDlgItemInt(IDC_MAXSOURCEPERFILE, NULL, FALSE);
	thePrefs.maxsourceperfile = (u > INT_MAX ? 1 : u);

	bool bRestartApp = false;

	u = GetDlgItemInt(IDC_PORT, NULL, FALSE);
	uint16 nNewPort = (uint16)(u > _UI16_MAX ? 0 : u);
	if (nNewPort && nNewPort != thePrefs.port) {
		thePrefs.port = nNewPort;
		if (theApp.IsPortchangeAllowed())
			theApp.listensocket->Rebind();
		else
			bRestartApp = true;
	}

	u = GetDlgItemInt(IDC_UDPPORT, NULL, FALSE);
	nNewPort = (uint16)(u > _UI16_MAX ? 0 : u);
	if (nNewPort != thePrefs.udpport) {
		thePrefs.udpport = nNewPort;
		if (theApp.IsPortchangeAllowed())
			theApp.clientudp->Rebind();
		else
			bRestartApp = true;
	}

	if (thePrefs.m_bshowoverhead != (IsDlgButtonChecked(IDC_SHOWOVERHEAD) != 0)) {
		thePrefs.m_bshowoverhead = !thePrefs.m_bshowoverhead;
		// free memory and reset overhead data counters
		theStats.ResetDownDatarateOverhead();
		theStats.ResetUpDatarateOverhead();
	}

	thePrefs.SetNetworkKademlia(IsDlgButtonChecked(IDC_NETWORK_KADEMLIA) != 0);

	thePrefs.SetNetworkED2K(IsDlgButtonChecked(IDC_NETWORK_ED2K) != 0);

	GetDlgItem(IDC_UDPPORT)->EnableWindow(!IsDlgButtonChecked(IDC_UDPDISABLE));

	thePrefs.autoconnect = IsDlgButtonChecked(IDC_AUTOCONNECT) != 0;
	thePrefs.reconnect = IsDlgButtonChecked(IDC_RECONN) != 0;

	if (lastmaxgu != thePrefs.maxGraphUploadRate)
		theApp.emuledlg->statisticswnd->SetARange(false, thePrefs.GetMaxGraphUploadRate(true));
	if (lastmaxgd != thePrefs.maxGraphDownloadRate)
		theApp.emuledlg->statisticswnd->SetARange(true, thePrefs.maxGraphDownloadRate);

	UINT tempcon;
	u = GetDlgItemInt(IDC_MAXCON, NULL, FALSE);
	if (u <= 0)
		tempcon = thePrefs.maxconnections;
	else
		tempcon = (u >= INT_MAX ? CPreferences::GetRecommendedMaxConnections() : u);

	if (tempcon > (UINT)GetMaxWindowsTCPConnections()) {
		CString strMessage;
		strMessage.Format(GetResString(IDS_PW_WARNING), (LPCTSTR)GetResString(IDS_PW_MAXC), GetMaxWindowsTCPConnections());
		int iResult = AfxMessageBox(strMessage, MB_ICONWARNING | MB_YESNO);
		if (iResult != IDYES) {
			//TODO: set focus to max connection?
			SetDlgItemInt(IDC_MAXCON, thePrefs.maxconnections);
			tempcon = GetMaxWindowsTCPConnections();
		}
	}
	thePrefs.maxconnections = tempcon;

	if (thePrefs.IsUPnPEnabled() != (IsDlgButtonChecked(IDC_PREF_UPNPONSTART) != 0)) {
		thePrefs.m_bEnableUPnP = !thePrefs.m_bEnableUPnP;
		if (thePrefs.m_bEnableUPnP)
			theApp.emuledlg->StartUPnP();
		if (theApp.emuledlg->preferenceswnd->m_wndWebServer)
			theApp.emuledlg->preferenceswnd->m_wndWebServer.SetUPnPState();
	}

	theApp.scheduler->SaveOriginals();

	SetModified(FALSE);
	LoadSettings();

	theApp.emuledlg->ShowConnectionState();

	if (bRestartApp)
		LocMessageBox(IDS_NOPORTCHANGEPOSSIBLE, MB_OK, 0);

	ChangePorts(2);	//"Test ports" button enable/disable

	return CPropertyPage::OnApply();
}

void CPPgConnection::Localize()
{
	if (m_hWnd) {
		SetWindowText(GetResString(IDS_CONNECTION));
		SetDlgItemText(IDC_CAPACITIES_FRM, GetResString(IDS_PW_CON_CAPFRM));
		SetDlgItemText(IDC_DCAP_LBL, GetResString(IDS_PW_CON_DOWNLBL));
		SetDlgItemText(IDC_UCAP_LBL, GetResString(IDS_PW_CON_UPLBL));
		SetDlgItemText(IDC_LIMITS_FRM, GetResString(IDS_PW_CON_LIMITFRM));
		SetDlgItemText(IDC_DLIMIT_LBL, GetResString(IDS_PW_DOWNL));
		SetDlgItemText(IDC_ULIMIT_LBL, GetResString(IDS_PW_UPL));
		SetDlgItemText(IDC_CONNECTION_NETWORK, GetResString(IDS_NETWORK));
		SetDlgItemText(IDC_KBS2, GetResString(IDS_KBYTESPERSEC));
		SetDlgItemText(IDC_KBS3, GetResString(IDS_KBYTESPERSEC));
		SetDlgItemText(IDC_MAXCONN_FRM, GetResString(IDS_PW_CONLIMITS));
		SetDlgItemText(IDC_MAXCONLABEL, GetResString(IDS_PW_MAXC));
		SetDlgItemText(IDC_SHOWOVERHEAD, GetResString(IDS_SHOWOVERHEAD));
		SetDlgItemText(IDC_CLIENTPORT_FRM, GetResString(IDS_PW_CLIENTPORT));
		SetDlgItemText(IDC_MAXSRC_FRM, GetResString(IDS_PW_MAXSOURCES));
		SetDlgItemText(IDC_AUTOCONNECT, GetResString(IDS_PW_AUTOCON));
		SetDlgItemText(IDC_RECONN, GetResString(IDS_PW_RECON));
		SetDlgItemText(IDC_MAXSRCHARD_LBL, GetResString(IDS_HARDLIMIT));
		SetDlgItemText(IDC_WIZARD, GetResString(IDS_WIZARD) + _T("..."));
		SetDlgItemText(IDC_UDPDISABLE, GetResString(IDS_UDPDISABLED));
		SetDlgItemText(IDC_OPENPORTS, GetResString(IDS_FO_PREFBUTTON));
		SetDlgItemText(IDC_STARTTEST, GetResString(IDS_STARTTEST));
		SetDlgItemText(IDC_PREF_UPNPONSTART, GetResString(IDS_UPNPSTART));
		ShowLimitValues();
	}
}

void CPPgConnection::OnBnClickedWizard()
{
	CConnectionWizardDlg conWizard;
	conWizard.DoModal();
}

bool CPPgConnection::CheckUp(uint32 mUp, uint32 &mDown)
{
	if (thePrefs.maxGraphDownloadRate <= 0)
		return false;
	uint32 uDown = mDown;
	if (mUp < 4 && mDown > mUp * 3)
		mDown = mUp * 3;
	else if (mUp < 10 && mDown > mUp * 4)
		mDown = mUp * 4;
	else if (mUp < 20 && mDown > mUp * 5)
		mDown = mUp * 5;
	if (mDown > (uint32)thePrefs.maxGraphDownloadRate) {
		mDown = thePrefs.maxGraphDownloadRate;
		return true;
	}
	return uDown != mDown;
}

bool CPPgConnection::CheckDown(uint32 &mUp, uint32 mDown)
{
	if (thePrefs.maxGraphUploadRate <= 0)
		return false;
	uint32 uUp = mUp;
	if (mDown < 13 && mUp * 3 < mDown)
		mUp = (mDown + 2) / 3;
	else if (mDown < 41 && mUp * 4 < mDown)
		mUp = (mDown + 3) / 4;
	else if (mUp < 20 && mUp * 5 < mDown)
		mUp = (mDown + 4) / 5;
	if (mUp > (uint32)thePrefs.maxGraphUploadRate) {
		mUp = thePrefs.maxGraphUploadRate;
		return true;
	}
	return uUp != mUp;
}

void CPPgConnection::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
	SetModified(TRUE);

	uint32 maxup = m_ctlMaxUp.GetPos();
	uint32 maxdown = m_ctlMaxDown.GetPos();

	if (pScrollBar->GetSafeHwnd() == m_ctlMaxUp.m_hWnd) {
		if (CheckUp(maxup, maxdown)) {
			if (CheckDown(maxup, maxdown))
				m_ctlMaxUp.SetPos(maxup);
			m_ctlMaxDown.SetPos(maxdown);
		}
	} else { /*if (hWnd == m_ctlMaxDown.m_hWnd) { */
		if (CheckDown(maxup, maxdown)) {
			if (CheckUp(maxup, maxdown))
				m_ctlMaxDown.SetPos(maxdown);
			m_ctlMaxUp.SetPos(maxup);
		}
	}

	ShowLimitValues();

	UpdateData(FALSE);
	CPropertyPage::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CPPgConnection::ShowLimitValues()
{
	static LPCTSTR const pszFmt = _T("%i %s");
	CString buffer;

	if (IsDlgButtonChecked(IDC_ULIMIT_LBL))
		buffer.Format(pszFmt, m_ctlMaxUp.GetPos(), (LPCTSTR)GetResString(IDS_KBYTESPERSEC));
	SetDlgItemText(IDC_KBS4, buffer);

	if (!IsDlgButtonChecked(IDC_DLIMIT_LBL))
		buffer.Empty();
	else
		buffer.Format(pszFmt, m_ctlMaxDown.GetPos(), (LPCTSTR)GetResString(IDS_KBYTESPERSEC));
	SetDlgItemText(IDC_KBS1, buffer);
}

void CPPgConnection::OnLimiterChange()
{
	m_ctlMaxDown.ShowWindow(IsDlgButtonChecked(IDC_DLIMIT_LBL) ? SW_SHOW : SW_HIDE);
	m_ctlMaxUp.ShowWindow(IsDlgButtonChecked(IDC_ULIMIT_LBL) ? SW_SHOW : SW_HIDE);

	ShowLimitValues();
	SetModified(TRUE);
}

void CPPgConnection::OnHelp()
{
	theApp.ShowHelp(eMule_FAQ_Preferences_Connection);
}

BOOL CPPgConnection::OnCommand(WPARAM wParam, LPARAM lParam)
{
	return (wParam == ID_HELP) ? OnHelpInfo(NULL) : __super::OnCommand(wParam, lParam);
}

BOOL CPPgConnection::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}

void CPPgConnection::OnBnClickedOpenports()
{
	OnApply();
	theApp.m_pFirewallOpener->RemoveRule(EMULE_DEFAULTRULENAME_UDP);
	theApp.m_pFirewallOpener->RemoveRule(EMULE_DEFAULTRULENAME_TCP);
	bool bAlreadyExisted = theApp.m_pFirewallOpener->DoesRuleExist(thePrefs.GetPort(), NAT_PROTOCOL_TCP)
		|| theApp.m_pFirewallOpener->DoesRuleExist(thePrefs.GetUDPPort(), NAT_PROTOCOL_UDP);
	bool bResult = theApp.m_pFirewallOpener->OpenPort(thePrefs.GetPort(), NAT_PROTOCOL_TCP, EMULE_DEFAULTRULENAME_TCP, false);
	if (thePrefs.GetUDPPort() != 0)
		bResult = bResult && theApp.m_pFirewallOpener->OpenPort(thePrefs.GetUDPPort(), NAT_PROTOCOL_UDP, EMULE_DEFAULTRULENAME_UDP, false);
	if (bResult) {
		if (!bAlreadyExisted)
			LocMessageBox(IDS_FO_PREF_SUCCCEEDED, MB_ICONINFORMATION | MB_OK, 0);
		else
			// TODO: actually we could offer the user to remove existing rules
			LocMessageBox(IDS_FO_PREF_EXISTED, MB_ICONINFORMATION | MB_OK, 0);
	} else
		LocMessageBox(IDS_FO_PREF_FAILED, MB_ICONSTOP | MB_OK, 0);
}

void CPPgConnection::OnStartPortTest()
{
	uint16 tcp = (uint16)GetDlgItemInt(IDC_PORT, NULL, FALSE);
	uint16 udp = (uint16)GetDlgItemInt(IDC_UDPPORT, NULL, FALSE);

	TriggerPortTest(tcp, udp);
}

void CPPgConnection::SetRateSliderTicks(CSliderCtrl &rRate)
{
	rRate.ClearTics();
	int iMin, iMax;
	rRate.GetRange(iMin, iMax);
	int iDiff = iMax - iMin;
	if (iDiff > 0) {
		CRect rc;
		rRate.GetWindowRect(&rc);
		if (rc.Width() > 0) {
			int iTic;
			int iPixels = rc.Width() / iDiff;
			if (iPixels >= 6)
				iTic = 1;
			else {
				iTic = 10;
				while (rc.Width() / (iDiff / iTic) < 8)
					iTic *= 10;
			}
			if (iTic)
				for (int i = ((iMin + (iTic - 1)) / iTic) * iTic; i < iMax; i += iTic)
					rRate.SetTic(i);
			rRate.SetPageSize(iTic);
		}
	}
}