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
#include "PPgIRC.h"
#include "OtherFunctions.h"
#include "emuledlg.h"
#include "Preferences.h"
#include "IrcWnd.h"
#include "HelpIDs.h"
#include "UserMsgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CPPgIRC, CPropertyPage)

BEGIN_MESSAGE_MAP(CPPgIRC, CPropertyPage)
	ON_BN_CLICKED(IDC_IRC_USECHANFILTER, OnBtnClickPerform)
	ON_BN_CLICKED(IDC_IRC_USEPERFORM, OnBtnClickPerform)
	ON_EN_CHANGE(IDC_IRC_NICK_BOX, OnSettingsChange)
	ON_EN_CHANGE(IDC_IRC_PERFORM_BOX, OnSettingsChange)
	ON_EN_CHANGE(IDC_IRC_SERVER_BOX, OnSettingsChange)
	ON_EN_CHANGE(IDC_IRC_NAME_BOX, OnSettingsChange)
	ON_EN_CHANGE(IDC_IRC_MINUSER_BOX, OnSettingsChange)
	ON_WM_DESTROY()
	ON_MESSAGE(UM_TREEOPTSCTRL_NOTIFY, OnTreeOptsCtrlNotify)
	ON_WM_HELPINFO()
END_MESSAGE_MAP()

CPPgIRC::CPPgIRC()
	: CPropertyPage(CPPgIRC::IDD)
	, m_bTimeStamp()
	, m_bSoundEvents()
	, m_bMiscMessage()
	, m_bJoinMessage()
	, m_bPartMessage()
	, m_bQuitMessage()
	, m_bPingPongMessage()
	, m_bEmuleAddFriend()
	, m_bEmuleAllowAddFriend()
	, m_bEmuleSendLink()
	, m_bAcceptLinks()
	, m_bIRCAcceptLinksFriendsOnly()
	, m_bHelpChannel()
	, m_bChannelsOnConnect()
	, m_bIRCEnableSmileys()
	, m_bIRCEnableUTF8()
	, m_ctrlTreeOptions(theApp.m_iDfltImageListColorFlags)
	, m_bInitializedTreeOpts()
	, m_htiSoundEvents()
	, m_htiTimeStamp()
	, m_htiInfoMessage()
	, m_htiMiscMessage()
	, m_htiJoinMessage()
	, m_htiPartMessage()
	, m_htiQuitMessage()
	, m_htiPingPongMessage()
	, m_htiEmuleProto()
	, m_htiEmuleAddFriend()
	, m_htiEmuleAllowAddFriend()
	, m_htiEmuleSendLink()
	, m_htiAcceptLinks()
	, m_htiAcceptLinksFriends()
	, m_htiHelpChannel()
	, m_htiChannelsOnConnect()
	, m_htiSmileys()
	, m_htiUTF8()
{
}

void CPPgIRC::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MISC_IRC, m_ctrlTreeOptions);
	if (!m_bInitializedTreeOpts) {
		m_htiHelpChannel = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_HELPCHANNEL), TVI_ROOT, m_bHelpChannel);
		m_htiChannelsOnConnect = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_LOADCHANNELLISTONCON), TVI_ROOT, m_bChannelsOnConnect);
		m_htiTimeStamp = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_ADDTIMESTAMP), TVI_ROOT, m_bTimeStamp);
		m_htiInfoMessage = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_IGNOREINFOMESSAGE), TVI_ROOT, FALSE);
		m_htiMiscMessage = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_IGNOREMISCMESSAGE), m_htiInfoMessage, m_bMiscMessage);
		m_htiJoinMessage = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_IGNOREJOINMESSAGE), m_htiInfoMessage, m_bJoinMessage);
		m_htiPartMessage = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_IGNOREPARTMESSAGE), m_htiInfoMessage, m_bPartMessage);
		m_htiQuitMessage = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_IGNOREQUITMESSAGE), m_htiInfoMessage, m_bQuitMessage);
		m_htiPingPongMessage = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_IGNOREPINGPONGMESSAGE), m_htiInfoMessage, m_bPingPongMessage);
		thePrefs.m_bIRCIgnorePingPongMessages = m_bPingPongMessage;
		m_htiEmuleProto = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_EMULEPROTO_IGNOREINFOMESSAGE), TVI_ROOT, FALSE);
		m_htiEmuleAddFriend = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_EMULEPROTO_IGNOREADDFRIEND), m_htiEmuleProto, m_bEmuleAddFriend);
		m_htiEmuleSendLink = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_EMULEPROTO_IGNORESENDLINK), m_htiEmuleProto, m_bEmuleSendLink);
		m_htiEmuleAllowAddFriend = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_EMULEPROTO_ALLOWADDFRIEND), TVI_ROOT, m_bEmuleAllowAddFriend);
		m_htiAcceptLinks = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_ACCEPTLINKS), TVI_ROOT, m_bAcceptLinks);
		m_htiAcceptLinksFriends = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_ACCEPTLINKSFRIENDS), TVI_ROOT, m_bIRCAcceptLinksFriendsOnly);
		m_htiSmileys = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_SHOWSMILEYS), TVI_ROOT, m_bIRCEnableSmileys);
		m_htiSoundEvents = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_SOUNDEVENTS), TVI_ROOT, m_bSoundEvents);
		m_htiUTF8 = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_IRC_ENABLEUTF8), TVI_ROOT, m_bIRCEnableUTF8);

		m_ctrlTreeOptions.Expand(m_htiInfoMessage, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiEmuleProto, TVE_EXPAND);

		m_ctrlTreeOptions.SendMessage(WM_VSCROLL, SB_TOP);

		m_bInitializedTreeOpts = true;
	}
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiTimeStamp, m_bTimeStamp);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiSoundEvents, m_bSoundEvents);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiMiscMessage, m_bMiscMessage);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiJoinMessage, m_bJoinMessage);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiPartMessage, m_bPartMessage);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiQuitMessage, m_bQuitMessage);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiPingPongMessage, m_bPingPongMessage);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiEmuleAddFriend, m_bEmuleAddFriend);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiEmuleAllowAddFriend, m_bEmuleAllowAddFriend);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiEmuleSendLink, m_bEmuleSendLink);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiAcceptLinks, m_bAcceptLinks);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiAcceptLinksFriends, m_bIRCAcceptLinksFriendsOnly);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiHelpChannel, m_bHelpChannel);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiChannelsOnConnect, m_bChannelsOnConnect);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiSmileys, m_bIRCEnableSmileys);
	DDX_TreeCheck(pDX, IDC_MISC_IRC, m_htiUTF8, m_bIRCEnableUTF8);

	m_ctrlTreeOptions.UpdateCheckBoxGroup(m_htiEmuleProto);
	m_ctrlTreeOptions.UpdateCheckBoxGroup(m_htiInfoMessage);
	m_ctrlTreeOptions.SetCheckBoxEnable(m_htiAcceptLinksFriends, m_bAcceptLinks);
}

BOOL CPPgIRC::OnInitDialog()
{
	m_bTimeStamp = thePrefs.GetIRCAddTimeStamp();
	m_bSoundEvents = thePrefs.GetIRCPlaySoundEvents();
	m_bMiscMessage = thePrefs.GetIRCIgnoreMiscMessages();
	m_bJoinMessage = thePrefs.GetIRCIgnoreJoinMessages();
	m_bPartMessage = thePrefs.GetIRCIgnorePartMessages();
	m_bQuitMessage = thePrefs.GetIRCIgnoreQuitMessages();
	m_bPingPongMessage = thePrefs.GetIRCIgnorePingPongMessages();
	m_bEmuleAddFriend = thePrefs.GetIRCIgnoreEmuleAddFriendMsgs();
	m_bEmuleAllowAddFriend = thePrefs.GetIRCAllowEmuleAddFriend();
	m_bEmuleSendLink = thePrefs.GetIRCIgnoreEmuleSendLinkMsgs();
	m_bAcceptLinks = thePrefs.GetIRCAcceptLinks();
	m_bIRCAcceptLinksFriendsOnly = thePrefs.GetIRCAcceptLinksFriendsOnly();
	m_bHelpChannel = thePrefs.GetIRCJoinHelpChannel();
	m_bChannelsOnConnect = thePrefs.GetIRCGetChannelsOnConnect();
	m_bIRCEnableSmileys = thePrefs.GetIRCEnableSmileys();
	m_bIRCEnableUTF8 = thePrefs.GetIRCEnableUTF8();

	m_ctrlTreeOptions.SetImageListColorFlags(theApp.m_iDfltImageListColorFlags);
	CPropertyPage::OnInitDialog();
	InitWindowStyles(this);
	static_cast<CEdit*>(GetDlgItem(IDC_IRC_NICK_BOX))->SetLimitText(20);
	static_cast<CEdit*>(GetDlgItem(IDC_IRC_MINUSER_BOX))->SetLimitText(5);
	static_cast<CEdit*>(GetDlgItem(IDC_IRC_SERVER_BOX))->SetLimitText(40);
	static_cast<CEdit*>(GetDlgItem(IDC_IRC_NAME_BOX))->SetLimitText(40);
	static_cast<CEdit*>(GetDlgItem(IDC_IRC_PERFORM_BOX))->SetLimitText(250);
	LoadSettings();
	Localize();

	UpdateControls();

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPgIRC::OnKillActive()
{
	// if prop page is closed by pressing ENTER we have to explicitly commit any possibly pending
	// data from an open edit control
	m_ctrlTreeOptions.HandleChildControlLosingFocus();
	return CPropertyPage::OnKillActive();
}

void CPPgIRC::LoadSettings()
{
	CheckDlgButton(IDC_IRC_USECHANFILTER, static_cast<UINT>(thePrefs.m_bIRCUseChannelFilter));
	CheckDlgButton(IDC_IRC_USEPERFORM, static_cast<UINT>(thePrefs.m_bIRCUsePerform));

	SetDlgItemText(IDC_IRC_SERVER_BOX, thePrefs.m_strIRCServer);
	SetDlgItemText(IDC_IRC_NICK_BOX, thePrefs.m_strIRCNick);
	SetDlgItemText(IDC_IRC_NAME_BOX, thePrefs.m_strIRCChannelFilter);
	SetDlgItemText(IDC_IRC_PERFORM_BOX, thePrefs.m_strIRCPerformString);
	SetDlgItemInt(IDC_IRC_MINUSER_BOX, thePrefs.m_uIRCChannelUserFilter);
}

BOOL CPPgIRC::OnApply()
{
	// if prop page is closed by pressing ENTER we have to explicitly commit any possibly pending
	// data from an open edit control
	m_ctrlTreeOptions.HandleChildControlLosingFocus();

	if (!UpdateData())
		return FALSE;

	thePrefs.m_bIRCAddTimeStamp = m_bTimeStamp;
	thePrefs.m_bIRCPlaySoundEvents = m_bSoundEvents;
	thePrefs.m_bIRCIgnoreMiscMessages = m_bMiscMessage;
	thePrefs.m_bIRCIgnoreJoinMessages = m_bJoinMessage;
	thePrefs.m_bIRCIgnorePartMessages = m_bPartMessage;
	thePrefs.m_bIRCIgnoreQuitMessages = m_bQuitMessage;
	thePrefs.m_bIRCIgnorePingPongMessages = m_bPingPongMessage;
	thePrefs.m_bIRCIgnoreEmuleAddFriendMsgs = m_bEmuleAddFriend;
	thePrefs.m_bIRCAllowEmuleAddFriend = m_bEmuleAllowAddFriend;
	thePrefs.m_bIRCIgnoreEmuleSendLinkMsgs = m_bEmuleSendLink;
	thePrefs.m_bIRCAcceptLinks = m_bAcceptLinks;
	thePrefs.m_bIRCAcceptLinksFriendsOnly = m_bIRCAcceptLinksFriendsOnly;
	thePrefs.m_bIRCJoinHelpChannel = m_bHelpChannel;
	thePrefs.m_bIRCGetChannelsOnConnect = m_bChannelsOnConnect;
	bool bOldSmileys = thePrefs.GetIRCEnableSmileys();
	thePrefs.m_bIRCEnableSmileys = m_bIRCEnableSmileys;
	thePrefs.m_bIRCEnableUTF8 = m_bIRCEnableUTF8;

	if (bOldSmileys != thePrefs.GetIRCEnableSmileys())
		theApp.emuledlg->ircwnd->EnableSmileys(thePrefs.GetIRCEnableSmileys());

	thePrefs.m_bIRCUseChannelFilter = IsDlgButtonChecked(IDC_IRC_USECHANFILTER) != 0;

	thePrefs.m_bIRCUsePerform = IsDlgButtonChecked(IDC_IRC_USEPERFORM) != 0;

	CString input;
	GetDlgItemText(IDC_IRC_NICK_BOX, input);
	if (input.Trim() != thePrefs.m_strIRCNick) {
		input = input.SpanExcluding(CIrcWnd::sBadCharsIRC);
		if (input[0] < _T('A')) { //names cannot begin with '-' or digit
			if (!theApp.emuledlg->ircwnd->IsConnected())
				thePrefs.m_strIRCNick.Empty();
		} else {
			thePrefs.m_strIRCNick = input;
			theApp.emuledlg->ircwnd->SendString(_T("NICK ") + input);
		}
	}

	if (GetDlgItem(IDC_IRC_SERVER_BOX)->GetWindowTextLength())
		GetDlgItemText(IDC_IRC_SERVER_BOX, thePrefs.m_strIRCServer);
	GetDlgItemText(IDC_IRC_NAME_BOX, thePrefs.m_strIRCChannelFilter);
	GetDlgItemText(IDC_IRC_PERFORM_BOX, thePrefs.m_strIRCPerformString);

	thePrefs.m_uIRCChannelUserFilter = GetDlgItemInt(IDC_IRC_MINUSER_BOX, NULL, FALSE);

	LoadSettings();
	SetModified(FALSE);
	return CPropertyPage::OnApply();
}

void CPPgIRC::LocalizeItemText(HTREEITEM item, UINT strid)
{
	if (item)
		m_ctrlTreeOptions.SetItemText(item, GetResString(strid));
}

void CPPgIRC::Localize()
{
	if (m_hWnd) {
		SetDlgItemText(IDC_IRC_SERVER_FRM, GetResString(IDS_PW_SERVER));
		SetDlgItemText(IDC_IRC_MISC_FRM, GetResString(IDS_PW_MISC));
		SetDlgItemText(IDC_IRC_NICK_FRM, GetResString(IDS_PW_NICK));
		SetDlgItemText(IDC_IRC_NAME_TEXT, GetResString(IDS_IRC_NAME));
		SetDlgItemText(IDC_IRC_MINUSER_TEXT, GetResString(IDS_UUSERS));
		SetDlgItemText(IDC_IRC_FILTER_FRM, GetResString(IDS_IRC_CHANNELLIST));
		SetDlgItemText(IDC_IRC_USECHANFILTER, GetResString(IDS_IRC_USEFILTER));
		SetDlgItemText(IDC_IRC_PERFORM_FRM, GetResString(IDS_IRC_PERFORM));
		SetDlgItemText(IDC_IRC_USEPERFORM, GetResString(IDS_IRC_USEPERFORM));

		LocalizeItemText(m_htiSoundEvents, IDS_IRC_SOUNDEVENTS);
		LocalizeItemText(m_htiTimeStamp, IDS_IRC_ADDTIMESTAMP);
		LocalizeItemText(m_htiInfoMessage, IDS_IRC_IGNOREINFOMESSAGE);
		LocalizeItemText(m_htiMiscMessage, IDS_IRC_IGNOREMISCMESSAGE);
		LocalizeItemText(m_htiJoinMessage, IDS_IRC_IGNOREJOINMESSAGE);
		LocalizeItemText(m_htiPartMessage, IDS_IRC_IGNOREPARTMESSAGE);
		LocalizeItemText(m_htiQuitMessage, IDS_IRC_IGNOREQUITMESSAGE);
		LocalizeItemText(m_htiPingPongMessage, IDS_IRC_IGNOREPINGPONGMESSAGE);
		LocalizeItemText(m_htiEmuleProto, IDS_IRC_EMULEPROTO_IGNOREINFOMESSAGE);
		LocalizeItemText(m_htiEmuleAddFriend, IDS_IRC_EMULEPROTO_IGNOREADDFRIEND);
		LocalizeItemText(m_htiEmuleAllowAddFriend, IDS_IRC_EMULEPROTO_ALLOWADDFRIEND);
		LocalizeItemText(m_htiEmuleSendLink, IDS_IRC_EMULEPROTO_IGNORESENDLINK);
		LocalizeItemText(m_htiAcceptLinks, IDS_IRC_ACCEPTLINKS);
		LocalizeItemText(m_htiAcceptLinksFriends, IDS_IRC_ACCEPTLINKSFRIENDS);
		LocalizeItemText(m_htiHelpChannel, IDS_IRC_HELPCHANNEL);
		LocalizeItemText(m_htiChannelsOnConnect, IDS_IRC_LOADCHANNELLISTONCON);
		LocalizeItemText(m_htiSmileys, IDS_SHOWSMILEYS);
	}
}

void CPPgIRC::OnBtnClickPerform()
{
	SetModified();
	UpdateControls();
}

void CPPgIRC::UpdateControls()
{
	GetDlgItem(IDC_IRC_PERFORM_BOX)->EnableWindow(IsDlgButtonChecked(IDC_IRC_USEPERFORM));
	GetDlgItem(IDC_IRC_NAME_BOX)->EnableWindow(IsDlgButtonChecked(IDC_IRC_USECHANFILTER));
	GetDlgItem(IDC_IRC_MINUSER_BOX)->EnableWindow(IsDlgButtonChecked(IDC_IRC_USECHANFILTER));
}

void CPPgIRC::OnDestroy()
{
	m_ctrlTreeOptions.DeleteAllItems();
	m_ctrlTreeOptions.DestroyWindow();
	m_bInitializedTreeOpts = false;
	m_htiAcceptLinks = NULL;
	m_htiAcceptLinksFriends = NULL;
	m_htiEmuleProto = NULL;
	m_htiEmuleAddFriend = NULL;
	m_htiEmuleAllowAddFriend = NULL;
	m_htiEmuleSendLink = NULL;
	m_htiHelpChannel = NULL;
	m_htiChannelsOnConnect = NULL;
	m_htiSoundEvents = NULL;
	m_htiInfoMessage = NULL;
	m_htiMiscMessage = NULL;
	m_htiJoinMessage = NULL;
	m_htiPartMessage = NULL;
	m_htiQuitMessage = NULL;
	m_htiPingPongMessage = NULL;
	m_htiSmileys = NULL;
	m_htiTimeStamp = NULL;
	CPropertyPage::OnDestroy();
}

LRESULT CPPgIRC::OnTreeOptsCtrlNotify(WPARAM wParam, LPARAM lParam)
{
	if (wParam == IDC_MISC_IRC) {
		TREEOPTSCTRLNOTIFY *pton = (TREEOPTSCTRLNOTIFY*)lParam;
		if (pton->hItem == m_htiAcceptLinks) {
			BOOL bCheck;
			if (m_ctrlTreeOptions.GetCheckBox(m_htiAcceptLinks, bCheck))
				m_ctrlTreeOptions.SetCheckBoxEnable(m_htiAcceptLinksFriends, bCheck);
		}
		SetModified();
	}
	return 0;
}

void CPPgIRC::OnHelp()
{
	theApp.ShowHelp(eMule_FAQ_Preferences_IRC);
}

BOOL CPPgIRC::OnCommand(WPARAM wParam, LPARAM lParam)
{
	return (wParam == ID_HELP) ? OnHelpInfo(NULL) : __super::OnCommand(wParam, lParam);
}

BOOL CPPgIRC::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}