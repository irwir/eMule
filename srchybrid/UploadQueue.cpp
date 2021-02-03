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
#include "UploadQueue.h"
#include "Packets.h"
#include "KnownFile.h"
#include "ListenSocket.h"
#include "Exceptions.h"
#include "Scheduler.h"
#include "PerfLog.h"
#include "UploadBandwidthThrottler.h"
#include "ClientList.h"
#include "LastCommonRouteFinder.h"
#include "DownloadQueue.h"
#include "FriendList.h"
#include "Statistics.h"
#include "UpDownClient.h"
#include "SharedFileList.h"
#include "KnownFileList.h"
#include "ServerConnect.h"
#include "ClientCredits.h"
#include "Server.h"
#include "ServerList.h"
#include "WebServer.h"
#include "emuledlg.h"
#include "ServerWnd.h"
#include "TransferDlg.h"
#include "SearchDlg.h"
#include "StatisticsDlg.h"
#include "Kademlia/Kademlia/Kademlia.h"
#include "Kademlia/Kademlia/Prefs.h"
#include "Log.h"
#include "collection.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


static uint32 counter, sec, statsave;
static UINT s_uSaveStatistics = 0;
static uint32 igraph, istats, i2Secs;

#define HIGHSPEED_UPLOADRATE_START	(500*1024)
#define HIGHSPEED_UPLOADRATE_END	(300*1024)


CUploadQueue::CUploadQueue()
	: datarate()
	, friendDatarate()
	, successfullupcount()
	, failedupcount()
	, totaluploadtime()
	, m_nLastStartUpload()
	, m_imaxscore()
	, m_dwLastCalculatedAverageCombinedFilePrioAndCredit()
	, m_fAverageCombinedFilePrioAndCredit()
	, m_iHighestNumberOfFullyActivatedSlotsSinceLastCall()
	, m_MaxActiveClients()
	, m_MaxActiveClientsShortTime()
	, m_lastCalculatedDataRateTick()
	, m_average_dr_sum()
	, m_dwLastResortedUploadSlots()
	, m_bStatisticsWaitingListDirty(true)
{
	VERIFY((h_timer = ::SetTimer(0, 0, 100, UploadTimer)) != 0);
	if (thePrefs.GetVerbose() && !h_timer)
		AddDebugLogLine(true, _T("Failed to create 'upload queue' timer - %s"), (LPCTSTR)GetErrorMessage(::GetLastError()));
	counter = 0;
	statsave = 0;
	i2Secs = 0;
	m_dwRemovedClientByScore = ::GetTickCount();
}

CUploadQueue::~CUploadQueue()
{
	if (h_timer)
		::KillTimer(0, h_timer);
}

/**
 * Find the highest ranking client in the waiting queue, and return it.
 *
 * Low id client are ranked as lowest possible, unless they are currently connected.
 * A low id client that is not connected, but would have been ranked highest if it
 * had been connected, gets a flag set. This flag means that the client should be
 * allowed to get an upload slot immediately once it connects.
 *
 * @return address of the highest ranking client.
 */
CUpDownClient* CUploadQueue::FindBestClientInQueue()
{
	uint32 bestscore = 0;
	uint32 bestlowscore = 0;
	CUpDownClient *newclient = NULL;
	CUpDownClient *lowclient = NULL;
	DWORD tNow = ::GetTickCount();

	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		CUpDownClient *cur_client = waitinglist.GetNext(pos);
		//While we are going through this list. Lets check if a client appears to have left the network.
		ASSERT(cur_client->GetLastUpRequest());
		if ((tNow >= cur_client->GetLastUpRequest() + MAX_PURGEQUEUETIME) || !theApp.sharedfiles->GetFileByID(cur_client->GetUploadFileID())) {
			//This client has either not been seen in a long time, or we no longer share the file he wanted any more.
			cur_client->ClearWaitStartTime();
			RemoveFromWaitingQueue(pos2, true);
		} else {
			// finished clearing
			uint32 cur_score = cur_client->GetScore(false);

			if (cur_score > bestscore) {
				// cur_client is more worthy than current best client that is ready to go (connected).
				if (!cur_client->HasLowID() || (cur_client->socket && cur_client->socket->IsConnected())) {
					// this client is a HighID or a lowID client that is ready to go (connected)
					// and it is more worthy
					bestscore = cur_score;
					newclient = cur_client;
				} else if (!cur_client->m_bAddNextConnect) {
					// this client is a lowID client that is not ready to go (not connected)

					// now that we know this client is not ready to go, compare it to the best not ready client
					// the best not ready client may be better than the best ready client, so we need to check
					// against that client
					if (cur_score > bestlowscore) {
						// it is more worthy, keep it
						bestlowscore = cur_score;
						lowclient = cur_client;
					}
				}
			}
		}
	}

	if (lowclient && bestlowscore > bestscore)
		lowclient->m_bAddNextConnect = true;

	return newclient;
}

void CUploadQueue::InsertInUploadingList(CUpDownClient *newclient, bool bNoLocking)
{
	UploadingToClient_Struct *pNewClientUploadStruct = new UploadingToClient_Struct;
	pNewClientUploadStruct->m_pClient = newclient;
	InsertInUploadingList(pNewClientUploadStruct, bNoLocking);
}

void CUploadQueue::InsertInUploadingList(UploadingToClient_Struct *pNewClientUploadStruct, bool bNoLocking)
{
	//Lets make sure any client that is added to the list has this flag reset!
	pNewClientUploadStruct->m_pClient->m_bAddNextConnect = false;
	// Add it last
	theApp.uploadBandwidthThrottler->AddToStandardList(uploadinglist.GetCount(), pNewClientUploadStruct->m_pClient->GetFileUploadSocket());

	if (bNoLocking)
		uploadinglist.AddTail(pNewClientUploadStruct);
	else {
		m_csUploadListMainThrdWriteOtherThrdsRead.Lock();
		uploadinglist.AddTail(pNewClientUploadStruct);
		m_csUploadListMainThrdWriteOtherThrdsRead.Unlock();
	}
	pNewClientUploadStruct->m_pClient->SetSlotNumber((UINT)uploadinglist.GetCount());
}

bool CUploadQueue::AddUpNextClient(LPCTSTR pszReason, CUpDownClient *directadd)
{
	CUpDownClient *newclient;
	// select next client or use given client
	if (directadd)
		newclient = directadd;
	else {
		newclient = FindBestClientInQueue();
		if (newclient) {
			RemoveFromWaitingQueue(newclient, true);
			theApp.emuledlg->transferwnd->ShowQueueCount(waitinglist.GetCount());
		}
	}

	if (newclient == NULL)
		return false;

	if (!thePrefs.TransferFullChunks())
		UpdateMaxClientScore(); // refresh score caching, now that the highest score is removed

	if (IsDownloading(newclient))
		return false;

	if (pszReason && thePrefs.GetLogUlDlEvents())
		AddDebugLogLine(false, _T("Adding client to upload list: %s Client: %s"), pszReason, (LPCTSTR)newclient->DbgGetClientInfo());

	if (newclient->HasCollectionUploadSlot() && directadd == NULL) {
		ASSERT(0);
		newclient->SetCollectionUploadSlot(false);
	}

	// tell the client that we are now ready to upload
	if (!newclient->socket || !newclient->socket->IsConnected() || !newclient->CheckHandshakeFinished()) {
		newclient->SetUploadState(US_CONNECTING);
		if (!newclient->TryToConnect(true))
			return false;
	} else {
		if (thePrefs.GetDebugClientTCPLevel() > 0)
			DebugSend("OP_AcceptUploadReq", newclient);
		Packet *packet = new Packet(OP_ACCEPTUPLOADREQ, 0);
		theStats.AddUpDataOverheadFileRequest(packet->size);
		newclient->SendPacket(packet);
		newclient->SetUploadState(US_UPLOADING);
	}
	newclient->SetUpStartTime();
	newclient->ResetSessionUp();

	InsertInUploadingList(newclient, false);

	m_nLastStartUpload = ::GetTickCount();

	// statistic
	CKnownFile *reqfile = theApp.sharedfiles->GetFileByID((uchar*)newclient->GetUploadFileID());
	if (reqfile)
		reqfile->statistic.AddAccepted();

	theApp.emuledlg->transferwnd->GetUploadList()->AddClient(newclient);

	return true;
}

void CUploadQueue::UpdateActiveClientsInfo(DWORD curTick)
{
	// Save number of active clients for statistics
	INT_PTR tempHighest = theApp.uploadBandwidthThrottler->GetHighestNumberOfFullyActivatedSlotsSinceLastCallAndReset();

	/*if(thePrefs.GetLogUlDlEvents() && theApp.uploadBandwidthThrottler->GetStandardListSize() > uploadinglist.GetCount()) {
		// debug info, will remove this when I'm done.
		AddDebugLogLine(false, _T("UploadQueue: Error! Throttler has more slots than UploadQueue! Throttler: %i UploadQueue: %i Tick: %i"), theApp.uploadBandwidthThrottler->GetStandardListSize(), uploadinglist.GetCount(), ::GetTickCount());
	}*/

	if (tempHighest > uploadinglist.GetCount() + 1)
		tempHighest = uploadinglist.GetCount() + 1;

	m_iHighestNumberOfFullyActivatedSlotsSinceLastCall = tempHighest;

	// save some data about number of fully active clients
	int tempMaxRemoved = 0;
	while (!activeClients_tick_list.IsEmpty() && !activeClients_list.IsEmpty() && curTick >= activeClients_tick_list.GetHead() + SEC2MS(20)) {
		activeClients_tick_list.RemoveHead();
		int removed = activeClients_list.RemoveHead();

		if (removed > tempMaxRemoved)
			tempMaxRemoved = removed;
	}

	activeClients_list.AddTail((int)m_iHighestNumberOfFullyActivatedSlotsSinceLastCall);
	activeClients_tick_list.AddTail(curTick);

	if (activeClients_tick_list.GetCount() > 1) {
		INT_PTR tempMaxActiveClients = m_iHighestNumberOfFullyActivatedSlotsSinceLastCall;
		INT_PTR tempMaxActiveClientsShortTime = m_iHighestNumberOfFullyActivatedSlotsSinceLastCall;
		POSITION activeClientsTickPos = activeClients_tick_list.GetTailPosition();
		POSITION activeClientsListPos = activeClients_list.GetTailPosition();
		while (activeClientsListPos != NULL && (tempMaxRemoved > tempMaxActiveClients && tempMaxRemoved >= m_MaxActiveClients || curTick < activeClients_tick_list.GetAt(activeClientsTickPos) + SEC2MS(10))) {
			DWORD activeClientsTickSnapshot = activeClients_tick_list.GetAt(activeClientsTickPos);
			int activeClientsSnapshot = activeClients_list.GetAt(activeClientsListPos);

			if (activeClientsSnapshot > tempMaxActiveClients)
				tempMaxActiveClients = activeClientsSnapshot;

			if (activeClientsSnapshot > tempMaxActiveClientsShortTime && curTick < activeClientsTickSnapshot + SEC2MS(10))
				tempMaxActiveClientsShortTime = activeClientsSnapshot;

			activeClients_tick_list.GetPrev(activeClientsTickPos);
			activeClients_list.GetPrev(activeClientsListPos);
		}

		if (tempMaxRemoved >= m_MaxActiveClients || tempMaxActiveClients > m_MaxActiveClients)
			m_MaxActiveClients = tempMaxActiveClients;

		m_MaxActiveClientsShortTime = tempMaxActiveClientsShortTime;
	} else {
		m_MaxActiveClients = m_iHighestNumberOfFullyActivatedSlotsSinceLastCall;
		m_MaxActiveClientsShortTime = m_iHighestNumberOfFullyActivatedSlotsSinceLastCall;
	}
}

/**
 * Maintenance method for the uploading slots. It adds and removes clients to the
 * uploading list. It also makes sure that all the uploading slots' Sockets always have
 * enough packets in their queues, etc.
 *
 * This method is called approximately once every 100 milliseconds.
 */
void CUploadQueue::Process()
{
	DWORD curTick = ::GetTickCount();

	UpdateActiveClientsInfo(curTick);

	if (ForceNewClient())
		// There's not enough open uploads. Open another one.
		AddUpNextClient(_T("Not enough open upload slots for the current speed"));

	// The loop that feeds the upload slots with data.
	for (POSITION pos = uploadinglist.GetHeadPosition(); pos != NULL;) {
		// Get the client. Note! Also updates pos as a side effect.
		UploadingToClient_Struct *pCurClientStruct = uploadinglist.GetNext(pos);
		CUpDownClient *cur_client = pCurClientStruct->m_pClient;
		if (thePrefs.m_iDbgHeap >= 2)
			ASSERT_VALID(cur_client);
		//It seems chatting or friend slots can get stuck at times in upload. This needs to be looked into.
		if (cur_client->socket == NULL) {
			RemoveFromUploadQueue(cur_client, _T("Uploading to client without socket? (CUploadQueue::Process)"));
			if (cur_client->Disconnected(_T("CUploadQueue::Process")))
				delete cur_client;
		} else {
			cur_client->UpdateUploadingStatisticsData();
			if (pCurClientStruct->m_bIOError) {
				RemoveFromUploadQueue(cur_client, _T("IO/Other Error while creating data packet (see earlier log entries)"), true);
				continue;
			}
			if (CheckForTimeOver(cur_client)) {
				RemoveFromUploadQueue(cur_client, _T("Completed transfer"), true);
				cur_client->SendOutOfPartReqsAndAddToWaitingQueue();
				continue;
			}

			// check if the file id of the topmost block request matches with out current upload file, otherwise the IO thread will
			// wait for us (only for this client of course) to fix it for cross-thread sync reasons
			CSingleLock lockBlockLists(&pCurClientStruct->m_csBlockListsLock, TRUE);
			ASSERT(lockBlockLists.IsLocked());
			// be careful what functions to call while having locks, RemoveFromUploadQueue could,
			// for example, lead to a deadlock here because it tries to get the uploadlist lock,
			// while the IO thread tries to fetch the uploadlist lock and then the blocklist lock
			if (!pCurClientStruct->m_BlockRequests_queue.IsEmpty()
				&& !md4equ(((Requested_Block_Struct*)pCurClientStruct->m_BlockRequests_queue.GetHead())->FileID, cur_client->GetUploadFileID()))
			{
				Requested_Block_Struct *pHeadBlock = pCurClientStruct->m_BlockRequests_queue.GetHead();
				if (!md4equ(pHeadBlock->FileID, cur_client->GetUploadFileID())) {
					uchar aucNewID[16];
					md4cpy(aucNewID, pHeadBlock->FileID);

					lockBlockLists.Unlock();

					CKnownFile *pCurrentUploadFile = theApp.sharedfiles->GetFileByID(aucNewID);
					if (pCurrentUploadFile != NULL)
						cur_client->SetUploadFileID(pCurrentUploadFile);
					else
						RemoveFromUploadQueue(cur_client, _T("Requested FileID in blockrequest not found in sharedfiles"), true);
				}
			}
		}
	}

	// Save used bandwidth for speed calculations
	uint64 sentBytes = theApp.uploadBandwidthThrottler->GetNumberOfSentBytesSinceLastCallAndReset();
	average_dr_list.AddTail(sentBytes);
	m_average_dr_sum += sentBytes;

	(void)theApp.uploadBandwidthThrottler->GetNumberOfSentBytesOverheadSinceLastCallAndReset();

	average_friend_dr_list.AddTail(theStats.sessionSentBytesToFriend);

	// Save time between each speed snapshot
	average_tick_list.AddTail(curTick);

	// don't save more than 30 secs of data
	while (average_tick_list.GetCount() > 3 && !average_friend_dr_list.IsEmpty() && ::GetTickCount() >= average_tick_list.GetHead() + SEC2MS(30)) {
		m_average_dr_sum -= average_dr_list.RemoveHead();
		average_friend_dr_list.RemoveHead();
		average_tick_list.RemoveHead();
	}
};

bool CUploadQueue::AcceptNewClient(bool addOnNextConnect) const
{
	INT_PTR curUploadSlots = uploadinglist.GetCount();

	//We allow ONE extra slot to be created to accommodate lowID users.
	//This is because we skip these users when it was actually their turn
	//to get an upload slot.
	curUploadSlots -= static_cast<INT_PTR>(addOnNextConnect && curUploadSlots > 0);

	return AcceptNewClient(curUploadSlots);
}

bool CUploadQueue::AcceptNewClient(INT_PTR curUploadSlots) const
{
	// check if we can allow a new client to start downloading from us

	if (curUploadSlots < max(MIN_UP_CLIENTS_ALLOWED, 4))
		return true;

	uint32 MaxSpeed;
	if (thePrefs.IsDynUpEnabled())
		MaxSpeed = theApp.lastCommonRouteFinder->GetUpload() / 1024u;
	else
		MaxSpeed = thePrefs.GetMaxUpload();
	uint32 TargetRate = GetTargetClientDataRate(false);

	if (curUploadSlots >= (INT_PTR)mini(datarate / GetTargetClientDataRate(true), MaxSpeed * 1024u / TargetRate))
		return false;

	return MaxSpeed != UNLIMITED
		|| thePrefs.IsDynUpEnabled()
		|| thePrefs.GetMaxGraphUploadRate(true) <= 0
		|| curUploadSlots < (INT_PTR)(thePrefs.GetMaxGraphUploadRate(false) * 1024 / TargetRate);
}

uint32 CUploadQueue::GetTargetClientDataRate(bool bMinDatarate) const
{
	uint32 nOpenSlots = (uint32)GetUploadQueueLength();
	// 3 slots or less - 3KiB/s
	// 4 slots or more - linear growth by 1 KiB/s steps, cap off at UPLOAD_CLIENT_MAXDATARATE
	uint32 nResult;
	if (nOpenSlots <= 3)
		nResult = 3 * 1024;
	else
		nResult = min(UPLOAD_CLIENT_MAXDATARATE, nOpenSlots * 1024);

	return bMinDatarate ? nResult * 3 / 4 : nResult;
}

bool CUploadQueue::ForceNewClient(bool allowEmptyWaitingQueue)
{
	if (!allowEmptyWaitingQueue && waitinglist.IsEmpty())
		return false;

	INT_PTR curUploadSlots = uploadinglist.GetCount();
	if (curUploadSlots < MIN_UP_CLIENTS_ALLOWED)
		return true;

	if (::GetTickCount() < m_nLastStartUpload + SEC2MS(1) && datarate < 102400)
		return false;

	if (!AcceptNewClient(curUploadSlots) || !theApp.lastCommonRouteFinder->AcceptNewClient()) // UploadSpeedSense can veto a new slot if USS enabled
		return false;

	uint32 MaxSpeed;
	if (thePrefs.IsDynUpEnabled())
		MaxSpeed = theApp.lastCommonRouteFinder->GetUpload() / 1024u;
	else
		MaxSpeed = thePrefs.GetMaxUpload();

	uint32 upPerClient = GetTargetClientDataRate(false);

	// if throttler doesn't require another slot, go with a slightly more restrictive method
	if (MaxSpeed > 49 /*|| MaxSpeed == UNLIMITED */) { //because UNLIMITED > 20
		upPerClient += datarate / 43;
		if (upPerClient > UPLOAD_CLIENT_MAXDATARATE)
			upPerClient = UPLOAD_CLIENT_MAXDATARATE;
	}

	//now the final check
	if (MaxSpeed == UNLIMITED) {
		if ((uint32)curUploadSlots < (datarate / upPerClient))
			return true;
	} else {
		uint32 nMaxSlots;
		if (MaxSpeed > 25)
			nMaxSlots = max((MaxSpeed * 1024u) / upPerClient, (uint32)(MIN_UP_CLIENTS_ALLOWED + 3));
		else if (MaxSpeed > 16)
			nMaxSlots = MIN_UP_CLIENTS_ALLOWED + 2;
		else if (MaxSpeed > 9)
			nMaxSlots = MIN_UP_CLIENTS_ALLOWED + 1;
		else
			nMaxSlots = MIN_UP_CLIENTS_ALLOWED;
		//AddLogLine(true,"maxslots=%u, upPerClient=%u, datarateslot=%u|%u|%u",nMaxSlots,upPerClient,datarate/UPLOAD_CHECK_CLIENT_DR, datarate, UPLOAD_CHECK_CLIENT_DR);

		if ((uint32)curUploadSlots < nMaxSlots)
			return true;
	}
/*
	if(m_iHighestNumberOfFullyActivatedSlotsSinceLastCall > uploadinglist.GetCount()) {
		// uploadThrottler requests another slot. If throttler says it needs another slot, we will allow more slots
		// than what we require ourself. Never allow more slots than to give each slot high enough average transfer speed, though (checked above).
		//if(thePrefs.GetLogUlDlEvents() && !waitinglist.IsEmpty())
		//	AddDebugLogLine(false, _T("UploadQueue: Added new slot since throttler needs it. m_iHighestNumberOfFullyActivatedSlotsSinceLastCall: %i uploadinglist.GetCount(): %i tick: %i"), m_iHighestNumberOfFullyActivatedSlotsSinceLastCall, uploadinglist.GetCount(), ::GetTickCount());
		return true;
	}
	//nope
	return false;
*/
	return m_iHighestNumberOfFullyActivatedSlotsSinceLastCall > uploadinglist.GetCount();
}

CUpDownClient* CUploadQueue::GetWaitingClientByIP_UDP(uint32 dwIP, uint16 nUDPPort, bool bIgnorePortOnUniqueIP, bool *pbMultipleIPs)
{
	CUpDownClient *pMatchingIPClient = NULL;
	uint32 cMatches = 0;
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = waitinglist.GetNext(pos);
		if (dwIP == cur_client->GetIP() && nUDPPort == cur_client->GetUDPPort())
			return cur_client;
		if (bIgnorePortOnUniqueIP) {
			pMatchingIPClient = cur_client;
			++cMatches;
		}
	}
	if (pbMultipleIPs != NULL)
		*pbMultipleIPs = cMatches > 1;

	if (pMatchingIPClient != NULL && cMatches == 1)
		return pMatchingIPClient;
	return NULL;
}

CUpDownClient* CUploadQueue::GetWaitingClientByIP(uint32 dwIP) const
{
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = waitinglist.GetNext(pos);
		if (dwIP == cur_client->GetIP())
			return cur_client;
	}
	return NULL;
}

/**
 * Add a client to the waiting queue for uploads.
 *
 * @param client address of the client that should be added to the waiting queue
 *
 * @param bIgnoreTimelimit don't check time limit to possibly ban the client.
 */
void CUploadQueue::AddClientToQueue(CUpDownClient *client, bool bIgnoreTimelimit)
{
	//This is to keep users from abusing the limits we put on lowID callbacks.
	//1)Check if we are connected to any network and that we are a lowID.
	//(Although this check shouldn't matter as they wouldn't have found us.
	// But, maybe I'm missing something, so it's best to check as a precaution.)
	//2)Check if the user is connected to Kad. We do allow all Kad Callbacks.
	//3)Check if the user is in our download list or a friend.
	//We give these users a special pass as they are helping us.
	//4)Are we connected to a server? If we are, is the user on the same server?
	//TCP lowID callbacks are also allowed.
	//5)If the queue is very short, allow anyone in as we want to make sure
	//our upload is always used.
	if (   theApp.IsConnected()
		&& theApp.IsFirewalled()
		&& !client->GetKadPort()
		&& client->GetDownloadState() == DS_NONE
		&& !client->IsFriend()
		&& theApp.serverconnect
		&& !theApp.serverconnect->IsLocalServer(client->GetServerIP(), client->GetServerPort())
		&& GetWaitingUserCount() > 50)
	{
		return;
	}
	client->AddAskedCount();
	client->SetLastUpRequest();
	if (!bIgnoreTimelimit)
		client->AddRequestCount(client->GetUploadFileID());
	if (client->IsBanned())
		return;
	uint16 cSameIP = 0;
	// check for duplicates
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		CUpDownClient *cur_client = waitinglist.GetNext(pos);
		if (cur_client == client) {
			if (client->m_bAddNextConnect && AcceptNewClient(client->m_bAddNextConnect)) {
				//Special care is given to lowID clients that missed their upload slot
				//due to the saving bandwidth on callbacks.
				if (thePrefs.GetLogUlDlEvents())
					AddDebugLogLine(true, _T("Adding ****lowid when reconnecting. Client: %s"), (LPCTSTR)client->DbgGetClientInfo());
				client->m_bAddNextConnect = false;
				RemoveFromWaitingQueue(client, true);
				// statistic values // TODO: Maybe we should change this to count each request for a file only once and ignore re-asks
				CKnownFile *reqfile = theApp.sharedfiles->GetFileByID((uchar*)client->GetUploadFileID());
				if (reqfile)
					reqfile->statistic.AddRequest();
				AddUpNextClient(_T("Adding ****lowid when reconnecting."), client);
			} else {
				client->SendRankingInfo();
				theApp.emuledlg->transferwnd->GetQueueList()->RefreshClient(client);
			}
			return;
		}
		if (client->Compare(cur_client)) {
			theApp.clientlist->AddTrackClient(client); // in any case keep track of this client

			// another client with same ip:port or hash
			// this happens only in rare cases, because same userhash / ip:ports are assigned to the right client on connecting in most cases
			if (cur_client->credits != NULL && cur_client->credits->GetCurrentIdentState(cur_client->GetIP()) == IS_IDENTIFIED) {
				//cur_client has a valid secure hash, don't remove him
				if (thePrefs.GetVerbose())
					AddDebugLogLine(false, (LPCTSTR)GetResString(IDS_SAMEUSERHASH), client->GetUserName(), cur_client->GetUserName(), client->GetUserName());
				return;
			}
			if (client->credits == NULL || client->credits->GetCurrentIdentState(client->GetIP()) != IS_IDENTIFIED) {
				// remove both since we do not know who the bad one is
				if (thePrefs.GetVerbose())
					AddDebugLogLine(false, (LPCTSTR)GetResString(IDS_SAMEUSERHASH), client->GetUserName(), cur_client->GetUserName(), _T("Both"));
				RemoveFromWaitingQueue(pos2, true);
				if (!cur_client->socket && cur_client->Disconnected(_T("AddClientToQueue - same userhash 2")))
					delete cur_client;
				return;
			}
			//client has a valid secure hash, add him and remove the other one
			if (thePrefs.GetVerbose())
				AddDebugLogLine(false, (LPCTSTR)GetResString(IDS_SAMEUSERHASH), client->GetUserName(), cur_client->GetUserName(), cur_client->GetUserName());
			RemoveFromWaitingQueue(pos2, true);
			if (!cur_client->socket && cur_client->Disconnected(_T("AddClientToQueue - same userhash 1")))
				delete cur_client;
		} else if (client->GetIP() == cur_client->GetIP()) {
			// same IP, different port, different userhash
			++cSameIP;
		}
	}
	if (cSameIP >= 3) {
		// do not accept more than 3 clients from the same IP
		if (thePrefs.GetVerbose())
			DEBUG_ONLY(AddDebugLogLine(false, _T("%s's (%s) request to enter the queue was rejected, because of too many clients with the same IP"), client->GetUserName(), (LPCTSTR)ipstr(client->GetConnectIP())));
		return;
	}
	if (theApp.clientlist->GetClientsFromIP(client->GetIP()) >= 3) {
		if (thePrefs.GetVerbose())
			DEBUG_ONLY(AddDebugLogLine(false, _T("%s's (%s) request to enter the queue was rejected, because of too many clients with the same IP (found in TrackedClientsList)"), client->GetUserName(), (LPCTSTR)ipstr(client->GetConnectIP())));
		return;
	}
	// done

	// statistic values
	// TODO: Maybe we should change this to count each request for a file only once and ignore re-asks
	CKnownFile *reqfile = theApp.sharedfiles->GetFileByID((uchar*)client->GetUploadFileID());
	if (reqfile)
		reqfile->statistic.AddRequest();

	// emule collection will bypass the queue
	if (reqfile != NULL && CCollection::HasCollectionExtention(reqfile->GetFileName()) && reqfile->GetFileSize() < (uint64)MAXPRIORITYCOLL_SIZE
		&& !client->IsDownloading() && client->socket != NULL && client->socket->IsConnected())
	{
		client->SetCollectionUploadSlot(true);
		RemoveFromWaitingQueue(client, true);
		AddUpNextClient(_T("Collection Priority Slot"), client);
		return;
	}

	client->SetCollectionUploadSlot(false);

	// cap the list
	// the queue limit in prefs is only a soft limit. Hard limit is 25% higher, to let in
	// powershare clients and other high ranking clients after soft limit has been reached
	INT_PTR softQueueLimit = thePrefs.GetQueueSize();
	INT_PTR hardQueueLimit = softQueueLimit + max(softQueueLimit, 800) / 4;

	// if soft queue limit has been reached, only let in high ranking clients
	if (waitinglist.GetCount() >= hardQueueLimit
		|| (waitinglist.GetCount() >= softQueueLimit // soft queue limit is reached
			&& (!client->IsFriend() || !client->GetFriendSlot()) // client is not a friend with friend slot
			&& client->GetCombinedFilePrioAndCredit() < GetAverageCombinedFilePrioAndCredit() // and client has lower credits/wants lower prio file than average client in queue
		   )
	   )
	{
		// block client from getting on queue
		return;
	}
	if (client->IsDownloading()) {
		// he's already downloading and probably only wants another file
		if (thePrefs.GetDebugClientTCPLevel() > 0)
			DebugSend("OP_AcceptUploadReq", client);
		Packet *packet = new Packet(OP_ACCEPTUPLOADREQ, 0);
		theStats.AddUpDataOverheadFileRequest(packet->size);
		client->SendPacket(packet);
		return;
	}
	if (waitinglist.IsEmpty() && ForceNewClient(true)) {
		client->SetWaitStartTime();
		AddUpNextClient(_T("Direct add with empty queue."), client);
	} else {
		m_bStatisticsWaitingListDirty = true;
		waitinglist.AddTail(client);
		client->SetUploadState(US_ONUPLOADQUEUE);
		theApp.emuledlg->transferwnd->GetQueueList()->AddClient(client, true);
		theApp.emuledlg->transferwnd->ShowQueueCount(waitinglist.GetCount());
		client->SendRankingInfo();
	}
}

float CUploadQueue::GetAverageCombinedFilePrioAndCredit()
{
	DWORD curTick = ::GetTickCount();

	if (curTick >= m_dwLastCalculatedAverageCombinedFilePrioAndCredit + SEC2MS(5)) {
		m_dwLastCalculatedAverageCombinedFilePrioAndCredit = curTick;

		// TODO: is there a risk of overflow? I don't think so...
		double sum = 0;
		for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;)
			sum += waitinglist.GetNext(pos)->GetCombinedFilePrioAndCredit();

		m_fAverageCombinedFilePrioAndCredit = (float)(sum / waitinglist.GetCount());
	}

	return m_fAverageCombinedFilePrioAndCredit;
}

bool CUploadQueue::RemoveFromUploadQueue(CUpDownClient *client, LPCTSTR pszReason, bool updatewindow, bool earlyabort)
{
	bool result = false;
	uint32 slotCounter = 1;
	for (POSITION pos = uploadinglist.GetHeadPosition(); pos != NULL;) {
		POSITION curPos = pos;
		UploadingToClient_Struct *curClientStruct = uploadinglist.GetNext(pos);
		if (client == curClientStruct->m_pClient) {
			if (updatewindow)
				theApp.emuledlg->transferwnd->GetUploadList()->RemoveClient(client);

			if (thePrefs.GetLogUlDlEvents()) {
				AddDebugLogLine(DLP_DEFAULT, true, _T("Removing client from upload list: %s Client: %s Transferred: %s SessionUp: %s QueueSessionPayload: %s In buffer: %s Req blocks: %i File: %s")
					, pszReason == NULL ? _T("") : pszReason
					, (LPCTSTR)client->DbgGetClientInfo()
					, (LPCTSTR)CastSecondsToHM(client->GetUpStartTimeDelay() / SEC2MS(1))
					, (LPCTSTR)CastItoXBytes(client->GetSessionUp())
					, (LPCTSTR)CastItoXBytes(client->GetQueueSessionPayloadUp())
					, (LPCTSTR)CastItoXBytes(client->GetPayloadInBuffer()), curClientStruct->m_BlockRequests_queue.GetCount()
					, theApp.sharedfiles->GetFileByID(client->GetUploadFileID()) ? (LPCTSTR)theApp.sharedfiles->GetFileByID(client->GetUploadFileID())->GetFileName() : _T(""));
			}
			client->m_bAddNextConnect = false;

			m_csUploadListMainThrdWriteOtherThrdsRead.Lock();
			uploadinglist.RemoveAt(curPos);
			m_csUploadListMainThrdWriteOtherThrdsRead.Unlock();
			delete curClientStruct; // m_csBlockListsLock.Lock();

			//if (thePrefs.GetLogUlDlEvents() && !theApp.uploadBandwidthThrottler->RemoveFromStandardList(client->socket) && !theApp.uploadBandwidthThrottler->RemoveFromStandardList((CClientReqSocket*)client->m_pPCUpSocket)))
			//	AddDebugLogLine(false, _T("UploadQueue: Didn't find socket to delete. Address: 0x%x"), client->socket);
			theApp.uploadBandwidthThrottler->RemoveFromStandardList(client->socket);
			theApp.uploadBandwidthThrottler->RemoveFromStandardList((CClientReqSocket*)client->m_pPCUpSocket);

			if (client->GetSessionUp() > 0) {
				++successfullupcount;
				totaluploadtime += client->GetUpStartTimeDelay() / SEC2MS(1);
			} else
				failedupcount += static_cast<uint32>(!earlyabort);

			CKnownFile *requestedFile = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
			if (requestedFile != NULL)
				requestedFile->UpdatePartsInfo();

			theApp.clientlist->AddTrackClient(client); // Keep track of this client
			client->SetUploadState(US_NONE);
			client->SetCollectionUploadSlot(false);

			m_iHighestNumberOfFullyActivatedSlotsSinceLastCall = 0;

			result = true;
		} else {
			curClientStruct->m_pClient->SetSlotNumber(slotCounter);
			++slotCounter;
		}
	}
	return result;
}

uint32 CUploadQueue::GetAverageUpTime() const
{
	return successfullupcount ? (totaluploadtime / successfullupcount) : 0;
}

bool CUploadQueue::RemoveFromWaitingQueue(CUpDownClient *client, bool updatewindow)
{
	POSITION pos = waitinglist.Find(client);
	if (pos) {
		RemoveFromWaitingQueue(pos, updatewindow);
		return true;
	}
	return false;
}

void CUploadQueue::RemoveFromWaitingQueue(POSITION pos, bool updatewindow)
{
	m_bStatisticsWaitingListDirty = true;
	CUpDownClient *todelete = waitinglist.GetAt(pos);
	waitinglist.RemoveAt(pos);
	if (updatewindow) {
		theApp.emuledlg->transferwnd->GetQueueList()->RemoveClient(todelete);
		theApp.emuledlg->transferwnd->ShowQueueCount(waitinglist.GetCount());
	}
	todelete->m_bAddNextConnect = false;
	todelete->SetUploadState(US_NONE);
}

void CUploadQueue::UpdateMaxClientScore()
{
	m_imaxscore = 0;
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;) {
		uint32 score = waitinglist.GetNext(pos)->GetScore(true, false);
		if (score > m_imaxscore)
			m_imaxscore = score;
	}
}

bool CUploadQueue::CheckForTimeOver(const CUpDownClient *client)
{
	//If we have nobody in the queue, do NOT remove the current uploads.
	//This will save some bandwidth and some unneeded swapping from upload/queue/upload.
	if (waitinglist.IsEmpty() || client->GetFriendSlot())
		return false;

	if (client->HasCollectionUploadSlot()) {
		const CKnownFile *pDownloadingFile = theApp.sharedfiles->GetFileByID(client->requpfileid);
		if (pDownloadingFile == NULL)
			return true;
		if (CCollection::HasCollectionExtention(pDownloadingFile->GetFileName()) && pDownloadingFile->GetFileSize() < (uint64)MAXPRIORITYCOLL_SIZE)
			return false;
		if (thePrefs.GetLogUlDlEvents())
			AddDebugLogLine(DLP_HIGH, false, _T("%s: Upload session ended - client with Collection Slot tried to request blocks from another file"), client->GetUserName());
		return true;
	}

	if (thePrefs.TransferFullChunks()) {
		// Allow the client to download a specified amount per session; but keep going if no one needs this slot
		if (client->GetQueueSessionPayloadUp() > SESSIONMAXTRANS && !ForceNewClient()) {
			if (thePrefs.GetLogUlDlEvents())
				AddDebugLogLine(DLP_DEFAULT, false, _T("%s: Upload session ended due to max transferred amount (%s)"), client->GetUserName(), (LPCTSTR)CastItoXBytes(SESSIONMAXTRANS));
			return true;
		}
	} else {
		// Try to keep the clients from downloading forever; but keep going if no one needs this slot
		if (client->GetUpStartTimeDelay() > SESSIONMAXTIME && !ForceNewClient()) {
			if (thePrefs.GetLogUlDlEvents())
				AddDebugLogLine(DLP_LOW, false, _T("%s: Upload session ended due to max time %s."), client->GetUserName(), (LPCTSTR)CastSecondsToHM(SESSIONMAXTIME / SEC2MS(1)));
			return true;
		}

		// Cache current client score
		const uint32 score = client->GetScore(true, true);

		// Check if another client has a bigger score
		if (score < GetMaxClientScore() && ::GetTickCount() >= m_dwRemovedClientByScore) {
			if (thePrefs.GetLogUlDlEvents())
				AddDebugLogLine(DLP_VERYLOW, false, _T("%s: Upload session ended due to score."), client->GetUserName());
			//Set timer to prevent too many upload slots getting kicked due to score.
			//Upload slots are delayed by a min of 1 sec and the max score is reset every 5 sec.
			//So, I choose 6 secs to make sure the max score it updated before doing this again.
			m_dwRemovedClientByScore = ::GetTickCount() + SEC2MS(6);
			return true;
		}
	}

	return false;
}

void CUploadQueue::DeleteAll()
{
	waitinglist.RemoveAll();
	m_csUploadListMainThrdWriteOtherThrdsRead.Lock();
	while (!uploadinglist.IsEmpty())
		delete uploadinglist.RemoveHead();
	m_csUploadListMainThrdWriteOtherThrdsRead.Unlock();
	// PENDING: Remove from UploadBandwidthThrottler as well!
}

UINT CUploadQueue::GetWaitingPosition(CUpDownClient *client)
{
	if (!IsOnUploadQueue(client))
		return 0;
	UINT rank = 1;
	UINT myscore = client->GetScore(false);
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;)
		rank += static_cast<UINT>(waitinglist.GetNext(pos)->GetScore(false) > myscore);

	return rank;
}

VOID CALLBACK CUploadQueue::UploadTimer(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/) noexcept
{
	// NOTE: Always handle all type of MFC exceptions in TimerProcs - otherwise we'll get mem leaks
	try {
		// Barry - Don't do anything if the app is shutting down - can cause unhandled exceptions
		if (theApp.IsClosing())
			return;

		// Elandal:ThreadSafeLogging -->
		// other threads may have queued up log lines. This prints them.
		theApp.HandleDebugLogQueue();
		theApp.HandleLogQueue();
		// Elandal: ThreadSafeLogging <--

		// ZZ:UploadSpeedSense -->
		theApp.lastCommonRouteFinder->SetPrefs(thePrefs.IsDynUpEnabled()
			, theApp.uploadqueue->GetDatarate()
			, thePrefs.GetMinUpload() * 1024
			, ((thePrefs.GetMaxUpload() != 0) ? thePrefs.GetMaxUpload() : thePrefs.GetMaxGraphUploadRate(false)) * 1024
			, thePrefs.IsDynUpUseMillisecondPingTolerance()
			, (thePrefs.GetDynUpPingTolerance() > 100) ? ((thePrefs.GetDynUpPingTolerance() - 100) / 100.0) : 0
			, thePrefs.GetDynUpPingToleranceMilliseconds()
			, thePrefs.GetDynUpGoingUpDivider()
			, thePrefs.GetDynUpGoingDownDivider()
			, thePrefs.GetDynUpNumberOfPings()
			, 20); // PENDING: Hard coded min pLowestPingAllowed
		// ZZ:UploadSpeedSense <--

		theApp.uploadqueue->Process();
		theApp.downloadqueue->Process();
		if (thePrefs.ShowOverhead()) {
			theStats.CompUpDatarateOverhead();
			theStats.CompDownDatarateOverhead();
		}

		// one second
		if (++counter >= 10) {
			counter = 0;

			// try to use different time intervals here to not create any disk-IO bottlenecks by saving all files at once
			theApp.clientcredits->Process();	// 13 minutes
			theApp.serverlist->Process();		// 17 minutes
			theApp.knownfiles->Process();		// 11 minutes
			theApp.friendlist->Process();		// 19 minutes
			theApp.clientlist->Process();
			theApp.sharedfiles->Process();
			if (Kademlia::CKademlia::IsRunning()) {
				Kademlia::CKademlia::Process();
				if (Kademlia::CKademlia::GetPrefs()->HasLostConnection()) {
					Kademlia::CKademlia::Stop();
					theApp.emuledlg->ShowConnectionState();
				}
			}
			if (theApp.serverconnect->IsConnecting() && !theApp.serverconnect->IsSingleConnect())
				theApp.serverconnect->TryAnotherConnectionRequest();

			theApp.listensocket->UpdateConnectionsStatus();
			if (thePrefs.WatchClipboard4ED2KLinks()) {
				// TODO: Remove this from here. This has to be done with a clipboard chain
				// and *not* with a timer!!
				theApp.SearchClipboard();
			}

			if (theApp.serverconnect->IsConnecting())
				theApp.serverconnect->CheckForTimeout();

			// 2 seconds
			if (++i2Secs >= 2) {
				i2Secs = 0;

				// Update connection stats...
				theStats.UpdateConnectionStats(theApp.uploadqueue->GetDatarate() / 1024.0f, theApp.downloadqueue->GetDatarate() / 1024.0f);

#ifdef HAVE_WIN7_SDK_H
				if (thePrefs.IsWin7TaskbarGoodiesEnabled())
					theApp.emuledlg->UpdateStatusBarProgress();
#endif
			}

			// display graphs
			if (thePrefs.GetTrafficOMeterInterval() > 0 && ++igraph >= (uint32)thePrefs.GetTrafficOMeterInterval()) {
				igraph = 0;
				theApp.emuledlg->statisticswnd->SetCurrentRate(theApp.uploadqueue->GetDatarate() / 1024.0f, theApp.downloadqueue->GetDatarate() / 1024.0f);
			}
			if (theApp.emuledlg->activewnd == theApp.emuledlg->statisticswnd && theApp.emuledlg->IsWindowVisible())
				// display stats
				if (thePrefs.GetStatsInterval() > 0 && ++istats >= (uint32)thePrefs.GetStatsInterval()) {
					istats = 0;
					theApp.emuledlg->statisticswnd->ShowStatistics();
				}

			theApp.uploadqueue->UpdateDatarates();

			//save rates every second
			theStats.RecordRate();

			// ZZ:UploadSpeedSense -->
			theApp.emuledlg->ShowPing();

			bool gotEnoughHosts = theApp.clientlist->GiveClientsForTraceRoute();
			if (!gotEnoughHosts)
				theApp.serverlist->GiveServersForTraceRoute();

			// ZZ:UploadSpeedSense <--

			if (theApp.emuledlg->IsTrayIconToFlash())
				theApp.emuledlg->ShowTransferRate(true);

			// *** 5 seconds **********************************************
			if (++sec >= 5) {
#ifdef _DEBUG
				if (thePrefs.m_iDbgHeap > 0 && !AfxCheckMemory())
					AfxDebugBreak();
#endif

				sec = 0;
				theApp.listensocket->Process();
				theApp.OnlineSig(); // Added By Bouc7
				if (!theApp.emuledlg->IsTrayIconToFlash())
					theApp.emuledlg->ShowTransferRate();

				thePrefs.EstimateMaxUploadCap(theApp.uploadqueue->GetDatarate() / 1024);

				if (!thePrefs.TransferFullChunks())
					theApp.uploadqueue->UpdateMaxClientScore();

				// update cat-titles with downloads info only when needed
				if (thePrefs.ShowCatTabInfos()
					&& theApp.emuledlg->activewnd == theApp.emuledlg->transferwnd
					&& theApp.emuledlg->IsWindowVisible())
				{
					theApp.emuledlg->transferwnd->UpdateCatTabTitles(false);
				}

				if (thePrefs.IsSchedulerEnabled())
					theApp.scheduler->Check();

				theApp.emuledlg->transferwnd->UpdateListCount(CTransferDlg::wnd2Uploading, -1);
			}

			// *** 60 seconds *********************************************
			if (++statsave >= 60) {
				statsave = 0;

				if (thePrefs.GetWSIsEnabled())
					theApp.webserver->UpdateSessionCount();

				theApp.serverconnect->KeepConnectionAlive();

				if (thePrefs.GetPreventStandby())
					theApp.ResetStandByIdleTimer(); // Reset Windows idle standby timer if necessary
			}

			if (++s_uSaveStatistics >= thePrefs.GetStatsSaveInterval()) {
				s_uSaveStatistics = 0;
				thePrefs.SaveStats();
			}
		}

		// need more accuracy here. don't rely on the 'sec' and 'statsave' helpers.
		thePerfLog.LogSamples();
	}
	CATCH_DFLT_EXCEPTIONS(_T("CUploadQueue::UploadTimer"))
}

CUpDownClient* CUploadQueue::GetNextClient(const CUpDownClient *lastclient) const
{
	if (waitinglist.IsEmpty())
		return NULL;
	if (!lastclient)
		return waitinglist.GetHead();
	POSITION pos = waitinglist.Find(const_cast<CUpDownClient*>(lastclient));
	if (!pos) {
		TRACE("Error: CUploadQueue::GetNextClient");
		return waitinglist.GetHead();
	}
	waitinglist.GetNext(pos);
	return pos ? waitinglist.GetAt(pos) : NULL;
}

void CUploadQueue::UpdateDatarates()
{
	// Calculate average data rate
	const DWORD tick = ::GetTickCount();
	if (tick >= m_lastCalculatedDataRateTick + 500) {
		m_lastCalculatedDataRateTick = tick;

		if (average_dr_list.GetCount() >= 2 && average_tick_list.GetTail() > average_tick_list.GetHead()) {
			DWORD duration = average_tick_list.GetTail() - average_tick_list.GetHead();
			datarate = (uint32)(SEC2MS(m_average_dr_sum - average_dr_list.GetHead()) / duration);
			friendDatarate = (uint32)(SEC2MS(average_friend_dr_list.GetTail() - average_friend_dr_list.GetHead()) / duration);
		}
	}
}

uint32 CUploadQueue::GetToNetworkDatarate() const
{
	return (datarate > friendDatarate) ? datarate - friendDatarate : 0;
}

void CUploadQueue::ReSortUploadSlots(bool force)
{
	const DWORD tick = ::GetTickCount();
	if (force || tick >= m_dwLastResortedUploadSlots + SEC2MS(10)) {
		m_dwLastResortedUploadSlots = tick;

		theApp.uploadBandwidthThrottler->Pause(true);

		CUploadingPtrList tempUploadinglist;

		// Remove all clients from uploading list and store in tempList
		m_csUploadListMainThrdWriteOtherThrdsRead.Lock();
		while (!uploadinglist.IsEmpty()) {
			// Get and remove the client from upload list.
			UploadingToClient_Struct *pCurClientStruct = uploadinglist.RemoveHead();
			const CUpDownClient *cur_client = pCurClientStruct->m_pClient;

			// Remove the found Client from UploadBandwidthThrottler
			theApp.uploadBandwidthThrottler->RemoveFromStandardList(cur_client->socket);
			theApp.uploadBandwidthThrottler->RemoveFromStandardList((CClientReqSocket*)cur_client->m_pPCUpSocket);

			tempUploadinglist.AddTail(pCurClientStruct);
		}

		// Remove one at a time from temp list and reinsert in correct position in uploading list
		while (!tempUploadinglist.IsEmpty())
			InsertInUploadingList(tempUploadinglist.RemoveHead(), true);

		m_csUploadListMainThrdWriteOtherThrdsRead.Unlock();

		theApp.uploadBandwidthThrottler->Pause(false);
	}
}

uint32 CUploadQueue::GetWaitingUserForFileCount(const CSimpleArray<CObject*> &raFiles, bool bOnlyIfChanged)
{
	if (bOnlyIfChanged && !m_bStatisticsWaitingListDirty)
		return _UI32_MAX;

	m_bStatisticsWaitingListDirty = false;
	uint32 nResult = 0;
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL; ) {
		const CUpDownClient *cur_client = waitinglist.GetNext(pos);
		for (int i = raFiles.GetSize(); --i >= 0;)
			nResult += static_cast<uint32>(md4equ(static_cast<CKnownFile*>(raFiles[i])->GetFileHash(), cur_client->GetUploadFileID()));
	}
	return nResult;
}

uint32 CUploadQueue::GetDatarateForFile(const CSimpleArray<CObject*> &raFiles) const
{
	uint32 nResult = 0;
	for (POSITION pos = uploadinglist.GetHeadPosition(); pos != NULL;) {
		const CUpDownClient *cur_client = uploadinglist.GetNext(pos)->m_pClient;
		for (int i = raFiles.GetSize(); --i >= 0;)
			if (md4equ(static_cast<CKnownFile*>(raFiles[i])->GetFileHash(), cur_client->GetUploadFileID()))
				nResult += cur_client->GetDatarate();
	}
	return nResult;
}

const CUploadingPtrList& CUploadQueue::GetUploadListTS(CCriticalSection **outUploadListReadLock)
{
	ASSERT(*outUploadListReadLock == NULL);
	*outUploadListReadLock = &m_csUploadListMainThrdWriteOtherThrdsRead;
	return uploadinglist;
}

UploadingToClient_Struct* CUploadQueue::GetUploadingClientStructByClient(const CUpDownClient *pClient) const
{
	//TODO: Check if this function is too slow for its usage (esp. when rendering the GUI bars)
	//		if necessary we will have to speed it up with an additional map
	for (POSITION pos = uploadinglist.GetHeadPosition(); pos != NULL;) {
		UploadingToClient_Struct *pCurClientStruct = uploadinglist.GetNext(pos);
		if (pCurClientStruct->m_pClient == pClient)
			return pCurClientStruct;
	}
	return NULL;
}

UploadingToClient_Struct::~UploadingToClient_Struct()
{
	m_pClient->FlushSendBlocks();

	m_csBlockListsLock.Lock();
	while (!m_BlockRequests_queue.IsEmpty())
		delete m_BlockRequests_queue.RemoveHead();

	while (!m_DoneBlocks_list.IsEmpty())
		delete m_DoneBlocks_list.RemoveHead();
	m_csBlockListsLock.Unlock();
}