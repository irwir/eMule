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

#include "StdAfx.h"
#define MMNODRV			// mmsystem: Installable driver support
//#define MMNOSOUND		// mmsystem: Sound support
#define MMNOWAVE		// mmsystem: Waveform support
#define MMNOMIDI		// mmsystem: MIDI support
#define MMNOAUX			// mmsystem: Auxiliary audio support
#define MMNOMIXER		// mmsystem: Mixer support
#define MMNOTIMER		// mmsystem: Timer support
#define MMNOJOY			// mmsystem: Joystick support
#define MMNOMCI			// mmsystem: MCI support
#define MMNOMMIO		// mmsystem: Multimedia file I/O support
#define MMNOMMSYSTEM	// mmsystem: General MMSYSTEM functions
#include <Mmsystem.h>
#include "IrcChannelTabCtrl.h"
#include "emule.h"
#include "IrcWnd.h"
#include "IrcMain.h"
#include "otherfunctions.h"
#include "MenuCmds.h"
#include "HTRichEditCtrl.h"
#include "UserMsgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNAMIC(CIrcChannelTabCtrl, CClosableTabCtrl)

BEGIN_MESSAGE_MAP(CIrcChannelTabCtrl, CClosableTabCtrl)
	ON_MESSAGE(UM_CLOSETAB, OnCloseTab)
	ON_MESSAGE(UM_QUERYTAB, OnQueryTab)
	ON_NOTIFY_REFLECT(TCN_SELCHANGE, OnTcnSelChange)
END_MESSAGE_MAP()

CIrcChannelTabCtrl::CIrcChannelTabCtrl()
{
	m_pCurrentChannel = NULL;
	m_pChanStatus = NULL;
	m_pChanList = NULL;
	m_pParent = NULL;
	m_bClosable = true;
}

CIrcChannelTabCtrl::~CIrcChannelTabCtrl()
{
	//Remove and delete all our open channels.
	DeleteAllChannels();
}

void CIrcChannelTabCtrl::Init()
{
	//This adds the two static windows, Status and ChanneList
	m_pChanStatus = NewChannel(GetResString(IDS_STATUS), Channel::ctStatus);
	m_pCurrentChannel = m_lstChannels.GetTail(); //Initialize the IRC window
	SelectChannel(0); //switch to Status
	SetAllIcons();
}

void CIrcChannelTabCtrl::OnSysColorChange()
{
	CClosableTabCtrl::OnSysColorChange();
	SetAllIcons();
}

void CIrcChannelTabCtrl::AutoComplete()
{
	CString sSend;
	m_pParent->m_wndInput.GetWindowText(sSend);
	if (sSend.IsEmpty()) {
		m_pCurrentChannel->m_sTyped.Empty();
		m_pCurrentChannel->m_sTabd.Empty();
		return;
	}
	if (sSend != m_pCurrentChannel->m_sTabd) { //string has changed
		m_pCurrentChannel->m_sTyped = sSend;
		m_pCurrentChannel->m_sTabd.Empty(); //restart autocompletion
	}
	int i = m_pCurrentChannel->m_sTyped.ReverseFind(_T(' ')); //find the last word
	CString sName, sPrev;
	if (i < 0) {
		sName = m_pCurrentChannel->m_sTyped; //all input is one word
		sPrev = m_pCurrentChannel->m_sTabd;
		sSend.Empty();
	} else {
		sName = m_pCurrentChannel->m_sTyped.Mid(++i); //word to complete
		sPrev = m_pCurrentChannel->m_sTabd.Mid(i); //the latest auto-completed value
		sSend.Truncate(i); //the rest of the string
	}
	i = sName.GetLength();
	CString sFirst; //to wrap around after the last value
	CString sNext; //next value
	for (POSITION pos = m_pCurrentChannel->m_lstNicks.GetHeadPosition(); pos != NULL;) {
		const Nick *pNick = m_pCurrentChannel->m_lstNicks.GetNext(pos);
		if (pNick->m_sNick.Left(i).CompareNoCase(sName) == 0) {
			if (sFirst.IsEmpty() || sFirst.CompareNoCase(pNick->m_sNick) > 0)
				sFirst = pNick->m_sNick;
			if (sPrev.CompareNoCase(pNick->m_sNick) < 0 && (sNext.IsEmpty() || sNext.CompareNoCase(pNick->m_sNick) > 0))
				sNext = pNick->m_sNick;
		}
	}
	if (!sNext.IsEmpty())
		m_pCurrentChannel->m_sTabd = sSend + sNext;
	else if (!sFirst.IsEmpty())
		m_pCurrentChannel->m_sTabd = sSend + sFirst;
	m_pCurrentChannel->m_sTyped = sSend + sName;
	if (!m_pCurrentChannel->m_sTyped.IsEmpty() && m_pCurrentChannel->m_sTabd.IsEmpty())
		m_pCurrentChannel->m_sTabd = m_pCurrentChannel->m_sTyped;
	SetInput(m_pCurrentChannel->m_sTabd);
}

void CIrcChannelTabCtrl::SetAllIcons()
{
	CImageList imlist;
	imlist.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	imlist.Add(CTempIconLoader(_T("Log")));
	imlist.Add(CTempIconLoader(_T("IRC")));
	imlist.Add(CTempIconLoader(_T("Message")));
	imlist.Add(CTempIconLoader(_T("MessagePending")));
	SetImageList(&imlist);
	m_imlistIRC.DeleteImageList();
	m_imlistIRC.Attach(imlist.Detach());
	SetPadding(CSize(12, 3));
}

Channel* CIrcChannelTabCtrl::FindChannelByName(const CString &sChannel)
{
	CString sTempName(sChannel);
	sTempName.Trim();

	for (POSITION pos = m_lstChannels.GetHeadPosition(); pos != NULL;) {
		Channel *pChannel = m_lstChannels.GetNext(pos);
		if ((pChannel->m_eType == Channel::ctNormal || pChannel->m_eType == Channel::ctPrivate) && pChannel->m_sName.CompareNoCase(sTempName) == 0)
			return pChannel;
	}
	return NULL;
}

Channel* CIrcChannelTabCtrl::FindOrCreateChannel(const CString &sChannel)
{
	if (sChannel.IsEmpty())
		return NULL;
	Channel *pChannel = FindChannelByName(sChannel);
	if (!pChannel) {
		if (sChannel[0] == _T('#'))
			pChannel = NewChannel(sChannel, Channel::ctNormal);
		else if (sChannel.CompareNoCase(thePrefs.GetIRCNick()) == 0) {
			// A 'Notice' message for myself - display in current channel window
			pChannel = m_pCurrentChannel;
			if (pChannel && pChannel->m_eType == Channel::ctChannelList)
				pChannel = NULL;	// channels list window -> open a new channel window
		}
		if (!pChannel)
			pChannel = NewChannel(sChannel, Channel::ctPrivate);
	}
	return pChannel;
}

Channel* CIrcChannelTabCtrl::NewChannel(const CString &sChannel, Channel::EType eType)
{
	Channel *pChannel = new Channel;
	pChannel->m_sName = sChannel;
	pChannel->m_sTitle = sChannel;
	pChannel->m_eType = eType;
	pChannel->m_iHistoryPos = 0;
	pChannel->m_bDetached = false;
	if (eType != Channel::ctChannelList) {
		RECT rcChannelPane;
		m_pParent->m_wndChanList.GetWindowRect(&rcChannelPane);
		m_pParent->ScreenToClient(&rcChannelPane);

		RECT rcLog(rcChannelPane);
		if (eType == Channel::ctNormal
#ifdef _DEBUG
//#define DEBUG_IRC_TEXT
#endif
#ifdef DEBUG_IRC_TEXT
			|| eType == Channel::ctStatus
#endif
			)
		{
			RECT rcTopic = { rcChannelPane.left, rcChannelPane.top, rcChannelPane.right, rcChannelPane.top + IRC_TITLE_WND_DFLT_HEIGHT };
			pChannel->m_wndTopic.Create(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_NOHIDESEL, rcTopic, m_pParent, IDC_TITLEWINDOW);
			pChannel->m_wndTopic.ModifyStyleEx(0, WS_EX_STATICEDGE, SWP_FRAMECHANGED);
			pChannel->m_wndTopic.SendMessage(EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(3, 3));
			pChannel->m_wndTopic.SetEventMask(pChannel->m_wndTopic.GetEventMask() | ENM_LINK);
			pChannel->m_wndTopic.SetAutoScroll(false);
			pChannel->m_wndTopic.SetFont(&theApp.m_fontHyperText);
			pChannel->m_wndTopic.SetTitle(sChannel);
			pChannel->m_wndTopic.SetProfileSkinKey(_T("IRCChannel"));
			// The idea is to show the channel title with black background, thus giving the
			// user a chance to read easier the colorful strings which are often created
			// to be read against a black or at least dark background. Though, there is one
			// problem. If the user has customized the system default selection color to
			// black, he would not be able to read any highlighted URLs against a black
			// background any longer because the RE control is using that very same color for
			// hyperlinks. So, we select a black background only if the user is already using
			// the default windows background (white), because it is very unlikely that such
			// a user would have customized also the selection color to white.
			if (::GetSysColor(COLOR_WINDOW) == RGB(255, 255, 255)) {
				pChannel->m_wndTopic.SetDfltForegroundColor(RGB(255, 255, 255));
				pChannel->m_wndTopic.SetDfltBackgroundColor(RGB(0, 0, 0));
			}
			pChannel->m_wndTopic.ApplySkin();
			pChannel->m_wndTopic.EnableSmileys(thePrefs.GetIRCEnableSmileys());

#ifdef DEBUG_IRC_TEXT
			if (eType == Channel::ctStatus)
				pChannel->m_wndTitle.AddLine(_T("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ^°1234567890!\"§$%&/()=?´`²³{[]}ß\\öäüÖÄÜ+*~#',.-;:_"));
#endif

			RECT rcSplitter = { rcChannelPane.left, rcTopic.bottom, rcChannelPane.right, rcTopic.bottom + IRC_CHANNEL_SPLITTER_HEIGHT };
			pChannel->m_wndSplitter.Create(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, rcSplitter, m_pParent, IDC_SPLITTER_IRC_CHANNEL);
			pChannel->m_wndSplitter.SetRange(rcTopic.top + IRC_TITLE_WND_MIN_HEIGHT + IRC_CHANNEL_SPLITTER_HEIGHT / 2, rcTopic.top + IRC_TITLE_WND_MAX_HEIGHT - IRC_CHANNEL_SPLITTER_HEIGHT / 2);
			rcLog.top = rcSplitter.bottom;
			rcLog.bottom = rcChannelPane.bottom;
		}

		pChannel->m_wndLog.Create(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_NOHIDESEL, rcLog, m_pParent, UINT_MAX);
		pChannel->m_wndLog.ModifyStyleEx(0, WS_EX_STATICEDGE, SWP_FRAMECHANGED);
		pChannel->m_wndLog.SendMessage(EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(3, 3));
		pChannel->m_wndLog.SetEventMask(pChannel->m_wndLog.GetEventMask() | ENM_LINK);
		pChannel->m_wndLog.SetFont(&theApp.m_fontHyperText);
		pChannel->m_wndLog.SetTitle(sChannel);
		pChannel->m_wndLog.SetProfileSkinKey(_T("IRCChannel"));
		pChannel->m_wndLog.ApplySkin();
		pChannel->m_wndLog.EnableSmileys(thePrefs.GetIRCEnableSmileys());

		if (eType == Channel::ctNormal || eType == Channel::ctPrivate) {
			PARAFORMAT pf = {};
			pf.cbSize = (UINT)sizeof pf;
			pf.dwMask = PFM_OFFSET;
			pf.dxOffset = 150;
			pChannel->m_wndLog.SetParaFormat(pf);
		}

#ifdef DEBUG_IRC_TEXT
		if (eType == Channel::ctStatus) {
			//pChannel->m_wndLog.AddLine(_T(":) debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug debug"));
			m_pParent->AddColorLine(_T("normal\002bold\002normal\r\n"), pChannel->m_wndLog);
			m_pParent->AddColorLine(_T("\0034red\002bold\002red\r\n"), pChannel->m_wndLog);
			m_pParent->AddColorLine(_T("normal\r\n"), pChannel->m_wndLog);
			m_pParent->AddColorLine(_T("\0032,4red\002bold\002red\r\n"), pChannel->m_wndLog);
			m_pParent->AddColorLine(_T("\017normal\r\n"), pChannel->m_wndLog);

			LPCWSTR log = L"C:\\Program\\mIRC 2\\channels\\MindForge_Sorted_X.txt";
			FILE *fp = _wfopen(log, L"rt");
			if (fp) {
				int i = 0;
				int iMax = 10000;
				TCHAR szLine[1024];
				while (i++ < iMax && fgetws(szLine, _countof(szLine), fp)) {
					size_t nLen = wcslen(szLine);
					if (nLen >= 1 && szLine[nLen - 1] == L'\n')
						--nLen;
					szLine[nLen++] = L'\r';
					szLine[nLen++] = L'\n';
					//TRACE(_T("%u: %s\n"), i, szLine);
					CString strLine(szLine, nLen);
					m_pParent->AddColorLine(strLine, pChannel->m_wndLog);
				}
				fclose(fp);
			}
		}
#endif
	}
	m_lstChannels.AddTail(pChannel);

	TCITEM newitem;
	newitem.mask = TCIF_PARAM | TCIF_TEXT | TCIF_IMAGE;
	newitem.lParam = (LPARAM)pChannel;
	CString strTcLabel(sChannel);
	strTcLabel.Replace(_T("&"), _T("&&"));
	newitem.pszText = const_cast<LPTSTR>((LPCTSTR)strTcLabel);
	if (eType == Channel::ctStatus)
		newitem.iImage = 0;
	else if (eType == Channel::ctChannelList)
		newitem.iImage = 1;
	else //Channel::ctNormal or Channel::ctPrivate
		newitem.iImage = 2;
	int iItem = InsertItem((eType == Channel::ctChannelList ? 1 : GetItemCount()), &newitem); //append
	if (eType == Channel::ctNormal)
		SelectChannel(iItem);
	return pChannel;
}

void CIrcChannelTabCtrl::DetachChannel(Channel *pChannel)
{
	if (!pChannel || pChannel->m_bDetached)
		return;
	ASSERT(pChannel != m_pChanStatus);
	pChannel->m_bDetached = true;
	m_pParent->m_wndNicks.DeleteAllNick(pChannel);
	if (pChannel == m_pCurrentChannel)
		m_pParent->m_wndNicks.RefreshNickList(m_pCurrentChannel);
	int iIndex = FindTabIndex(pChannel);
	if (iIndex >= 0) {
		pChannel->m_sTitle = _T('(') + pChannel->m_sTitle + _T(')');
		TCITEM item;
		item.mask = TCIF_TEXT;
		item.pszText = const_cast<LPTSTR>((LPCTSTR)pChannel->m_sTitle);
		SetItem(iIndex, &item);
	}
}

void CIrcChannelTabCtrl::DetachChannel(const CString &sChannel)
{
	DetachChannel(FindChannelByName(sChannel));
}

int CIrcChannelTabCtrl::FindTabIndex(const Channel *pChannel)
{
	TCITEM item;
	item.mask = TCIF_PARAM;
	item.lParam = -1;
	for (int iIndex = GetItemCount(); --iIndex >= 0;)
		if (GetItem(iIndex, &item) && reinterpret_cast<Channel*>(item.lParam) == pChannel)
			return iIndex;

	return -1;
}

void CIrcChannelTabCtrl::SelectChannel(int iItem)
{
	ASSERT(iItem >= 0 && iItem < GetItemCount());
	SetCurSel(iItem);
	SetCurFocus(iItem);
	OnTcnSelChange(NULL, NULL);
}

void CIrcChannelTabCtrl::SelectChannel(const Channel *pChannel)
{
	int iItem = FindTabIndex(pChannel);
	if (iItem >= 0)
		SelectChannel(iItem);
}

void CIrcChannelTabCtrl::RemoveChannel(Channel *pChannel)
{
	if (!pChannel)
		return;
	ASSERT(pChannel != m_pChanStatus);
	int iIndex = FindTabIndex(pChannel);
	if (iIndex < 0)
		return;
	DeleteItem(iIndex);
	if (pChannel->m_eType == Channel::ctChannelList) {
		m_pParent->m_wndChanList.ResetServerChannelList();
		m_pChanList = NULL;
	}
	if (pChannel == m_pCurrentChannel) {
		m_pParent->m_wndNicks.DeleteAllItems();
		//try to keep the current tab index
		int iItems = GetItemCount();
		ASSERT(iItems > 0);
		if (iIndex >= iItems)
			iIndex = iItems - 1; //the last available tab
		SelectChannel(iIndex);
	}
	m_lstChannels.RemoveAt(m_lstChannels.Find(pChannel));
	m_pParent->m_wndNicks.DeleteAllNick(pChannel);
	delete pChannel;
}

void CIrcChannelTabCtrl::RemoveChannel(const CString &sChannel)
{
	RemoveChannel(FindChannelByName(sChannel));
}

void CIrcChannelTabCtrl::DeleteAllChannels()
{
	while (!m_lstChannels.IsEmpty()) {
		Channel *pCurChannel = m_lstChannels.RemoveHead();
		m_pParent->m_wndNicks.DeleteAllNick(pCurChannel);
		delete pCurChannel;
	}
	m_pChanStatus = NULL;
	m_pChanList = NULL;
}

bool CIrcChannelTabCtrl::ChangeChanMode(const CString &sChannel, const CString &/*sParam*/, const TCHAR cDir, const TCHAR cCommand)
{
	//Must have a command and a channel.
	if (sChannel[0] != _T('#') || !cCommand)
		return false;

	//Make sure this channel is in our list.
	Channel *pUpdate = FindChannelByName(sChannel);
	if (!pUpdate)
		return false;

	//Update modes.
	return CIrcWnd::UpdateModes(m_sChannelModeSettingsTypeA, pUpdate->m_sModesA, cDir, cCommand)
		|| CIrcWnd::UpdateModes(m_sChannelModeSettingsTypeB, pUpdate->m_sModesB, cDir, cCommand)
		|| CIrcWnd::UpdateModes(m_sChannelModeSettingsTypeC, pUpdate->m_sModesC, cDir, cCommand)
		|| CIrcWnd::UpdateModes(m_sChannelModeSettingsTypeD, pUpdate->m_sModesD, cDir, cCommand);
}

void CIrcChannelTabCtrl::OnTcnSelChange(LPNMHDR, LRESULT *pResult)
{
	//What channel did we select?
	int iCurSel = GetCurSel();
	if (iCurSel == -1)
		return;	//No channel, abort.

	TCITEM item;
	item.mask = TCIF_PARAM;
	if (!GetItem(iCurSel, &item)) {
		//We had no valid item here. Something isn't right.
		//TODO: this should never happen, so maybe we should remove this tab?
		return;
	}

	CString sSend;
	m_pParent->m_wndInput.GetWindowText(sSend);
	if (sSend != m_pCurrentChannel->m_sTabd) { //string has changed
		m_pCurrentChannel->m_sTyped = sSend;
		m_pCurrentChannel->m_sTabd.Empty(); //restart autocompletion
	}

	//Set our current channel to the new one for quick reference.
	m_pCurrentChannel = reinterpret_cast<Channel*>(item.lParam);

	//We entered the channel, set activity flag off.
	SetActivity(m_pCurrentChannel, false);
	m_pParent->GetDlgItem(IDC_CLOSECHAT)->EnableWindow(m_pCurrentChannel->m_eType != Channel::ctStatus);

	if (m_pCurrentChannel->m_eType == Channel::ctChannelList) {
		//Since some channels can have a LOT of nicks, hide the window then remove them to speed it up.
		m_pParent->m_wndNicks.ShowWindow(SW_HIDE);
		m_pParent->m_wndNicks.DeleteAllItems();
		m_pParent->m_wndNicks.UpdateNickCount();
		m_pParent->m_wndNicks.ShowWindow(SW_SHOW);
		//Show our ChanList.
		m_pParent->m_wndChanList.ShowWindow(SW_SHOW);
		TCITEM tci;
		tci.mask = TCIF_PARAM;
		//Go through the channel tabs and hide the channels.
		//Maybe overkill? Maybe just remember our previous channel and hide it?

		for (int iIndex = 0; GetItem(iIndex++, &tci);) {
			Channel *pCh2 = reinterpret_cast<Channel*>(tci.lParam);
			if (pCh2 != m_pCurrentChannel && pCh2->m_wndLog.m_hWnd != NULL)
				pCh2->Hide();
		}
		SetInput(m_pCurrentChannel->m_sTabd.IsEmpty() ? m_pCurrentChannel->m_sTyped : m_pCurrentChannel->m_sTabd);
		return;
	}

	//Show new current channel.
	m_pCurrentChannel->Show();
	m_pParent->UpdateChannelChildWindowsSize();

	//Hide all channels not in focus.
	//Maybe an overkill? Maybe remember previous channel and hide?
	TCITEM tci;
	tci.mask = TCIF_PARAM;
	int iIndex = 0;
	while (GetItem(iIndex++, &tci)) {
		Channel *pCh2 = reinterpret_cast<Channel*>(tci.lParam);
		if (pCh2 != m_pCurrentChannel && pCh2->m_wndLog.m_hWnd != NULL)
			pCh2->Hide();
	}

	//Make sure channelList is hidden.
	m_pParent->m_wndChanList.ShowWindow(SW_HIDE);
	//Update nicklist to the new channel.
	m_pParent->m_wndNicks.RefreshNickList(m_pCurrentChannel);
	//Push focus back to the input box.
	SetInput(m_pCurrentChannel->m_sTabd.IsEmpty() ? m_pCurrentChannel->m_sTyped : m_pCurrentChannel->m_sTabd);
	if (pResult)
		*pResult = 0;
}

void CIrcChannelTabCtrl::ScrollHistory(bool bDown)
{
	if ((m_pCurrentChannel->m_iHistoryPos <= 0 && !bDown) || (m_pCurrentChannel->m_iHistoryPos >= m_pCurrentChannel->m_astrHistory.GetCount() && bDown))
		return;

	if (bDown)
		++m_pCurrentChannel->m_iHistoryPos;
	else
		--m_pCurrentChannel->m_iHistoryPos;
	m_pCurrentChannel->m_sTabd.Empty(); //reset autocompletion
	m_pCurrentChannel->m_sTyped = (m_pCurrentChannel->m_iHistoryPos >= m_pCurrentChannel->m_astrHistory.GetCount()) ? CString() : m_pCurrentChannel->m_astrHistory[m_pCurrentChannel->m_iHistoryPos];
	SetInput(m_pCurrentChannel->m_sTyped);
}

void CIrcChannelTabCtrl::SetActivity(Channel *pChannel, bool bFlag)
{
	if (bFlag && pChannel == m_pCurrentChannel)
		return;
	if (!pChannel) {
		pChannel = m_lstChannels.GetHead();
		if (!pChannel)
			return;
	}

	int iIndex = FindTabIndex(pChannel);
	if (iIndex < 0)
		return;

	if (pChannel->m_eType == Channel::ctNormal || pChannel->m_eType == Channel::ctPrivate) {
		TCITEM item;
		item.mask = TCIF_IMAGE;
		if (!GetItem(iIndex, &item))
			return;
		if (bFlag) {
			if (item.iImage != 3) {
				item.iImage = 3; // 'MessagePending'
				SetItem(iIndex, &item);
			}
		} else {
			if (item.iImage != 2) {
				item.iImage = 2; // 'Message'
				SetItem(iIndex, &item);
			}
		}
	}
	HighlightItem(iIndex, static_cast<BOOL>(bFlag));
}

void CIrcChannelTabCtrl::ChatSend(CString sSend)
{
	if (sSend.IsEmpty() || !m_pParent->IsConnected())
		return;
	m_pCurrentChannel->m_sTabd.Empty();
	m_pCurrentChannel->m_sTyped.Empty();
	if (m_pCurrentChannel->m_astrHistory.GetCount() == thePrefs.GetMaxChatHistoryLines())
		m_pCurrentChannel->m_astrHistory.RemoveAt(0);
	m_pCurrentChannel->m_astrHistory.Add(sSend);
	m_pCurrentChannel->m_iHistoryPos = (int)m_pCurrentChannel->m_astrHistory.GetCount();

	if (sSend.Left(1) == _T("/")) {
		if (sSend.Left(4).CompareNoCase(_T("/hop")) == 0) {
			if (m_pCurrentChannel->m_eType == Channel::ctNormal) {
				m_pParent->m_pIrcMain->SendString(_T("PART ") + m_pCurrentChannel->m_sName);
				m_pParent->m_pIrcMain->SendString(_T("JOIN ") + m_pCurrentChannel->m_sName);
			}
			return;
		}

		if (sSend.Left(3).CompareNoCase(_T("/me")) != 0 && sSend.Left(6).CompareNoCase(_T("/sound")) != 0) {
			if (sSend.Left(4).CompareNoCase(_T("/msg")) == 0) {
				int i = 5;
				const CString &cs(sSend.Tokenize(_T(" "), i));
				if (m_pCurrentChannel->m_eType < Channel::ctNormal || m_pCurrentChannel->m_bDetached)
					m_pParent->AddStatusF(_T(" -> *%s* %s"), (LPCTSTR)cs, (LPCTSTR)sSend.Mid(i));
				else
					m_pParent->AddInfoMessageF(m_pCurrentChannel->m_sName, _T(" -> *%s* %s"), (LPCTSTR)cs, (LPCTSTR)sSend.Mid(i));
				sSend = _T("/PRIVMSG") + sSend.Mid(4);
			} else if (sSend.Left(7).CompareNoCase(_T("/notice")) == 0) {
				int i = 8;
				const CString &cs(sSend.Tokenize(_T(" "), i));
				if (m_pCurrentChannel->m_eType < Channel::ctNormal || m_pCurrentChannel->m_bDetached)
					m_pParent->AddStatusF(_T(" -> *%s* %s"), (LPCTSTR)cs, (LPCTSTR)sSend.Mid(i));
				else
					m_pParent->AddInfoMessageF(m_pCurrentChannel->m_sName, _T(" -> *%s* %s"), (LPCTSTR)cs, (LPCTSTR)sSend.Mid(i));
			}

			if (sSend.Left(17).CompareNoCase(_T("/PRIVMSG nickserv")) == 0)
				sSend.Format(_T("/ns%s"), (LPCTSTR)sSend.Mid(17));
			else if (sSend.Left(17).CompareNoCase(_T("/PRIVMSG chanserv")) == 0)
				sSend.Format(_T("/cs%s"), (LPCTSTR)sSend.Mid(17));
			else if (sSend.Left(5).CompareNoCase(_T("/part")) == 0) {
				if (sSend.TrimRight().GetLength() == 5 && m_pCurrentChannel->m_eType == Channel::ctNormal)
					sSend.AppendFormat(_T(" %s"), (LPCTSTR)m_pCurrentChannel->m_sName);
			} else if (sSend.Left(8).CompareNoCase(_T("/PRIVMSG")) == 0) {
				int iIndex = sSend.Find(_T(' '), sSend.Find(_T(' ')) + 1);
				sSend.Insert(iIndex + 1, _T(":"));
			} else if (sSend.Left(6).CompareNoCase(_T("/TOPIC")) == 0) {
				int iIndex = sSend.Find(_T(' '), sSend.Find(_T(' ')) + 1);
				sSend.Insert(iIndex + 1, _T(":"));
			}
			m_pParent->m_pIrcMain->SendString(sSend.Mid(1));
			return;
		}
	}

	if (m_pCurrentChannel->m_eType < Channel::ctNormal || m_pCurrentChannel->m_bDetached) {
		m_pParent->m_pIrcMain->SendString(sSend);
		return;
	}

	CString sBuild;
	if (sSend.Left(3).CompareNoCase(_T("/me")) == 0) {
		sBuild.Format(_T("PRIVMSG %s :\001ACTION %s\001"), (LPCTSTR)m_pCurrentChannel->m_sName, (LPCTSTR)sSend.Mid(4));
		m_pParent->AddInfoMessageCF(m_pCurrentChannel->m_sName, RGB(156, 0, 156), _T("* %s %s"), (LPCTSTR)m_pParent->m_pIrcMain->GetNick(), (LPCTSTR)sSend.Mid(4));
		m_pParent->m_pIrcMain->SendString(sBuild);
		return;
	}

	if (sSend.Left(6).CompareNoCase(_T("/sound")) == 0) {
		CString sound;
		sBuild.Format(_T("PRIVMSG %s :\001SOUND %s\001"), (LPCTSTR)m_pCurrentChannel->m_sName, (LPCTSTR)sSend.Mid(7));
		m_pParent->m_pIrcMain->SendString(sBuild);
		sSend.Delete(0, 7);
		int soundlen = sSend.Find(_T(' '));
		if (soundlen >= 0) {
			sSend.Truncate(soundlen);
			sBuild = sSend;
			sBuild.Remove(_T('\\'));
		} else {
			sBuild = sSend;
			sSend = _T("[SOUND]");
		}
		sound.Format(_T("%sSounds\\IRC\\%s"), (LPCTSTR)thePrefs.GetMuleDirectory(EMULE_EXECUTABLEDIR), (LPCTSTR)sBuild);
		m_pParent->AddInfoMessageF(m_pCurrentChannel->m_sName, _T("* %s %s"), (LPCTSTR)m_pParent->m_pIrcMain->GetNick(), (LPCTSTR)sSend);
		PlaySound(sound, NULL, SND_FILENAME | SND_NOSTOP | SND_NOWAIT | SND_ASYNC);
		return;
	}
	sBuild.Format(_T("PRIVMSG %s :%s"), (LPCTSTR)m_pCurrentChannel->m_sName, (LPCTSTR)sSend);
	m_pParent->m_pIrcMain->SendString(sBuild);

	m_pParent->AddMessageF(m_pCurrentChannel->m_sName, m_pParent->m_pIrcMain->GetNick(), _T("%s"), (LPCTSTR)sSend);
}

void CIrcChannelTabCtrl::Localize()
{
	TCITEM item;
	item.mask = TCIF_PARAM;
	for (int iIndex = GetItemCount(); --iIndex >= 0;) {
		if (GetItem(iIndex, &item)) {
			Channel *pChannel = reinterpret_cast<Channel*>(item.lParam);
			if (pChannel && (pChannel->m_eType == Channel::ctStatus || pChannel->m_eType == Channel::ctChannelList)) {
				pChannel->m_sTitle = GetResString(pChannel->m_eType == Channel::ctStatus ? IDS_STATUS : IDS_IRC_CHANNELLIST);
				item.mask = TCIF_TEXT;
				CString strTcLabel(pChannel->m_sTitle);
				strTcLabel.Replace(_T("&"), _T("&&"));
				item.pszText = const_cast<LPTSTR>((LPCTSTR)strTcLabel);
				SetItem(iIndex, &item);
			}
		}
	}
	SetAllIcons();
}

LRESULT CIrcChannelTabCtrl::OnCloseTab(WPARAM wParam, LPARAM)
{
	m_pParent->OnBnClickedCloseChannel((int)wParam);
	return 1;
}

LRESULT CIrcChannelTabCtrl::OnQueryTab(WPARAM wParam, LPARAM)
{
	TCITEM item;
	item.mask = TCIF_PARAM;
	if (GetItem((int)wParam, &item)) {
		const Channel *pPartChannel = reinterpret_cast<Channel*>(item.lParam);
		if (pPartChannel && pPartChannel->m_eType >= Channel::ctChannelList)
			return 0;
	}
	return 1;
}

void CIrcChannelTabCtrl::EnableSmileys(bool bEnable)
{
	for (POSITION pos = m_lstChannels.GetHeadPosition(); pos != NULL;) {
		Channel *pChannel = m_lstChannels.GetNext(pos);
		if (pChannel->m_eType != Channel::ctChannelList) {
			pChannel->m_wndLog.EnableSmileys(bEnable);
			if (pChannel->m_eType == Channel::ctNormal)
				pChannel->m_wndTopic.EnableSmileys(bEnable);
		}
	}
}

void CIrcChannelTabCtrl::SetInput(const CString &rStr)
{
	m_pParent->m_wndInput.SetWindowText(rStr);
	m_pParent->m_wndInput.SetFocus();
	m_pParent->m_wndInput.SetSel(_UI32_MAX);
}

void Channel::Show()
{
	if (m_wndTopic.m_hWnd) {
		m_wndTopic.EnableWindow(TRUE);
		m_wndTopic.ShowWindow(SW_SHOW);
	}

	if (m_wndSplitter.m_hWnd) {
		m_wndSplitter.EnableWindow(TRUE);
		m_wndSplitter.ShowWindow(SW_SHOW);
	}

	if (m_wndLog.m_hWnd) {
		m_wndLog.EnableWindow(TRUE);
		m_wndLog.ShowWindow(SW_SHOW);
	}
}

void Channel::Hide()
{
	if (m_wndTopic.m_hWnd) {
		m_wndTopic.ShowWindow(SW_HIDE);
		m_wndTopic .EnableWindow(FALSE);
	}

	if (m_wndSplitter.m_hWnd) {
		m_wndSplitter.ShowWindow(SW_HIDE);
		m_wndSplitter.EnableWindow(FALSE);
	}

	if (m_wndLog.m_hWnd) {
		m_wndLog.ShowWindow(SW_HIDE);
		m_wndLog.EnableWindow(FALSE);
	}
}