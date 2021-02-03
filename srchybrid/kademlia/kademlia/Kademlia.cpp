/*
Copyright (C)2003 Barry Dunne (http://www.emule-project.net)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

// Note To Mods //
/*
Please do not change anything here and release it.
There is going to be a new forum created just for the Kademlia side of the client.
If you feel there is an error or a way to improve something, please
post it in the forum first and let us look at it. If it is a real improvement,
it will be added to the official client. Changing something without knowing
what all it does can cause great harm to the network if released in mass form.
Any mod that changes anything within the Kademlia side will not be allowed to advertise
their client on the eMule forum.
*/

#include "stdafx.h"
#include "emule.h"
#include "preferences.h"
#include "emuledlg.h"
#include "opcodes.h"
#include "Log.h"
#include "MD4.h"
#include "StringConversion.h"
#include "kademliawnd.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/defines.h"
#include "kademlia/kademlia/Prefs.h"
#include "kademlia/kademlia/SearchManager.h"
#include "kademlia/kademlia/Indexed.h"
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "kademlia/net/KademliaUDPListener.h"
#include "kademlia/routing/RoutingZone.h"
#include "kademlia/routing/contact.h"
#include "kademlia/utils/KadUDPKey.h"
#include "kademlia/utils/KadClientSearcher.h"
#include "kademlia/kademlia/tag.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace Kademlia;

CKademlia	*CKademlia::m_pInstance = NULL;
EventMap	CKademlia::m_mapEvents;
time_t		CKademlia::m_tNextSearchJumpStart;
time_t		CKademlia::m_tNextSelfLookup;
time_t		CKademlia::m_tStatusUpdate;
time_t		CKademlia::m_tBigTimer;
time_t		CKademlia::m_tNextFirewallCheck;
time_t		CKademlia::m_tNextUPnPCheck;
time_t		CKademlia::m_tNextFindBuddy;
time_t		CKademlia::m_tBootstrap;
time_t		CKademlia::m_tConsolidate;
time_t		CKademlia::m_tExternPortLookup;
time_t		CKademlia::m_tLANModeCheck = 0;
volatile bool	CKademlia::m_bRunning = false;
bool		CKademlia::m_bLANMode = false;
CList<uint32, uint32> CKademlia::m_liStatsEstUsersProbes;
_ContactList CKademlia::s_liBootstrapList;
bool		CKademlia::m_bootstrapping = false;

CKademlia::CKademlia()
	: m_pPrefs()
	, m_pRoutingZone()
	, m_pUDPListener()
	, m_pIndexed()
{
}

void CKademlia::Start()
{
	// Create a new default pref object.
	Start(new CPrefs());
}

void CKademlia::Start(CPrefs *pPrefs)
{
	try {
		// If we already have an instance, something is wrong.
		if (m_pInstance) {
			delete pPrefs;
			ASSERT(m_pInstance->m_bRunning);
			ASSERT(m_pInstance->m_pPrefs);
			return;
		}

		// Make sure a prefs was passed in.
		if (!pPrefs)
			return;

		AddDebugLogLine(false, _T("Starting Kademlia"));

		time_t tNow = time(NULL);
		// Init jump start timer.
		m_tNextSearchJumpStart = tNow;
		// Force a FindNodeComplete within the first 3 minutes.
		m_tNextSelfLookup = tNow + MIN2S(3);
		// Init status timer.
		m_tStatusUpdate = tNow;
		// Init big timer for Zones
		m_tBigTimer = tNow;
		// First Firewall check is done on connect, init next check.
		m_tNextFirewallCheck = tNow + HR2S(1);
		m_tNextUPnPCheck = m_tNextFirewallCheck - MIN2S(1);
		// Find a buddy after the first 5 mins of starting the client.
		// We wait just in case it takes a bit for the client to determine firewall status.
		m_tNextFindBuddy = tNow + MIN2S(5);
		// Init contact consolidate timer;
		m_tConsolidate = tNow + MIN2S(45);
		// Looking up our extern port
		m_tExternPortLookup = tNow;
		// Init bootstrap time.
		m_tBootstrap = 0;
		// Init our random seed.
		srand((UINT)tNow);
		// Create our Kad objects.
		m_pInstance = new CKademlia();
		m_pInstance->m_pPrefs = pPrefs;
		m_pInstance->m_pIndexed = new CIndexed();
		m_pInstance->m_pRoutingZone = new CRoutingZone();
		m_pInstance->m_pUDPListener = new CKademliaUDPListener();
		// Mark Kad as running state.
		m_bRunning = true;
	} catch (CException *e) {
		// Although this has never been an issue, maybe some code needs
		// to be created here just in case things go real bad. But if things
		// went real bad, the entire client most likely is in bad shape, so this may
		// not be something to worry about as the client most likely will crap out anyway.
		TCHAR err[512];
		GetExceptionMessage(*e, err, 512);
		AddDebugLogLine(false, _T("%s"), err);
		e->Delete();
	}
}

void CKademlia::Stop()
{
	// Make sure we are running to begin with.
	if (!m_bRunning)
		return;

	// Mark Kad as being in the stop state to make sure nothing else is used.
	m_bRunning = false;

	AddDebugLogLine(false, _T("Stopping Kademlia"));

	// Reset Firewall state
	CUDPFirewallTester::Reset();

	// Remove all active searches.
	CSearchManager::StopAllSearches();

	// Delete all Kad Objects.
	delete m_pInstance->m_pUDPListener;
	m_pInstance->m_pUDPListener = NULL;

	delete m_pInstance->m_pRoutingZone;
	m_pInstance->m_pRoutingZone = NULL;

	delete m_pInstance->m_pIndexed;
	m_pInstance->m_pIndexed = NULL;

	delete m_pInstance->m_pPrefs;
	m_pInstance->m_pPrefs = NULL;

	delete m_pInstance;
	m_pInstance = NULL;

	while (!s_liBootstrapList.IsEmpty())
		delete s_liBootstrapList.RemoveHead();

	m_bootstrapping = false;

	// Make sure all zones are removed.
	m_mapEvents.clear();
}

void CKademlia::Process()
{
	if (m_pInstance == NULL || !m_bRunning)
		return;
	uint32 uMaxUsers = 0;
	time_t tNow = time(NULL);
	ASSERT(m_pInstance->m_pPrefs != NULL);
	time_t uLastContact = m_pInstance->m_pPrefs->GetLastContact();
	CSearchManager::UpdateStats();
	bool bUpdateUserFile = (tNow >= m_tStatusUpdate);
	if (bUpdateUserFile) {
		m_tStatusUpdate = tNow + MIN2S(1);
#ifdef _BOOTSTRAPNODESDAT
		// do some random lookup to fill out contact list with fresh (but for routing useless) nodes which we can
		// use for our bootstrap nodes.dat
		if (GetRoutingZone()->GetNumContacts() < 1500) {
			CUInt128 uRandom;
			uRandom.SetValueRandom();
			CSearchManager::FindNode(uRandom, false);
		}
#endif
	}
	if (tNow >= m_tNextFirewallCheck)
		RecheckFirewalled();
	if (m_tNextUPnPCheck != 0 && tNow >= m_tNextUPnPCheck) {
		theApp.emuledlg->RefreshUPnP();
		m_tNextUPnPCheck = 0; // will be reset on firewall check
	}

	if (tNow >= m_tNextSelfLookup) {
		CSearchManager::FindNode(m_pInstance->m_pPrefs->GetKadID(), true);
		m_tNextSelfLookup = tNow + HR2S(4);
	}
	if (tNow >= m_tNextFindBuddy) {
		m_pInstance->m_pPrefs->SetFindBuddy();
		m_tNextFindBuddy = tNow + MIN2S(20);
	}
	if (tNow >= m_tExternPortLookup && CUDPFirewallTester::IsFWCheckUDPRunning() && GetPrefs()->FindExternKadPort(false)) {
		// if our UDP firewall check is running and we don't know our external port, we send a request every 15 seconds
		CContact *pContact = GetRoutingZone()->GetRandomContact(3, KADEMLIA_VERSION6_49aBETA);
		if (pContact != NULL) {
			DEBUG_ONLY(DebugLog(_T("Requesting our external port from %s"), (LPCTSTR)ipstr(pContact->GetNetIP())));
			GetUDPListener()->SendNullPacket(KADEMLIA2_PING, pContact->GetIPAddress(), pContact->GetUDPPort(), pContact->GetUDPKey(), &pContact->GetClientID());
		} else
			DEBUG_ONLY(DebugLogWarning(_T("No valid client for requesting external port available")));
		m_tExternPortLookup = tNow + 15;
	}
	for (EventMap::const_iterator itEventMap = m_mapEvents.begin(); itEventMap != m_mapEvents.end(); ++itEventMap) {
		CRoutingZone *pZone = itEventMap->first;
		if (bUpdateUserFile) {
			// The EstimateCount function is not made for really small networks, if we are in LAN mode, it is actually
			// better to assume that all users of the network are in our routing table and use the real count function
			uint32 uTempUsers = IsRunningInLANMode() ? pZone->GetNumContacts() : pZone->EstimateCount();
			if (uMaxUsers < uTempUsers)
				uMaxUsers = uTempUsers;
		}
		if (tNow >= m_tBigTimer
			&& (tNow >= pZone->m_tNextBigTimer || (uLastContact && tNow >= uLastContact + KADEMLIADISCONNECTDELAY - MIN2S(5)))
			&& pZone->OnBigTimer())
		{
			pZone->m_tNextBigTimer = tNow + HR2S(1);
			m_tBigTimer = tNow + SEC(10);
		}
		if (tNow >= pZone->m_tNextSmallTimer) {
			pZone->OnSmallTimer();
			pZone->m_tNextSmallTimer = tNow + MIN2S(1);
		}
	}

	// This is a convenient place to add this, although not related to routing
	if (tNow >= m_tNextSearchJumpStart) {
		CSearchManager::JumpStart();
		m_tNextSearchJumpStart = tNow + SEARCH_JUMPSTART;
	}

	// Try to consolidate any zones that are close to empty.
	if (tNow >= m_tConsolidate) {
		uint32 uMergedCount = m_pInstance->m_pRoutingZone->Consolidate();
		if (uMergedCount)
			AddDebugLogLine(false, _T("Kad merged %u Zones"), uMergedCount);
		m_tConsolidate = tNow + MIN2S(45);
	}

	//Update user count only if changed.
	if (bUpdateUserFile) {
		if (uMaxUsers != m_pInstance->m_pPrefs->GetKademliaUsers()) {
			m_pInstance->m_pPrefs->SetKademliaUsers(uMaxUsers);
			m_pInstance->m_pPrefs->SetKademliaFiles();
			theApp.emuledlg->ShowUserCount();
		}
	}

	if (!IsConnected() && (tNow >= m_tBootstrap + 15 || (GetRoutingZone()->GetNumContacts() == 0 && tNow >= m_tBootstrap + 2)))
		if (!s_liBootstrapList.IsEmpty()) {
			CContact *pContact = s_liBootstrapList.RemoveHead();
			m_tBootstrap = tNow;
			m_bootstrapping = true;
			DebugLog(_T("Trying to Bootstrap Kad from %s, Distance: %s, Version: %u, %u Contacts left"), (LPCTSTR)ipstr(pContact->GetNetIP()), (LPCTSTR)pContact->GetDistance().ToHexString(), pContact->GetVersion(), s_liBootstrapList.GetCount());
			m_pInstance->m_pUDPListener->Bootstrap(pContact->GetIPAddress(), pContact->GetUDPPort(), pContact->GetVersion(), &pContact->GetClientID());
			delete pContact;
			theApp.emuledlg->kademliawnd->StartUpdateContacts();
		} else if (m_bootstrapping) {
			// failed to bootstrap
			m_bootstrapping = false;
			AddLogLine(true, GetResString(IDS_BOOTSTRAPFAILED));
		}

	if (GetUDPListener() != NULL)
		GetUDPListener()->ExpireClientSearch(); // function does only one compare in most cases, so no real need for a timer
}

void CKademlia::AddEvent(CRoutingZone *pZone)
{
	m_mapEvents[pZone] = pZone;
}

void CKademlia::RemoveEvent(CRoutingZone *pZone)
{
	m_mapEvents.erase(pZone);
}

bool CKademlia::IsConnected()
{
	if (m_pInstance && m_pInstance->m_pPrefs)
		return m_pInstance->m_pPrefs->HasHadContact();
	return false;
}

bool CKademlia::IsFirewalled()
{
	if (m_pInstance && m_pInstance->m_pPrefs)
		return m_pInstance->m_pPrefs->GetFirewalled() && !IsRunningInLANMode();
	return true;
}

uint32 CKademlia::GetKademliaUsers(bool bNewMethod)
{
	if (m_pInstance && m_pInstance->m_pPrefs) {
		if (bNewMethod)
			return CalculateKadUsersNew();
		else
			return m_pInstance->m_pPrefs->GetKademliaUsers();
	}
	return 0;
}

uint32 CKademlia::GetKademliaFiles()
{
	if (m_pInstance && m_pInstance->m_pPrefs)
		return m_pInstance->m_pPrefs->GetKademliaFiles();
	return 0;
}

uint32 CKademlia::GetTotalStoreKey()
{
	if (m_pInstance && m_pInstance->m_pPrefs)
		return m_pInstance->m_pPrefs->GetTotalStoreKey();
	return 0;
}

uint32 CKademlia::GetTotalStoreSrc()
{
	if (m_pInstance && m_pInstance->m_pPrefs)
		return m_pInstance->m_pPrefs->GetTotalStoreSrc();
	return 0;
}

uint32 CKademlia::GetTotalStoreNotes()
{
	if (m_pInstance && m_pInstance->m_pPrefs)
		return m_pInstance->m_pPrefs->GetTotalStoreNotes();
	return 0;
}

uint32 CKademlia::GetTotalFile()
{
	if (m_pInstance && m_pInstance->m_pPrefs)
		return m_pInstance->m_pPrefs->GetTotalFile();
	return 0;
}

uint32 CKademlia::GetIPAddress()
{
	if (m_pInstance && m_pInstance->m_pPrefs)
		return m_pInstance->m_pPrefs->GetIPAddress();
	return 0;
}

void CKademlia::ProcessPacket(const byte *pbyData, uint32 uLenData, uint32 uIP, uint16 uPort, bool bValidReceiverKey, const CKadUDPKey &senderUDPKey)
{
	if (m_pInstance && m_pInstance->m_pUDPListener)
		m_pInstance->m_pUDPListener->ProcessPacket(pbyData, uLenData, uIP, uPort, bValidReceiverKey, senderUDPKey);
}

bool CKademlia::GetPublish()
{
	if (m_pInstance && m_pInstance->m_pPrefs)
		return m_pInstance->m_pPrefs->GetPublish();
	return 0;
}

void CKademlia::Bootstrap(LPCTSTR szHost, uint16 uPort)
{
	if (m_pInstance && m_pInstance->m_pUDPListener && !IsConnected() && time(NULL) >= m_tBootstrap + 10) {
		m_tBootstrap = time(NULL);
		m_pInstance->m_pUDPListener->Bootstrap(szHost, uPort);
	}
}

void CKademlia::Bootstrap(uint32 uIP, uint16 uPort)
{
	if (m_pInstance && m_pInstance->m_pUDPListener && !IsConnected() && time(NULL) >= m_tBootstrap + 10) {
		m_tBootstrap = time(NULL);
		m_pInstance->m_pUDPListener->Bootstrap(uIP, uPort);
	}
}

void CKademlia::RecheckFirewalled()
{
	if (m_pInstance && m_pInstance->GetPrefs() && !IsRunningInLANMode()) {
		// Something is forcing a new firewall check
		// Stop any new buddy requests, and tell the client
		// to recheck it's IP which in turns rechecks firewall.
		m_pInstance->m_pPrefs->SetFindBuddy(false);
		m_pInstance->m_pPrefs->SetRecheckIP();
		// also UDP check
		CUDPFirewallTester::ReCheckFirewallUDP(false);

		time_t tNow = time(NULL);
		// Delay the next buddy search to at least 5 minutes after our firewall check so we are sure to be still firewalled
		if (m_tNextFindBuddy < tNow + MIN2S(5))
			m_tNextFindBuddy = tNow + MIN2S(5);
		m_tNextFirewallCheck = tNow + HR2S(1);
		m_tNextUPnPCheck = m_tNextFirewallCheck - MIN2S(1);
	}
}

CPrefs* CKademlia::GetPrefs()
{
	if (m_pInstance == NULL || m_pInstance->m_pPrefs == NULL) {
		//ASSERT(0);
		return NULL;
	}
	return m_pInstance->m_pPrefs;
}

CKademliaUDPListener* CKademlia::GetUDPListener()
{
	if (m_pInstance == NULL || m_pInstance->m_pUDPListener == NULL) {
		ASSERT(0);
		return NULL;
	}
	return m_pInstance->m_pUDPListener;
}

CRoutingZone* CKademlia::GetRoutingZone()
{
	if (m_pInstance == NULL || m_pInstance->m_pRoutingZone == NULL) {
		ASSERT(0);
		return NULL;
	}
	return m_pInstance->m_pRoutingZone;
}

CIndexed* CKademlia::GetIndexed()
{
	if (m_pInstance == NULL || m_pInstance->m_pIndexed == NULL) {
		ASSERT(0);
		return NULL;
	}
	return m_pInstance->m_pIndexed;
}

bool CKademlia::IsRunning()
{
	return m_bRunning;
}

bool CKademlia::FindNodeIDByIP(CKadClientSearcher &rRequester, uint32 dwIP, uint16 nTCPPort, uint16 nUDPPort)
{
	if (!IsRunning() || m_pInstance == NULL || GetUDPListener() == NULL || GetRoutingZone() == NULL) {
		ASSERT(0);
		return false;
	}
	// first search our known contacts if we can deliver a result without asking, otherwise forward the request
	const CContact *pContact = GetRoutingZone()->GetContact(ntohl(dwIP), nTCPPort, true);
	if (pContact != NULL) {
		uchar uchID[16];
		pContact->GetClientID().ToByteArray(uchID);
		rRequester.KadSearchNodeIDByIPResult(KCSR_SUCCEEDED, uchID);
		return true;
	}
	return GetUDPListener()->FindNodeIDByIP(&rRequester, ntohl(dwIP), nTCPPort, nUDPPort);
}

bool CKademlia::FindIPByNodeID(CKadClientSearcher &rRequester, const uchar *pachNodeID)
{
	if (!IsRunning() || m_pInstance == NULL || GetRoutingZone() == NULL) {
		ASSERT(0);
		return false;
	}
	// first search our known contacts if we can deliver a result without asking, otherwise forward the request
	const CContact *pContact = GetRoutingZone()->GetContact(CUInt128(pachNodeID));
	if (pContact != NULL) {
		// make sure that this entry is not too old, otherwise just do a search to be sure
		if (pContact->GetLastSeen() != 0 && time(NULL) < pContact->GetLastSeen() + MIN2S(30)) {
			rRequester.KadSearchIPByNodeIDResult(KCSR_SUCCEEDED, pContact->GetNetIP(), pContact->GetTCPPort());
			return true;
		}
	}
	return CSearchManager::FindNodeSpecial(CUInt128(pachNodeID), &rRequester);
}

void CKademlia::CancelClientSearch(CKadClientSearcher &rFromRequester)
{
	if (m_pInstance == NULL || GetUDPListener() == NULL) {
		ASSERT(0);
		return;
	}
	GetUDPListener()->ExpireClientSearch(&rFromRequester);
	CSearchManager::CancelNodeSpecial(&rFromRequester);
}

void KadGetKeywordHash(const CStringA &rstrKeywordA, Kademlia::CUInt128 *pKadID)
{
	CMD4 md4;
	md4.Add((byte*)(LPCSTR)rstrKeywordA, rstrKeywordA.GetLength());
	md4.Finish();
	pKadID->SetValueBE(md4.GetHash());
}

CStringA KadGetKeywordBytes(const Kademlia::CKadTagValueString &rstrKeywordW)
{
	return CStringA(wc2utf8(rstrKeywordW));
}

void KadGetKeywordHash(const Kademlia::CKadTagValueString &rstrKeywordW, Kademlia::CUInt128 *pKadID)
{
	KadGetKeywordHash(KadGetKeywordBytes(rstrKeywordW), pKadID);
}

void CKademlia::StatsAddClosestDistance(const CUInt128 &uDist)
{
	if (uDist.Get32BitChunk(0) > 0) {
		uint32 nToAdd = (_UI32_MAX / uDist.Get32BitChunk(0)) / 2;
		if (m_liStatsEstUsersProbes.Find(nToAdd) == NULL)
			m_liStatsEstUsersProbes.AddHead(nToAdd);
	}
	if (m_liStatsEstUsersProbes.GetCount() > 100)
		m_liStatsEstUsersProbes.RemoveTail();
}

uint32 CKademlia::CalculateKadUsersNew()
{
	// the idea of calculating the user count with this method is simple:
	// whenever we do search for any NodeID (except in certain cases were the result is not usable),
	// we remember the distance of the closest node we found. Because we assume all NodeIDs
	// are distributed equally, we can calculate based on this distance how "filled" the possible
	// NodesID room is and by this calculate how many users there are. Of course this only works
	// Each single sample will be wrong, but if we have enough samples, the average should
	// produce a usable number. To avoid drifts caused by a single (or more) really close or really
	// far away hits, we do use median-average instead

	// doesn't works well if we have no files to index and nothing to download and the numbers seems
	// to be a bit too low compared to our other method. So lets stay with the old one for now,
	// but keeps this here as an alternative

	if (m_liStatsEstUsersProbes.GetCount() < 10)
		return 0;

	CList<uint32, uint32> liMedian;
	for (POSITION pos = m_liStatsEstUsersProbes.GetHeadPosition(); pos != NULL; ) {
		uint32 nProbe = m_liStatsEstUsersProbes.GetNext(pos);
		bool bInserted = false;
		for (POSITION pos2 = liMedian.GetHeadPosition(); pos2 != NULL;) {
			POSITION pos3 = pos2;
			if (liMedian.GetNext(pos2) > nProbe) {
				liMedian.InsertBefore(pos3, nProbe);
				bInserted = true;
				break;
			}
		}
		if (!bInserted)
			liMedian.AddTail(nProbe);
	}
	// cut away 1/3 of the values - 1/6 of the top and 1/6 of the bottom  to avoid spikes having too much influence, build the average of the rest
	for (INT_PTR nCut = liMedian.GetCount() / 6; --nCut >= 0;) {
		liMedian.RemoveHead();
		liMedian.RemoveTail();
	}
	uint64 nMedian = 0;
	for (POSITION pos = liMedian.GetHeadPosition(); pos != NULL; )
		nMedian += liMedian.GetNext(pos);
	nMedian /= liMedian.GetCount();

	// LowIDModififier
	// Modify count by assuming 20% of the users are firewalled and can't be a contact for < 0.49b nodes
	// Modify count by actual statistics of Firewalled ratio for >= 0.49b if we are not firewalled ourself
	// Modify count by 40% for >= 0.49b if we are firewalled ourself (the actual Firewalled count at this date on kad is 35-55%)
	const float fFirewalledModifyOld = 1.20F;
	float fNewRatio = 0;
	float fFirewalledModifyNew;
	if (CUDPFirewallTester::IsFirewalledUDP(true))
		fFirewalledModifyNew = 1.40F; // we are firewalled and get the real statistic, assume 40% firewalled >=0.49b nodes
	else {
		CPrefs *pPrefs = GetPrefs();
		if (pPrefs && pPrefs->StatsGetFirewalledRatio(true) > 0) {
			fFirewalledModifyNew = 1.0F + (pPrefs->StatsGetFirewalledRatio(true)); // apply the firewalled ratio to the modify
			fNewRatio = pPrefs->StatsGetKadV8Ratio();
			ASSERT(fFirewalledModifyNew > 1.0F && fFirewalledModifyNew < 1.90F);
		} else
			fFirewalledModifyNew = 0;
	}
	float fFirewalledModifyTotal;
	if (fNewRatio > 0 && fFirewalledModifyNew > 0) // weight the old and the new modifier based on how many new contacts we have
		fFirewalledModifyTotal = (fNewRatio * fFirewalledModifyNew) + ((1 - fNewRatio) * fFirewalledModifyOld);
	else
		fFirewalledModifyTotal = fFirewalledModifyOld;
	ASSERT(fFirewalledModifyTotal > 1.0F && fFirewalledModifyTotal < 1.90F);

	return (uint32)(nMedian * fFirewalledModifyTotal);
}

bool CKademlia::IsRunningInLANMode()
{
	if (thePrefs.FilterLANIPs() || !IsRunning() || GetRoutingZone() == NULL)
		return false;
	if (time(NULL) >= m_tLANModeCheck + 10) {
		m_tLANModeCheck = time(NULL);
		uint32 nCount = GetRoutingZone()->GetNumContacts();
		// Limit to 256 nodes, if we have more we don't want to use the LAN mode which is assuming we use a small home LAN
		// (otherwise we might need to do firewall check, external port requests etc after all)
		if (nCount == 0 || nCount > 256)
			m_bLANMode = false;
		else {
			if (GetRoutingZone()->HasOnlyLANNodes()) {
				if (!m_bLANMode) {
					m_bLANMode = true;
					theApp.emuledlg->ShowConnectionState();
					DebugLog(_T("Kademlia: Activating LAN Mode"));
				}
			} else if (m_bLANMode) {
				m_bLANMode = false;
				theApp.emuledlg->ShowConnectionState();
			}
		}
	}
	return m_bLANMode;
}