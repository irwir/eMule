/*
Copyright (C)2003 Barry Dunne (http://www.emule-project.net)
Copyright (C)2004-2010 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )

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
#include "ClientList.h"
#include "DownloadQueue.h"
#include "emule.h"
#include "emuledlg.h"
#include "kademliawnd.h"
#include "KadSearchListCtrl.h"
#include "Log.h"
#include "Packets.h"
#include "partfile.h"
#include "SearchList.h"
#include "sharedfilelist.h"
#include "UpDownClient.h"
#include "KnownFileList.h"
#include "kademlia/io/ByteIO.h"
#include "kademlia/io/IOException.h"
#include "kademlia/kademlia/Defines.h"
#include "kademlia/kademlia/Entry.h"
#include "kademlia/kademlia/Indexed.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Prefs.h"
#include "kademlia/kademlia/Search.h"
#include "kademlia/kademlia/SearchManager.h"
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "kademlia/net/KademliaUDPListener.h"
#include "kademlia/routing/RoutingZone.h"
#include "kademlia/utils/KadClientSearcher.h"
#include "kademlia/utils/LookupHistory.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace Kademlia;

//void DebugSend(LPCTSTR pszMsg, uint32 uIP, uint16 uUDPPort);

CSearch::CSearch()
	: m_bStoping()
	, m_uType(_UI32_MAX)
	, m_uAnswers()
	, m_uTotalRequestAnswers()
	, m_uKadPacketSent()
	, m_uTotalLoad()
	, m_uTotalLoadResponses()
	, m_uSearchID(_UI32_MAX)
	, m_uSearchTermsDataSize()
	, m_pucSearchTermsData()
	, m_pSearchTerm()
	, pNodeSpecialSearchRequester()
	, m_uTarget()
	, m_uClosestDistantFound()
	, pRequestedMoreNodesContact()
{
	m_pLookupHistory = new CLookupHistory();
	m_tCreated = m_uLastResponse = time(NULL);
	theApp.emuledlg->kademliawnd->searchList->SearchAdd(this);
}

CSearch::~CSearch()
{
	// remember the closest node we found and tried to contact (if any) during this search
	// for statistical calculations, but only if it's a certain type
	switch (m_uType) {
	case NODECOMPLETE:
	case FILE:
	case KEYWORD:
	case NOTES:
	case STOREFILE:
	case STOREKEYWORD:
	case STORENOTES:
	case FINDSOURCE: // maybe also exclude
		if (m_uClosestDistantFound != 0)
			CKademlia::StatsAddClosestDistance(m_uClosestDistantFound);
	default: // NODE, NODESPECIAL, NODEFWCHECKUDP, FINDBUDDY
		break;
	}

	if (pNodeSpecialSearchRequester != NULL) {
		// inform requester that our search failed
		pNodeSpecialSearchRequester->KadSearchIPByNodeIDResult(KCSR_NOTFOUND, 0, 0);
		pNodeSpecialSearchRequester = NULL;
	}

	// Remove search from GUI
	theApp.emuledlg->kademliawnd->searchList->SearchRem(this);

	// delete/deref search history (will delete itself if not used by the GUI)
	if (m_pLookupHistory != NULL) {
		m_pLookupHistory->SetSearchDeleted();
		m_pLookupHistory = NULL;
	}
	theApp.emuledlg->kademliawnd->UpdateSearchGraph(NULL);

	// Check if a source search is currently being done.
	CPartFile *pPartFile = theApp.downloadqueue->GetFileByKadFileSearchID(GetSearchID());

	// Reset the searchID if a source search is currently being done.
	if (pPartFile)
		pPartFile->SetKadFileSearchID(0);

	if (m_uType == NOTES) {
		CAbstractFile *pAbstractFile = theApp.knownfiles->FindKnownFileByID(CUInt128(GetTarget().GetData()).GetData());
		if (pAbstractFile != NULL)
			pAbstractFile->SetKadCommentSearchRunning(false);

		pAbstractFile = theApp.downloadqueue->GetFileByID(CUInt128(GetTarget().GetData()).GetData());
		if (pAbstractFile != NULL)
			pAbstractFile->SetKadCommentSearchRunning(false);

		theApp.searchlist->SetNotesSearchStatus(CUInt128(GetTarget().GetData()).GetData(), false);
	}

	// Decrease the use count for any contacts that are in your contact list.
	for (ContactMap::const_iterator itInUseMap = m_mapInUse.begin(); itInUseMap != m_mapInUse.end(); ++itInUseMap)
		itInUseMap->second->DecUse();

	// Delete any temp contacts.
	for (ContactList::const_iterator itDeleteList = m_listDelete.begin(); itDeleteList != m_listDelete.end(); ++itDeleteList)
		if (!(*itDeleteList)->InUse())
			delete *itDeleteList;

	// Check if this search was contacting an overloaded node and adjust time of next time we use that node.
	if (CKademlia::IsRunning() && GetNodeLoad() > 20 && GetSearchType() == CSearch::STOREKEYWORD)
		Kademlia::CKademlia::GetIndexed()->AddLoad(GetTarget(), time(NULL) + (time_t)(DAY2S(7) * (GetNodeLoad() / 100.0)));
	delete[] m_pucSearchTermsData;

	CKademlia::GetUDPListener()->Free(m_pSearchTerm);
	m_pSearchTerm = NULL;
}

void CSearch::Go()
{
	// Start with a lot of possible contacts, this is a fallback in case search stalls due to dead contacts
	if (m_mapPossible.empty()) {
		CUInt128 uDistance(CKademlia::GetPrefs()->GetKadID());
		uDistance.Xor(m_uTarget);
		CKademlia::GetRoutingZone()->GetClosestTo(3, m_uTarget, uDistance, 50, &m_mapPossible, true, true);

		for (ContactMap::const_iterator itPossibleMap = m_mapPossible.begin(); itPossibleMap != m_mapPossible.end(); ++itPossibleMap)
			m_pLookupHistory->ContactReceived(itPossibleMap->second, NULL, itPossibleMap->first, false);
		theApp.emuledlg->kademliawnd->UpdateSearchGraph(m_pLookupHistory);
	}
	if (!m_mapPossible.empty()) {
		//Lets keep our contact list entries in mind to dec the inUse flag.
		for (ContactMap::const_iterator itPossibleMap = m_mapPossible.begin(); itPossibleMap != m_mapPossible.end(); ++itPossibleMap)
			m_mapInUse[itPossibleMap->first] = itPossibleMap->second;

		ASSERT(m_mapPossible.size() == m_mapInUse.size());

		// Take top ALPHA_QUERY to start search with.
		int iCount = (m_uType == NODE) ? 1 : min(ALPHA_QUERY, (int)m_mapPossible.size());

		ContactMap::const_iterator itPossibleMap = m_mapPossible.begin();
		// Send initial packets to start the search.
		while (iCount-- > 0) {
			CContact *pContact = itPossibleMap->second;
			// Move to tried
			m_mapTried[itPossibleMap->first] = pContact;
			// Send the KadID so other side can check if I think it has the right KadID. (Safety net)
			// Send request
			SendFindValue(pContact);
			++itPossibleMap;
		}
	}
	// Update search for the GUI
	theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
}

//If we allow about a 15 sec delay before deleting, we won't miss a lot of delayed returning packets.
void CSearch::PrepareToStop()
{
	// Check if already stopping.
	if (m_bStoping)
		return;

	// Set base time by search type.
	uint32 uBaseTime;
	switch (m_uType) {
	case NODE:
	case NODECOMPLETE:
	case NODESPECIAL:
	case NODEFWCHECKUDP:
		uBaseTime = SEARCHNODE_LIFETIME;
		break;
	case FILE:
		uBaseTime = SEARCHFILE_LIFETIME;
		break;
	case KEYWORD:
		uBaseTime = SEARCHKEYWORD_LIFETIME;
		break;
	case NOTES:
		uBaseTime = SEARCHNOTES_LIFETIME;
		break;
	case STOREFILE:
		uBaseTime = SEARCHSTOREFILE_LIFETIME;
		break;
	case STOREKEYWORD:
		uBaseTime = SEARCHSTOREKEYWORD_LIFETIME;
		break;
	case STORENOTES:
		uBaseTime = SEARCHSTORENOTES_LIFETIME;
		break;
	case FINDBUDDY:
		uBaseTime = SEARCHFINDBUDDY_LIFETIME;
		break;
	case FINDSOURCE:
		uBaseTime = SEARCHFINDSOURCE_LIFETIME;
		break;
	default:
		uBaseTime = SEARCH_LIFETIME;
	}

	// Adjust created time so that search will be deleted within 15 seconds.
	// This gives late results time to be processed.
	m_tCreated = time(NULL) - uBaseTime + SEC(15);
	m_bStoping = true;

	//Update search within GUI.
	theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
	if (m_pLookupHistory != NULL) {
		m_pLookupHistory->SetSearchStopped();
		theApp.emuledlg->kademliawnd->UpdateSearchGraph(m_pLookupHistory);
	}
}

void CSearch::JumpStart()
{
	// If we had a response within the last 3 seconds, no need to jump-start the search.
	if (time(NULL) < m_uLastResponse + SEC(3))
		return;

	// If we ran out of contacts, stop search.
	if (m_mapPossible.empty()) {
		PrepareToStop();
		return;
	}

	// Is this a find lookup and are the best two (=KADEMLIA_FIND_VALUE) nodes dead/unreachable?
	// In this case try to discover more close nodes before using our other results
	// The reason for this is that we may not have found the closest node alive due to results being limited
	// to 2 contacts, which could very well have been the duplicates of our dead closest nodes [link paper]
	if (pRequestedMoreNodesContact == NULL && GetRequestContactCount() == KADEMLIA_FIND_VALUE && m_mapTried.size() >= 3 * KADEMLIA_FIND_VALUE) {
		ContactMap::const_iterator itTriedMap = m_mapTried.begin();
		bool bLookupCloserNodes = true;
		for (int i = KADEMLIA_FIND_VALUE; --i >= 0;) {
			if (m_mapResponded.find(itTriedMap->first) != m_mapResponded.end()) {
				bLookupCloserNodes = false;
				break;
			}
			++itTriedMap;
		}
		if (bLookupCloserNodes)
			for (; itTriedMap != m_mapTried.end(); ++itTriedMap)
				if (m_mapResponded.find(itTriedMap->first) != m_mapResponded.end()) {
					DEBUG_ONLY(DebugLogWarning(_T("Best KADEMLIA_FIND_VALUE nodes for LookUp (%s) were unreachable or dead, re-asking closest for more"), (LPCTSTR)(CString)GetGUIName()));
					SendFindValue(itTriedMap->second, true);
					return;
				}
	}

	// Search for contacts that can be used to jump-start a stalled search.
	while (!m_mapPossible.empty()) {
		const ContactMap::const_iterator itPossibleMap = m_mapPossible.begin();
		// Get a contact closest to our target.
		CContact *pContact = itPossibleMap->second;

		// Have we already tried to contact this node?
		if (m_mapTried.find(itPossibleMap->first) == m_mapTried.end()) {
			// Add to tried list.
			m_mapTried[itPossibleMap->first] = pContact;
			// Send the KadID so other side can check if I think it has the right KadID. (Safety net)
			// Send request
			SendFindValue(pContact);
			return;
		}
		// Did we get a response from this node? if so, try to store or get info.
		if (m_mapResponded.find(itPossibleMap->first) != m_mapResponded.end())
			StorePacket();

		// Remove from possible list.
		m_mapPossible.erase(itPossibleMap);
	}
}

void CSearch::ProcessResponse(uint32 uFromIP, uint16 uFromPort, ContactList *plistResults)
{
	// Remember the contacts to be deleted when finished
	for (ContactList::const_iterator itContactList = plistResults->begin(); itContactList != plistResults->end(); ++itContactList)
		m_listDelete.push_back(*itContactList);

	m_uLastResponse = time(NULL);

	//Find contact that is responding.
	CUInt128 uFromDistance;
	CContact *pFromContact = NULL;
	for (ContactMap::const_iterator itTriedMap = m_mapTried.begin(); itTriedMap != m_mapTried.end(); ++itTriedMap) {
		CContact *pTmpContact = itTriedMap->second;
		if ((pTmpContact->GetIPAddress() == uFromIP) && (pTmpContact->GetUDPPort() == uFromPort)) {
			uFromDistance = itTriedMap->first;
			pFromContact = pTmpContact;
			break;
		}
	}

	// Make sure the node is not sending more results than we requested, which is not only a protocol violation,
	// but most likely a malicious answer
	if (plistResults->size() > GetRequestContactCount() && !(pRequestedMoreNodesContact == pFromContact && plistResults->size() <= KADEMLIA_FIND_VALUE_MORE)) {
		DebugLogWarning(_T("Node %s sent more contacts than requested on a routing query, ignoring response"), (LPCTSTR)ipstr(htonl(uFromIP)));
		delete plistResults;
		return;
	}

	if (m_uType == NODEFWCHECKUDP) {
		++m_uAnswers;
		// Results are not passed to the search and not much point in changing this, but make sure we show on the graph that the contact responded
		m_pLookupHistory->ContactReceived(NULL, pFromContact, CUInt128(), true);
		theApp.emuledlg->kademliawnd->UpdateSearchGraph(m_pLookupHistory);
		delete plistResults;
		// Update search on the GUI.
		theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
		return;
	}
	// Not interested in responses for FIND_NODE.
	// Once we get a results we stop the search.
	// These contacts are added to contacts by UDPListener.
	if (m_uType == NODE) {
		// Note we got an answer
		++m_uAnswers;
		// Add contacts to the History for GUI purposes
		for (ContactList::const_iterator itResultsList = plistResults->begin(); itResultsList != plistResults->end(); ++itResultsList) {
			CUInt128 uDistance((*itResultsList)->GetClientID().Xor(m_uTarget));
			m_pLookupHistory->ContactReceived(*itResultsList, pFromContact, uDistance, uDistance < uFromDistance, true);
		}
		theApp.emuledlg->kademliawnd->UpdateSearchGraph(m_pLookupHistory);
		// We clear the possible list to force the search to stop.
		// We do this so the user has time to visually see the results.
		m_mapPossible.clear();
		delete plistResults;
		// Update search on the GUI.
		theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
		return;
	}

	try {
		if (pFromContact != NULL) {
			bool bProvidedCloserContacts = false;
			std::map<uint32, uint32> mapReceivedIPs;
			std::map<uint32, uint32> mapReceivedSubnets;
			mapReceivedIPs[uFromIP] = 1; // A node is not allowed to answer with contacts to itself
			mapReceivedSubnets[uFromIP & ~0xFF] = 1;
			// Loop through their responses
			for (ContactList::const_iterator itResultsList = plistResults->begin(); itResultsList != plistResults->end(); ++itResultsList) {
				// Get next result
				CContact *pContact = *itResultsList;

				// Calc distance from this result to the target.
				CUInt128 uDistance(pContact->GetClientID());
				uDistance.Xor(m_uTarget);

				if (uDistance < uFromDistance)
					bProvidedCloserContacts = true;

				m_pLookupHistory->ContactReceived(pContact, pFromContact, uDistance, bProvidedCloserContacts);
				theApp.emuledlg->kademliawnd->UpdateSearchGraph(m_pLookupHistory);

				// Ignore this contact if already know or tried it.
				if (m_mapPossible.find(uDistance) != m_mapPossible.end() || m_mapTried.find(uDistance) != m_mapTried.end())
					continue;
				// we only accept unique IPs in the answer, having multiple IDs pointing to one IP in the routing tables
				// is no longer allowed since 0.49a anyway
				if (mapReceivedIPs.find(pContact->GetIPAddress()) != mapReceivedIPs.end()) {
					DebugLogWarning(_T("Multiple KadIDs pointing to same IP (%s) in KADEMLIA(2)_RES answer - ignored, sent by %s")
						, (LPCTSTR)ipstr(pContact->GetNetIP()), (LPCTSTR)ipstr(pFromContact->GetNetIP()));
					continue;
				}

				mapReceivedIPs[pContact->GetIPAddress()] = 1;

				// and no more than 2 IPs from the same /24 block
				const uint32 subnetIP = pContact->GetIPAddress() & ~0xFF;
				std::map<uint32, uint32>::iterator it = IsLANIP(pContact->GetNetIP()) ? mapReceivedSubnets.end() : mapReceivedSubnets.find(subnetIP);
				if (it == mapReceivedSubnets.end())
					mapReceivedSubnets[subnetIP] = 1;
				else {
					if (it->second >= 2) {
						DebugLogWarning(_T("More than 2 KadIDs pointing to same Subnet (%s) in KADEMLIA(2)_RES answer - ignored, sent by %s")
							, (LPCTSTR)ipstr(htonl(subnetIP)), (LPCTSTR)ipstr(pFromContact->GetNetIP()));
						continue;
					}
					it->second++;
				}

				// Add to possible
				m_mapPossible[uDistance] = pContact;

				// Verify if the result is closer to the target then the one we just checked.
				if (uDistance < uFromDistance) {
					// The top APLPHA_QUERY of results are used to determine if we send a request.
					bool bTop = (m_mapBest.size() < ALPHA_QUERY);
					if (bTop)
						m_mapBest[uDistance] = pContact;
					else {
						ContactMap::const_iterator itContactMapBest = m_mapBest.end();
						--itContactMapBest;
						if (uDistance < itContactMapBest->first) {
							// Prevent having more then ALPHA_QUERY within the Best list.
							m_mapBest.erase(itContactMapBest);
							m_mapBest[uDistance] = pContact;
							bTop = true;
						}
					}

					if (bTop) {
						// We determined this contact is a candidate for a request.
						// Add to the tried list.
						m_mapTried[uDistance] = pContact;
						// Send the KadID so other side can check if I think it has the right KadID. (Safety net)
						// Send request
						SendFindValue(pContact);
					}
				}
			}

			// Add to the list of contacts that responded
			m_mapResponded[uFromDistance] = bProvidedCloserContacts;

			// Complete node search, just increase the answers and update the GUI
			if (m_uType == NODECOMPLETE || m_uType == NODESPECIAL) {
				++m_uAnswers;
				theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
			}
		}
	} catch (...) {
		AddDebugLogLine(false, _T("Exception in CSearch::ProcessResponse"));
	}
	delete plistResults;
}

void CSearch::StorePacket()
{
	ASSERT(!m_mapPossible.empty());

	// This method is currently only called by jump-start so only use best possible.
	ContactMap::const_iterator itPossibleMap = m_mapPossible.begin();
	CUInt128 uFromDistance(itPossibleMap->first);
	CContact *pFromContact = itPossibleMap->second;

	if (uFromDistance < m_uClosestDistantFound || m_uClosestDistantFound == 0)
		m_uClosestDistantFound = uFromDistance;
	// Make sure this is a valid Node to store too.
	if (uFromDistance.Get32BitChunk(0) > SEARCHTOLERANCE && !IsLANIP(pFromContact->GetNetIP()))
		return;
	m_pLookupHistory->ContactAskedKeyword(pFromContact);
	theApp.emuledlg->kademliawnd->UpdateSearchGraph(m_pLookupHistory);
	// What kind of search are we doing?
	switch (m_uType) {
	case FILE:
		{
			CSafeMemFile m_pfileSearchTerms;
			m_pfileSearchTerms.WriteUInt128(&m_uTarget);
			if (pFromContact->GetVersion() >= KADEMLIA_VERSION3_47b) {
				// Find file we are storing info about.
				uchar ucharFileid[MDX_DIGEST_SIZE];
				m_uTarget.ToByteArray(ucharFileid);
				CKnownFile *pFile = theApp.downloadqueue->GetFileByID(ucharFileid);
				if (!pFile) {
					PrepareToStop();
					break;
				}
				// JOHNTODO -- Add start position
				// Start Position range (0x0 to 0x7FFF)
				m_pfileSearchTerms.WriteUInt16(0);
				m_pfileSearchTerms.WriteUInt64(pFile->GetFileSize());
				if (thePrefs.GetDebugClientKadUDPLevel() > 0)
					DebugSend("KADEMLIA2_SEARCH_SOURCE_REQ", pFromContact->GetIPAddress(), pFromContact->GetUDPPort());
				if (pFromContact->GetVersion() >= KADEMLIA_VERSION6_49aBETA) {
					CUInt128 uClientID = pFromContact->GetClientID();
					CKademlia::GetUDPListener()->SendPacket(&m_pfileSearchTerms, KADEMLIA2_SEARCH_SOURCE_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), pFromContact->GetUDPKey(), &uClientID);
				} else {
					CKademlia::GetUDPListener()->SendPacket(&m_pfileSearchTerms, KADEMLIA2_SEARCH_SOURCE_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), CKadUDPKey(), NULL);
					ASSERT(CKadUDPKey() == pFromContact->GetUDPKey());
				}
			} else {
				m_pfileSearchTerms.WriteUInt8(1);
				if (thePrefs.GetDebugClientKadUDPLevel() > 0)
					DebugSend("KADEMLIA_SEARCH_REQ(File)", pFromContact->GetIPAddress(), pFromContact->GetUDPPort());
				CKademlia::GetUDPListener()->SendPacket(&m_pfileSearchTerms, KADEMLIA_SEARCH_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), CKadUDPKey(), NULL);
			}
			// Inc total request answers
			++m_uTotalRequestAnswers;
			// Update search in the GUI
			theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
		}
		break;
	case KEYWORD:
		{
			//JOHNTODO -- We cannot pre-create these packets as we do not know
			// beforehand if we are talking to Kad1.0 or Kad2.0
			CSafeMemFile m_pfileSearchTerms;
			m_pfileSearchTerms.WriteUInt128(&m_uTarget);
			if (pFromContact->GetVersion() >= KADEMLIA_VERSION3_47b) {
				if (m_uSearchTermsDataSize == 0) {
					// JOHNTODO - Need to add ability to change start position.
					// Start position range (0x0 to 0x7fff)
					m_pfileSearchTerms.WriteUInt16(0x0000ui16);
				} else {
					// JOHNTODO - Need to add ability to change start position.
					// Start position range (0x8000 to 0xffff)
					m_pfileSearchTerms.WriteUInt16(0x8000ui16);
					m_pfileSearchTerms.Write(m_pucSearchTermsData, m_uSearchTermsDataSize);
				}
			} else {
				if (m_uSearchTermsDataSize == 0) {
					m_pfileSearchTerms.WriteUInt8(0);
					// We send this extra byte to flag we handle large files.
					m_pfileSearchTerms.WriteUInt8(0);
				} else {
					// Set to 2 to flag we handle large files.
					m_pfileSearchTerms.WriteUInt8(2);
					m_pfileSearchTerms.Write(m_pucSearchTermsData, m_uSearchTermsDataSize);
				}
			}

			if (pFromContact->GetVersion() >= KADEMLIA_VERSION6_49aBETA) {
				if (thePrefs.GetDebugClientKadUDPLevel() > 0)
					DebugSend("KADEMLIA2_SEARCH_KEY_REQ", pFromContact->GetIPAddress(), pFromContact->GetUDPPort());
				CUInt128 uClientID = pFromContact->GetClientID();
				CKademlia::GetUDPListener()->SendPacket(&m_pfileSearchTerms, KADEMLIA2_SEARCH_KEY_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), pFromContact->GetUDPKey(), &uClientID);
			} else if (pFromContact->GetVersion() >= KADEMLIA_VERSION3_47b) {
				if (thePrefs.GetDebugClientKadUDPLevel() > 0)
					DebugSend("KADEMLIA2_SEARCH_KEY_REQ", pFromContact->GetIPAddress(), pFromContact->GetUDPPort());
				CKademlia::GetUDPListener()->SendPacket(&m_pfileSearchTerms, KADEMLIA2_SEARCH_KEY_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), CKadUDPKey(), NULL);
				ASSERT(CKadUDPKey() == pFromContact->GetUDPKey());
			} else {
				if (thePrefs.GetDebugClientKadUDPLevel() > 0)
					DebugSend("KADEMLIA_SEARCH_REQ(KEYWORD)", pFromContact->GetIPAddress(), pFromContact->GetUDPPort());
				CKademlia::GetUDPListener()->SendPacket(&m_pfileSearchTerms, KADEMLIA_SEARCH_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), CKadUDPKey(), NULL);
			}
			// Inc total request answers
			++m_uTotalRequestAnswers;
			// Update search in the GUI
			theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
		}
		break;
	case NOTES:
		{
			// Write complete packet
			CSafeMemFile m_pfileSearchTerms;
			m_pfileSearchTerms.WriteUInt128(&m_uTarget);

			if (pFromContact->GetVersion() >= KADEMLIA_VERSION3_47b) {
				// Find file we are storing info about.
				uchar ucharFileid[MDX_DIGEST_SIZE];
				m_uTarget.ToByteArray(ucharFileid);
				CKnownFile *pFile = theApp.sharedfiles->GetFileByID(ucharFileid);
				if (!pFile) {
					PrepareToStop();
					break;
				}
				m_pfileSearchTerms.WriteUInt64(pFile->GetFileSize());
				if (thePrefs.GetDebugClientKadUDPLevel() > 0)
					DebugSend("KADEMLIA2_SEARCH_NOTES_REQ", pFromContact->GetIPAddress(), pFromContact->GetUDPPort());
				if (pFromContact->GetVersion() >= KADEMLIA_VERSION6_49aBETA) {
					CUInt128 uClientID = pFromContact->GetClientID();
					CKademlia::GetUDPListener()->SendPacket(&m_pfileSearchTerms, KADEMLIA2_SEARCH_NOTES_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), pFromContact->GetUDPKey(), &uClientID);
				} else {
					CKademlia::GetUDPListener()->SendPacket(&m_pfileSearchTerms, KADEMLIA2_SEARCH_NOTES_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), CKadUDPKey(), NULL);
					ASSERT(CKadUDPKey() == pFromContact->GetUDPKey());
				}
			} else {
				m_pfileSearchTerms.WriteUInt128(&CKademlia::GetPrefs()->GetKadID());
				if (thePrefs.GetDebugClientKadUDPLevel() > 0)
					DebugSend("KADEMLIA_SEARCH_NOTES_REQ", pFromContact->GetIPAddress(), pFromContact->GetUDPPort());
				CKademlia::GetUDPListener()->SendPacket(&m_pfileSearchTerms, KADEMLIA_SEARCH_NOTES_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), CKadUDPKey(), NULL);
			}
			// Inc total request answers
			++m_uTotalRequestAnswers;
			// Update search in the GUI
			theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
		}
		break;
	case STOREFILE:
		{
			// Try to store yourself as a source to a Node.
			// As a safeguard, check to see if we already stored to the Max Nodes
			if (m_uAnswers > SEARCHSTOREFILE_TOTAL) {
				PrepareToStop();
				break;
			}

			// Find the file we are trying to store as a source too.
			uchar ucharFileid[MDX_DIGEST_SIZE];
			m_uTarget.ToByteArray(ucharFileid);
			CKnownFile *pFile = theApp.sharedfiles->GetFileByID(ucharFileid);
			if (!pFile) {
				PrepareToStop();
				break;
			}

			// We set this mostly for GUI response.
			SetGUIName((CStringW)pFile->GetFileName());

			// Get our clientID for the packet.
			CUInt128 uID(CKademlia::GetPrefs()->GetClientHash());

			//We can use type for different types of sources.
			//1 HighID Sources.
			//2 cannot be used as older clients will not work.
			//3 Firewalled Kad Source.
			//4 >4GB file HighID Source.
			//5 >4GB file Firewalled Kad source.
			//6 Firewalled Source with Direct Callback (supports >4GB)

			TagList listTag;
			if (theApp.IsFirewalled()) {
				bool bDirectCallback = (Kademlia::CKademlia::IsRunning() && !Kademlia::CUDPFirewallTester::IsFirewalledUDP(true) && Kademlia::CUDPFirewallTester::IsVerified());
				if (!bDirectCallback && !theApp.clientlist->GetBuddy()) {
					// We are firewalled, no direct callback and no buddy. Stop everything.
					PrepareToStop();
					break;
				}

				if (bDirectCallback) {
					// firewalled, but direct UDP callback is possible without buddies
					listTag.push_back(new CKadTagUInt(TAG_SOURCETYPE, 6));
					listTag.push_back(new CKadTagUInt(TAG_SOURCEPORT, thePrefs.GetPort()));
					if (!CKademlia::GetPrefs()->GetUseExternKadPort())
						listTag.push_back(new CKadTagUInt16(TAG_SOURCEUPORT, CKademlia::GetPrefs()->GetInternKadPort()));
					if (pFromContact->GetVersion() >= KADEMLIA_VERSION2_47a)
						listTag.push_back(new CKadTagUInt(TAG_FILESIZE, pFile->GetFileSize()));
				} else { // We are firewalled, but do have a buddy.
					// We send the ID to our buddy so they can do a callback.
					CUInt128 uBuddyID(true);
					uBuddyID.Xor(CKademlia::GetPrefs()->GetKadID());
					listTag.push_back(new CKadTagUInt8(TAG_SOURCETYPE, (pFile->GetFileSize() > OLD_MAX_EMULE_FILE_SIZE ? 5 : 3)));
					listTag.push_back(new CKadTagUInt(TAG_SERVERIP, theApp.clientlist->GetBuddy()->GetIP()));
					listTag.push_back(new CKadTagUInt(TAG_SERVERPORT, theApp.clientlist->GetBuddy()->GetUDPPort()));
					listTag.push_back(new CKadTagStr(TAG_BUDDYHASH, (CStringW)md4str(uBuddyID.GetData())));
					listTag.push_back(new CKadTagUInt(TAG_SOURCEPORT, thePrefs.GetPort()));
					if (!CKademlia::GetPrefs()->GetUseExternKadPort())
						listTag.push_back(new CKadTagUInt16(TAG_SOURCEUPORT, CKademlia::GetPrefs()->GetInternKadPort()));

					if (pFromContact->GetVersion() >= KADEMLIA_VERSION2_47a)
						listTag.push_back(new CKadTagUInt(TAG_FILESIZE, pFile->GetFileSize()));
				}
			} else {
				// We are not firewalled.
				if (pFile->GetFileSize() > OLD_MAX_EMULE_FILE_SIZE)
					listTag.push_back(new CKadTagUInt(TAG_SOURCETYPE, 4));
				else
					listTag.push_back(new CKadTagUInt(TAG_SOURCETYPE, 1));
				listTag.push_back(new CKadTagUInt(TAG_SOURCEPORT, thePrefs.GetPort()));
				if (!CKademlia::GetPrefs()->GetUseExternKadPort())
					listTag.push_back(new CKadTagUInt16(TAG_SOURCEUPORT, CKademlia::GetPrefs()->GetInternKadPort()));

				if (pFromContact->GetVersion() >= KADEMLIA_VERSION2_47a)
					listTag.push_back(new CKadTagUInt(TAG_FILESIZE, pFile->GetFileSize()));
			}

			listTag.push_back(new CKadTagUInt8(TAG_ENCRYPTION, CKademlia::GetPrefs()->GetMyConnectOptions(true, true)));

			// Send packet
			CKademlia::GetUDPListener()->SendPublishSourcePacket(pFromContact, m_uTarget, uID, listTag);
			// Inc total request answers
			++m_uTotalRequestAnswers;
			// Update search in the GUI
			theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
			// Delete all tags.
			for (TagList::const_iterator itTagList = listTag.begin(); itTagList != listTag.end(); ++itTagList)
				delete *itTagList;
		}
		break;
	case STOREKEYWORD:
		{
			// Try to store keywords to a Node.
			int iCount = static_cast<int>(m_listFileIDs.size());
			// As a safeguard, check to see if we already stored to the Max Nodes
			if (iCount <= 0 || m_uAnswers > SEARCHSTOREKEYWORD_TOTAL) {
				PrepareToStop();
				break;
			}
			if (iCount > 150)
				iCount = 150;

			for (UIntList::const_iterator itListFileID = m_listFileIDs.begin(); iCount > 0 && itListFileID != m_listFileIDs.end();) {
				uchar ucharFileid[MDX_DIGEST_SIZE];
				uint16 iPacketCount = 0;
				byte byPacket[1024 * 50];
				CByteIO byIO(byPacket, sizeof(byPacket));
				byIO.WriteUInt128(m_uTarget);
				byIO.WriteUInt16(0); // Will be corrected before sending.
				while ((iPacketCount < 50) && (itListFileID != m_listFileIDs.end())) {
					CUInt128 iID = *itListFileID++;
					iID.ToByteArray(ucharFileid);
					CKnownFile *pFile = theApp.sharedfiles->GetFileByID(ucharFileid);
					if (pFile) {
						--iCount;
						++iPacketCount;
						byIO.WriteUInt128(iID);
						PreparePacketForTags(&byIO, pFile, pFromContact->GetVersion());
					}
				}

				// Correct file count.
				uint32 current_pos = byIO.GetUsed();
				byIO.Seek(16);
				byIO.WriteUInt16(iPacketCount);
				byIO.Seek(current_pos);

				// Send packet
				if (pFromContact->GetVersion() >= KADEMLIA_VERSION6_49aBETA) {
					if (thePrefs.GetDebugClientKadUDPLevel() > 0)
						DebugSend("KADEMLIA2_PUBLISH_KEY_REQ", pFromContact->GetIPAddress(), pFromContact->GetUDPPort());
					CUInt128 uClientID = pFromContact->GetClientID();
					CKademlia::GetUDPListener()->SendPacket(byPacket, sizeof(byPacket) - byIO.GetAvailable(), KADEMLIA2_PUBLISH_KEY_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), pFromContact->GetUDPKey(), &uClientID);
				} else if (pFromContact->GetVersion() >= KADEMLIA_VERSION2_47a) {
					if (thePrefs.GetDebugClientKadUDPLevel() > 0)
						DebugSend("KADEMLIA2_PUBLISH_KEY_REQ", pFromContact->GetIPAddress(), pFromContact->GetUDPPort());
					CKademlia::GetUDPListener()->SendPacket(byPacket, sizeof(byPacket) - byIO.GetAvailable(), KADEMLIA2_PUBLISH_KEY_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), CKadUDPKey(), NULL);
					ASSERT(CKadUDPKey() == pFromContact->GetUDPKey());
				} else
					ASSERT(0);
			}
			// Inc total request answers
			++m_uTotalRequestAnswers;
			// Update search in the GUI
			theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
		}
		break;
	case STORENOTES:
		{
			// Find file we are storing info about.
			uchar ucharFileid[MDX_DIGEST_SIZE];
			m_uTarget.ToByteArray(ucharFileid);
			CKnownFile *pFile = theApp.sharedfiles->GetFileByID(ucharFileid);
			if (!pFile) {
				PrepareToStop();
				break;
			}
			byte byPacket[1024 * 2];
			CByteIO byIO(byPacket, sizeof byPacket);

			// Send the Hash of the file we are storing info about.
			byIO.WriteUInt128(m_uTarget);
			// Send our ID with the info.
			byIO.WriteUInt128(CKademlia::GetPrefs()->GetKadID());

			// Create our taglist
			TagList listTag;
			listTag.push_back(new CKadTagStr(TAG_FILENAME, pFile->GetFileName()));
			if (pFile->GetFileRating() != 0)
				listTag.push_back(new CKadTagUInt(TAG_FILERATING, pFile->GetFileRating()));
			if (!pFile->GetFileComment().IsEmpty())
				listTag.push_back(new CKadTagStr(TAG_DESCRIPTION, pFile->GetFileComment()));
			if (pFromContact->GetVersion() >= KADEMLIA_VERSION2_47a)
				listTag.push_back(new CKadTagUInt(TAG_FILESIZE, pFile->GetFileSize()));
			byIO.WriteTagList(listTag);

			// Send packet
			if (pFromContact->GetVersion() >= KADEMLIA_VERSION6_49aBETA) {
				if (thePrefs.GetDebugClientKadUDPLevel() > 0)
					DebugSend("KADEMLIA2_PUBLISH_NOTES_REQ", pFromContact->GetIPAddress(), pFromContact->GetUDPPort());
				CUInt128 uClientID = pFromContact->GetClientID();
				CKademlia::GetUDPListener()->SendPacket(byPacket, (sizeof byPacket) - byIO.GetAvailable(), KADEMLIA2_PUBLISH_NOTES_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), pFromContact->GetUDPKey(), &uClientID);
			} else if (pFromContact->GetVersion() >= KADEMLIA_VERSION2_47a) {
				if (thePrefs.GetDebugClientKadUDPLevel() > 0)
					DebugSend("KADEMLIA2_PUBLISH_NOTES_REQ", pFromContact->GetIPAddress(), pFromContact->GetUDPPort());
				CKademlia::GetUDPListener()->SendPacket(byPacket, (sizeof byPacket) - byIO.GetAvailable(), KADEMLIA2_PUBLISH_NOTES_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), CKadUDPKey(), NULL);
				ASSERT(CKadUDPKey() == pFromContact->GetUDPKey());
			} else
				ASSERT(0);
			// Inc total request answers
			++m_uTotalRequestAnswers;
			// Update search in the GUI
			theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
			// Delete all tags.
			for (TagList::const_iterator itTagList = listTag.begin(); itTagList != listTag.end(); ++itTagList)
				delete *itTagList;
		}
		break;
	case FINDBUDDY:
		{
			// Send a buddy request as we are firewalled.
			// As a safe guard, check to see if we already requested the Max Nodes
			if (m_uAnswers > SEARCHFINDBUDDY_TOTAL) {
				PrepareToStop();
				break;
			}

			CSafeMemFile m_pfileSearchTerms;
			// Send the ID we used to find our buddy. Used for checks later and allows users to callback someone if they change buddies.
			m_pfileSearchTerms.WriteUInt128(&m_uTarget);
			// Send client hash so they can do a callback.
			m_pfileSearchTerms.WriteUInt128(&CKademlia::GetPrefs()->GetClientHash());
			// Send client port so they can do a callback
			m_pfileSearchTerms.WriteUInt16(thePrefs.GetPort());

			// Do a keyword/source search request to this Node.
			// Send packet
			if (thePrefs.GetDebugClientKadUDPLevel() > 0)
				DebugSend("KADEMLIA_FINDBUDDY_REQ", pFromContact->GetIPAddress(), pFromContact->GetUDPPort());
			if (pFromContact->GetVersion() >= KADEMLIA_VERSION6_49aBETA) {
				CUInt128 uClientID = pFromContact->GetClientID();
				CKademlia::GetUDPListener()->SendPacket(&m_pfileSearchTerms, KADEMLIA_FINDBUDDY_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), pFromContact->GetUDPKey(), &uClientID);
			} else {
				CKademlia::GetUDPListener()->SendPacket(&m_pfileSearchTerms, KADEMLIA_FINDBUDDY_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), CKadUDPKey(), NULL);
				ASSERT(CKadUDPKey() == pFromContact->GetUDPKey());
			}
			// Inc total request answers
			++m_uAnswers;
			// Update search in the GUI
			theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
		}
		break;
	case FINDSOURCE:
		{
			// Try to find if this is a buddy to someone we want to contact.
			// As a safe guard, check to see if we already requested the Max Nodes
			if (m_uAnswers > SEARCHFINDSOURCE_TOTAL) {
				PrepareToStop();
				break;
			}

			CSafeMemFile fileIO(34);
			// This is the ID the person we want to contact used to find a buddy.
			fileIO.WriteUInt128(&m_uTarget);
			if (m_listFileIDs.size() != 1)
				throw CString(_T("Kademlia.CSearch.ProcessResponse: m_listFileIDs.size() != 1"));
			// Currently, we limit they type of callbacks for sources. We must know a file it person has for it to work.
			fileIO.WriteUInt128(&m_listFileIDs.front());
			// Send our port so the callback works.
			fileIO.WriteUInt16(thePrefs.GetPort());
			// Send packet
			if (thePrefs.GetDebugClientKadUDPLevel() > 0)
				DebugSend("KADEMLIA_CALLBACK_REQ", pFromContact->GetIPAddress(), pFromContact->GetUDPPort());
			if (pFromContact->GetVersion() >= KADEMLIA_VERSION6_49aBETA) {
				CUInt128 uClientID = pFromContact->GetClientID();
				CKademlia::GetUDPListener()->SendPacket(&fileIO, KADEMLIA_CALLBACK_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), pFromContact->GetUDPKey(), &uClientID);
			} else {
				CKademlia::GetUDPListener()->SendPacket(&fileIO, KADEMLIA_CALLBACK_REQ, pFromContact->GetIPAddress(), pFromContact->GetUDPPort(), CKadUDPKey(), NULL);
				ASSERT(CKadUDPKey() == pFromContact->GetUDPKey());
			}
			// Inc total request answers
			++m_uAnswers;
			// Update search in the GUI
			theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
		}
		break;
	case NODESPECIAL:
		// we are looking for the IP of a given nodeid, so we just check if we 0 distance and if so, report the
		// tip to the requester
		if (uFromDistance == CUInt128(0ul)) {
			pNodeSpecialSearchRequester->KadSearchIPByNodeIDResult(KCSR_SUCCEEDED, pFromContact->GetNetIP(), pFromContact->GetTCPPort());
			pNodeSpecialSearchRequester = NULL;
			PrepareToStop();
		}
	}
}

void CSearch::ProcessResult(const CUInt128 &uAnswer, TagList *plistInfo, uint32 uFromIP, uint16 uFromPort)
{
	// We received a result, process it based on type.
	uint32 iAnswerBefore = m_uAnswers;
	switch (m_uType) {
	case FILE:
		ProcessResultFile(uAnswer, plistInfo);
		break;
	case KEYWORD:
		ProcessResultKeyword(uAnswer, plistInfo, uFromIP, uFromPort);
		break;
	case NOTES:
		ProcessResultNotes(uAnswer, plistInfo);
	}
	if (iAnswerBefore < m_uAnswers) {
		m_pLookupHistory->ContactRespondedKeyword(uFromIP, uFromPort, m_uAnswers - iAnswerBefore);
		theApp.emuledlg->kademliawnd->UpdateSearchGraph(m_pLookupHistory);
	}
	// Update search for the GUI
	theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
}

void CSearch::ProcessResultFile(const CUInt128 &uAnswer, TagList *plistInfo)
{
	// Process a possible source to a file.
	// Set of data we could receive from the result.
	uint8 uType = 0;
	uint32 uIP = 0;
	uint16 uTCPPort = 0;
	uint16 uUDPPort = 0;
	uint32 uBuddyIP = 0;
	uint16 uBuddyPort = 0;
	//uint32 uClientID = 0;
	CUInt128 uBuddy;
	uint8 byCryptOptions = 0; // 0 = not supported

	for (TagList::const_iterator itInfoList = plistInfo->begin(); itInfoList != plistInfo->end(); ++itInfoList) {
		CKadTag *pTag = *itInfoList;
		if (!pTag->m_name.Compare(TAG_SOURCETYPE))
			uType = (uint8)pTag->GetInt();
		else if (!pTag->m_name.Compare(TAG_SOURCEIP))
			uIP = (uint32)pTag->GetInt();
		else if (!pTag->m_name.Compare(TAG_SOURCEPORT))
			uTCPPort = (uint16)pTag->GetInt();
		else if (!pTag->m_name.Compare(TAG_SOURCEUPORT))
			uUDPPort = (uint16)pTag->GetInt();
		else if (!pTag->m_name.Compare(TAG_SERVERIP))
			uBuddyIP = (uint32)pTag->GetInt();
		else if (!pTag->m_name.Compare(TAG_SERVERPORT))
			uBuddyPort = (uint16)pTag->GetInt();
		//else if (!pTag->m_name.Compare(TAG_CLIENTLOWID))
		//  uClientID = pTag->GetInt();
		else if (!pTag->m_name.Compare(TAG_BUDDYHASH)) {
			uchar ucharBuddyHash[MDX_DIGEST_SIZE];
			if (pTag->IsStr() && strmd4(pTag->GetStr(), ucharBuddyHash))
				md4cpy(uBuddy.GetDataPtr(), ucharBuddyHash);
			else
				TRACE("+++ Invalid TAG_BUDDYHASH tag\n");
		} else if (!pTag->m_name.Compare(TAG_ENCRYPTION))
			byCryptOptions = (uint8)pTag->GetInt();

		delete pTag;
	}
	delete plistInfo;

	// Process source based on its type. Currently only one method is needed to process all types.
	switch (uType) {
	case 1:
	case 3:
	case 4:
	case 5:
	case 6:
		++m_uAnswers;
		theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
		theApp.downloadqueue->KademliaSearchFile(m_uSearchID, &uAnswer, &uBuddy, uType, uIP, uTCPPort, uUDPPort, uBuddyIP, uBuddyPort, byCryptOptions);
		break;
	}
}

void CSearch::ProcessResultNotes(const CUInt128 &uAnswer, TagList *plistInfo)
{
	// Process a received Note to a file.
	// Create a Note and set the ID's.
	CEntry *pEntry = new CEntry();
	pEntry->m_uKeyID.SetValue(m_uTarget);
	pEntry->m_uSourceID.SetValue(uAnswer);
	// Create flag to determine if we keep this note.
	bool bFilterComment = false;

	// Loops through tags and pull wanted into. Currently we only keep Filename, Rating, Comment.
	for (TagList::const_iterator itInfoList = plistInfo->begin(); itInfoList != plistInfo->end(); ++itInfoList) {
		CKadTag *pTag = *itInfoList;
		if (!pTag->m_name.Compare(TAG_SOURCEIP)) {
			pEntry->m_uIP = (uint32)pTag->GetInt();
			delete pTag;
		} else if (!pTag->m_name.Compare(TAG_SOURCEPORT)) {
			pEntry->m_uTCPPort = (uint16)pTag->GetInt();
			delete pTag;
		} else if (!pTag->m_name.Compare(TAG_FILENAME) || !pTag->m_name.Compare(TAG_DESCRIPTION)) {
			const CString &cfilter(thePrefs.GetCommentFilter());
			// Run the filter against the comment as well as against the filename since both values could be misused
			if (!cfilter.IsEmpty()) {
				CString strCommentLower(pTag->GetStr());
				// Verified Locale Dependency: Locale dependent string conversion (OK)
				strCommentLower.MakeLower();

				for (int iPos = 0; iPos >= 0;) {
					const CString &strFilter(cfilter.Tokenize(_T("|"), iPos));
					// comment filters are already in lower case, compare with temp. lower cased received comment
					if (!strFilter.IsEmpty() && strCommentLower.Find(strFilter) >= 0) {
						bFilterComment = true;
						break;
					}
				}
			}
			if (!pTag->m_name.Compare(TAG_FILENAME)) {
				pEntry->SetFileName(pTag->GetStr());
				delete pTag;
			} else {
				ASSERT(!pTag->m_name.Compare(TAG_DESCRIPTION));
				if (pTag->GetStr().GetLength() > MAXFILECOMMENTLEN) {
					CKadTagStr *pReplace = new CKadTagStr(pTag->m_name, pTag->GetStr().Left(MAXFILECOMMENTLEN));
					delete pTag;
					pTag = pReplace;
				}
				pEntry->AddTag(pTag);
			}
		} else if (!pTag->m_name.Compare(TAG_FILERATING))
			pEntry->AddTag(pTag);
		else
			delete pTag;
	}
	delete plistInfo;

	// If we think this should be filtered, delete the note.
	if (bFilterComment) {
		delete pEntry;
		return;
	}

	uchar ucharFileid[MDX_DIGEST_SIZE];
	m_uTarget.ToByteArray(ucharFileid);

	// Add notes to any searches we have done.
	// The returned entry object will never be attached
	// to anything. So you can delete the entry object
	// at any time after this call.
	bool bFlag = theApp.searchlist->AddNotes(pEntry, ucharFileid);

	// Check if this hash is in our shared files.
	CAbstractFile *pFile = static_cast<CAbstractFile*>(theApp.sharedfiles->GetFileByID(ucharFileid));

	// If we didn't find a file in the shares check if it's in our download queue.
	if (!pFile)
		pFile = static_cast<CAbstractFile*>(theApp.downloadqueue->GetFileByID(ucharFileid));

	// If we found a file try to add the Note to the file.
	if (pFile && pFile->AddNote(pEntry)) {
		// Inc the number of answers.
		++m_uAnswers;
		// Update the search in the GUI
		theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
		// We do note delete the NOTE in this case.
		return;
	}

	// It is possible that pFile->AddNote can fail even if we found a File.
	if (bFlag) {
		// Inc the number of answers.
		++m_uAnswers;
		// Update the search in the GUI
		theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
	}

	// We always delete the entry object if pFile->AddNote fails.
	delete pEntry;
}

void CSearch::ProcessResultKeyword(const CUInt128 &uAnswer, TagList *plistInfo, uint32 uFromIP, uint16 uFromPort)
{
	// Find the contact who sent the answer - we need to know its protocol version
	// Special publish answer tags need to be filtered based on its remote protocol version,
	// because if an old node is not aware of those special tags, it doesn't know
	// it is not supposed to accept and store such tags on publish request, so a malicious
	// publisher could fake them and our remote node would relay them in answers
	CContact *pFromContact = NULL;
	for (ContactMap::const_iterator itTriedMap = m_mapTried.begin(); itTriedMap != m_mapTried.end(); ++itTriedMap) {
		CContact *pTmpContact = itTriedMap->second;
		if (pTmpContact->GetIPAddress() == uFromIP && pTmpContact->GetUDPPort() == uFromPort) {
			pFromContact = pTmpContact;
			break;
		}
	}
	uint8 uFromKadVersion;
	if (pFromContact == NULL) {
		uFromKadVersion = 0;
		DebugLogWarning(_T("Unable to find answering contact in ProcessResultKeyword - %s"), (LPCTSTR)ipstr(htonl(uFromIP)));
	} else
		uFromKadVersion = pFromContact->GetVersion();
	// Process the received keyword
	// Set of data we can use for the keyword result
	uint64 uSize = 0;
	CKadTagValueString sName;
	CKadTagValueString sType;
	CKadTagValueString sFormat;
	CKadTagValueString sArtist;
	CKadTagValueString sAlbum;
	CKadTagValueString sTitle;
	CKadTagValueString sCodec;
	uint32 uLength = 0;
	uint32 uBitrate = 0;
	uint32 uAvailability = 0;
	uint32 uPublishInfo = 0;
	CArray<CAICHHash> aAICHHashes;
	CArray<uint8> aAICHHashPopularity;
	// Flags that are set if we want this keyword.
	bool bFileName = false;
	bool bFileSize = false;

	for (TagList::const_iterator itInfoList = plistInfo->begin(); itInfoList != plistInfo->end(); ++itInfoList) {
		CKadTag *pTag = *itInfoList;

		if (!pTag->m_name.Compare(TAG_FILENAME)) {
			// Set flag based on last tag we saw.
			sName = pTag->GetStr();
			bFileName = !sName.IsEmpty();
		} else if (!pTag->m_name.Compare(TAG_FILESIZE)) {
			if (pTag->IsBsob() && pTag->GetBsobSize() == 8)
				uSize = *((uint64*)pTag->GetBsob());
			else
				uSize = pTag->GetInt();

			// Set flag based on last tag we saw.
			bFileSize = (uSize > 0);
		} else if (!pTag->m_name.Compare(TAG_FILETYPE))
			sType = pTag->GetStr();
		else if (!pTag->m_name.Compare(TAG_FILEFORMAT))
			sFormat = pTag->GetStr();
		else if (!pTag->m_name.Compare(TAG_MEDIA_ARTIST))
			sArtist = pTag->GetStr();
		else if (!pTag->m_name.Compare(TAG_MEDIA_ALBUM))
			sAlbum = pTag->GetStr();
		else if (!pTag->m_name.Compare(TAG_MEDIA_TITLE))
			sTitle = pTag->GetStr();
		else if (!pTag->m_name.Compare(TAG_MEDIA_LENGTH))
			uLength = (uint32)pTag->GetInt();
		else if (!pTag->m_name.Compare(TAG_MEDIA_BITRATE))
			uBitrate = (uint32)pTag->GetInt();
		else if (!pTag->m_name.Compare(TAG_MEDIA_CODEC))
			sCodec = pTag->GetStr();
		else if (!pTag->m_name.Compare(TAG_SOURCES)) {
			// Some rouge client was setting an invalid availability, just set it to 0
			uAvailability = (uint32)pTag->GetInt();
			if (uAvailability > 65500)
				uAvailability = 0;
		} else if (!pTag->m_name.Compare(TAG_PUBLISHINFO)) {
			if (uFromKadVersion >= KADEMLIA_VERSION6_49aBETA) {
				// we don't keep this as a tag, but as a member property of the search file,
				// because we only need its information in the search list and don't want to carry
				// the tag over when downloading the file (and maybe even wrongly publishing it)
				uPublishInfo = (uint32)pTag->GetInt();
/*
#ifdef _DEBUG
				uint32 byDifferentNames = (uPublishInfo >> 24) & 0xFF;
				uint32 byPublishersKnown = (uPublishInfo >> 16) & 0xFF;
				uint32 wTrustValue = uPublishInfo & 0xFFFF;
				DebugLog(_T("Received PublishInfoTag: %u different names, %u Publishers, %.2f Trustvalue"), byDifferentNames, byPublishersKnown, (float)wTrustValue / 100.0f);
#endif
*/
			} else
				DebugLogWarning(_T("ProcessResultKeyword: Received special publish tag (TAG_PUBLISHINFO) from node (version %u, ip: %s) which is not aware of it, filtering")
					, uFromKadVersion, (LPCTSTR)ipstr(htonl(uFromIP)));
		} else if (!pTag->m_name.Compare(TAG_KADAICHHASHRESULT)) {
			if (uFromKadVersion >= KADEMLIA_VERSION9_50a && pTag->IsBsob()) {
				CSafeMemFile fileAICHTag(pTag->GetBsob(), pTag->GetBsobSize());
				try {
					for (uint8 byCount = fileAICHTag.ReadUInt8(); byCount > 0; --byCount) {
						uint8 byPopularity = fileAICHTag.ReadUInt8();
						if (byPopularity > 0) {
							aAICHHashPopularity.Add(byPopularity);
							aAICHHashes.Add(CAICHHash(&fileAICHTag));
						}
					}
				} catch (CFileException *pError) {
					DebugLogError(_T("ProcessResultKeyword: Corrupt or invalid TAG_KADAICHHASHRESULT received - ip: %s)"), (LPCTSTR)ipstr(htonl(uFromIP)));
					pError->Delete();
					aAICHHashPopularity.RemoveAll();
					aAICHHashes.RemoveAll();
				}
			} else
				DebugLogWarning(_T("ProcessResultKeyword: Received special publish tag (TAG_KADAICHHASHRESULT) from node (version %u, ip: %s) which is not aware of it, filtering")
					, uFromKadVersion, (LPCTSTR)ipstr(htonl(uFromIP)));
		}
		delete pTag;
	}
	delete plistInfo;

	// If we don't have a valid filename or file size, drop this keyword.
	if (!bFileName || !bFileSize)
		return;

	// Check that this result matches the original criteria
	WordList listTestWords;
	CSearchManager::GetWords(sName, &listTestWords);
	CKadTagValueString keyword;
	for (WordList::const_iterator itWordListWords = m_listWords.begin(); itWordListWords != m_listWords.end(); ++itWordListWords) {
		keyword = *itWordListWords;
		bool bInterested = false;
		for (WordList::const_iterator itWordListTestWords = listTestWords.begin(); itWordListTestWords != listTestWords.end(); ++itWordListTestWords)
			if (!KadTagStrCompareNoCase(keyword, *itWordListTestWords)) {
				bInterested = true;
				break;
			}

		if (!bInterested)
			return;
	}

	if (m_pSearchTerm == NULL && m_pucSearchTermsData != NULL && m_uSearchTermsDataSize != 0) {
		// we create this to pass on to the search list, which will check it against the result to filter bad ones
		CSafeMemFile tmpFile(m_pucSearchTermsData, m_uSearchTermsDataSize);
		m_pSearchTerm = CKademliaUDPListener::CreateSearchExpressionTree(tmpFile, 0);
		ASSERT(m_pSearchTerm != NULL);
	}

	// Inc the number of answers.
	++m_uAnswers;
	// Update the search in the GUI
	theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
	// Send the keyword to search list for processing.
	// This method is still legacy from the multithreaded Kad, maybe this can be changed for better handling.
	theApp.searchlist->KademliaSearchKeyword(m_uSearchID, &uAnswer, sName, uSize, sType, uPublishInfo
		, aAICHHashes, aAICHHashPopularity, m_pSearchTerm
		, 8
		, TAGTYPE_STRING, TAG_FILEFORMAT, (LPCTSTR)sFormat
		, TAGTYPE_STRING, TAG_MEDIA_ARTIST, (LPCTSTR)sArtist
		, TAGTYPE_STRING, TAG_MEDIA_ALBUM, (LPCTSTR)sAlbum
		, TAGTYPE_STRING, TAG_MEDIA_TITLE, (LPCTSTR)sTitle
		, TAGTYPE_UINT32, TAG_MEDIA_LENGTH, uLength
		, TAGTYPE_UINT32, TAG_MEDIA_BITRATE, uBitrate
		, TAGTYPE_STRING, TAG_MEDIA_CODEC, (LPCTSTR)sCodec
		, TAGTYPE_UINT32, TAG_SOURCES, uAvailability);
}

void CSearch::SendFindValue(CContact *pContact, bool bReAskMore)
{
	// Found a Node that we think has contacts closer to our target.
	try {
		// Make sure we are not in the process of stopping.
		if (m_bStoping)
			return;
		CSafeMemFile fileIO(33);
		// The number of returned contacts is based on the type of search.
		uint8 byContactCount = GetRequestContactCount();
		if (bReAskMore)
			if (pRequestedMoreNodesContact == NULL) {
				pRequestedMoreNodesContact = pContact;
				ASSERT(byContactCount == KADEMLIA_FIND_VALUE);
				byContactCount = KADEMLIA_FIND_VALUE_MORE;
			} else
				ASSERT(0);

			if (byContactCount <= 0)
				return;

			fileIO.WriteUInt8(byContactCount);
			// Put the target we want into the packet.
			fileIO.WriteUInt128(&m_uTarget);
			// Add the ID of the contact we are contacting for sanity checks on the other end.
			fileIO.WriteUInt128(&pContact->GetClientID());
			// Inc the number of packets sent.
			++m_uKadPacketSent;
			// Update the search for the GUI.
			theApp.emuledlg->kademliawnd->searchList->SearchRef(this);

			if (pContact->GetVersion() >= KADEMLIA_VERSION2_47a) {
				m_pLookupHistory->ContactAskedKad(pContact);
				theApp.emuledlg->kademliawnd->UpdateSearchGraph(m_pLookupHistory);
				if (pContact->GetVersion() >= KADEMLIA_VERSION6_49aBETA) {
					CUInt128 uClientID = pContact->GetClientID();
					CKademlia::GetUDPListener()->SendPacket(&fileIO, KADEMLIA2_REQ, pContact->GetIPAddress(), pContact->GetUDPPort(), pContact->GetUDPKey(), &uClientID);
				} else {
					CKademlia::GetUDPListener()->SendPacket(&fileIO, KADEMLIA2_REQ, pContact->GetIPAddress(), pContact->GetUDPPort(), CKadUDPKey(), NULL);
					ASSERT(CKadUDPKey() == pContact->GetUDPKey());
				}
				if (thePrefs.GetDebugClientKadUDPLevel() > 0) {
					LPCSTR pszOp;
					switch (m_uType) {
					case NODE:
						pszOp = "KADEMLIA2_REQ(NODE)";
						break;
					case NODECOMPLETE:
						pszOp = "KADEMLIA2_REQ(NODECOMPLETE)";
						break;
					case NODESPECIAL:
						pszOp = "KADEMLIA2_REQ(NODESPECIAL)";
						break;
					case NODEFWCHECKUDP:
						pszOp = "KADEMLIA2_REQ(NODEFWCHECKUDP)";
						break;
					case FILE:
						pszOp = "KADEMLIA2_REQ(FILE)";
						break;
					case KEYWORD:
						pszOp = "KADEMLIA2_REQ(KEYWORD)";
						break;
					case STOREFILE:
						pszOp = "KADEMLIA2_REQ(STOREFILE)";
						break;
					case STOREKEYWORD:
						pszOp = "KADEMLIA2_REQ(STOREKEYWORD)";
						break;
					case STORENOTES:
						pszOp = "KADEMLIA2_REQ(STORENOTES)";
						break;
					case NOTES:
						pszOp = "KADEMLIA2_REQ(NOTES)";
						break;
					default:
						pszOp = "KADEMLIA2_REQ()";
					}
					DebugSend(pszOp, pContact->GetIPAddress(), pContact->GetUDPPort());
				}
			} else
				ASSERT(0);
	} catch (CIOException *ioe) {
		AddDebugLogLine(false, _T("Exception in CSearch::SendFindValue (IO error(%i))"), ioe->m_iCause);
		ioe->Delete();
	} catch (...) {
		AddDebugLogLine(false, _T("Exception in CSearch::SendFindValue"));
	}
}

void CSearch::AddFileID(const CUInt128 &uID)
{
	// Add a file hash to the search list.
	// This is used mainly for storing keywords, but was also reused for storing notes.
	m_listFileIDs.push_back(uID);
}

static INT_PTR GetMetaDataWords(CStringArray &rastrWords, const CString &rstrData)
{
	// Create a list of the 'words' found in 'data'. This is similar but though not equal
	// to the 'CSearchManager::GetWords' function which needs to follow some other rules.
	for (int iPos = 0; iPos >= 0;) {
		const CString &strWord(rstrData.Tokenize(g_aszInvKadKeywordChars, iPos));
		if (!strWord.IsEmpty())
			rastrWords.Add(strWord);
	}
	return rastrWords.GetCount();
}

static bool IsRedundantMetaData(const CStringArray &rastrFileNameWords, const CString &rstrMetaData)
{
	// Verify if the meta data string 'rstrMetaData' is already contained within the filename.
	if (rstrMetaData.IsEmpty())
		return true;

	int iMetaDataWords = 0;
	int iFoundInFileName = 0;
	for (int iPos = 0; iPos >= 0;) {
		const CString &strMetaDataWord(rstrMetaData.Tokenize(g_aszInvKadKeywordChars, iPos));
		if (strMetaDataWord.IsEmpty())
			break;
		++iMetaDataWords;
		for (INT_PTR i = rastrFileNameWords.GetCount(); --i >= 0;) {
			// Verified Locale Dependency: Locale dependent string comparison (OK)
			if (rastrFileNameWords[i].CompareNoCase(strMetaDataWord) == 0) {
				++iFoundInFileName;
				break;
			}
		}
		if (iFoundInFileName < iMetaDataWords)
			return false;
	}
	return (iMetaDataWords == 0 || iMetaDataWords == iFoundInFileName);
}

void CSearch::PreparePacketForTags(CByteIO *byIO, CKnownFile *pFile, uint8 byTargetKadVersion)
{
	// We are going to publish a keyword, setup the tag list.
	TagList listTag;
	try {
		if (pFile && byIO) {
			// Name, Size
			listTag.push_back(new CKadTagStr(TAG_FILENAME, pFile->GetFileName()));
			//if (pFile->GetFileSize() > OLD_MAX_EMULE_FILE_SIZE)
			//{
			//	// TODO: As soon as we drop Kad1 support, we should switch to Int64 tags (we could do now already for kad2 nodes only but no advantage in that)
			//	uint64 uSize = (uint64)pFile->GetFileSize();
			//	listTag.push_back(new CKadTagBsob(TAG_FILESIZE, (BYTE*)&uSize, sizeof(uint64)));
			//}
			//else
			listTag.push_back(new CKadTagUInt(TAG_FILESIZE, (uint64)pFile->GetFileSize()));

			listTag.push_back(new CKadTagUInt(TAG_SOURCES, pFile->m_nCompleteSourcesCount));

			if (byTargetKadVersion >= KADEMLIA_VERSION9_50a && pFile->GetFileIdentifier().HasAICHHash())
				listTag.push_back(new CKadTagBsob(TAG_KADAICHHASHPUB, pFile->GetFileIdentifier().GetAICHHash().GetRawHashC()
					, (uint8)CAICHHash::GetHashSize()));

			// eD2K file type (Audio, Video, ...)
			// NOTE: Archives and CD-Images are published with file type "Pro"
			const CString &strED2KFileType(GetED2KFileTypeSearchTerm(GetED2KFileTypeID(pFile->GetFileName())));
			if (!strED2KFileType.IsEmpty())
				listTag.push_back(new CKadTagStr(TAG_FILETYPE, strED2KFileType));

			// file format (filename extension)
			// 21-Sep-2006 []: TAG_FILEFORMAT is no longer explicitly published nor stored as
			// it is already part of the filename.
			//int iExt = pFile->GetFileName().ReverseFind(_T('.'));
			//if (iExt >= 0) {
			//	CString strExt(pFile->GetFileName().Mid(iExt));
			//	if (!strExt.IsEmpty()) {
			//		strExt = strExt.Mid(1);
			//		if (!strExt.IsEmpty())
			//			listTag.push_back(new CKadTagStr(TAG_FILEFORMAT, strExt));
			//	}
			//}

			// additional meta data (Artist, Album, Codec, Length, ...)
			// only send verified meta data to nodes
			if (pFile->GetMetaDataVer() > 0) {
				static const struct
				{
					uint8 uName;
					uint8 uType;
				}
				_aMetaTags[] =
				{
					{ FT_MEDIA_ARTIST,  TAGTYPE_STRING },
					{ FT_MEDIA_ALBUM,   TAGTYPE_STRING },
					{ FT_MEDIA_TITLE,   TAGTYPE_STRING },
					{ FT_MEDIA_LENGTH,  TAGTYPE_UINT32 },
					{ FT_MEDIA_BITRATE, TAGTYPE_UINT32 },
					{ FT_MEDIA_CODEC,   TAGTYPE_STRING }
				};
				CStringArray astrFileNameWords;
				for (unsigned iIndex = 0; iIndex < _countof(_aMetaTags); ++iIndex) {
					const CTag *pTag = pFile->GetTag(_aMetaTags[iIndex].uName, _aMetaTags[iIndex].uType);
					if (pTag) {
						// skip string tags with empty string values
						if (pTag->IsStr() && pTag->GetStr().IsEmpty())
							continue;
						// skip integer tags with '0' values
						if (pTag->IsInt() && pTag->GetInt() == 0)
							continue;
						char szKadTagName[2];
						szKadTagName[0] = (char)pTag->GetNameID();
						szKadTagName[1] = '\0';
						if (pTag->IsStr()) {
							bool bIsRedundant = false;
							switch (pTag->GetNameID()) {
							case FT_MEDIA_ARTIST:
							case FT_MEDIA_ALBUM:
							case FT_MEDIA_TITLE:
								if (astrFileNameWords.IsEmpty())
									GetMetaDataWords(astrFileNameWords, pFile->GetFileName());
								bIsRedundant = IsRedundantMetaData(astrFileNameWords, pTag->GetStr());
								//if (bIsRedundant)
								//	TRACE(_T("Skipping meta data tag \"%s\" for file \"%s\"\n"), pTag->GetStr(), pFile->GetFileName());
							}
							if (!bIsRedundant)
								listTag.push_back(new CKadTagStr(szKadTagName, pTag->GetStr()));
						} else
							listTag.push_back(new CKadTagUInt(szKadTagName, pTag->GetInt()));
					}
				}
			}
			byIO->WriteTagList(listTag);
		} else {
			//If we get here. Bad things happen. Will fix this later if it is a real issue.
			ASSERT(0);
		}
	} catch (CIOException *ioe) {
		AddDebugLogLine(false, _T("Exception in CSearch::PreparePacketForTags (IO error(%i))"), ioe->m_iCause);
		ioe->Delete();
	} catch (...) {
		AddDebugLogLine(false, _T("Exception in CSearch::PreparePacketForTags"));
	}
	for (TagList::const_iterator itTagList = listTag.begin(); itTagList != listTag.end(); ++itTagList)
		delete *itTagList;
}

uint32 CSearch::GetNodeLoad() const
{
	// Node load is the average of all node load responses.
	return m_uTotalLoadResponses ? m_uTotalLoad / m_uTotalLoadResponses : 0;
}

uint32 CSearch::GetSearchID() const
{
	return m_uSearchID;
}
uint32 CSearch::GetSearchType() const
{
	return m_uType;
}
void CSearch::SetSearchType(uint32 uVal)
{
	m_uType = uVal;
	m_pLookupHistory->SetSearchType(uVal);
}

void CSearch::SetTargetID(const CUInt128 &uVal)
{
	m_uTarget = uVal;
}

uint32 CSearch::GetAnswers() const
{
	if (m_listFileIDs.empty())
		return m_uAnswers;
	// If we sent more then one packet per node, we have to average the answers for the real count.
	return (uint32)(m_uAnswers / ((m_listFileIDs.size() + 49) / 50));
}

uint32 CSearch::GetKadPacketSent() const
{
	return m_uKadPacketSent;
}

uint32 CSearch::GetRequestAnswer() const
{
	return m_uTotalRequestAnswers;
}

const CStringW& CSearch::GetGUIName() const
{
	return m_pLookupHistory->GetGUIName();
}

void CSearch::SetGUIName(LPCWSTR sGUIName)
{
	m_pLookupHistory->SetGUIName(sGUIName);
}

CUInt128 CSearch::GetTarget() const
{
	return m_uTarget;
}

bool CSearch::Stoping() const
{
	return m_bStoping;
}

uint32 CSearch::GetNodeLoadResponse() const
{
	return m_uTotalLoadResponses;
}

uint32 CSearch::GetNodeLoadTotal() const
{
	return m_uTotalLoad;
}

void CSearch::UpdateNodeLoad(uint8 uLoad)
{
	// Since all nodes do not return a load value, keep track of total responses and total load.
	m_uTotalLoad += uLoad;
	++m_uTotalLoadResponses;
}

void CSearch::SetSearchTermData(uint32 uSearchTermDataSize, LPBYTE pucSearchTermsData)
{
	m_uSearchTermsDataSize = uSearchTermDataSize;
	m_pucSearchTermsData = new BYTE[uSearchTermDataSize];
	memcpy(m_pucSearchTermsData, pucSearchTermsData, uSearchTermDataSize);
}

CString CSearch::GetTypeName(uint32 uType)
{
	UINT uid;
	switch (uType) {
	case CSearch::FILE:
		uid = IDS_KAD_SEARCHSRC;
		break;
	case CSearch::KEYWORD:
		uid = IDS_KAD_SEARCHKW;
		break;
	case CSearch::NODE:
	case CSearch::NODECOMPLETE:
	case CSearch::NODESPECIAL:
	case CSearch::NODEFWCHECKUDP:
		uid = IDS_KAD_NODE;
		break;
	case CSearch::STOREFILE:
		uid = IDS_KAD_STOREFILE;
		break;
	case CSearch::STOREKEYWORD:
		uid = IDS_KAD_STOREKW;
		break;
	case CSearch::FINDBUDDY:
		uid = IDS_FINDBUDDY;
		break;
	case CSearch::STORENOTES:
		uid = IDS_STORENOTES;
		break;
	case CSearch::NOTES:
		uid = IDS_NOTES;
		break;
	default:
		uid = IDS_KAD_UNKNOWN;
	}
	return GetResString(uid);
}

uint8 CSearch::GetRequestContactCount() const
{
	// Returns the amount of contacts we request on routing queries based on the search type
	switch (m_uType) {
	case NODE:
	case NODECOMPLETE:
	case NODESPECIAL:
	case NODEFWCHECKUDP:
		return KADEMLIA_FIND_NODE;
	case FILE:
	case KEYWORD:
	case FINDSOURCE:
	case NOTES:
		return KADEMLIA_FIND_VALUE;
	case FINDBUDDY:
	case STOREFILE:
	case STOREKEYWORD:
	case STORENOTES:
		return KADEMLIA_STORE;
	default:
		DebugLogError(LOG_DEFAULT, _T("Invalid search type. (CSearch::GetRequestContactCount())"));
		ASSERT(0);
	}
	return 0;
}