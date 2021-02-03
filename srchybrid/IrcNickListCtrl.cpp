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
#include "IrcNickListCtrl.h"
#include "otherfunctions.h"
#include "IrcWnd.h"
#include "IrcMain.h"
#include "MenuCmds.h"
#include "HTRichEditCtrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNAMIC(CIrcNickListCtrl, CMuleListCtrl)

BEGIN_MESSAGE_MAP(CIrcNickListCtrl, CMuleListCtrl)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnLvnColumnClick)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnNmDblClk)
	ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()

CIrcNickListCtrl::CIrcNickListCtrl()
{
	m_pParent = NULL;
	SetSkinKey(_T("IRCNicksLv"));
}

void CIrcNickListCtrl::Init()
{
	SetPrefsKey(_T("IrcNickListCtrl"));
	SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	InsertColumn(0, GetResString(IDS_IRC_NICK), LVCFMT_LEFT, 90);

	LoadSettings();
	SetSortArrow();
	SortItems(&SortProc, GetSortItem() + (GetSortAscending() ? 0 : 10));
}

int CALLBACK CIrcNickListCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	LPARAM iColumn = lParamSort >= 10 ? lParamSort - 10 : lParamSort;
	if (iColumn)
		return 0;
	const Nick *pItem1 = reinterpret_cast<Nick*>(lParam1);
	const Nick *pItem2 = reinterpret_cast<Nick*>(lParam2);
	if (pItem1->m_iLevel != pItem2->m_iLevel) {
		if (pItem1->m_iLevel == -1)
			return 1;
		if (pItem2->m_iLevel == -1)
			return -1;
		return pItem1->m_iLevel - pItem2->m_iLevel;
	}
	int iResult = pItem1->m_sNick.CompareNoCase(pItem2->m_sNick);
	return (lParamSort >= 10) ? -iResult : iResult;
}

void CIrcNickListCtrl::OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	const LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

	bool bSortAscending = (GetSortItem() != pNMLV->iSubItem) ? true : !GetSortAscending();
	SetSortArrow(pNMLV->iSubItem, bSortAscending);
	SortItems(&SortProc, pNMLV->iSubItem + (bSortAscending ? 0 : 10));

	*pResult = 0;
}

void CIrcNickListCtrl::OnContextMenu(CWnd*, CPoint point)
{
	int iCurSel = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	if (iCurSel == -1)
		return;
	const Nick *pNick = reinterpret_cast<Nick*>(GetItemData(iCurSel));
	if (!pNick)
		return;

	CTitleMenu menuNick;
	menuNick.CreatePopupMenu();
	CString sTitle(GetResString(IDS_IRC_NICK));
	sTitle.AppendFormat(_T(" : %s"), (LPCTSTR)pNick->m_sNick);
	menuNick.AddMenuTitle(sTitle);
	menuNick.AppendMenu(MF_STRING, Irc_Priv, GetResString(IDS_IRC_PRIVMESSAGE));
	menuNick.AppendMenu(MF_STRING, Irc_AddFriend, GetResString(IDS_IRC_ADDTOFRIENDLIST));
	menuNick.AppendMenu(MF_STRING, Irc_SendLink, GetResString(IDS_IRC_SENDLINK)
		+ (m_pParent->GetSendFileString().IsEmpty() ? GetResString(IDS_IRC_NOSFS) : m_pParent->GetSendFileString()));
	menuNick.AppendMenu(MF_STRING, Irc_Kick, GetResString(IDS_IRC_KICK));
	menuNick.AppendMenu(MF_STRING, Irc_Ban, GetResString(IDS_IRC_BAN));
	//Ban currently uses chanserv to ban which seems to kick also. May change this later.
	//	menuNick.AppendMenu(MF_STRING, Irc_KB, _T("Kick/Ban"));
	menuNick.AppendMenu(MF_STRING, Irc_Slap, GetResString(IDS_IRC_SLAP));
	menuNick.SetDefaultItem(Irc_Priv);
	GetPopupMenuPos(*this, point);
	menuNick.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
	VERIFY(menuNick.DestroyMenu());
}

void CIrcNickListCtrl::OpenPrivateChannel(const Nick *pNick)
{
	if (!pNick)
		return;
	const CString &sNick = pNick->m_sNick;
	if (sNick.CompareNoCase(m_pParent->m_pIrcMain->GetNick()) == 0 && !m_pParent->m_wndChanSel.FindChannelByName(sNick))
		m_pParent->m_wndChanSel.NewChannel(sNick, Channel::ctPrivate);
	m_pParent->AddInfoMessage(sNick, GetResString(IDS_IRC_PRIVATECHANSTART), true);
}

void CIrcNickListCtrl::OnNmDblClk(LPNMHDR, LRESULT *pResult)
{
	//We double clicked a nick. Try to open a private channel
	int iSel = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	if (iSel >= 0)
		OpenPrivateChannel(reinterpret_cast<const Nick*>(GetItemData(iSel)));
	*pResult = 0;
}

Nick* CIrcNickListCtrl::FindNickByName(const CString &sChannel, const CString &sName)
{
	const Channel *pChannel = m_pParent->m_wndChanSel.FindChannelByName(sChannel);
	return pChannel ? FindNickByName(pChannel, sName) : NULL;
}

Nick* CIrcNickListCtrl::FindNickByName(const Channel *pChannel, const CString &sName)
{
	for (POSITION pos = pChannel->m_lstNicks.GetHeadPosition(); pos != NULL;) {
		Nick *pNick = pChannel->m_lstNicks.GetNext(pos);
		if (pNick->m_sNick.CompareNoCase(sName) == 0)
			return pNick;
	}
	return NULL;
}

Nick* CIrcNickListCtrl::NewNick(const CString &sChannel, const CString &sNick)
{
	Channel *pChannel = m_pParent->m_wndChanSel.FindChannelByName(sChannel);
	if (!pChannel)
		return NULL;
	//This is a little clumsy and makes you think the previous check wasn't needed,
	//But we need the channel object and FindNickByName doesn't do it.
	if (FindNickByName(pChannel, sNick))
		return NULL;

	Nick *pNick = new Nick;

	//Remove all modes from the front of this nick
	pNick->m_sNick = sNick;
	while (pNick->m_sNick.GetLength() >= 1 && m_sUserModeSymbols.Find(pNick->m_sNick[0]) >= 0) {
		pNick->m_sSymbols += pNick->m_sNick[0];
		pNick->m_sNick.Delete(0, 1);
	}

	//Set user level
	pNick->m_iLevel = pNick->m_sSymbols.IsEmpty() ? -1 : m_sUserModeSymbols.Find(pNick->m_sSymbols[0]);

	//Add new nick to channel.
	pChannel->m_lstNicks.AddTail(pNick);
	if (pChannel == m_pParent->m_wndChanSel.m_pCurrentChannel)
		//This is our current channel, add it to our nick list.
		if (InsertItem(LVIF_TEXT | LVIF_PARAM, GetItemCount(), pNick->m_sSymbols + pNick->m_sNick, 0, 0, 0, (LPARAM)pNick) >= 0)
			UpdateNickCount();
	return pNick;
}

void CIrcNickListCtrl::RefreshNickList(const Channel *pChannel)
{
	//Hide nickList to speed things up.
	ShowWindow(SW_HIDE);
	DeleteAllItems();
	if (pChannel)
		for (POSITION pos = pChannel->m_lstNicks.GetHeadPosition(); pos != NULL;) {
			const Nick *pNick = pChannel->m_lstNicks.GetNext(pos);
			if (InsertItem(LVIF_TEXT | LVIF_PARAM, GetItemCount(), pNick->m_sSymbols + pNick->m_sNick, 0, 0, 0, (LPARAM)pNick) < 0)
				break;
		}
	UpdateNickCount();
	ShowWindow(SW_SHOW);
}

void CIrcNickListCtrl::RefreshNickList(const CString &sChannel)
{
	RefreshNickList(m_pParent->m_wndChanSel.FindChannelByName(sChannel));
}

bool CIrcNickListCtrl::RemoveNick(const CString &sChannel, const CString &sNick)
{
	Channel *pChannel = m_pParent->m_wndChanSel.FindChannelByName(sChannel);
	if (!pChannel)
		return false;

	for (POSITION pos = pChannel->m_lstNicks.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		Nick *pNick = pChannel->m_lstNicks.GetNext(pos);
		if (pNick->m_sNick == sNick) {
			if (pChannel == m_pParent->m_wndChanSel.m_pCurrentChannel) {
				//If it's our current channel, delete the nick from nickList
				LVFINDINFO find;
				find.flags = LVFI_PARAM;
				find.lParam = (LPARAM)pNick;
				int iItem = FindItem(&find);
				if (iItem >= 0) {
					DeleteItem(iItem);
					UpdateNickCount();
				}
			}
			pChannel->m_lstNicks.RemoveAt(pos2);
			delete pNick;
			return true;
		}
	}
	return false;
}

void CIrcNickListCtrl::DeleteAllNick(Channel *pChannel)
{
	if (pChannel)
		while (!pChannel->m_lstNicks.IsEmpty())
			delete pChannel->m_lstNicks.RemoveHead();
}

void CIrcNickListCtrl::DeleteNickInAll(const CString &sNick, const CString &sMessage)
{
	for (POSITION pos = m_pParent->m_wndChanSel.m_lstChannels.GetHeadPosition(); pos != NULL;) {
		Channel *pChannel = m_pParent->m_wndChanSel.m_lstChannels.GetNext(pos);
		if (RemoveNick(pChannel->m_sName, sNick)) {
			if (!thePrefs.GetIRCIgnoreQuitMessages())
				m_pParent->AddInfoMessageCF(pChannel->m_sName, RGB(0, 0, 127), GetResString(IDS_IRC_HASQUIT), (LPCTSTR)sNick, (LPCTSTR)sMessage);
		}
	}
}

bool CIrcNickListCtrl::ChangeNick(const CString &sChannel, const CString &sOldNick, const CString &sNewNick)
{
	Channel *pChannel = m_pParent->m_wndChanSel.FindChannelByName(sChannel);
	if (!pChannel)
		return false;

	for (POSITION pos = pChannel->m_lstNicks.GetHeadPosition(); pos != NULL;) {
		Nick *pNick = pChannel->m_lstNicks.GetNext(pos);
		if (pNick->m_sNick == sOldNick) {
			if (pChannel == m_pParent->m_wndChanSel.m_pCurrentChannel) {
				//This channel is in focus, update nick in nickList
				LVFINDINFO find;
				find.flags = LVFI_PARAM;
				find.lParam = (LPARAM)pNick;
				int iItem = FindItem(&find);
				if (iItem >= 0)
					SetItemText(iItem, 0, pNick->m_sSymbols + sNewNick);
			}
			pNick->m_sNick = sNewNick;
			return true;
		}
	}
	return false;
}

bool CIrcNickListCtrl::ChangeNickMode(const CString &sChannel, const CString &sNick, const TCHAR cDir, const TCHAR &cMode)
{
	if (sChannel[0] != _T('#') || sNick.IsEmpty())
		return true;
	Channel *pChannel = m_pParent->m_wndChanSel.FindChannelByName(sChannel);
	if (!pChannel)
		return false;

	for (POSITION pos = pChannel->m_lstNicks.GetHeadPosition(); pos != NULL;) {
		Nick *pNick = pChannel->m_lstNicks.GetNext(pos);
		if (pNick->m_sNick == sNick) {
			int iModeLevel = m_sUserModeSettings.Find(cMode);
			if (iModeLevel >= 0)
				CIrcWnd::UpdateModes(m_sUserModeSymbols, pNick->m_sSymbols, cDir, m_sUserModeSymbols[iModeLevel]);
			else {
				//This should never happen
				pNick->m_sSymbols.Empty();
				ASSERT(0);
			}

			//Update user level
			pNick->m_iLevel = pNick->m_sSymbols.IsEmpty() ? -1 : m_sUserModeSymbols.Find(pNick->m_sSymbols[0]);

			if (pChannel == m_pParent->m_wndChanSel.m_pCurrentChannel) {
				//Channel was in focus, update the nickList.
				LVFINDINFO find;
				find.flags = LVFI_PARAM;
				find.lParam = (LPARAM)pNick;
				int iItem = FindItem(&find);
				if (iItem >= 0)
					SetItemText(iItem, 0, pNick->m_sSymbols + pNick->m_sNick);
			}
			return true;
		}
	}

	//Nick was not found in list??
	return false;
}

bool CIrcNickListCtrl::ChangeAllNick(const CString &sOldNick, const CString &sNewNick)
{
	bool bChanged = false;
	//Change the nick in ALL channels.
	Channel *pChannel = m_pParent->m_wndChanSel.FindChannelByName(sOldNick);
	if (pChannel) {
		//We had a private room opened with this nick. Update the title of the channel!
		pChannel->m_sName = sNewNick;

		TCITEM item;
		item.mask = TCIF_PARAM;
		item.lParam = -1;
		for (int iItem = 0; iItem < m_pParent->m_wndChanSel.GetItemCount(); ++iItem) {
			m_pParent->m_wndChanSel.GetItem(iItem, &item);
			if (reinterpret_cast<Channel*>(item.lParam) == pChannel) {
				item.mask = TCIF_TEXT;
				CString strTcLabel(sNewNick);
				strTcLabel.Replace(_T("&"), _T("&&"));
				item.pszText = const_cast<LPTSTR>((LPCTSTR)strTcLabel);
				m_pParent->m_wndChanSel.SetItem(iItem, &item);
				bChanged = true;
				break;
			}
		}
	}

	// Go through all other channel nicklists.
	for (POSITION pos = m_pParent->m_wndChanSel.m_lstChannels.GetHeadPosition(); pos != NULL;) {
		pChannel = m_pParent->m_wndChanSel.m_lstChannels.GetNext(pos);
		if (ChangeNick(pChannel->m_sName, sOldNick, sNewNick)) {
			if (!thePrefs.GetIRCIgnoreMiscMessages())
				m_pParent->AddInfoMessageF(pChannel->m_sName, GetResString(IDS_IRC_NOWKNOWNAS), (LPCTSTR)sOldNick, (LPCTSTR)sNewNick);
			bChanged = true;
		}
	}
	return bChanged;
}

void CIrcNickListCtrl::UpdateNickCount()
{
	CString strRes(GetResString(IDS_IRC_NICK));
	int iItemCount = GetItemCount();
	if (iItemCount)
		strRes.AppendFormat(_T(" (%d)"), iItemCount);
	HDITEM hdi;
	hdi.mask = HDI_TEXT;
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	GetHeaderCtrl()->SetItem(0, &hdi);
}

void CIrcNickListCtrl::Localize()
{
	CString strRes(GetResString(IDS_STATUS));
	HDITEM hdi;
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	hdi.mask = HDI_TEXT;
	GetHeaderCtrl()->SetItem(1, &hdi);
	UpdateNickCount();
}

BOOL CIrcNickListCtrl::OnCommand(WPARAM wParam, LPARAM)
{
	int iNickItem = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	int iChanItem = m_pParent->m_wndChanSel.GetCurSel();
	const Nick *pNick = reinterpret_cast<Nick*>(GetItemData(iNickItem));
	TCITEM item;
	item.mask = TCIF_PARAM;
	m_pParent->m_wndChanSel.GetItem(iChanItem, &item);
	Channel *pChannel = reinterpret_cast<Channel*>(item.lParam);

	switch (wParam) {
	case Irc_Priv:
		OpenPrivateChannel(pNick);
		break;
	case Irc_Kick:
		if (pNick && pChannel) {
			CString sSend;
			sSend.Format(_T("KICK %s %s"), (LPCTSTR)pChannel->m_sName, (LPCTSTR)pNick->m_sNick);
			m_pParent->m_pIrcMain->SendString(sSend);
		}
		break;
	case Irc_Ban:
		if (pNick && pChannel) {
			CString sSend;
//				sSend.Format(_T("cs ban %s %s"), (LPCTSTR)pChannel->m_sName, (LPCTSTR)pNick->m_sNick);
			sSend.Format(_T("MODE %s +b %s"), (LPCTSTR)pChannel->m_sName, (LPCTSTR)pNick->m_sNick);
			m_pParent->m_pIrcMain->SendString(sSend);
		}
		break;
	case Irc_Slap:
		if (pNick && pChannel) {
			CString sSend;
			sSend.Format(GetResString(IDS_IRC_SLAPMSGSEND), (LPCTSTR)pChannel->m_sName, (LPCTSTR)pNick->m_sNick);
			m_pParent->AddInfoMessageF(pChannel->m_sName, GetResString(IDS_IRC_SLAPMSG), (LPCTSTR)m_pParent->m_pIrcMain->GetNick(), (LPCTSTR)pNick->m_sNick);
			m_pParent->m_pIrcMain->SendString(sSend);
		}
		break;
	case Irc_AddFriend:
		if (pNick && pChannel) {
			//SetVerify() sets a new challenge which is required by the other end to respond with for some protection.
			CString sSend;
			sSend.Format(_T("PRIVMSG %s :\001RQSFRIEND|%u|\001"), (LPCTSTR)pNick->m_sNick, m_pParent->m_pIrcMain->SetVerify());
			m_pParent->m_pIrcMain->SendString(sSend);
		}
		break;
	case Irc_SendLink:
		if (!m_pParent->GetSendFileString().IsEmpty() && pNick && pChannel) {
			//We send our nick and ClientID to allow the other end to only accept links from friends.
			CString sSend;
			sSend.Format(_T("PRIVMSG %s :\001SENDLINK|%s|%s\001"), (LPCTSTR)pNick->m_sNick, (LPCTSTR)md4str(thePrefs.GetUserHash()), (LPCTSTR)m_pParent->GetSendFileString());
			m_pParent->m_pIrcMain->SendString(sSend);
		}
	}
	return TRUE;
}