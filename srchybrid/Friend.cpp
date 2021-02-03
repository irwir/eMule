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
#include "Friend.h"
#include "FriendList.h"
#include "OtherFunctions.h"
#include "UpDownClient.h"
#include "Packets.h"
#include "SafeFile.h"
#include "clientlist.h"
#include "ListenSocket.h"
#include "Kademlia/Kademlia/Kademlia.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void CFriend::init()
{
	md4clr(m_abyKadID);
	m_friendSlot = false;
	m_dwLastKadSearch = 0;
	m_FriendConnectState = FCS_NONE;
	m_LinkedClient = NULL;
}

CFriend::CFriend()
	: m_abyUserhash()
	, m_dwLastSeen()
	, m_dwLastChatted()
	, m_strName()
	, m_dwLastUsedIP()
	, m_nLastUsedPort()
{
	init();
}

//Added this to work with the IRC. Probably a better way to do it. But wanted this in the release.
CFriend::CFriend(const uchar *abyUserhash, time_t dwLastSeen, uint32 dwLastUsedIP, uint16 nLastUsedPort
	, uint32 dwLastChatted, LPCTSTR pszName, uint32 dwHasHash)
	: m_dwLastSeen(dwLastSeen)
	, m_dwLastChatted(dwLastChatted)
	, m_strName(pszName)
	, m_dwLastUsedIP(dwLastUsedIP)
	, m_nLastUsedPort(nLastUsedPort)
{
	init();
	if (dwHasHash && abyUserhash)
		md4cpy(m_abyUserhash, abyUserhash);
	else
		md4clr(m_abyUserhash);
}

CFriend::CFriend(CUpDownClient *client)
	: m_dwLastSeen(time(NULL))
	, m_dwLastChatted()
{
	ASSERT(client);
	init();
	m_dwLastUsedIP = client->GetIP();
	m_nLastUsedPort = client->GetUserPort();
	md4cpy(m_abyUserhash, client->GetUserHash());
	SetLinkedClient(client);
}

CFriend::~CFriend()
{
	if (GetLinkedClient(true) != NULL) {
		m_LinkedClient->SetFriendSlot(false);
		m_LinkedClient->m_Friend = NULL;
		m_LinkedClient = NULL;
	}
	// remove any possible pending kad request
	if (Kademlia::CKademlia::IsRunning())
		Kademlia::CKademlia::CancelClientSearch(*this);
}

void CFriend::LoadFromFile(CFileDataIO *file)
{
	file->ReadHash16(m_abyUserhash);
	m_dwLastUsedIP = file->ReadUInt32();
	m_nLastUsedPort = file->ReadUInt16();
	m_dwLastSeen = file->ReadUInt32();
	m_dwLastChatted = file->ReadUInt32();

	for (uint32 tagcount = file->ReadUInt32(); tagcount > 0; --tagcount) {
		const CTag *newtag = new CTag(file, false);
		switch (newtag->GetNameID()) {
		case FF_NAME:
			ASSERT(newtag->IsStr());
			if (newtag->IsStr() && m_strName.IsEmpty())
				m_strName = newtag->GetStr();
			break;
		case FF_KADID:
			ASSERT(newtag->IsHash());
			if (newtag->IsHash())
				md4cpy(m_abyKadID, newtag->GetHash());
		}
		delete newtag;
	}
}

void CFriend::WriteToFile(CFileDataIO *file)
{
	file->WriteHash16(m_abyUserhash);
	file->WriteUInt32(m_dwLastUsedIP);
	file->WriteUInt16(m_nLastUsedPort);
	file->WriteUInt32((uint32)m_dwLastSeen);
	file->WriteUInt32((uint32)m_dwLastChatted);

	uint32 uTagCount = 0;
	ULONGLONG uTagCountFilePos = file->GetPosition();
	file->WriteUInt32(0);

	if (!m_strName.IsEmpty()) {
		CTag nametag(FF_NAME, m_strName);
		nametag.WriteTagToFile(file, UTF8strOptBOM);
		++uTagCount;
	}
	if (HasKadID()) {
		CTag tag(FF_KADID, (BYTE*)m_abyKadID);
		tag.WriteNewEd2kTag(file);
		++uTagCount;
	}

	file->Seek(uTagCountFilePos, CFile::begin);
	file->WriteUInt32(uTagCount);
	file->Seek(0, CFile::end);
}

bool CFriend::HasUserhash() const
{
	return !isnulmd4(m_abyUserhash); //isbadhash()
}

bool CFriend::HasKadID() const
{
	return !isnulmd4(m_abyKadID);
}

void CFriend::SetFriendSlot(bool newValue)
{
	if (GetLinkedClient() != NULL)
		m_LinkedClient->SetFriendSlot(newValue);

	m_friendSlot = newValue;
}

bool CFriend::GetFriendSlot() const
{
	return GetLinkedClient() ? m_LinkedClient->GetFriendSlot() : m_friendSlot;
}

void CFriend::SetLinkedClient(CUpDownClient *linkedClient)
{
	if (linkedClient != m_LinkedClient) {
		if (linkedClient != NULL) {
			if (m_LinkedClient == NULL)
				linkedClient->SetFriendSlot(m_friendSlot);
			else
				linkedClient->SetFriendSlot(m_LinkedClient->GetFriendSlot());

			m_dwLastSeen = time(NULL);
			m_dwLastUsedIP = linkedClient->GetConnectIP();
			m_nLastUsedPort = linkedClient->GetUserPort();
			m_strName = linkedClient->GetUserName();
			md4cpy(m_abyUserhash, linkedClient->GetUserHash());

			linkedClient->m_Friend = this;
		} else if (m_LinkedClient != NULL)
			m_friendSlot = m_LinkedClient->GetFriendSlot();

		if (m_LinkedClient != NULL) {
			// the old client is no longer friend, since it is no longer the linked client
			m_LinkedClient->SetFriendSlot(false);
			m_LinkedClient->m_Friend = NULL;
		}

		m_LinkedClient = linkedClient;
	}
	theApp.friendlist->RefreshFriend(this);
}

CUpDownClient* CFriend::GetLinkedClient(bool bValidCheck) const
{
	if (bValidCheck && m_LinkedClient != NULL && !theApp.clientlist->IsValidClient(m_LinkedClient)) {
		ASSERT(0);
		return NULL;
	}
	return m_LinkedClient;
};

CUpDownClient* CFriend::GetClientForChatSession()
{
	CUpDownClient *pResult;
	if (GetLinkedClient(true) != NULL)
		pResult = GetLinkedClient(false);
	else {
		pResult = new CUpDownClient(0, m_nLastUsedPort, m_dwLastUsedIP, 0, 0, true);
		pResult->SetUserName(m_strName);
		pResult->SetUserHash(m_abyUserhash);
		theApp.clientlist->AddClient(pResult);
		SetLinkedClient(pResult);
	}
	pResult->SetChatState(MS_CHATTING);
	return pResult;
};

bool CFriend::TryToConnect(CFriendConnectionListener *pConnectionReport)
{
	if (m_FriendConnectState != FCS_NONE) {
		m_liConnectionReport.AddTail(pConnectionReport);
		return true;
	}
	if (isnulmd4(m_abyKadID) && (m_dwLastUsedIP == 0 || m_nLastUsedPort == 0)
		&& (GetLinkedClient() == NULL || GetLinkedClient()->GetIP() == 0 || GetLinkedClient()->GetUserPort() == 0))
	{
		pConnectionReport->ReportConnectionProgress(m_LinkedClient, _T("*** ") + GetResString(IDS_CONNECTING), false);
		pConnectionReport->ConnectingResult(GetLinkedClient(), false);
		return false;
	}

	m_liConnectionReport.AddTail(pConnectionReport);
	if (GetLinkedClient(true) == NULL) {
		//ASSERT( pConnectionReport != &theApp.emuledlg->chatwnd->chatselector ); // shouldn't happen, if the chat connector calls, we he always should have a client for the session already
		ASSERT(0);
		GetClientForChatSession();
	}
	ASSERT(GetLinkedClient(true) != NULL);
	m_FriendConnectState = FCS_CONNECTING;
	m_LinkedClient->SetChatState(MS_CONNECTING);
	if (m_LinkedClient->socket != NULL && m_LinkedClient->socket->IsConnected()) {
		// this client is already connected, but we need to check if it has also passed the secureident already
		UpdateFriendConnectionState(FCR_ESTABLISHED);
	}
	// otherwise (standard case) try to connect
	pConnectionReport->ReportConnectionProgress(m_LinkedClient, _T("*** ") + GetResString(IDS_CONNECTING), false);
	m_LinkedClient->TryToConnect(true);
	return true;
}

void CFriend::UpdateFriendConnectionState(EFriendConnectReport eEvent)
{
	/*#ifdef _DEBUG
		CString strDbg;
		strDbg.Format(_T("*** Debug: UpdateFriendConnectionState, Report: %u, CurrentState: %u \n"), eEvent, m_FriendConnectState);
		for (POSITION pos = m_liConnectionReport.GetHeadPosition(); pos != NULL;)
			m_liConnectionReport.GetNext(pos)->ReportConnectionProgress(GetLinkedClient(), strDbg, false);
	#endif*/
	if (m_FriendConnectState == FCS_NONE || (GetLinkedClient(true) == NULL && eEvent != FCR_DELETED)) {
		// we aren't currently trying to build up a friend connection, we shouldn't be called
		ASSERT(0);
		return;
	}
	switch (eEvent) {
	case FCR_ESTABLISHED:
	case FCR_USERHASHVERIFIED:
		// connection established, userhash fits, check secureident
		if (GetLinkedClient()->HasPassedSecureIdent(true)) {
			// well here we are done, connecting worked out fine
			m_FriendConnectState = FCS_NONE;
			for (POSITION pos = m_liConnectionReport.GetHeadPosition(); pos != NULL;)
				m_liConnectionReport.GetNext(pos)->ConnectingResult(GetLinkedClient(), true);
			m_liConnectionReport.RemoveAll();
			FindKadID(); // fetch the kad id of this friend if we don't have it already
		} else {
			ASSERT(eEvent != FCR_USERHASHVERIFIED);
			// we connected, the userhash matches, now we wait for the authentication
			// nothing todo, just report about it
			for (POSITION pos = m_liConnectionReport.GetHeadPosition(); pos != NULL;) {
				CFriendConnectionListener *flistener = m_liConnectionReport.GetNext(pos);
				flistener->ReportConnectionProgress(GetLinkedClient(), _T(" ...") + GetResString(IDS_TREEOPTIONS_OK) + _T('\n'), true);
				flistener->ReportConnectionProgress(GetLinkedClient(), _T("*** ") + CString(_T("Authenticating friend")) /*to stringlist*/, false);
			}
			if (m_FriendConnectState == FCS_CONNECTING)
				m_FriendConnectState = FCS_AUTH;
			else { // client must have connected to use while we tried something else (like search for him in kad)
				ASSERT(0);
				m_FriendConnectState = FCS_AUTH;
			}
		}
		break;
	case FCR_DISCONNECTED:
		// disconnected, lets see which state we were in
		if (m_FriendConnectState == FCS_CONNECTING || m_FriendConnectState == FCS_AUTH) {
			if (m_FriendConnectState == FCS_CONNECTING && Kademlia::CKademlia::IsRunning()
				&& Kademlia::CKademlia::IsConnected() && !isnulmd4(m_abyKadID)
				&& (m_dwLastKadSearch == 0 || ::GetTickCount() >= m_dwLastKadSearch + MIN2MS(10)))
			{
				// connecting failed to the last known IP, now we search kad for an updated IP of our friend
				m_FriendConnectState = FCS_KADSEARCHING;
				m_dwLastKadSearch = ::GetTickCount();
				for (POSITION pos = m_liConnectionReport.GetHeadPosition(); pos != NULL;) {
					CFriendConnectionListener *flistener = m_liConnectionReport.GetNext(pos);
					flistener->ReportConnectionProgress(GetLinkedClient(), _T(" ...") + GetResString(IDS_FAILED) + _T('\n'), true);
					flistener->ReportConnectionProgress(GetLinkedClient(), _T("*** ") + GetResString(IDS_SEARCHINGFRIENDKAD), false);
				}
				Kademlia::CKademlia::FindIPByNodeID(*this, m_abyKadID);
				break;
			}
			m_FriendConnectState = FCS_NONE;
			for (POSITION pos = m_liConnectionReport.GetHeadPosition(); pos != NULL;)
				m_liConnectionReport.GetNext(pos)->ConnectingResult(GetLinkedClient(), false);
			m_liConnectionReport.RemoveAll();
		} else // FCS_KADSEARCHING, shouldn't happen
			ASSERT(0);
		break;
	case FCR_USERHASHFAILED:
		{
			// the client we connected to, had a different userhash then we expected
			// drop the linked client object and create a new one, because we don't want to have anything todo
			// with this instance as it is not our friend which we try to connect to
			// the connection try counts as failed
			CUpDownClient *pOld = m_LinkedClient;
			SetLinkedClient(NULL); // removing old one
			GetClientForChatSession(); // creating new instance with the hash we search for
			m_LinkedClient->SetChatState(MS_CONNECTING);
			for (POSITION pos = m_liConnectionReport.GetHeadPosition(); pos != NULL;) // inform others about the change
				m_liConnectionReport.GetNext(pos)->ClientObjectChanged(pOld, GetLinkedClient());
			pOld->SetChatState(MS_NONE);

			if (m_FriendConnectState == FCS_CONNECTING || m_FriendConnectState == FCS_AUTH) {
				ASSERT(m_FriendConnectState == FCS_AUTH);
				// todo: kad here
				m_FriendConnectState = FCS_NONE;
				for (POSITION pos = m_liConnectionReport.GetHeadPosition(); pos != NULL;)
					m_liConnectionReport.GetNext(pos)->ConnectingResult(GetLinkedClient(), false);
				m_liConnectionReport.RemoveAll();
			} else // FCS_KADSEARCHING, shouldn't happen
				ASSERT(0);
			break;
		}
	case FCR_SECUREIDENTFAILED:
		// the client has the fitting userhash, but failed secureident - so we don't want to talk to him
		// we stop our search here in any case, multiple clientobjects with the same userhash would mess with other things
		// and its unlikely that we would find him on kad in this case too
		ASSERT(m_FriendConnectState == FCS_AUTH);
		m_FriendConnectState = FCS_NONE;
		for (POSITION pos = m_liConnectionReport.GetHeadPosition(); pos != NULL;)
			m_liConnectionReport.GetNext(pos)->ConnectingResult(GetLinkedClient(), false);
		m_liConnectionReport.RemoveAll();
		break;
	case FCR_DELETED:
		// mh, this should actually never happen I'm sure
		// todo: in any case, stop any connection tries, notify other etc
		m_FriendConnectState = FCS_NONE;
		for (POSITION pos = m_liConnectionReport.GetHeadPosition(); pos != NULL;)
			m_liConnectionReport.GetNext(pos)->ConnectingResult(GetLinkedClient(), false);
		m_liConnectionReport.RemoveAll();
		break;
	default:
		ASSERT(0);
	}
}

void CFriend::FindKadID()
{
	if (!HasKadID() && Kademlia::CKademlia::IsRunning() && GetLinkedClient(true) != NULL
		&& !GetLinkedClient()->GetKadPort() && GetLinkedClient()->GetKadVersion() >= KADEMLIA_VERSION2_47a)
	{
		DebugLog(_T("Searching KadID for friend %s by IP %s"), m_strName.IsEmpty() ? _T("(Unknown)") : (LPCTSTR)m_strName, (LPCTSTR)ipstr(GetLinkedClient()->GetConnectIP()));
		Kademlia::CKademlia::FindNodeIDByIP(*this, GetLinkedClient()->GetConnectIP(), GetLinkedClient()->GetUserPort(), GetLinkedClient()->GetKadPort());
	}
}

void CFriend::KadSearchNodeIDByIPResult(Kademlia::EKadClientSearchRes eStatus, const uchar *pachNodeID)
{
	if (!theApp.friendlist->IsValid(this)) {
		ASSERT(0);
		return;
	}
	if (eStatus == Kademlia::KCSR_SUCCEEDED) {
		ASSERT(pachNodeID != NULL);
		DebugLog(_T("Successfully fetched KadID (%s) for friend %s"), (LPCTSTR)md4str(pachNodeID), m_strName.IsEmpty() ? _T("(Unknown)") : (LPCTSTR)m_strName);
		md4cpy(m_abyKadID, pachNodeID);
	} else
		DebugLog(_T("Failed to fetch KadID for friend %s (%s)"), m_strName.IsEmpty() ? _T("(Unknown)") : (LPCTSTR)m_strName, (LPCTSTR)ipstr(m_dwLastUsedIP));
}

void CFriend::KadSearchIPByNodeIDResult(Kademlia::EKadClientSearchRes eStatus, uint32 dwIP, uint16 nPort)
{
	if (!theApp.friendlist->IsValid(this)) {
		ASSERT(0);
		return;
	}
	if (m_FriendConnectState == FCS_KADSEARCHING) {
		if (eStatus == Kademlia::KCSR_SUCCEEDED && GetLinkedClient(true) != NULL) {
			DebugLog(_T("Successfully fetched IP (%s) by KadID (%s) for friend %s"), (LPCTSTR)ipstr(dwIP), (LPCTSTR)md4str(m_abyKadID), m_strName.IsEmpty() ? _T("(Unknown)") : (LPCTSTR)m_strName);
			if (GetLinkedClient()->GetIP() != dwIP || GetLinkedClient()->GetUserPort() != nPort) {
				// retry to connect with our new found IP
				for (POSITION pos = m_liConnectionReport.GetHeadPosition(); pos != NULL;) {
					CFriendConnectionListener *flistener = m_liConnectionReport.GetNext(pos);
					flistener->ReportConnectionProgress(GetLinkedClient(), _T(" ...") + GetResString(IDS_FOUND) + _T('\n'), true);
					flistener->ReportConnectionProgress(m_LinkedClient, _T("*** ") + GetResString(IDS_CONNECTING), false);
				}
				m_FriendConnectState = FCS_CONNECTING;
				m_LinkedClient->SetChatState(MS_CONNECTING);
				if (m_LinkedClient->socket != NULL && m_LinkedClient->socket->IsConnected()) {
					// we shouldn't get here since we checked for FCS_KADSEARCHING
					ASSERT(0);
					UpdateFriendConnectionState(FCR_ESTABLISHED);
				}
				m_dwLastUsedIP = dwIP;
				m_nLastUsedPort = nPort;
				m_LinkedClient->SetConnectIP(dwIP);
				m_LinkedClient->SetUserPort(nPort);
				m_LinkedClient->TryToConnect(true);
				return;
			}
			DebugLog(_T("KadSearchIPByNodeIDResult: Result IP is the same as known (not working) IP (%s)"), (LPCTSTR)ipstr(dwIP));
		}
		DebugLog(_T("Failed to fetch IP by KadID (%s) for friend %s"), (LPCTSTR)md4str(m_abyKadID), m_strName.IsEmpty() ? _T("(Unknown)") : (LPCTSTR)m_strName);
		// here ends our journey to connect to our friend unsuccessfully
		m_FriendConnectState = FCS_NONE;
		for (POSITION pos = m_liConnectionReport.GetHeadPosition(); pos != NULL;)
			m_liConnectionReport.GetNext(pos)->ConnectingResult(GetLinkedClient(), false);
		m_liConnectionReport.RemoveAll();
	} else
		ASSERT(0);
}