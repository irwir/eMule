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
#include "ClientList.h"
#include "otherfunctions.h"
#include "Kademlia/Kademlia/kademlia.h"
#include "Kademlia/Kademlia/prefs.h"
#include "Kademlia/Kademlia/search.h"
#include "Kademlia/Kademlia/searchmanager.h"
#include "Kademlia/routing/contact.h"
#include "Kademlia/net/kademliaudplistener.h"
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "kademlia/utils/UInt128.h"
#include "LastCommonRouteFinder.h"
#include "UploadQueue.h"
#include "DownloadQueue.h"
#include "UpDownClient.h"
#include "ClientCredits.h"
#include "ListenSocket.h"
#include "Opcodes.h"
#include "ServerConnect.h"
#include "emuledlg.h"
#include "TransferDlg.h"
#include "serverwnd.h"
#include "Log.h"
#include "packets.h"
#include "Statistics.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CClientList::CClientList()
	: m_pBuddy()
	, m_nBuddyStatus(Disconnected)
{
	m_dwLastBanCleanUp = m_dwLastTrackedCleanUp = m_dwLastClientCleanUp = ::GetTickCount();
	m_bannedList.InitHashTable(331);
	m_trackedClientsList.InitHashTable(2011);
	m_globDeadSourceList.Init(true);
}

CClientList::~CClientList()
{
	RemoveAllTrackedClients();
}

void CClientList::GetStatistics(uint32 &ruTotalClients
	, int stats[NUM_CLIENTLIST_STATS]
	, CMap<uint32, uint32, uint32, uint32> &clientVersionEDonkey
	, CMap<uint32, uint32, uint32, uint32> &clientVersionEDonkeyHybrid
	, CMap<uint32, uint32, uint32, uint32> &clientVersionEMule
	, CMap<uint32, uint32, uint32, uint32> &clientVersionAMule)
{
	ruTotalClients = (uint32)list.GetCount();
	memset(stats, 0, NUM_CLIENTLIST_STATS * sizeof stats[0]);

	for (POSITION pos = list.GetHeadPosition(); pos != NULL; ) {
		const CUpDownClient *cur_client = list.GetNext(pos);

		if (cur_client->HasLowID())
			stats[14]++;

		switch (cur_client->GetClientSoft()) {
		case SO_EMULE:
		case SO_OLDEMULE:
			stats[2]++;
			clientVersionEMule[cur_client->GetVersion()]++;
			break;
		case SO_EDONKEYHYBRID:
			stats[4]++;
			clientVersionEDonkeyHybrid[cur_client->GetVersion()]++;
			break;
		case SO_AMULE:
			stats[10]++;
			clientVersionAMule[cur_client->GetVersion()]++;
			break;
		case SO_EDONKEY:
			stats[1]++;
			clientVersionEDonkey[cur_client->GetVersion()]++;
			break;
		case SO_MLDONKEY:
			stats[3]++;
			break;
		case SO_SHAREAZA:
			stats[11]++;
			break;

		// all remaining 'eMule Compatible' clients
		case SO_CDONKEY:
		case SO_XMULE:
		case SO_LPHANT:
			stats[5]++;
			break;

		default:
			stats[0]++;
		}

		if (cur_client->Credits() != NULL) {
			switch (cur_client->Credits()->GetCurrentIdentState(cur_client->GetIP())) {
			case IS_IDENTIFIED:
				stats[12]++;
				break;
			case IS_IDFAILED:
			case IS_IDNEEDED:
			case IS_IDBADGUY:
				stats[13]++;
			}
		}

		if (cur_client->GetDownloadState() == DS_ERROR)
			stats[6]++; // Error

		if (cur_client->GetUserPort() == 4662)
			stats[8]++; // Default Port
		else
			stats[9]++; // Other Port

		// Network client stats
		if (cur_client->GetServerIP() && cur_client->GetServerPort()) {
			stats[15]++;		// eD2K
			if (cur_client->GetKadPort()) {
				stats[17]++;	// eD2K/Kad
				stats[16]++;	// Kad
			}
		} else if (cur_client->GetKadPort())
			stats[16]++;		// Kad
		else
			stats[18]++;		// Unknown
	}
}

void CClientList::AddClient(CUpDownClient *toadd, bool bSkipDupTest)
{
	// skipping the check for duplicate list entries is only to be done for optimization purposes, if the calling
	// function has ensured that this client instance is not already within the list -> there are never duplicate
	// client instances in this list.
	if (bSkipDupTest || list.Find(toadd) == NULL) {
		theApp.emuledlg->transferwnd->GetClientList()->AddClient(toadd);
		list.AddTail(toadd);
	}
}

// ZZ:UploadSpeedSense -->
bool CClientList::GiveClientsForTraceRoute()
{
	// this is a host that lastCommonRouteFinder can use to traceroute
	return theApp.lastCommonRouteFinder->AddHostsToCheck(list);
}
// ZZ:UploadSpeedSense <--

void CClientList::RemoveClient(CUpDownClient *toremove, LPCTSTR pszReason)
{
	POSITION pos = list.Find(toremove);
	if (pos) {
		theApp.uploadqueue->RemoveFromUploadQueue(toremove, CString(_T("CClientList::RemoveClient: ")) + pszReason);
		theApp.uploadqueue->RemoveFromWaitingQueue(toremove);
		theApp.downloadqueue->RemoveSource(toremove);
		theApp.emuledlg->transferwnd->GetClientList()->RemoveClient(toremove);
		list.RemoveAt(pos);
	}
	RemoveFromKadList(toremove);
	RemoveConnectingClient(toremove);
}

void CClientList::DeleteAll()
{
	theApp.uploadqueue->DeleteAll();
	theApp.downloadqueue->DeleteAll();
	while (!list.IsEmpty())
		delete list.RemoveHead(); // recursive: this will call RemoveClient
}

bool CClientList::AttachToAlreadyKnown(CUpDownClient **client, CClientReqSocket *sender)
{
	CUpDownClient *tocheck = *client;
	CUpDownClient *found_client = NULL;
	CUpDownClient *found_client2 = NULL;
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = list.GetNext(pos);
		if (tocheck->Compare(cur_client, false)) //matching userhash
			found_client2 = cur_client;

		if (tocheck->Compare(cur_client, true)) { //matching IP
			found_client = cur_client;
			break;
		}
	}
	if (found_client == NULL)
		found_client = found_client2;
	if (found_client == NULL)
		return false;
	if (found_client == tocheck) {
		//we found the same client instance (client may have sent more than one OP_HELLO). do not delete that client!
		return true;
	}
	if (sender) {
		if (found_client->socket) {
			if (found_client->socket->IsConnected()
				&& (found_client->GetIP() != tocheck->GetIP() || found_client->GetUserPort() != tocheck->GetUserPort()))
			{
				if (found_client->Credits() && found_client->Credits()->GetCurrentIdentState(found_client->GetIP()) == IS_IDENTIFIED) {
					// if found_client is connected and has the IS_IDENTIFIED, it's safe to say that the other one is a bad guy
					if (thePrefs.GetLogBannedClients())
						AddDebugLogLine(false, _T("Clients: %s (%s), Ban reason: Userhash invalid"), tocheck->GetUserName(), (LPCTSTR)ipstr(tocheck->GetConnectIP()));
					tocheck->Ban();
				} else if (thePrefs.GetLogBannedClients()) {
					//IDS_CLIENTCOL Warning: Found matching client, to a currently connected client: %s (%s) and %s (%s)
					AddDebugLogLine(true, (LPCTSTR)GetResString(IDS_CLIENTCOL)
						, tocheck->GetUserName(), (LPCTSTR)ipstr(tocheck->GetConnectIP())
						, found_client->GetUserName(), (LPCTSTR)ipstr(found_client->GetConnectIP()));
				}
				return false;
			}
			found_client->socket->client = NULL;
			found_client->socket->Safe_Delete();
		}
		found_client->socket = sender;
		tocheck->socket = NULL;
	}
	*client = NULL;
//***	found_client->SetSourceFrom(tocheck->GetSourceFrom());
	delete tocheck;
	*client = found_client;
	return true;
}

CUpDownClient *CClientList::FindClientByConnIP(uint32 clientip, UINT port) const
{
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = list.GetNext(pos);
		if (cur_client->GetConnectIP() == clientip && cur_client->GetUserPort() == port)
			return cur_client;
	}
	return NULL;
}

CUpDownClient* CClientList::FindClientByIP(uint32 clientip, UINT port) const
{
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = list.GetNext(pos);
		if (cur_client->GetIP() == clientip && cur_client->GetUserPort() == port)
			return cur_client;
	}
	return NULL;
}

CUpDownClient* CClientList::FindClientByUserHash(const uchar *clienthash, uint32 dwIP, uint16 nTCPPort) const
{
	CUpDownClient *pFound = NULL;
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = list.GetNext(pos);
		if (md4equ(cur_client->GetUserHash(), clienthash)) {
			if ((dwIP == 0 || dwIP == cur_client->GetConnectIP()) && (nTCPPort == 0 || nTCPPort == cur_client->GetUserPort()))
				return cur_client;
			if (pFound == NULL)
				pFound = cur_client;
		}
	}
	return pFound;
}

CUpDownClient* CClientList::FindClientByIP(uint32 clientip) const
{
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = list.GetNext(pos);
		if (cur_client->GetIP() == clientip)
			return cur_client;
	}
	return NULL;
}

CUpDownClient* CClientList::FindClientByIP_UDP(uint32 clientip, UINT nUDPport) const
{
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = list.GetNext(pos);
		if (cur_client->GetIP() == clientip && cur_client->GetUDPPort() == nUDPport)
			return cur_client;
	}
	return NULL;
}

CUpDownClient* CClientList::FindClientByUserID_KadPort(uint32 clientID, uint16 kadPort) const
{
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = list.GetNext(pos);
		if (cur_client->GetUserIDHybrid() == clientID && cur_client->GetKadPort() == kadPort)
			return cur_client;
	}
	return NULL;
}

CUpDownClient* CClientList::FindClientByIP_KadPort(uint32 ip, uint16 port) const
{
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = list.GetNext(pos);
		if (cur_client->GetIP() == ip && cur_client->GetKadPort() == port)
			return cur_client;
	}
	return NULL;
}

CUpDownClient* CClientList::FindClientByServerID(uint32 uServerIP, uint32 uED2KUserID) const
{
	uint32 uHybridUserID = ntohl(uED2KUserID);
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = list.GetNext(pos);
		if (cur_client->GetServerIP() == uServerIP && cur_client->GetUserIDHybrid() == uHybridUserID)
			return cur_client;
	}
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// Banned clients

void CClientList::AddBannedClient(uint32 dwIP)
{
	m_bannedList.SetAt(dwIP, ::GetTickCount());
}

bool CClientList::IsBannedClient(uint32 dwIP) const
{
	DWORD dwBantime;
	return m_bannedList.Lookup(dwIP, dwBantime) && (::GetTickCount() < dwBantime + CLIENTBANTIME);
}

void CClientList::RemoveBannedClient(uint32 dwIP)
{
	m_bannedList.RemoveKey(dwIP);
}

void CClientList::RemoveAllBannedClients()
{
	m_bannedList.RemoveAll();
}


///////////////////////////////////////////////////////////////////////////////
// Tracked clients

void CClientList::AddTrackClient(CUpDownClient *toadd)
{
	CDeletedClient *pResult = NULL;
	if (m_trackedClientsList.Lookup(toadd->GetIP(), pResult)) {
		pResult->m_dwInserted = ::GetTickCount();
		for (int i = 0; i != pResult->m_ItemsList.GetCount(); ++i)
			if (pResult->m_ItemsList[i].nPort == toadd->GetUserPort()) {
				// already tracked, update
				pResult->m_ItemsList[i].pHash = toadd->Credits();
				return;
			}

		pResult->m_ItemsList.Add(PORTANDHASH{toadd->GetUserPort(), toadd->Credits()});
	} else
		m_trackedClientsList.SetAt(toadd->GetIP(), new CDeletedClient(toadd));
}

// true = everything OK, hash didn't change
// false = hash changed
bool CClientList::ComparePriorUserhash(uint32 dwIP, uint16 nPort, void *pNewHash)
{
	CDeletedClient *pResult = NULL;
	if (m_trackedClientsList.Lookup(dwIP, pResult)) {
		for (int i = 0; i != pResult->m_ItemsList.GetCount(); ++i) {
			if (pResult->m_ItemsList[i].nPort == nPort) {
				if (pResult->m_ItemsList[i].pHash != pNewHash)
					return false;
				break;
			}
		}
	}
	return true;
}

INT_PTR CClientList::GetClientsFromIP(uint32 dwIP) const
{
	CDeletedClient *pResult;
	if (m_trackedClientsList.Lookup(dwIP, pResult))
		return pResult->m_ItemsList.GetCount();
	return 0;
}

void CClientList::TrackBadRequest(const CUpDownClient *upcClient, int nIncreaseCounter)
{
	if (upcClient->GetIP() == 0) {
		ASSERT(0);
		return;
	}
	CDeletedClient *pResult;
	if (m_trackedClientsList.Lookup(upcClient->GetIP(), pResult)) {
		pResult->m_dwInserted = ::GetTickCount();
		pResult->m_cBadRequest += nIncreaseCounter;
	} else {
		CDeletedClient *ccToAdd = new CDeletedClient(upcClient);
		ccToAdd->m_cBadRequest = nIncreaseCounter;
		m_trackedClientsList.SetAt(upcClient->GetIP(), ccToAdd);
	}
}

uint32 CClientList::GetBadRequests(const CUpDownClient *upcClient) const
{
	if (upcClient->GetIP() == 0) {
		ASSERT(0);
		return 0;
	}
	CDeletedClient *pResult;
	if (m_trackedClientsList.Lookup(upcClient->GetIP(), pResult))
		return pResult->m_cBadRequest;
	return 0;
}

void CClientList::RemoveAllTrackedClients()
{
	for (POSITION pos = m_trackedClientsList.GetStartPosition(); pos != NULL;) {
		uint32 nKey;
		CDeletedClient *pResult;
		m_trackedClientsList.GetNextAssoc(pos, nKey, pResult);
		m_trackedClientsList.RemoveKey(nKey);
		delete pResult;
	}
}

void CClientList::Process()
{
	///////////////////////////////////////////////////////////////////////////
	// Cleanup banned client list
	//
	const DWORD tick = ::GetTickCount();
	if (tick >= m_dwLastBanCleanUp + BAN_CLEANUP_TIME) {
		m_dwLastBanCleanUp = tick;

		for (POSITION pos = m_bannedList.GetStartPosition(); pos != NULL;) {
			uint32 nKey;
			DWORD dwBantime;
			m_bannedList.GetNextAssoc(pos, nKey, dwBantime);
			if (tick >= dwBantime + CLIENTBANTIME)
				RemoveBannedClient(nKey);
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Cleanup tracked client list
	//
	if (tick >= m_dwLastTrackedCleanUp + TRACKED_CLEANUP_TIME) {
		m_dwLastTrackedCleanUp = tick;
		if (thePrefs.GetLogBannedClients())
			AddDebugLogLine(false, _T("Cleaning up TrackedClientList, %u clients on List..."), (unsigned)m_trackedClientsList.GetCount());
		for (POSITION pos = m_trackedClientsList.GetStartPosition(); pos != NULL;) {
			uint32 nKey;
			CDeletedClient *pResult;
			m_trackedClientsList.GetNextAssoc(pos, nKey, pResult);
			if (tick >= pResult->m_dwInserted + KEEPTRACK_TIME) {
				m_trackedClientsList.RemoveKey(nKey);
				delete pResult;
			}
		}
		if (thePrefs.GetLogBannedClients())
			AddDebugLogLine(false, _T("...done, %u clients left on list"), (unsigned)m_trackedClientsList.GetCount());
	}

	///////////////////////////////////////////////////////////////////////////
	// Process Kad client list
	//
	//We need to try to connect to the clients in m_KadList
	//If connected, remove them from the list and send a message back to Kad so we can send an ACK.
	//If we don't connect, we need to remove the client.
	//The sockets timeout should delete this object.

	// buddy is just a flag that is used to make sure we are still connected or connecting to a buddy.
	buddyState buddy = Disconnected;

	for (POSITION pos = m_KadList.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = m_KadList.GetNext(pos);
		if (!Kademlia::CKademlia::IsRunning()) {
			//Clear out this list if we stop running Kad.
			//Setting the Kad state to KS_NONE causes it to be removed in the switch below.
			cur_client->SetKadState(KS_NONE);
		}
		switch (cur_client->GetKadState()) {
		case KS_QUEUED_FWCHECK:
		case KS_QUEUED_FWCHECK_UDP:
			//Another client asked us to try to connect to them to check their firewalled status.
			cur_client->TryToConnect(true, true);
			break;
		case KS_CONNECTING_FWCHECK:
			//Ignore this state as we are just waiting for results.
			break;
		case KS_FWCHECK_UDP:
		case KS_CONNECTING_FWCHECK_UDP:
			// we want a UDP firewall check from this client and are just waiting to get connected to send the request
			break;
		case KS_CONNECTED_FWCHECK:
			//We successfully connected to the client.
			//We now send an ack to let them know.
			if (cur_client->GetKadVersion() >= KADEMLIA_VERSION7_49a) {
				// the result is now sent per TCP instead of UDP, because this will fail if our intern UDP port is unreachable.
				// But we want the TCP testresult regardless if UDP is firewalled, the new UDP state and test takes care of the rest
				ASSERT(cur_client->socket != NULL && cur_client->socket->IsConnected());
				if (thePrefs.GetDebugClientTCPLevel() > 0)
					DebugSend("OP_KAD_FWTCPCHECK_ACK", cur_client);
				Packet *pPacket = new Packet(OP_KAD_FWTCPCHECK_ACK, 0, OP_EMULEPROT);
				if (!cur_client->SafeConnectAndSendPacket(pPacket))
					break;
			} else {
				if (thePrefs.GetDebugClientKadUDPLevel() > 0)
					DebugSend("KADEMLIA_FIREWALLED_ACK_RES", cur_client->GetIP(), cur_client->GetKadPort());
				Kademlia::CKademlia::GetUDPListener()->SendNullPacket(KADEMLIA_FIREWALLED_ACK_RES, ntohl(cur_client->GetIP()), cur_client->GetKadPort(), Kademlia::CKadUDPKey(), NULL);
			}
			//We are done with this client. Set Kad status to KS_NONE and it will be removed in the next cycle.
			cur_client->SetKadState(KS_NONE);
			break;
		case KS_INCOMING_BUDDY:
			//A firewalled client wants us to be his buddy.
			//If we already have a buddy, we set Kad state to KS_NONE and it's removed in the next cycle.
			//If not, this client will change to KS_CONNECTED_BUDDY when it connects.
			if (m_nBuddyStatus == Connected)
				cur_client->SetKadState(KS_NONE);
			break;
		case KS_QUEUED_BUDDY:
			//We are firewalled and want to request this client to be a buddy.
			//But first we check to make sure we are not already trying another client.
			//If we are not already trying, we try to connect to this client.
			//If we are already connected to a buddy, we set this client to KS_NONE and it's removed in the next cycle.
			//If we are trying to connect to a buddy, we just ignore as the one we are trying may fail and we can then try this one.
			if (m_nBuddyStatus == Disconnected) {
				buddy = Connecting;
				m_nBuddyStatus = Connecting;
				cur_client->SetKadState(KS_CONNECTING_BUDDY);
				cur_client->TryToConnect(true, true);
				theApp.emuledlg->serverwnd->UpdateMyInfo();
			} else if (m_nBuddyStatus == Connected)
				cur_client->SetKadState(KS_NONE);
			break;
		case KS_CONNECTING_BUDDY:
			//We are trying to connect to this client.
			//Although it should NOT happen, we make sure we are not already connected to a buddy.
			//If we are we set to KS_NONE and it's removed next cycle.
			//But if we are not already connected, make sure we set the flag to connecting so we know
			//things are working correctly.
			if (m_nBuddyStatus == Connected)
				cur_client->SetKadState(KS_NONE);
			else {
				ASSERT(m_nBuddyStatus == Connecting);
				buddy = Connecting;
			}
			break;
		case KS_CONNECTED_BUDDY:
			//A potential connected buddy client wanting to me in the Kad network
			//We set our flag to connected to make sure things are still working correctly.
			buddy = Connected;

			//If m_nBuddyStatus is not connected already, we set this client as our buddy!
			if (m_nBuddyStatus != Connected) {
				m_pBuddy = cur_client;
				m_nBuddyStatus = Connected;
				theApp.emuledlg->serverwnd->UpdateMyInfo();
			}
			if (m_pBuddy == cur_client && theApp.IsFirewalled() && cur_client->SendBuddyPingPong()) {
				if (thePrefs.GetDebugClientTCPLevel() > 0)
					DebugSend("OP_BuddyPing", cur_client);
				Packet *buddyPing = new Packet(OP_BUDDYPING, 0, OP_EMULEPROT);
				theStats.AddUpDataOverheadOther(buddyPing->size);
				VERIFY(cur_client->SendPacket(buddyPing, true));
				cur_client->SetLastBuddyPingPongTime();
			}
			break;
		default:
			RemoveFromKadList(cur_client);
		}
	}

	//We either never had a buddy, or lost our buddy.
	if (buddy == Disconnected) {
		if (m_nBuddyStatus != Disconnected || m_pBuddy) {
			if (Kademlia::CKademlia::IsRunning() && theApp.IsFirewalled() && Kademlia::CUDPFirewallTester::IsFirewalledUDP(true)) {
				//We are a lowID client and we just lost our buddy.
				//Go ahead and instantly try to find a new buddy.
				Kademlia::CKademlia::GetPrefs()->SetFindBuddy();
			}
			m_pBuddy = NULL;
			m_nBuddyStatus = Disconnected;
			theApp.emuledlg->serverwnd->UpdateMyInfo();
		}
	}

	if (Kademlia::CKademlia::IsConnected()) {
		//we only need a buddy if direct callback is not available
		if (Kademlia::CKademlia::IsFirewalled() && Kademlia::CUDPFirewallTester::IsFirewalledUDP(true)) {
			//TODO 0.49b: Kad buddies won't work with RequireCrypt, so it is disabled for now but should (and will)
			//be fixed in later version
			// Update: Buddy connections itself support obfuscation properly since 0.49a (this makes it work fine if our buddy uses require crypt),
			// however callback requests don't support it yet so we wouldn't be able to answer callback requests with RequireCrypt, protocol change intended for the next version
			if (m_nBuddyStatus == Disconnected && Kademlia::CKademlia::GetPrefs()->GetFindBuddy() && !thePrefs.IsClientCryptLayerRequired()) {
				DEBUG_ONLY(DebugLog(_T("Starting Buddy search")));
				//We are a firewalled client with no buddy. We have also waited a set time
				//to try to avoid a false firewalled status. So lets look for a buddy.
				if (!Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::FINDBUDDY, true, Kademlia::CUInt128(true).Xor(Kademlia::CKademlia::GetPrefs()->GetKadID()))) {
					//This search ID was already going. Most likely reason is that
					//we found and lost our buddy very quickly and the last search hadn't
					//had time to be removed yet. Go ahead and set this to happen again
					//next time around.
					Kademlia::CKademlia::GetPrefs()->SetFindBuddy();
				}
			}
		} else {
			if (m_pBuddy) {
				//Lets make sure that if we have a buddy, they are firewalled!
				//If they are also not firewalled, then someone must have fixed their firewall or stopped saturating their line.
				//We just set the state of this buddy to KS_NONE and things will be cleared up with the next cycle.
				if (!m_pBuddy->HasLowID())
					m_pBuddy->SetKadState(KS_NONE);
			}
		}
	} else if (m_pBuddy) {
		//We are not connected any more. Just set this buddy to KS_NONE and things will be cleared out on next cycle.
		m_pBuddy->SetKadState(KS_NONE);
	}

	///////////////////////////////////////////////////////////////////////////
	// Cleanup client list
	//
	CleanUpClientList();

	///////////////////////////////////////////////////////////////////////////
	// Process Direct Callbacks for Timeouts
	//
	ProcessConnectingClientsList();
}

#ifdef _DEBUG
void CClientList::Debug_SocketDeleted(CClientReqSocket *deleted) const
{
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = list.GetNext(pos);
		if (!AfxIsValidAddress(cur_client, sizeof CUpDownClient))
			AfxDebugBreak();
		if (thePrefs.m_iDbgHeap >= 2)
			ASSERT_VALID(cur_client);
		if (cur_client->socket == deleted)
			AfxDebugBreak();
	}
}
#endif

bool CClientList::IsValidClient(CUpDownClient *tocheck) const
{
	if (thePrefs.m_iDbgHeap >= 2)
		ASSERT_VALID(tocheck);
	return list.Find(tocheck) != NULL;
}


///////////////////////////////////////////////////////////////////////////////
// Kad client list

bool CClientList::RequestTCP(Kademlia::CContact *contact, uint8 byConnectOptions)
{
	uint32 nContactIP = contact->GetNetIP();
	// don't connect ourself
	if (theApp.serverconnect->GetLocalIP() == nContactIP && thePrefs.GetPort() == contact->GetTCPPort())
		return false;

	CUpDownClient *pNewClient = FindClientByIP(nContactIP, contact->GetTCPPort());

	if (!pNewClient)
		pNewClient = new CUpDownClient(NULL, contact->GetTCPPort(), contact->GetIPAddress(), 0, 0, false);
	else {
		if (pNewClient->GetKadState() != KS_NONE)
			return false; // already busy with this client in some way (probably buddy stuff), don't mess with it
		if (pNewClient->socket != NULL)
			return false; //already existing socket can give false high ID
	}
	//Add client to the lists to be processed.
	pNewClient->SetKadPort(contact->GetUDPPort());
	pNewClient->SetKadState(KS_QUEUED_FWCHECK);
	if (contact->GetClientID() != 0) {
		byte ID[16];
		contact->GetClientID().ToByteArray(ID);
		pNewClient->SetUserHash(ID);
		pNewClient->SetConnectOptions(byConnectOptions, true, false);
	}
	m_KadList.AddTail(pNewClient);
	//This method checks if this is a dup already.
	AddClient(pNewClient);
	return true;
}

void CClientList::RequestBuddy(Kademlia::CContact *contact, uint8 byConnectOptions)
{
	uint32 nContactIP = contact->GetNetIP();
	// don't connect to ourself
	if (theApp.serverconnect->GetLocalIP() == nContactIP && thePrefs.GetPort() == contact->GetTCPPort())
		return;
	if (IsKadFirewallCheckIP(nContactIP)) { // doing a kad firewall check with this IP, abort
		DEBUG_ONLY(DebugLogWarning(_T("KAD TCP Firewall check / Buddy request collision for IP %s"), (LPCTSTR)ipstr(nContactIP)));
		return;
	}
	CUpDownClient *pNewClient = FindClientByIP(nContactIP, contact->GetTCPPort());
	if (!pNewClient)
		pNewClient = new CUpDownClient(NULL, contact->GetTCPPort(), contact->GetIPAddress(), 0, 0, false);
	else if (pNewClient->GetKadState() != KS_NONE)
		return; // already busy with this client in some way (probably fw stuff), don't mess with it
	//Add client to the lists to be processed.
	pNewClient->SetKadPort(contact->GetUDPPort());
	pNewClient->SetKadState(KS_QUEUED_BUDDY);
	byte ID[16];
	contact->GetClientID().ToByteArray(ID);
	pNewClient->SetUserHash(ID);
	pNewClient->SetConnectOptions(byConnectOptions, true, false);
	AddToKadList(pNewClient);
	//This method checked already if this is a dup.
	AddClient(pNewClient);
}

bool CClientList::IncomingBuddy(Kademlia::CContact *contact, Kademlia::CUInt128 *buddyID)
{
	uint32 nContactIP = contact->GetNetIP();
	//If eMule already knows this client, abort this. It could cause conflicts.
	//Although the odds of this happening is very small, it could still happen.
	if (FindClientByIP(nContactIP, contact->GetTCPPort()))
		return false;
	if (IsKadFirewallCheckIP(nContactIP)) { // doing a kad firewall check with this IP, abort
		DEBUG_ONLY(DebugLogWarning(_T("KAD TCP Firewall check / Buddy request collision for IP %s"), (LPCTSTR)ipstr(nContactIP)));
		return false;
	}
	if (theApp.serverconnect->GetLocalIP() == nContactIP && thePrefs.GetPort() == contact->GetTCPPort())
		return false; // don't connect ourself

	//Add client to the lists to be processed.
	CUpDownClient *pNewClient = new CUpDownClient(NULL, contact->GetTCPPort(), contact->GetIPAddress(), 0, 0, false);
	pNewClient->SetKadPort(contact->GetUDPPort());
	pNewClient->SetKadState(KS_INCOMING_BUDDY);
	byte ID[16];
	contact->GetClientID().ToByteArray(ID);
	pNewClient->SetUserHash(ID); //??
	buddyID->ToByteArray(ID);
	pNewClient->SetBuddyID(ID);
	AddToKadList(pNewClient);
	AddClient(pNewClient);
	return true;
}

void CClientList::RemoveFromKadList(CUpDownClient *torem)
{
	POSITION pos = m_KadList.Find(torem);
	if (pos) {
		if (torem == m_pBuddy) {
			m_pBuddy = NULL;
			theApp.emuledlg->serverwnd->UpdateMyInfo();
		}
		m_KadList.RemoveAt(pos);
	}
}

void CClientList::AddToKadList(CUpDownClient *toadd)
{
	if (toadd && !m_KadList.Find(toadd))
		m_KadList.AddTail(toadd);
}

bool CClientList::DoRequestFirewallCheckUDP(const Kademlia::CContact &contact)
{
	// first make sure we don't know this IP already from somewhere
	if (FindClientByIP(contact.GetNetIP()) != NULL)
		return false;
	// fine, just create the client object, set the state and wait
	// TODO: We don't know the clients user hash, this means we cannot build an obfuscated connection,
	// which again means that the whole check won't work on "Require Obfuscation" setting,
	// which is not a huge problem, but certainly not nice. The only somewhat acceptable way
	// to solve this is to use the KadID instead.
	CUpDownClient *pNewClient = new CUpDownClient(NULL, contact.GetTCPPort(), contact.GetIPAddress(), 0, 0, false);
	pNewClient->SetKadState(KS_QUEUED_FWCHECK_UDP);
	DebugLog(_T("Selected client for UDP Firewall check: %s"), (LPCTSTR)ipstr(contact.GetNetIP()));
	AddToKadList(pNewClient);
	AddClient(pNewClient);
	ASSERT(!pNewClient->SupportsDirectUDPCallback());
	return true;
}

void CClientList::CleanUpClientList()
{
	// we remove clients which are not needed any more by time
	// this check is also done on CUpDownClient::Disconnected, however it will not catch all
	// cases (if a client changes the state without being connected)
	//
	// Adding this check directly to every point where any state changes would be more effective,
	// though not compatible with the current code, because there are points where a client has
	// no state for some code lines and the code is also not prepared that a client object gets
	// invalid while working with it (aka setting a new state)
	// so this way is just the easy and safe one to go (as long as emule is basically single threaded)
	const DWORD tick = ::GetTickCount();
	if (tick >= m_dwLastClientCleanUp + CLIENTLIST_CLEANUP_TIME) {
		m_dwLastClientCleanUp = tick;
		uint32 cDeleted = 0;
		for (POSITION pos = list.GetHeadPosition(); pos != NULL;) {
			const CUpDownClient *pCurClient = list.GetNext(pos);
			if ((pCurClient->GetUploadState() == US_NONE || (pCurClient->GetUploadState() == US_BANNED && !pCurClient->IsBanned()))
				&& pCurClient->GetDownloadState() == DS_NONE
				&& pCurClient->GetChatState() == MS_NONE
				&& pCurClient->GetKadState() == KS_NONE
				&& pCurClient->socket == NULL)
			{
				++cDeleted;
				delete pCurClient;
			}
		}
		DEBUG_ONLY(AddDebugLogLine(false, _T("Cleaned ClientList, removed %i non-used known clients"), cDeleted));
	}
}


CDeletedClient::CDeletedClient(const CUpDownClient *pClient)
{
	m_cBadRequest = 0;
	m_dwInserted = ::GetTickCount();
	m_ItemsList.Add(PORTANDHASH{pClient->GetUserPort(), pClient->Credits()});
}

// ZZ:DownloadManager -->
void CClientList::ProcessA4AFClients() const
{
	//if(thePrefs.GetLogA4AF()) AddDebugLogLine(false, _T(">>> Starting A4AF check"));
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = list.GetNext(pos);

		if (cur_client->GetDownloadState() != DS_DOWNLOADING
			&& cur_client->GetDownloadState() != DS_CONNECTED
			&& (!cur_client->m_OtherRequests_list.IsEmpty() || !cur_client->m_OtherNoNeeded_list.IsEmpty()))
		{
			//AddDebugLogLine(false, _T("+++ ZZ:DownloadManager: Trying for better file for source: %s '%s'"), cur_client->GetUserName(), cur_client->m_reqfile->GetFileName());
			cur_client->SwapToAnotherFile(_T("Periodic A4AF check CClientList::ProcessA4AFClients()"), false, false, false, NULL, true, false);
		}
	}
	//if(thePrefs.GetLogA4AF()) AddDebugLogLine(false, _T(">>> Done with A4AF check"));
}
// <-- ZZ:DownloadManager

void CClientList::AddKadFirewallRequest(uint32 dwIP)
{
	const DWORD tick = ::GetTickCount();
	listFirewallCheckRequests.AddHead(IPANDTICS{dwIP, tick});
	while (!listFirewallCheckRequests.IsEmpty() && tick >= listFirewallCheckRequests.GetTail().dwInserted + SEC2MS(180))
		listFirewallCheckRequests.RemoveTail();
}

bool CClientList::IsKadFirewallCheckIP(uint32 dwIP) const
{
	const DWORD tick = ::GetTickCount();
	for (POSITION pos = listFirewallCheckRequests.GetHeadPosition(); pos != NULL;) {
		const IPANDTICS &iptick = listFirewallCheckRequests.GetNext(pos);
		if (tick >= iptick.dwInserted + SEC2MS(180))
			break;
		if (iptick.dwIP == dwIP)
			return true;
	}
	return false;
}

void CClientList::AddConnectingClient(CUpDownClient *pToAdd)
{
	for (POSITION pos = m_liConnectingClients.GetHeadPosition(); pos != NULL;)
		if (m_liConnectingClients.GetNext(pos).pClient == pToAdd) {
			ASSERT(0);
			return;
		}

	ASSERT(pToAdd->GetConnectingState() != CCS_NONE);
	m_liConnectingClients.AddTail(CONNECTINGCLIENT{pToAdd, ::GetTickCount()});
}

void CClientList::ProcessConnectingClientsList()
{
	// we do check if any connects have timed out by now
	const DWORD tick = ::GetTickCount();
	for (POSITION pos = m_liConnectingClients.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		const CONNECTINGCLIENT cc = m_liConnectingClients.GetNext(pos);
		if (tick >= cc.dwInserted + SEC2MS(45)) {
			ASSERT(cc.pClient->GetConnectingState() != CCS_NONE);
			m_liConnectingClients.RemoveAt(pos2);
			if (cc.pClient->Disconnected(_T("Connection try timeout")))
				delete cc.pClient;
		}
	}
}

void CClientList::RemoveConnectingClient(CUpDownClient *pToRemove)
{
	for (POSITION pos = m_liConnectingClients.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		if (m_liConnectingClients.GetNext(pos).pClient == pToRemove) {
			m_liConnectingClients.RemoveAt(pos2);
			return;
		}
	}
}

void CClientList::AddTrackCallbackRequests(uint32 dwIP)
{
	const DWORD tick = ::GetTickCount();
	listDirectCallbackRequests.AddHead(IPANDTICS{dwIP, tick});
	while (!listDirectCallbackRequests.IsEmpty() && tick >= listDirectCallbackRequests.GetTail().dwInserted + SEC2MS(180))
		listDirectCallbackRequests.RemoveTail();
}

bool CClientList::AllowCalbackRequest(uint32 dwIP) const
{
	const DWORD tick = ::GetTickCount();
	for (POSITION pos = listDirectCallbackRequests.GetHeadPosition(); pos != NULL;) {
		const IPANDTICS &iptick = listDirectCallbackRequests.GetNext(pos);
		if (iptick.dwIP == dwIP && tick < iptick.dwInserted + SEC2MS(180))
			return false;
	}
	return true;
}