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

//A lot of documentation for the IRC protocol within this code came
//directly from http://www.irchelp.org/irchelp/rfc/rfc2812.txt
//Much of it may never be used, but it's here just in case.
#include "stdafx.h"
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
#include "IrcMain.h"
#include "emule.h"
#include "otherfunctions.h"
#include "ED2KLink.h"
#include "DownloadQueue.h"
#include "server.h"
#include "IrcSocket.h"
#include "MenuCmds.h"
#include "ServerConnect.h"
#include "FriendList.h"
#include "emuleDlg.h"
#include "ServerWnd.h"
#include "IrcWnd.h"
#include "StringConversion.h"
#include "Log.h"
#include "Exceptions.h"
#include "opcodes.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define Irc_Version				_T("(SMIRCv00.69)")

CIrcMain::CIrcMain()
{
	m_pIRCSocket = NULL;
	m_pwndIRC = NULL;
	srand((unsigned)time(NULL));
	SetVerify();
	m_dwLastRequest = 0;
}

void CIrcMain::PreParseMessage(const char *pszBufferA)
{
	try {
		m_sPreParseBufferA += pszBufferA;
		int iIndex = m_sPreParseBufferA.Find('\n');
		while (iIndex != -1) {
			int iRawMessageLen = iIndex;
			LPCSTR pszRawMessageA = m_sPreParseBufferA;
			if (iRawMessageLen > 0 && pszRawMessageA[iRawMessageLen - 1] == '\r')
				iRawMessageLen -= 1;
			else
				ASSERT(0);
			TRACE(_T("%s\n"), (LPCTSTR)CString(pszRawMessageA, iRawMessageLen));
			if (thePrefs.GetIRCEnableUTF8())
				ParseMessage(OptUtf8ToStr(pszRawMessageA, iRawMessageLen));
			else
				ParseMessage(CString(pszRawMessageA, iRawMessageLen));
			m_sPreParseBufferA.Delete(0, iIndex + 1);
			iIndex = m_sPreParseBufferA.Find('\n');
		}
	}
	CATCH_DFLT_EXCEPTIONS(_T(__FUNCTION__))
		CATCH_DFLT_ALL(_T(__FUNCTION__))
}

void CIrcMain::ProcessLink(const CString &sED2KLink)
{
	try {
		const CString &sLink(OptUtf8ToStr(URLDecode(sED2KLink)));
		CED2KLink *pLink = CED2KLink::CreateLinkFromUrl(sLink);
		_ASSERT(pLink != 0);
		switch (pLink->GetKind()) {
		case CED2KLink::kFile:
			{
				CED2KFileLink *pFileLink = pLink->GetFileLink();
				_ASSERT(pFileLink != 0);
				theApp.downloadqueue->AddFileLinkToDownload(pFileLink);
			}
			break;
		case CED2KLink::kServerList:
			{
				CED2KServerListLink *pListLink = pLink->GetServerListLink();
				_ASSERT(pListLink != 0);
				const CString &sAddress(pListLink->GetAddress());
				if (!sAddress.IsEmpty())
					theApp.emuledlg->serverwnd->UpdateServerMetFromURL(sAddress);
			}
			break;
		case CED2KLink::kServer:
			{
				CString sDefName;
				CED2KServerLink *pSrvLink = pLink->GetServerLink();
				_ASSERT(pSrvLink != 0);
				CServer *pSrv = new CServer(pSrvLink->GetPort(), pSrvLink->GetAddress());
				_ASSERT(pSrv != 0);
				pSrvLink->GetDefaultName(sDefName);
				pSrv->SetListName(sDefName);

				if (thePrefs.GetManualAddedServersHighPriority())
					pSrv->SetPreference(SRV_PR_HIGH);

				if (!theApp.emuledlg->serverwnd->serverlistctrl.AddServer(pSrv, true))
					delete pSrv;
				else
					AddLogLine(true, GetResString(IDS_SERVERADDED), (LPCTSTR)pSrv->GetListName());
			}
		}
		delete pLink;
	} catch (...) {
		LogWarning(LOG_STATUSBAR, GetResString(IDS_LINKNOTADDED));
		ASSERT(0);
	}
}

void CIrcMain::ParseMessage(const CString &sRawMessage)
{
	try {
		if (sRawMessage.GetLength() < 6) {
			//TODO : We probably should disconnect here as I don't know of anything that should
			//come from the server this small.
			return;
		}
		if (_tcsncmp(sRawMessage, _T("PING :"), 6) == 0) {
			//If the server pinged us, we must pong back or get disconnected.
			//Anything after the ":" must be sent back or it will fail.
			CString sReply(sRawMessage);
			sReply.Replace(_T("PING :"), _T("PONG "));
			m_pIRCSocket->SendString(sReply);
			if (!thePrefs.GetIRCIgnorePingPongMessages())
				m_pwndIRC->AddStatus(_T("Ping? Pong!"), false);
			return;
		}
		if (_tcsncmp(sRawMessage, _T("ERROR"), 5) == 0) {
			m_pwndIRC->AddStatus(sRawMessage);
			return;
		}
		int iIndex = 0;
		CString sCommand;
		CString sServername;
		CString sNickname;
		//CString sUser; - not using this currently
		CString sHost;
		if (sRawMessage[0] != _T(':'))
			sCommand = sRawMessage.Tokenize(_T(" "), iIndex);
		else {
			sServername = sRawMessage.Tokenize(_T(" "), iIndex);
			sServername.Remove(_T(':'));
			int iPos = sServername.Find(_T('!'));
			if (iPos >= 0) {
				sNickname = sServername.Left(iPos);
				sServername.Delete(0, iPos + 1);
				iPos = sServername.Find(_T('@'));
				if (iPos >= 0) {
					//sUser = sServername.Left(iPos);
					sHost = sServername.Mid(iPos + 1);
				}
				sServername.Empty();
			}
			sCommand = sRawMessage.Tokenize(_T(" "), iIndex);
		}

		if (sCommand.IsEmpty())
			throw CString(_T("SMIRC Error: Received a message with no command."));

		if (sRawMessage.IsEmpty())
			throw CString(_T("SMIRC Error: Received a message with no target or empty."));

		if (sCommand == _T("PRIVMSG")) {
			CString sTarget = sRawMessage.Tokenize(_T(" "), iIndex);

			// Channel and Private message were merged into this one if statement, this check allows that to happen.
			if (sTarget[0] != _T('#'))
				sTarget = sNickname;

			//If this is a special message. Find out what kind.
			if (sRawMessage.Mid(iIndex, 2) == _T(":\001")) {
				//Check if this is an ACTION message.
				if (sRawMessage.Mid(iIndex, 9) == _T(":\001ACTION ")) {
					iIndex += 9;
					CString sMessage = sRawMessage.Mid(iIndex);
					sMessage.Remove(_T('\001'));
					//Channel Action.
					m_pwndIRC->AddInfoMessageCF(sTarget, RGB(156, 0, 156), _T("* %s %s"), (LPCTSTR)sNickname, (LPCTSTR)sMessage);
					return;
				}
				//Check if this is a SOUND message.
				if (sRawMessage.Mid(iIndex, 7) == _T(":\001SOUND")) {
					if (!thePrefs.GetIRCPlaySoundEvents())
						return;
					iIndex += 7;
					CString sSound(sRawMessage.Tokenize(_T(" "), iIndex));
					sSound.Remove(_T('\001'));
					//Check for proper form.
					sSound.Remove(_T('\\'));
					sSound.Remove(_T('/'));
					sSound.MakeLower();
					//Check if the there was a message to the sound.
					if (sSound.Right(4) == _T(".wav") || sSound.Right(4) == _T(".mp3")) {
						CString sMessage(sRawMessage.Mid(iIndex));
						sMessage.Remove(_T('\001'));
						if (sMessage.IsEmpty())
							sMessage = _T("[SOUND]");
						PlaySound(thePrefs.GetMuleDirectory(EMULE_EXECUTABLEDIR) + _T("Sounds\\IRC\\") + sSound
							, NULL
							, SND_FILENAME | SND_NODEFAULT | SND_NOSTOP | SND_NOWAIT | SND_ASYNC);
						m_pwndIRC->AddInfoMessageF(sTarget, _T("* %s %s"), (LPCTSTR)sNickname, (LPCTSTR)sMessage);
					}
					return;
				}
				//Check if this was a VERSION message.
				if (sRawMessage.Mid(iIndex, 9) == _T(":\001VERSION")) {
					if (::GetTickCount() < m_dwLastRequest + SEC2MS(1)) { // excess flood protection
						m_dwLastRequest = ::GetTickCount();
						return;
					}
					m_dwLastRequest = ::GetTickCount();

					//Get client version.
					iIndex += 9;
					m_sVersion = _T("eMule") + theApp.m_strCurVersionLong + Irc_Version;
					CString sBuild;
					sBuild.Format(_T("NOTICE %s :\001VERSION %s\001"), (LPCTSTR)sNickname, (LPCTSTR)m_sVersion);
					m_pIRCSocket->SendString(sBuild);
					return;
				}
				if (sRawMessage.Mid(iIndex, 6) == _T(":\001PING")) {
					if (::GetTickCount() < m_dwLastRequest + SEC2MS(1)) { // excess flood protection
						m_dwLastRequest = ::GetTickCount();
						return;
					}
					m_dwLastRequest = ::GetTickCount();
					iIndex += 6;
					CString sVerify = sRawMessage.Tokenize(_T(" "), iIndex);
					sVerify.Remove(_T('\001'));
					CString sBuild;
					sBuild.Format(_T("NOTICE %s :\001PING %s\001"), (LPCTSTR)sNickname, (LPCTSTR)sVerify);
					m_pIRCSocket->SendString(sBuild);
					return;
				}
				if (sRawMessage.Mid(iIndex, 11) == _T(":\001RQSFRIEND")) {
					if (::GetTickCount() < m_dwLastRequest + SEC2MS(1)) { // excess flood protection
						m_dwLastRequest = ::GetTickCount();
						return;
					}
					m_dwLastRequest = ::GetTickCount();

					//eMule user requested to add you as friend.
					if (!thePrefs.GetIRCAllowEmuleAddFriend())
						return;
					iIndex += 11;
					//Add a little protection
					const CString &sVerify(sRawMessage.Tokenize(_T("|"), iIndex));
					CString sIP, sPort;
					if (theApp.serverconnect->IsConnected()) {
						//Tell them what server we are on for possible lowID support later.
						sIP.Format(_T("%u"), theApp.serverconnect->GetCurrentServer()->GetIP());
						sPort.Format(_T("%u"), theApp.serverconnect->GetCurrentServer()->GetPort());
					} else {
						//Not on a server.
						sIP = _T("0.0.0.0");
						sPort += _T('0');
					}
					//Create our response.
					CString sBuild;
					sBuild.Format(_T("PRIVMSG %s :\001REPFRIEND eMule%s%s|%s|%u:%u|%s:%s|%s|\001"), (LPCTSTR)sTarget, (LPCTSTR)theApp.m_strCurVersionLong, Irc_Version, (LPCTSTR)sVerify, theApp.IsFirewalled() ? 0 : theApp.GetID(), thePrefs.GetPort(), (LPCTSTR)sIP, (LPCTSTR)sPort, (LPCTSTR)md4str(thePrefs.GetUserHash()));
					m_pIRCSocket->SendString(sBuild);
					sBuild.Format(_T("%s %s"), (LPCTSTR)sTarget, (LPCTSTR)GetResString(IDS_IRC_ADDASFRIEND));
					if (!thePrefs.GetIRCIgnoreEmuleAddFriendMsgs())
						m_pwndIRC->NoticeMessage(_T("*EmuleProto*"), _T(""), sBuild);
					return;
				}
				if (sRawMessage.Mid(iIndex, 11) == _T(":\001REPFRIEND")) {
					iIndex += 11;
					(void)sRawMessage.Tokenize(_T("|"), iIndex); //skip sVersion
					const CString &sVerify(sRawMessage.Tokenize(_T("|"), iIndex));
					//Make sure we requested this!
					if (m_uVerify != (uint32)_tstoi(sVerify))
						return;
					//Pick a new random verify.
					SetVerify();
					uint32 uNewClientID = _tstoi(sRawMessage.Tokenize(_T(":"), iIndex));
					uint16 uNewClientPort = (uint16)_tstoi(sRawMessage.Tokenize(_T("|"), iIndex));
					(void)sRawMessage.Tokenize(_T(":"), iIndex);
					(void)sRawMessage.Tokenize(_T("|"), iIndex);
					const CString &sHash(sRawMessage.Tokenize(_T("|"), iIndex));
					if (!theApp.friendlist->IsAlreadyFriend(sHash)) {
						uchar ucharUserID[MDX_DIGEST_SIZE];
						if (!strmd4(sHash, ucharUserID))
							throw CString(_T("SMIRC Error: Received invalid friend reply"));
						theApp.friendlist->AddFriend(ucharUserID, 0, uNewClientID, uNewClientPort, 0, sTarget, 1);
					}
					return;
				}
				if (sRawMessage.Mid(iIndex, 10) == _T(":\001SENDLINK")) {
					//Received an ED2K link from someone.
					iIndex += 10;
					if (!thePrefs.GetIRCAcceptLinks()) {
						if (!thePrefs.GetIRCIgnoreEmuleSendLinkMsgs())
							m_pwndIRC->NoticeMessage(_T("*EmuleProto*"), _T(""), sTarget + _T(" attempted to send you a file. If you wanted to accept the files from this person, enable Receive files in the IRC Preferences."));
						return;
					}
					const CString &sHash(sRawMessage.Tokenize(_T("|"), iIndex));
					if (thePrefs.GetIRCAcceptLinksFriendsOnly() && !theApp.friendlist->IsAlreadyFriend(sHash)) {
						if (!thePrefs.GetIRCIgnoreEmuleSendLinkMsgs())
							m_pwndIRC->NoticeMessage(_T("*EmuleProto*"), _T(""), sTarget + _T(" attempted to send you a file. If you wanted to accept the files from this person, add him as a friend or change your IRC Preferences."));
						return;
					}
					CString sLink = sRawMessage.Mid(iIndex);
					sLink.Remove(_T('\001'));
					if (!sLink.IsEmpty()) {
						if (!thePrefs.GetIRCIgnoreEmuleSendLinkMsgs()) {
							CString sBuild;
							sBuild.Format(GetResString(IDS_IRC_RECIEVEDLINK), (LPCTSTR)sTarget, (LPCTSTR)sLink);
							m_pwndIRC->NoticeMessage(_T("*EmuleProto*"), _T(""), sBuild);
						}
						ProcessLink(sLink);
					}
					return;
				}
			} else {
				//This is a normal channel message.
				if (iIndex < sRawMessage.GetLength() && sRawMessage.Mid(iIndex, 1) == _T(":"))
					++iIndex;
				m_pwndIRC->AddMessage(sTarget, sNickname, sRawMessage.Mid(iIndex));
			}
			return;
		}
		if (sCommand == _T("JOIN")) {
			//Channel join
			const CString &sChannel(sRawMessage.Tokenize(_T(":"), iIndex));
			//If this was you, just add a message to create a new channel.
			//The list of Nicks received from the server will include you, so don't
			//add yourself here.
			if (!thePrefs.GetIRCIgnoreJoinMessages() || sNickname == m_sNick) {
				if (sNickname == m_sNick) {
					Channel *pChannel = m_pwndIRC->m_wndChanSel.FindChannelByName(sChannel);
					if (pChannel && pChannel->m_bDetached)
						m_pwndIRC->m_wndChanSel.RemoveChannel(pChannel);
				}
				m_pwndIRC->AddInfoMessageF(sChannel, GetResString(IDS_IRC_HASJOINED), (LPCTSTR)sNickname, (LPCTSTR)sChannel);
				if (sNickname == m_sNick)
					return;
			}
			//Add new nick to your channel.
			m_pwndIRC->m_wndNicks.NewNick(sChannel, sNickname);
			return;
		}
		if (sCommand == _T("PART")) {
			//Part message
			const CString &sChannel(sRawMessage.Tokenize(_T(" "), iIndex));
			if (sRawMessage.Mid(iIndex, 1) == _T(":"))
				++iIndex;
			if (sNickname == m_sNick) {
				//This was you, so remove channel.
				m_pwndIRC->m_wndChanSel.RemoveChannel(sChannel);
				return;
			}
			if (!thePrefs.GetIRCIgnorePartMessages())
				m_pwndIRC->AddInfoMessageF(sChannel, GetResString(IDS_IRC_HASPARTED), (LPCTSTR)sNickname, (LPCTSTR)sChannel, (LPCTSTR)sRawMessage.Mid(iIndex));
			//Remove nick from your channel.
			m_pwndIRC->m_wndNicks.RemoveNick(sChannel, sNickname);
			return;
		}

		if (sCommand == _T("TOPIC")) {
			const CString &sChannel(sRawMessage.Tokenize(_T(" "), iIndex));
			if (sRawMessage.Mid(iIndex, 1) == _T(":"))
				++iIndex;
			const CString &sMessage(sRawMessage.Mid(iIndex));
			m_pwndIRC->AddInfoMessageF(sChannel, _T("* %s changes topic to '%s'"), (LPCTSTR)sNickname, (LPCTSTR)sMessage);
			m_pwndIRC->SetTopic(sChannel, sMessage);
			return;
		}

		if (sCommand == _T("QUIT")) {
			if (sRawMessage.Mid(iIndex, 1) == _T(":"))
				++iIndex;
			const CString &sMessage(sRawMessage.Mid(iIndex));
			//This user left the network. Remove from all Channels.
			m_pwndIRC->m_wndNicks.DeleteNickInAll(sNickname, sMessage);
			return;
		}

		if (sCommand == _T("NICK")) {
			// Someone changed a nick.
			if (sRawMessage.Mid(iIndex, 1) == _T(":"))
				++iIndex;
			bool bOwnNick = false;
			const CString &sNewNick(sRawMessage.Tokenize(_T(" "), iIndex));
			if (sNickname == m_sNick) {
				// It was you. Update!
				m_sNick = sNewNick;
				thePrefs.SetIRCNick(m_sNick);
				bOwnNick = true;
			}
			// Update new nick in all channels.
			if (!m_pwndIRC->m_wndNicks.ChangeAllNick(sNickname, sNewNick) && bOwnNick)
				m_pwndIRC->AddStatusF(GetResString(IDS_IRC_NOWKNOWNAS), (LPCTSTR)sNickname, (LPCTSTR)sNewNick);
			return;
		}

		if (sCommand == _T("KICK")) {
			//Someone was kicked from a channel.
			const CString &sChannel(sRawMessage.Tokenize(_T(" "), iIndex));
			const CString &sNick(sRawMessage.Tokenize(_T(" "), iIndex));
			if (sRawMessage.Mid(iIndex, 1) == _T(":"))
				++iIndex;

			if (!thePrefs.GetIRCIgnoreMiscMessages() || sNick == m_sNick) {
				if (sNick == m_sNick) { //It was you!
					m_pwndIRC->m_wndChanSel.DetachChannel(sChannel);
					m_pwndIRC->AddStatusF(GetResString(IDS_IRC_WASKICKEDBY), (LPCTSTR)sNick, (LPCTSTR)sNickname, (LPCTSTR)sRawMessage.Mid(iIndex)); //is it needed?
				}
				m_pwndIRC->AddInfoMessageF(sChannel, GetResString(IDS_IRC_WASKICKEDBY), (LPCTSTR)sNick, (LPCTSTR)sNickname, (LPCTSTR)sRawMessage.Mid(iIndex));
			}
			//Remove nick from your channel.
			if (sNick != m_sNick)
				m_pwndIRC->m_wndNicks.RemoveNick(sChannel, sNick);
			return;
		}
		if (sCommand == _T("MODE")) {
			//A mode was set.
			const CString &sTarget(sRawMessage.Tokenize(_T(" "), iIndex));
			if (sTarget[0] == _T('#')) {
				const CString &sCommands(sRawMessage.Tokenize(_T(" "), iIndex));

				if (sCommand.IsEmpty())
					throw CString(_T("SMIRC Error: Received Invalid Mode change."));

				const CString &sParams(sRawMessage.Mid(iIndex));
				if (sNickname.IsEmpty())
					sNickname = sServername;
				m_pwndIRC->ParseChangeMode(sTarget, sNickname, sCommands, sParams);
			} else {
				//The server just set a server user mode that relates to no channels!
				//Atm, we do not handle these modes.
			}
			return;
		}
		if (sCommand == _T("NOTICE")) {
			const CString &sTarget(sRawMessage.Tokenize(_T(" "), iIndex));
			if (sRawMessage.Mid(iIndex, 1) == _T(":"))
				++iIndex;
			if (!sNickname.IsEmpty())
				m_pwndIRC->NoticeMessage(sNickname, sTarget, sRawMessage.Mid(iIndex));
			else if (!sServername.IsEmpty())
				m_pwndIRC->NoticeMessage(sServername, sTarget, sRawMessage.Mid(iIndex));
			return;
		}

		//TODO: Double and triple check this. I don't know of any 3 letter commands
		//So I'm currently assuming it's a numeric command.
		if (sCommand.GetLength() == 3) {
			(void)sRawMessage.Tokenize(_T(" "), iIndex); //skip sUser
			UINT uCommand = _tstoi(sCommand);
			switch (uCommand) {
					//- The server sends Replies 001 to 004 to a user upon
					//successful registration.
					//001    RPL_WELCOME
					//"Welcome to the Internet Relay Network
					//<nick>!<user>@<host>"
					//002    RPL_YOURHOST
					//"Your host is <servername>, running version <ver>"
					//003    RPL_CREATED
					//"This server was created <date>"
					//004    RPL_MYINFO
					//"<servername> <version> <available user modes>
					//<available channel modes>"
			case 1:
				m_pwndIRC->SetLoggedIn(true);
				if (thePrefs.GetIRCGetChannelsOnConnect()) {
					CString strCommand(_T("LIST"));
					if (thePrefs.GetIRCUseChannelFilter() && !thePrefs.GetIRCChannelFilter().IsEmpty())
						strCommand.AppendFormat(_T(" %s"), (LPCTSTR)thePrefs.GetIRCChannelFilter());
					m_pIRCSocket->SendString(strCommand);
				}
				ParsePerform();
				/* fall through */
			case 2:
			case 3:
			case 4:
				if (sRawMessage.Mid(iIndex, 1) == _T(":"))
					++iIndex;
				m_pwndIRC->AddStatus(sRawMessage.Mid(iIndex));
				return;

				//005 is actually confusing (historically).
				//- Sent by the server to a user to suggest an alternative
				//server.  This is often used when the connection is
				//refused because the server is already full.
				//005    RPL_BOUNCE
				//"Try server <server name>, port <port number>"
				//
				//It appears this also could be RPL_ISUPPORT which tells
				//what modes are available to this server.
				//005     RPL_ISUPPORT
			case 5:
				//This gets our viable user modes
				m_pwndIRC->AddStatus(sRawMessage.Mid(iIndex));
				while (iIndex >= 0) {
					CString sSettings(sRawMessage.Tokenize(_T(" "), iIndex));
					if (sSettings.IsEmpty())
						break;
					if (_tcsncmp(sSettings, _T("PREFIX"), 6) == 0) { //user modes & symbols
						sSettings.Delete(0, sSettings.Find(_T('=')) + 1);
						int iFind = sSettings.Find(_T(')'));

						//Set our channel modes actions
						m_pwndIRC->m_wndNicks.m_sUserModeSettings = sSettings.Mid(1, iFind - 1); //qaohv
						sSettings.Delete(0, iFind + 1);
						//Set our channel modes symbols
						m_pwndIRC->m_wndNicks.m_sUserModeSymbols = sSettings; //~&@%+
					}
					if (_tcsncmp(sSettings, _T("CHANMODES"), 9) == 0) {
						sSettings.Delete(0, sSettings.Find(_T('=')) + 1);

						//Mode that adds or removes a nick or address to a list. Always has a parameter.
						//Modes of type A return the list when there is no parameter present.
						int iFind = sSettings.Find(_T(','));
						m_pwndIRC->m_wndChanSel.m_sChannelModeSettingsTypeA = sSettings.Left(iFind);
						sSettings.Delete(0, iFind + 1);

						//Mode that changes a setting and always has a parameter.
						iFind = sSettings.Find(_T(','));
						m_pwndIRC->m_wndChanSel.m_sChannelModeSettingsTypeB = sSettings.Left(iFind);
						sSettings.Delete(0, iFind + 1);

						//Mode that changes a setting and only has a parameter when set.
						iFind = sSettings.Find(_T(','));
						m_pwndIRC->m_wndChanSel.m_sChannelModeSettingsTypeC = sSettings.Left(iFind);
						sSettings.Delete(0, iFind + 1);

						//Mode that changes a setting and never has a parameter.
						m_pwndIRC->m_wndChanSel.m_sChannelModeSettingsTypeD = sSettings;
					}
				}
				return;

				//- Returned when a NICK message is processed that results
				//in an attempt to change to a currently existing
				//nickname.
				//433    ERR_NICKNAMEINUSE
				//"<nick> :Nickname is already in use"
			case 433:
				if (sRawMessage.Mid(iIndex, 1) == _T(":"))
					++iIndex;
				m_pwndIRC->AddStatus(sRawMessage.Mid(iIndex), true, uCommand);
				// clear nick and enter the IRC-nick message box on next connect
				if (!m_pwndIRC->GetLoggedIn()) {
					thePrefs.SetIRCNick(_T(""));
					Disconnect();
				}
				return;

				//- Reply format used by USERHOST to list replies to
				//the query list.  The reply string is composed as
				//follows:
				//
				//reply = nickname [ "*" ] "=" ( "+" / "-" ) hostname
				//
				//The '*' indicates whether the client has registered
				//as an Operator.  The '-' or '+' characters represent
				//whether the client has set an AWAY message or not
				//respectively.
				//302    RPL_USERHOST
				//":*1<reply> *( " " <reply> )"
			case 302:

				//- Reply format used by ISON to list replies to the
				//query list.

				//303    RPL_ISON
				//":*1<nick> *( " " <nick> )"
			case 303:

				//- These replies are used with the AWAY command (if
				//allowed).  RPL_AWAY is sent to any client sending a
				//PRIVMSG to a client which is away.  RPL_AWAY is only
				//sent by the server to which the client is connected.
				//Replies RPL_UNAWAY and RPL_NOWAWAY are sent when the
				//client removes and sets an AWAY message.
				//301    RPL_AWAY
				//"<nick> :<away message>"
				//305    RPL_UNAWAY
				//":You are no longer marked as being away"
				//306    RPL_NOWAWAY
				//":You have been marked as being away"
			case 301:
			case 305:
			case 306:
				if (sRawMessage.Mid(iIndex, 1) == _T(":"))
					++iIndex;
				m_pwndIRC->AddStatus((uCommand == 303 ? _T("ison: ") : _T("")) + sRawMessage.Mid(iIndex));
				return;

				//- Replies 311 - 313, 317 - 319 are all replies
				//generated in response to a WHOIS message.  Given that
				//there are enough parameters present, the answering
				//server MUST either formulate a reply out of the above
				//numerics (if the query nick is found) or return an
				//error reply.  The '*' in RPL_WHOISUSER is there as
				//the literal character and not as a wild card.  For
				//each reply set, only RPL_WHOISCHANNELS may appear
				//more than once (for long lists of channel names).
				//The '@' and '+' characters next to the channel name
				//indicate whether a client is a channel operator or
				//has been granted permission to speak on a moderated
				//channel.  The RPL_ENDOFWHOIS reply is used to mark
				//the end of processing a WHOIS message.
				//311    RPL_WHOISUSER
				//"<nick> <user> <host> * :<real name>"
				//312    RPL_WHOISSERVER
				//"<nick> <server> :<server info>"
				//313    RPL_WHOISOPERATOR
				//"<nick> :is an IRC operator"
				//317    RPL_WHOISIDLE
				//"<nick> <integer> :seconds idle"
				//318    RPL_ENDOFWHOIS
				//"<nick> :End of WHOIS list"
				//319    RPL_WHOISCHANNELS
				//"<nick> :*( ( "@" / "+" ) <channel> " " )"
			case 311:
			case 312:
			case 313:
			case 317:
				if (uCommand == 317) {
					CString s(sRawMessage.Tokenize(_T(" "), iIndex));
					CTimeSpan idle(_tstoi64(sRawMessage.Tokenize(_T(" "), iIndex)));
					s += _T(' ');
					if (idle.GetHours() > 0)
						s += idle.Format(_T("%Hhrs %Mmins %Ssecs"));
					else if (idle.GetMinutes() > 0)
						s += idle.Format(_T("%Mmins %Ssecs"));
					else
						s += idle.Format(_T("%Ssecs"));
					s += _T(" idle");
					CTime signon(_tstoi64(sRawMessage.Tokenize(_T(" "), iIndex)));
					if (signon != 0)
						s += signon.Format(_T(", signed on %a %b %d %H:%M:%S"));
					m_pwndIRC->AddCurrent(s);
					return;
				}
			case 318:
			case 319:

				//- When replying to a WHOWAS message, a server MUST use
				//the replies RPL_WHOWASUSER, RPL_WHOISSERVER or
				//ERR_WASNOSUCHNICK for each nickname in the presented
				//list.  At the end of all reply batches, there MUST
				//be RPL_ENDOFWHOWAS (even if there was only one reply
				//and it was an error).
				//314    RPL_WHOWASUSER
				//"<nick> <user> <host> * :<real name>"
				//369    RPL_ENDOFWHOWAS
				//"<nick> :End of WHOWAS"
			case 314:
			case 369:
				if (sRawMessage.Mid(iIndex, 1) == _T(":"))
					++iIndex;
				m_pwndIRC->AddCurrent(sRawMessage.Mid(iIndex));
				return;

				//- Replies RPL_LIST, RPL_LISTEND mark the actual replies
				//with data and end of the server's response to a LIST
				//command.  If there are no channels available to return,
				//only the end reply MUST be sent.
				//321    RPL_LISTSTART
				//Obsolete. Not used.
				//322    RPL_LIST
				//"<channel> <# visible> :<topic>"
				//323    RPL_LISTEND
				//":End of LIST"
			case 321:
				//Although it says this is obsolete, so far every server has sent it, so I use it. :/
				m_pwndIRC->AddStatus(_T("Start of /LIST"));
				if (m_pwndIRC->m_wndChanSel.m_pChanList == NULL)
					m_pwndIRC->m_wndChanSel.m_pChanList = m_pwndIRC->m_wndChanSel.NewChannel(GetResString(IDS_IRC_CHANNELLIST), Channel::ctChannelList);
				else
					m_pwndIRC->m_wndChanList.ResetServerChannelList();
				return;
			case 322:
				{
					const CString &sChannel(sRawMessage.Tokenize(_T(" "), iIndex));
					if (sChannel[0] == _T('#')) {
						const CString &sChanNum(sRawMessage.Tokenize(_T(" "), iIndex));
						++iIndex;
						if (m_pwndIRC->m_wndChanList.AddChannelToList(sChannel, sChanNum, sRawMessage.Mid(iIndex)))
							m_pwndIRC->m_wndChanSel.SetActivity(m_pwndIRC->m_wndChanSel.m_pChanList, true);
					}
				}
				return;
			case 323:

				//- When sending a TOPIC message to determine the
				//channel topic, one of two replies is sent.  If
				//the topic is set, RPL_TOPIC is sent back else
				//RPL_NOTOPIC.
				//325    RPL_UNIQOPIS
				//"<channel> <nickname>"
				//324    RPL_CHANNELMODEIS
				//"<channel> <mode> <mode params>"
				//331    RPL_NOTOPIC
				//"<channel> :No topic is set"
				//332    RPL_TOPIC
				//"<channel> :<topic>"
			case 325:
			case 331:
				if (sRawMessage.Mid(iIndex, 1) == _T(":"))
					++iIndex;
				m_pwndIRC->AddStatus(sRawMessage.Mid(iIndex));
				return;
			case 324:
				{
					//A mode was set.
					const CString &sChannel(sRawMessage.Tokenize(_T(" "), iIndex));
					const CString &sCommands(sRawMessage.Tokenize(_T(" "), iIndex));
					m_pwndIRC->ParseChangeMode(sChannel, sNickname, sCommands, sRawMessage.Mid(iIndex));
				}
				return;
			case 332:
				{
					//Set Channel Topic
					const CString &sChannel(sRawMessage.Tokenize(_T(" "), iIndex));
					if (sChannel[0] == _T('#')) {
						if (sRawMessage.Mid(iIndex, 1) == _T(":"))
							++iIndex;
						m_pwndIRC->SetTopic(sChannel, sRawMessage.Mid(iIndex));
						m_pwndIRC->AddInfoMessageF(sChannel, _T("* Channel Topic: %s"), (LPCTSTR)sRawMessage.Mid(iIndex));
					}
				}
				return;

				//- Returned by the server to indicate that the
				//attempted INVITE message was successful and is
				//being passed onto the end client.
				//341    RPL_INVITING
				//"<channel> <nick>"
			case 341:

				//- Returned by a server answering a SUMMON message to
				//indicate that it is summoning that user.
				//342    RPL_SUMMONING
				//"<user> :Summoning user to IRC"
			case 342:

				//- When listing the 'invitations masks' for a given channel,
				//a server is required to send the list back using the
				//RPL_INVITELIST and RPL_ENDOFINVITELIST messages.  A
				//separate RPL_INVITELIST is sent for each active mask.
				//After the masks have been listed (or if none present) a
				//RPL_ENDOFINVITELIST MUST be sent.
				//346    RPL_INVITELIST
				//"<channel> <invitemask>"
				//347    RPL_ENDOFINVITELIST
				//"<channel> :End of channel invite list
			case 346:
			case 347:

				//- When listing the 'exception masks' for a given channel,
				//a server is required to send the list back using the
				//RPL_EXCEPTLIST and RPL_ENDOFEXCEPTLIST messages.  A
				//separate RPL_EXCEPTLIST is sent for each active mask.
				//After the masks have been listed (or if none present)
				//a RPL_ENDOFEXCEPTLIST MUST be sent.
				//348    RPL_EXCEPTLIST
				//"<channel> <exceptionmask>"
				//349    RPL_ENDOFEXCEPTLIST
				//"<channel> :End of channel exception list"
			case 348:
			case 349:

				//- Reply by the server showing its version details.
				//The <version> is the version of the software being
				//used (including any patchlevel revisions) and the
				//<debuglevel> is used to indicate if the server is
				//running in "debug mode".
				//The "comments" field may contain any comments about
				//the version or further version details.
				//351    RPL_VERSION
				//"<version>.<debuglevel> <server> :<comments>"
			case 351:

				//- The RPL_WHOREPLY and RPL_ENDOFWHO pair are used
				//to answer a WHO message.  The RPL_WHOREPLY is only
				//sent if there is an appropriate match to the WHO
				//query.  If there is a list of parameters supplied
				//with a WHO message, a RPL_ENDOFWHO MUST be sent
				//after processing each list item with <name> being
				//the item.
				//352    RPL_WHOREPLY
				//"<channel> <user> <host> <server> <nick>
				//( "H" / "G" > ["*"] [ ( "@" / "+" ) ]
				//:<hopcount> <real name>"
				//315    RPL_ENDOFWHO
				//"<name> :End of WHO list"
			case 352:
			case 315:
				if (sRawMessage.Mid(iIndex, 1) == _T(":"))
					++iIndex;
				m_pwndIRC->AddStatus(sRawMessage.Mid(iIndex));
				return;

				//- To reply to a NAMES message, a reply pair consisting
				//of RPL_NAMREPLY and RPL_ENDOFNAMES is sent by the
				//server back to the client.  If there is no channel
				//found as in the query, then only RPL_ENDOFNAMES is
				//returned.  The exception to this is when a NAMES
				//message is sent with no parameters and all visible
				//channels and contents are sent back in a series of
				//RPL_NAMEREPLY messages with a RPL_ENDOFNAMES to mark
				//the end.
				//353    RPL_NAMREPLY
				//"( "=" / "*" / "@" ) <channel>
				//:[ "@" / "+" ] <nick> *( " " [ "@" / "+" ] <nick> )
				//- "@" is used for secret channels, "*" for private
				//channels, and "=" for others (public channels).
				//366    RPL_ENDOFNAMES
				//"<channel> :End of NAMES list"
			case 353:
				{
					m_pwndIRC->m_wndNicks.ShowWindow(SW_HIDE);
					(void)sRawMessage.Tokenize(_T(" "), iIndex); //skip sChannelType
					const CString &sChannel(sRawMessage.Tokenize(_T(" "), iIndex));
					if (sChannel[0] == _T('#')) {
						for (++iIndex; iIndex >= 0;) {
							const CString &sNewNick(sRawMessage.Tokenize(_T(" "), iIndex));
							if (!sNewNick.IsEmpty())
								m_pwndIRC->m_wndNicks.NewNick(sChannel, sNewNick);
						}

						m_pwndIRC->m_wndNicks.RefreshNickList(sChannel);
					}
				}
				return;
			case 366:
				{
					const CString &sChannel(sRawMessage.Tokenize(_T(" "), iIndex));
					SendString(_T("MODE ") + sChannel);
					m_pwndIRC->m_wndNicks.ShowWindow(SW_SHOW);
					++iIndex;
					m_pwndIRC->AddStatus(sRawMessage.Mid(iIndex));
				}
				return;

				//- In replying to the LINKS message, a server MUST send
				//replies back using the RPL_LINKS numeric and mark the
				//end of the list using a RPL_ENDOFLINKS reply.
				//364    RPL_LINKS
				//"<mask> <server> :<hopcount> <server info>"
				//365    RPL_ENDOFLINKS
				//"<mask> :End of LINKS list"
			case 364:
			case 365:

				//- When listing the active 'bans' for a given channel,
				//a server is required to send the list back using the
				//RPL_BANLIST and RPL_ENDOFBANLIST messages.  A separate
				//RPL_BANLIST is sent for each active banmask.  After the
				//banmasks have been listed (or if none present) a
				//RPL_ENDOFBANLIST MUST be sent.
				//367    RPL_BANLIST
				//"<channel> <banmask>"
				//368    RPL_ENDOFBANLIST
				//"<channel> :End of channel ban list"
			case 367:
			case 368:

				//- A server responding to an INFO message is required to
				//send all its 'info' in a series of RPL_INFO messages
				//with a RPL_ENDOFINFO reply to indicate the end of the
				//replies.
				//371    RPL_INFO
				//":<string>"
				//374    RPL_ENDOFINFO
				//":End of INFO list"
			case 371:
			case 374:

				//- When responding to the MOTD message and the MOTD file
				//is found, the file is displayed line by line, with
				//each line no longer than 80 characters, using
				//RPL_MOTD format replies.  These MUST be surrounded
				//by a RPL_MOTDSTART (before the RPL_MOTDs) and an
				//RPL_ENDOFMOTD (after).
				//375    RPL_MOTDSTART
				//":- <server> Message of the day - "
				//372    RPL_MOTD
				//":- <text>"
				//376    RPL_ENDOFMOTD
				//":End of MOTD command"
			case 375:
			case 372:
			case 376:

				//- RPL_YOUREOPER is sent back to a client which has
				//just successfully issued an OPER message and gained
				//operator status.
				//381    RPL_YOUREOPER
				//":You are now an IRC operator"
			case 381:

				//- If the REHASH option is used and an operator sends
				//a REHASH message, a RPL_REHASHING is sent back to
				//the operator.
				//382    RPL_REHASHING
				//"<config file> :Rehashing"
			case 382:

				//- Sent by the server to a service upon successful
				//registration.
				//383    RPL_YOURESERVICE
				//"You are service <servicename>"
			case 383:
				//- When replying to the TIME message, a server MUST send
				//the reply using the RPL_TIME format above.  The string
				//showing the time need only contain the correct day and
				//time there.  There is no further requirement for the
				//time string.
				//391    RPL_TIME
				//"<server> :<string showing server's local time>"
			case 391:

				//- If the USERS message is handled by a server, the
				//replies RPL_USERSTART, RPL_USERS, RPL_ENDOFUSERS and
				//RPL_NOUSERS are used.  RPL_USERSSTART MUST be sent
				//first, following by either a sequence of RPL_USERS
				//or a single RPL_NOUSER.  Following this is
				//RPL_ENDOFUSERS.
				//392    RPL_USERSSTART
				//":UserID   Terminal  Host"
				//393    RPL_USERS
				//":<username> <ttyline> <hostname>"
				//       394    RPL_ENDOFUSERS
				//              ":End of users"
				//395    RPL_NOUSERS
				//":Nobody logged in"
			case 392:
			case 393:
			case 395:

				//- The RPL_TRACE* are all returned by the server in
				//response to the TRACE message.  How many are
				//returned is dependent on the TRACE message and
				//whether it was sent by an operator or not.  There
				//is no predefined order for which occurs first.
				//Replies RPL_TRACEUNKNOWN, RPL_TRACECONNECTING and
				//RPL_TRACEHANDSHAKE are all used for connections
				//which have not been fully established and are either
				//unknown, still attempting to connect or in the
				//process of completing the 'server handshake'.
				//RPL_TRACELINK is sent by any server which handles
				//a TRACE message and has to pass it on to another
				//server.  The list of RPL_TRACELINKs sent in
				//response to a TRACE command traversing the IRC
				//network should reflect the actual connectivity of
				//the servers themselves along that path.
				//RPL_TRACENEWTYPE is to be used for any connection
				//which does not fit in the other categories but is
				//being displayed anyway.
				//RPL_TRACEEND is sent to indicate the end of the list.
				//200    RPL_TRACELINK
				//"Link <version & debug level> <destination>
				//<next server> V<protocol version>
				//<link uptime in seconds> <backstream sendq>
				//<upstream sendq>"
				//201    RPL_TRACECONNECTING
				//"Try. <class> <server>"
				//202    RPL_TRACEHANDSHAKE
				//"H.S. <class> <server>"
				//203    RPL_TRACEUNKNOWN
				//"???? <class> [<client IP address in dot form>]"
				//204    RPL_TRACEOPERATOR
				//"Oper <class> <nick>"
				//205    RPL_TRACEUSER
				//"User <class> <nick>"
				//206    RPL_TRACESERVER
				//"Serv <class> <int>S <int>C <server>
				//<nick!user|*!*>@<host|server> V<protocol version>"
				//207    RPL_TRACESERVICE
				//"Service <class> <name> <type> <active type>"
				//208    RPL_TRACENEWTYPE
				//"<newtype> 0 <client name>"
				//209    RPL_TRACECLASS
				//"Class <class> <count>"
				//210    RPL_TRACERECONNECT
				//Unused.
				//261    RPL_TRACELOG
				//"File <logfile> <debug level>"
				//262    RPL_TRACEEND
				//"<server name> <version & debug level> :End of TRACE"
			case 200:
			case 201:
			case 202:
			case 203:
			case 204:
			case 205:
			case 206:
			case 207:
			case 208:
			case 209:
			case 210:
			case 261:
			case 262:

				//- reports statistics on a connection.
				// <linkname> identifies the particular connection,
				// <sendq> is the amount of data that is queued and
				//waiting to be sent
				// <sent messages> the number of messages sent, and
				// <sent kBytes> the amount of data sent, in kBytes.
				// <received messages> and <received kBytes> are
				//equivalents of <sent messages> and <sent kBytes>
				//for received data, respectively.
				// <time open> indicates how long ago the connection
				//was opened, in seconds.
				//211    RPL_STATSLINKINFO
				//"<linkname> <sendq> <sent messages>
				//<sent kBytes> <received messages>
				//<received kBytes> <time open>"
			case 211:

				//- reports statistics on commands usage.
				//212    RPL_STATSCOMMANDS
				//"<command> <count> <byte count> <remote count>"
			case 212:

				//- reports the server uptime.
				//219    RPL_ENDOFSTATS
				//"<stats letter> :End of STATS report"
				//242    RPL_STATSUPTIME
				//":Server Up %d days %d:%02d:%02d"
			case 219:
			case 242:

				//- reports the allowed hosts from where user may become IRC
				//operators.
				//243    RPL_STATSOLINE
				//"O <hostmask> * <name>"
			case 243:

				//- To answer a query about a client's own mode,
				//RPL_UMODEIS is sent back.
				//221    RPL_UMODEIS
				//"<user mode string>"
			case 221:

				//- When listing services in reply to a SERVLIST message,
				//a server is required to send the list back using the
				//RPL_SERVLIST and RPL_SERVLISTEND messages.  A separate
				//RPL_SERVLIST is sent for each service.  After the
				//services have been listed (or if none present) a
				//RPL_SERVLISTEND MUST be sent.
				//234    RPL_SERVLIST
				//"<name> <server> <mask> <type> <hopcount> <info>"
				//235    RPL_SERVLISTEND
				//"<mask> <type> :End of service listing"
			case 234:
			case 235:

				//- In processing a LUSERS message, the server
				//sends a set of replies from RPL_LUSERCLIENT,
				//RPL_LUSEROP, RPL_USERUNKNOWN,
				//RPL_LUSERCHANNELS and RPL_LUSERME.  When
				//replying, a server MUST send back
				//RPL_LUSERCLIENT and RPL_LUSERME.  The other
				//replies are only sent back if a non-zero count
				//is found for them.
				//251    RPL_LUSERCLIENT
				//":There are <integer> users and <integer>
				//services on <integer> servers"
				//252    RPL_LUSEROP
				//"<integer> :operator(s) online"
				//253    RPL_LUSERUNKNOWN
				//"<integer> :unknown connection(s)"
				//254    RPL_LUSERCHANNELS
				//"<integer> :channels formed"
				//255    RPL_LUSERME
				//":I have <integer> clients and <integer>
				//servers"
			case 251:
			case 252:
			case 253:
			case 254:
			case 255:

				//- When replying to an ADMIN message, a server
				//is expected to use replies RPL_ADMINME
				//through to RPL_ADMINEMAIL and provide a text
				//message with each.  For RPL_ADMINLOC1 a
				//description of what city, state and country
				//the server is in is expected, followed by
				//details of the institution (RPL_ADMINLOC2)
				//and finally the administrative contact for the
				//server (an email address here is REQUIRED)
				//in RPL_ADMINEMAIL.
				//256    RPL_ADMINME
				//"<server> :Administrative info"
				//257    RPL_ADMINLOC1
				//":<admin info>"
				//258    RPL_ADMINLOC2
				//":<admin info>"
				//259    RPL_ADMINEMAIL
				//":<admin info>"
			case 256:
			case 257:
			case 258:
			case 259:

				//- When a server drops a command without processing it,
				//it MUST use the reply RPL_TRYAGAIN to inform the
				//originating client.
				//263    RPL_TRYAGAIN
				//"<command> :Please wait a while and try again."
			case 263:

				//- Used to indicate the nickname parameter supplied to a
				//command is currently unused.
				//401    ERR_NOSUCHNICK
				//"<nickname> :No such nick/channel"
			case 401:

				//- Used to indicate the server name given currently
				//does not exist.
				//402    ERR_NOSUCHSERVER
				//"<server name> :No such server"
			case 402:

				//- Used to indicate the given channel name is invalid.
				//403    ERR_NOSUCHCHANNEL
				//"<channel name> :No such channel"
			case 403:

				//- Sent to a user who is either (a) not on a channel
				//which is mode +n or (b) not a chanop (or mode +v) on
				//a channel which has mode +m set or where the user is
				//banned and is trying to send a PRIVMSG message to
				//that channel.
				//404    ERR_CANNOTSENDTOCHAN
				//"<channel name> :Cannot send to channel"
			case 404:

				//- Sent to a user when they have joined the maximum
				//number of allowed channels and they try to join
				//another channel.
				//405    ERR_TOOMANYCHANNELS
				//"<channel name> :You have joined too many channels"
			case 405:

				//- Returned by WHOWAS to indicate there is no history
				//information for that nickname.
				//406    ERR_WASNOSUCHNICK
				//"<nickname> :There was no such nickname"
			case 406:

				//- Returned to a client which is attempting to send a
				//PRIVMSG/NOTICE using the user@host destination format
				//and for a user@host which has several occurrences.
				//- Returned to a client which trying to send a
				//PRIVMSG/NOTICE to too many recipients.
				//- Returned to a client which is attempting to JOIN a safe
				//channel using the shortname when there are more than one
				//such channel.
				//407    ERR_TOOMANYTARGETS
				//"<target> :<error code> recipients. <abort message>"
			case 407:

				//- Returned to a client which is attempting to send a SQUERY
				//to a service which does not exist.
				//408    ERR_NOSUCHSERVICE
				//"<service name> :No such service"
			case 408:

				//- PING or PONG message missing the originator parameter.
				//409    ERR_NOORIGIN
				//":No origin specified"
			case 409:

				//- 412 - 415 are returned by PRIVMSG to indicate that
				//the message wasn't delivered for some reason.
				//ERR_NOTOPLEVEL and ERR_WILDTOPLEVEL are errors that
				//are returned when an invalid use of
				//"PRIVMSG $<server>" or "PRIVMSG #<host>" is attempted.
				//411    ERR_NORECIPIENT
				//":No recipient given (<command>)"
				//412    ERR_NOTEXTTOSEND
				//":No text to send"
				//413    ERR_NOTOPLEVEL
				//"<mask> :No toplevel domain specified"
				//414    ERR_WILDTOPLEVEL
				//"<mask> :Wildcard in toplevel domain"
				//415    ERR_BADMASK
				//"<mask> :Bad Server/host mask"
			case 411:
			case 412:
			case 413:
			case 414:
			case 415:

				//- Returned to a registered client to indicate that the
				//command sent is unknown by the server.
				//421    ERR_UNKNOWNCOMMAND
				//"<command> :Unknown command"
			case 421:

				//- Server's MOTD file could not be opened by the server.
				//422    ERR_NOMOTD
				//":MOTD File is missing"
			case 422:

				//- Returned by a server in response to an ADMIN message
				//when there is an error in finding the appropriate
				//information.
				//423    ERR_NOADMININFO
				//"<server> :No administrative info available"
			case 423:

				//- Generic error message used to report a failed file
				//operation during the processing of a message.
				//424    ERR_FILEERROR
				//":File error doing <file op> on <file>"
			case 424:

				//- Returned when a nickname parameter expected for a
				//command and isn't found.
				//431    ERR_NONICKNAMEGIVEN
				//":No nickname given"
			case 431:

				//- Returned after receiving a NICK message which contains
				//characters which do not fall in the defined set.  See
				//section 2.3.1 for details on valid nicknames.
				//432    ERR_ERRONEUSNICKNAME
				//"<nick> :Erroneous nickname"
			case 432:

				//- Returned by a server to a client when it detects a
				//nickname collision (registered of a NICK that
				//already exists by another server).
				//436    ERR_NICKCOLLISION
				//"<nick> :Nickname collision KILL from <user>@<host>"
			case 436:

				//- Returned by a server to a user trying to join a channel
				//currently blocked by the channel delay mechanism.
				//- Returned by a server to a user trying to change nickname
				//when the desired nickname is blocked by the nick delay
				//mechanism.
				//437    ERR_UNAVAILRESOURCE
				//"<nick/channel> :Nick/channel is temporarily unavailable"
			case 437:

				//- Returned by the server to indicate that the sTarget
				//user of the command is not on the given channel.
				//441    ERR_USERNOTINCHANNEL
				//"<nick> <channel> :They aren't on that channel"
			case 441:

				//- Returned by the server whenever a client tries to
				//perform a channel affecting command for which the
				//client isn't a member.
				//442    ERR_NOTONCHANNEL
				//"<channel> :You're not on that channel"
			case 442:

				//- Returned when a client tries to invite a user to a
				//channel they are already on.
				//443    ERR_USERONCHANNEL
				//"<user> <channel> :is already on channel"
			case 443:

				//- Returned by the summon after a SUMMON command for a
				//user was unable to be performed since they were not
				//logged in.
				//444    ERR_NOLOGIN
				//"<user> :User not logged in"
			case 444:

				//- Returned as a response to the SUMMON command.  MUST be
				//returned by any server which doesn't implement it.
				//445    ERR_SUMMONDISABLED
				//":SUMMON has been disabled"
			case 445:

				//- Returned as a response to the USERS command.  MUST be
				//returned by any server which does not implement it.
				//446    ERR_USERSDISABLED
				//":USERS has been disabled"
			case 446:

				//- Returned by the server to indicate that the client
				//MUST be registered before the server will allow it
				//to be parsed in detail.
				//451    ERR_NOTREGISTERED
				//":You have not registered"
			case 451:

				//- Returned by the server by numerous commands to
				//indicate to the client that it didn't supply enough
				//parameters.
				//461    ERR_NEEDMOREPARAMS
				//"<command> :Not enough parameters"
			case 461:

				//- Returned by the server to any link which tries to
				//change part of the registered details (such as
				//password or user details from second USER message).
				//462    ERR_ALREADYREGISTRED
				//":Unauthorized command (already registered)"
			case 462:

				//- Returned to a client which attempts to register with
				//a server which does not been setup to allow
				//connections from the host the attempted connection
				//is tried.
				//463    ERR_NOPERMFORHOST
				//":Your host isn't among the privileged"
			case 463:

				//- Returned to indicate a failed attempt at registering
				//a connection for which a password was required and
				//was either not given or incorrect.
				//464    ERR_PASSWDMISMATCH
				//":Password incorrect"
			case 464:

				//- Returned after an attempt to connect and register
				//yourself with a server which has been setup to
				//explicitly deny connections to you.
				//465    ERR_YOUREBANNEDCREEP
				//":You are banned from this server"
			case 465:

				//- Sent by a server to a user to inform that access to the
				//server will soon be denied.
				//466    ERR_YOUWILLBEBANNED
			case 466:

				//- Any command requiring operator privileges to operate
				//MUST return this error to indicate the attempt was
				//unsuccessful.
				//467    ERR_KEYSET
				//"<channel> :Channel key already set"
				//471    ERR_CHANNELISFULL"<channel> :Cannot join channel (+l)"
				//472    ERR_UNKNOWNMODE
				//"<char> :is unknown mode char to me for <channel>"
				//473    ERR_INVITEONLYCHAN
				//"<channel> :Cannot join channel (+i)"
				//474    ERR_BANNEDFROMCHAN
				//"<channel> :Cannot join channel (+b)"
				//475    ERR_BADCHANNELKEY
				//"<channel> :Cannot join channel (+k)"
				//476    ERR_BADCHANMASK
				//"<channel> :Bad Channel Mask"
				//477    ERR_NOCHANMODES
				//"<channel> :Channel doesn't support modes"
				//478    ERR_BANLISTFULL
				//"<channel> <char> :Channel list is full"
				//481    ERR_NOPRIVILEGES
				//":Permission Denied- You're not an IRC operator"
			case 467:
			case 471:
			case 472:
			case 473:
			case 474:
			case 475:
			case 476:
			case 477:
			case 478:
			case 481:

				//- Any command requiring 'chanop' privileges (such as
				//MODE messages) MUST return this error if the client
				//making the attempt is not a chanop on the specified
				//channel.
				//482    ERR_CHANOPRIVSNEEDED
				//"<channel> :You're not channel operator"
			case 482:

				//- Any attempts to use the KILL command on a server
				//are to be refused and this error returned directly
				//to the client.
				//483    ERR_CANTKILLSERVER
				//":You can't kill a server!"
			case 483:

				//- Sent by the server to a user upon connection to indicate
				//the restricted nature of the connection (user mode "+r").
				//484    ERR_RESTRICTED
				//":Your connection is restricted!"
			case 484:

				//- Any MODE requiring "channel creator" privileges MUST
				//return this error if the client making the attempt is not
				//a chanop on the specified channel.
				//485    ERR_UNIQOPPRIVSNEEDED
				//":You're not the original channel operator"
			case 485:

				//- If a client sends an OPER message and the server has
				//not been configured to allow connections from the
				//client's host as an operator, this error MUST be
				//returned.
				//491    ERR_NOOPERHOST
				//":No O-lines for your host"
			case 491:

				//- Returned by the server to indicate that a MODE
				//message was sent with a nickname parameter and that
				//the a mode flag sent was not recognized.
				//501    ERR_UMODEUNKNOWNFLAG
				//":Unknown MODE flag"
			case 501:

				//- Error sent to any user trying to view or change the
				//user mode for a user other than themselves.
				//502    ERR_USERSDONTMATCH
				//":Cannot change mode for other users"
			case 502:
				if (sRawMessage.Mid(iIndex, 1) == _T(":"))
					++iIndex;
				m_pwndIRC->AddStatus(sRawMessage.Mid(iIndex), true, uCommand);
				return;
			}
		}
	} CATCH_MFC_EXCEPTION(_T(__FUNCTION__))
		catch (const CString &e) {
		m_pwndIRC->AddStatus(e);
	} CATCH_DFLT_ALL(_T(__FUNCTION__))
}

void CIrcMain::SendLogin()
{
	try {
		m_pIRCSocket->SendString(m_sUser);
		m_pIRCSocket->SendString(_T("NICK ") + m_sNick);
	} CATCH_DFLT_EXCEPTIONS(_T(__FUNCTION__))
	CATCH_DFLT_ALL(_T(__FUNCTION__))
}

void CIrcMain::ParsePerform()
{
	//We need to do the perform first and separate from the help option.
	//This allows you to do all your passwords and stuff before joining the
	//help channel and to keep both options from interfering with each other.
	if (thePrefs.GetIRCUsePerform())
		PerformString(thePrefs.GetIRCPerformString());

// NOTE: putting this IRC command string into the language resource file is not a good idea. Most
// translators do not know that this resource string does NOT have to be translated.

// Well, I meant to make this option a static perform string within the language so the default help could
// be changed to whatever channel by just changing the language. I will just have to check these strings
// before release.
// This also allows the help string to do more then join one channel. It could add other features later.
	if (thePrefs.GetIRCJoinHelpChannel())
		PerformString(GetResString(IDS_IRC_HELPCHANNELPERFORM));
}

void CIrcMain::Connect()
{
	try {
		const CString &sHash(md4str(thePrefs.GetUserHash()));
		CString sIdent;
		sIdent.Format(_T("%u%s"), thePrefs.GetLanguageID(), (LPCTSTR)sHash);
		if (m_pIRCSocket)
			Disconnect();
		m_pIRCSocket = new CIrcSocket(this);
		m_sNick = thePrefs.GetIRCNick();
		if (m_sNick.CompareNoCase(_T("emule")) == 0)
			m_sNick.Format(_T("eMuleIRC%hu-%s"), thePrefs.GetLanguageID(), (LPCTSTR)sHash);
		m_sNick = m_sNick.Left(25);
		m_sVersion.Format(_T("eMule%s%s"), (LPCTSTR)theApp.m_strCurVersionLong, (LPCTSTR)Irc_Version);
		m_sUser.Format(_T("USER %s 8 * :%s"), (LPCTSTR)sIdent, (LPCTSTR)m_sVersion);
		m_pIRCSocket->Create();
		m_pwndIRC->AddStatus(GetResString(IDS_CONNECTING));
		m_pIRCSocket->Connect();
	} CATCH_DFLT_EXCEPTIONS(_T(__FUNCTION__))
	CATCH_DFLT_ALL(_T(__FUNCTION__))
}

void CIrcMain::Disconnect(bool bIsShuttingDown)
{
	try {
		m_pIRCSocket->Close();
		delete m_pIRCSocket;
		m_pIRCSocket = NULL;
		m_sPreParseBufferA.Empty();
		if (!bIsShuttingDown)
			m_pwndIRC->SetConnectStatus(false);
	} CATCH_DFLT_EXCEPTIONS(_T(__FUNCTION__))
	CATCH_DFLT_ALL(_T(__FUNCTION__))
}

void CIrcMain::SetConnectStatus(bool bConnected)
{
	try {
		m_pwndIRC->SetConnectStatus(bConnected);
	} CATCH_DFLT_EXCEPTIONS(_T(__FUNCTION__))
	CATCH_DFLT_ALL(_T(__FUNCTION__))
}

int CIrcMain::SendString(const CString &sSend)
{
	return m_pIRCSocket->SendString(sSend);
}

void CIrcMain::SetIRCWnd(CIrcWnd *pwndIRC)
{
	m_pwndIRC = pwndIRC;
}

uint32 CIrcMain::SetVerify()
{
	m_uVerify = GetRandomUInt32();
	return m_uVerify;
}

CString CIrcMain::GetNick() const
{
	return m_sNick;
}

void CIrcMain::PerformString(const CString &sPerform)
{
	try {
		for (int iPos = 0; iPos >= 0;) {
			CString sCommand(sPerform.Tokenize(_T("|"), iPos));
			if (sCommand.Trim().IsEmpty())
				continue;
			if (sCommand[0] == _T('/'))
				sCommand.Delete(0, 1);
			if (sCommand.Left(3) == _T("msg"))
				sCommand = _T("PRIVMSG") + sCommand.Mid(3);
			if (sCommand.Left(16).CompareNoCase(_T("PRIVMSG nickserv")) == 0)
				sCommand = _T("ns") + sCommand.Mid(16);
			else if (sCommand.Left(16).CompareNoCase(_T("PRIVMSG chanserv")) == 0)
				sCommand = _T("cs") + sCommand.Mid(16);
			m_pIRCSocket->SendString(sCommand);
		}
	} CATCH_DFLT_EXCEPTIONS(_T(__FUNCTION__))
	CATCH_DFLT_ALL(_T(__FUNCTION__))
}