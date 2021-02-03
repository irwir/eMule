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
#include "resource.h"
#include "SafeFile.h"
#include "Log.h"
#include "emule.h"
#include "emuledlg.h"
#include "kademliawnd.h"
#include "KadSearchListCtrl.h"
#include "SearchDlg.h"
#include "kademlia/kademlia/SearchManager.h"
#include "kademlia/kademlia/Search.h"
#include "kademlia/kademlia/Tag.h"
#include "kademlia/kademlia/Defines.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Indexed.h"
#include "kademlia/kademlia/prefs.h"
#include "kademlia/io/IOException.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

LPCSTR g_aszInvKadKeywordCharsA = INV_KAD_KEYWORD_CHARS;
LPCTSTR g_aszInvKadKeywordChars = _T(INV_KAD_KEYWORD_CHARS);
LPCWSTR g_awszInvKadKeywordChars = L" ()[]{}<>,._-!?:;\\/\"";

using namespace Kademlia;

uint32 CSearchManager::m_uNextID = 0;
SearchMap CSearchManager::m_mapSearches;

bool CSearchManager::IsSearching(uint32 uSearchID)
{
	// Check if this searchID is within the searches.
	for (SearchMap::const_iterator itSearchMap = m_mapSearches.begin(); itSearchMap != m_mapSearches.end(); ++itSearchMap)
		if (itSearchMap->second->m_uSearchID == uSearchID)
			return true;

	return false;
}

void CSearchManager::StopSearch(uint32 uSearchID, bool bDelayDelete)
{
	// Stop a specific searchID
	for (SearchMap::const_iterator itSearchMap = m_mapSearches.begin(); itSearchMap != m_mapSearches.end(); ++itSearchMap)
		if (itSearchMap->second->m_uSearchID == uSearchID) {
			// Do not delete as we want to get a chance for late packets to be processed.
			if (bDelayDelete)
				itSearchMap->second->PrepareToStop();
			else {
				// Delete this search now.
				// If this method is changed to continue looping, take care of the iterator as we will already
				// be pointing to the next entry and the for-loop could cause you to iterate past the end.
				delete itSearchMap->second;
				m_mapSearches.erase(itSearchMap);
			}
			return;
		}
}

void CSearchManager::StopAllSearches()
{
	// Stop and delete all searches.
	for (SearchMap::const_iterator itSearchMap = m_mapSearches.begin(); itSearchMap != m_mapSearches.end(); ++itSearchMap)
		delete itSearchMap->second;
	m_mapSearches.clear();
}

bool CSearchManager::StartSearch(CSearch *pSearch)
{
	// A search object was created, now try to start the search.
	if (AlreadySearchingFor(pSearch->m_uTarget)) {
		// There was already a search in progress with this target.
		delete pSearch;
		return false;
	}
	// Add to the search map
	m_mapSearches[pSearch->m_uTarget] = pSearch;
	// Start the search.
	pSearch->Go();
	return true;
}

CSearch* CSearchManager::PrepareFindKeywords(LPCWSTR szKeyword, UINT uSearchTermsSize, LPBYTE pucSearchTermsData)
{
	// Create a keyword search object.
	CSearch *pSearch = new CSearch;
	try {
		// Set search to a keyword type.
		pSearch->SetSearchType(CSearch::KEYWORD);

		// Make sure we have a keyword list.
		GetWords(szKeyword, &pSearch->m_listWords);
		if (pSearch->m_listWords.empty()) {
			delete pSearch;
			throw GetResString(IDS_KAD_SEARCH_KEYWORD_TOO_SHORT);
		}

		// Get the targetID based on the primary keyword.
		CKadTagValueString wstrKeyword = pSearch->m_listWords.front();
		KadGetKeywordHash(wstrKeyword, &pSearch->m_uTarget);

		// Verify that we are not already searching for this target.
		if (AlreadySearchingFor(pSearch->m_uTarget)) {
			delete pSearch;
			CString strError;
			strError.Format(GetResString(IDS_KAD_SEARCH_KEYWORD_ALREADY_SEARCHING), (LPCTSTR)wstrKeyword);
			throw strError;
		}

		pSearch->SetSearchTermData(uSearchTermsSize, pucSearchTermsData);
		pSearch->SetGUIName(szKeyword);
		// Inc our searchID
		pSearch->m_uSearchID = ++m_uNextID;
		// Insert search into map.
		m_mapSearches[pSearch->m_uTarget] = pSearch;
		// Start search.
		pSearch->Go();
	} catch (CIOException *ioe) {
		CString strError;
		strError.Format(_T("IO-Exception in %hs: Error %i"), __FUNCTION__, ioe->m_iCause);
		ioe->Delete();
		delete pSearch;
		throw strError;
	} catch (CFileException *e) {
		TCHAR szError[MAX_CFEXP_ERRORMSG];
		e->m_strFileName = _T("search packet");
		GetExceptionMessage(*e, szError, _countof(szError));
		CString strError;
		strError.Format(_T("Exception in %hs: %s"), __FUNCTION__, szError);
		e->Delete();
		delete pSearch;
		throw strError;
	} catch (const CString&) {
		throw;
	} catch (...) {
		CString strError;
		strError.Format(_T("Unknown exception in %hs"), __FUNCTION__);
		delete pSearch;
		throw strError;
	}
	return pSearch;
}

CSearch* CSearchManager::PrepareLookup(uint32 uType, bool bStart, const CUInt128 &uID)
{
	// Prepare a kad lookup.
	// Make sure this target is not already in progress.
	if (AlreadySearchingFor(uID))
		return NULL;

	// Create a new search.
	CSearch *pSearch = new CSearch;

	// Set type and target.
	pSearch->SetSearchType(uType);
	pSearch->m_uTarget = uID;

	try {
		switch (pSearch->m_uType) {
		case CSearch::STOREKEYWORD:
			if (!Kademlia::CKademlia::GetIndexed()->SendStoreRequest(uID)) {
				// Keyword Store was determined to be an overloaded node, abort store.
				delete pSearch;
				return NULL;
			}
		}

		// Inc search ID.
		pSearch->m_uSearchID = ++m_uNextID;
		if (bStart) {
			// Auto start this search.
			m_mapSearches[pSearch->m_uTarget] = pSearch;
			pSearch->Go();
		}
	} catch (CIOException *ioe) {
		AddDebugLogLine(false, _T("Exception in CSearchManager::PrepareLookup (IO error(%i))"), ioe->m_iCause);
		ioe->Delete();
		delete pSearch;
		return NULL;
	} catch (...) {
		AddDebugLogLine(false, _T("Exception in CSearchManager::PrepareLookup"));
		delete pSearch;
		return NULL;
	}
	return pSearch;
}

void CSearchManager::FindNode(const CUInt128 &uID, bool bComplete)
{
	// Do a node lookup.
	CSearch *pSearch = new CSearch;
	pSearch->SetSearchType(bComplete ? CSearch::NODECOMPLETE : CSearch::NODE);
	pSearch->m_uTarget = uID;
	StartSearch(pSearch);
}

bool CSearchManager::AlreadySearchingFor(const CUInt128 &uTarget)
{
	// Check if this target is in the search map.
	return (m_mapSearches.find(uTarget) != m_mapSearches.end());
}

bool CSearchManager::IsFWCheckUDPSearch(const CUInt128 &uTarget)
{
	// Check if this target is in the search map.
	SearchMap::const_iterator itSearchMap = m_mapSearches.find(uTarget);
	if (itSearchMap == m_mapSearches.end())
		return false;
	return itSearchMap->second->GetSearchType() == CSearch::NODEFWCHECKUDP;
}

void CSearchManager::GetWords(LPCWSTR sz, WordList *plistWords)
{
	size_t uChars = 0;
	size_t uBytes = 0;
	for (LPCWSTR szS = sz; *szS;) {
		uChars = wcscspn(szS, g_aszInvKadKeywordChars);
		CKadTagValueString sWord = Kademlia::CKadTagValueString(szS);
		sWord.Truncate((int)uChars);
		// TODO: We'd need a safe way to determine if a sequence which contains only 3 chars is a real word.
		// Currently we do this by evaluating the UTF-8 byte count. This will work well for Western locales,
		// AS LONG AS the min. byte count is 3(!). If the byte count is once changed to 2, this will not
		// work properly any longer because there are a lot of Western characters which need 2 bytes in UTF-8.
		// Maybe we need to evaluate the Unicode character values itself whether the characters are located
		// in code ranges where single characters are known to represent words.
		uBytes = KadGetKeywordBytes(sWord).GetLength();
		if (uBytes >= 3) {
			KadTagStrMakeLower(sWord);
			plistWords->remove(sWord);
			plistWords->push_back(sWord);
		}
		szS += uChars;
		szS += static_cast<int>(uChars < wcslen(szS));
	}

	// if the last word we have added, contains 3 chars (and 3 bytes), in almost all cases it's a file's extension.
	if (plistWords->size() > 1 && (uChars == 3 && uBytes == 3))
		plistWords->pop_back();
}

void CSearchManager::JumpStart()
{
	// Find any searches that has stalled and jump-start them.
	// This will also prune all searches.
	time_t tNow = time(NULL);
	for (SearchMap::const_iterator itSearchMap = m_mapSearches.begin(); itSearchMap != m_mapSearches.end();) {
		CSearch *pSearch = itSearchMap->second;
		// Each type has its own criteria for being deleted or jump-started.
		bool bDel = false;
		bool bStop = false;
		switch (itSearchMap->second->GetSearchType()) {
		case CSearch::FILE:
			if (tNow >= pSearch->m_tCreated + SEARCHFILE_LIFETIME)
				bDel = true;
			else if (pSearch->GetAnswers() >= SEARCHFILE_TOTAL || tNow >= pSearch->m_tCreated + SEARCHFILE_LIFETIME - SEC(20))
				bStop = true;
			break;
		case CSearch::KEYWORD:
			if (tNow >= pSearch->m_tCreated + SEARCHKEYWORD_LIFETIME) {
				bDel = true;
				// Tell GUI that search ended
				if (theApp.emuledlg->searchwnd)
					theApp.emuledlg->searchwnd->CancelKadSearch(pSearch->GetSearchID());
			} else if (pSearch->GetAnswers() >= SEARCHKEYWORD_TOTAL || tNow >= pSearch->m_tCreated + SEARCHKEYWORD_LIFETIME - SEC(20))
				bStop = true;
			break;
		case CSearch::NOTES:
			if (tNow >= pSearch->m_tCreated + SEARCHNOTES_LIFETIME)
				bDel = true;
			else if (pSearch->GetAnswers() >= SEARCHNOTES_TOTAL || tNow >= pSearch->m_tCreated + SEARCHNOTES_LIFETIME - SEC(20))
				bStop = true;
			break;
		case CSearch::FINDBUDDY:
			if (tNow >= pSearch->m_tCreated + SEARCHFINDBUDDY_LIFETIME)
				bDel = true;
			else if (pSearch->GetAnswers() >= SEARCHFINDBUDDY_TOTAL || tNow >= pSearch->m_tCreated + SEARCHFINDBUDDY_LIFETIME - SEC(20))
				bStop = true;
			break;
		case CSearch::FINDSOURCE:
			if (tNow >= pSearch->m_tCreated + SEARCHFINDSOURCE_LIFETIME)
				bDel = true;
			else if (pSearch->GetAnswers() >= SEARCHFINDSOURCE_TOTAL || tNow >= pSearch->m_tCreated + SEARCHFINDSOURCE_LIFETIME - SEC(20))
				bStop = true;
			break;
		case CSearch::NODE:
		case CSearch::NODESPECIAL:
		case CSearch::NODEFWCHECKUDP:
			if (tNow >= pSearch->m_tCreated + SEARCHNODE_LIFETIME)
				bDel = true;
			break;
		case CSearch::NODECOMPLETE:
			if (tNow >= pSearch->m_tCreated + SEARCHNODE_LIFETIME || (tNow >= pSearch->m_tCreated + SEARCHNODECOMP_LIFETIME && pSearch->GetAnswers() >= SEARCHNODECOMP_TOTAL)) {
				bDel = true;
				// Tell Kad that it can start publishing.
				CKademlia::GetPrefs()->SetPublish(true);
			}
			break;

		case CSearch::STOREFILE:
			if (tNow >= pSearch->m_tCreated + SEARCHSTOREFILE_LIFETIME)
				bDel = true;
			else if (pSearch->GetAnswers() >= SEARCHSTOREFILE_TOTAL || tNow >= pSearch->m_tCreated + SEARCHSTOREFILE_LIFETIME - SEC(20))
				bStop = true;
			break;
		case CSearch::STOREKEYWORD:
			if (tNow >= pSearch->m_tCreated + SEARCHSTOREKEYWORD_LIFETIME)
				bDel = true;
			else if (pSearch->GetAnswers() >= SEARCHSTOREKEYWORD_TOTAL || tNow >= pSearch->m_tCreated + SEARCHSTOREKEYWORD_LIFETIME - SEC(20))
				bStop = true;
			break;
		case CSearch::STORENOTES:
			if (tNow >= pSearch->m_tCreated + SEARCHSTORENOTES_LIFETIME)
				bDel = true;
			else if (pSearch->GetAnswers() >= SEARCHSTORENOTES_TOTAL || tNow >= pSearch->m_tCreated + SEARCHSTORENOTES_LIFETIME - SEC(20))
				bStop = true;
			break;
		default:
			if (tNow >= pSearch->m_tCreated + SEARCH_LIFETIME)
				bDel = true;
		}
		if (bDel) {
			delete pSearch;
			itSearchMap = m_mapSearches.erase(itSearchMap);	//Don't do anything after this. We are already at the next entry.
		} else {
			if (bStop)
				pSearch->PrepareToStop();
			else
				pSearch->JumpStart();
			++itSearchMap;
		}
	}
}

void CSearchManager::UpdateStats()
{
	CPrefs *pPrefs = CKademlia::GetPrefs();
	if (!pPrefs)
		return;
	// Update stats on the searches, this info can be used to determine if we need can start new searches.
	uint8 uTotalFile = 0;
	uint8 uTotalStoreSrc = 0;
	uint8 uTotalStoreKey = 0;
	uint8 uTotalSource = 0;
	uint8 uTotalNotes = 0;
	uint8 uTotalStoreNotes = 0;
	for (SearchMap::const_iterator itSearchMap = m_mapSearches.begin(); itSearchMap != m_mapSearches.end(); ++itSearchMap)
		switch (itSearchMap->second->GetSearchType()) {
		case CSearch::FILE:
			++uTotalFile;
			break;
		case CSearch::STOREFILE:
			++uTotalStoreSrc;
			break;
		case CSearch::STOREKEYWORD:
			++uTotalStoreKey;
			break;
		case CSearch::FINDSOURCE:
			++uTotalSource;
			break;
		case CSearch::STORENOTES:
			++uTotalStoreNotes;
			break;
		case CSearch::NOTES:
			++uTotalNotes;
		}

	pPrefs->SetTotalFile(uTotalFile);
	pPrefs->SetTotalStoreSrc(uTotalStoreSrc);
	pPrefs->SetTotalStoreKey(uTotalStoreKey);
	pPrefs->SetTotalSource(uTotalSource);
	pPrefs->SetTotalNotes(uTotalNotes);
	pPrefs->SetTotalStoreNotes(uTotalStoreNotes);
}

void CSearchManager::ProcessPublishResult(const CUInt128 &uTarget, const uint8 uLoad, const bool bLoadResponse)
{
	// We tried to publish some info and got a result.
	SearchMap::const_iterator itSearchMap = m_mapSearches.find(uTarget);
	if (itSearchMap == m_mapSearches.end())
		return;
	CSearch *pSearch = itSearchMap->second;
	// Result could be very late and search deleted, abort.
	if (pSearch == NULL)
		return;

	switch (pSearch->GetSearchType()) {
	case CSearch::STOREKEYWORD:
		if (bLoadResponse)
			pSearch->UpdateNodeLoad(uLoad);
	//case CSearch::STOREFILE:
	//case CSearch::STORENOTES:
	}

	// Inc the number of answers.
	pSearch->m_uAnswers++;
	// Update the search for the GUI
	theApp.emuledlg->kademliawnd->searchList->SearchRef(pSearch);
}

void CSearchManager::ProcessResponse(const CUInt128 &uTarget, uint32 uFromIP, uint16 uFromPort, ContactList *plistResults)
{
	// We got a response to a kad lookup.
	SearchMap::const_iterator itSearchMap = m_mapSearches.find(uTarget);
	if (itSearchMap != m_mapSearches.end() && itSearchMap->second != NULL)
		itSearchMap->second->ProcessResponse(uFromIP, uFromPort, plistResults);
	else {
		// This search was deleted before this response, delete contacts and abort
		for (ContactList::const_iterator itContactList = plistResults->begin(); itContactList != plistResults->end(); ++itContactList)
			delete *itContactList;
		delete plistResults;
	}
}

uint8 CSearchManager::GetExpectedResponseContactCount(const CUInt128 &uTarget)
{
	SearchMap::const_iterator itSearchMap = m_mapSearches.find(uTarget);
	// If this search was deleted before this response, delete contacts and abort, otherwise process them.
	return (itSearchMap == m_mapSearches.end()) ? 0 : itSearchMap->second->GetRequestContactCount();
}

void CSearchManager::ProcessResult(const CUInt128 &uTarget, const CUInt128 &uAnswer, TagList *plistInfo, uint32 uFromIP, uint16 uFromPort)
{
	// We have results for a request for info.
	SearchMap::const_iterator itSearchMap = m_mapSearches.find(uTarget);
	if (itSearchMap != m_mapSearches.end() && itSearchMap->second != NULL)
		itSearchMap->second->ProcessResult(uAnswer, plistInfo, uFromIP, uFromPort);
	else {
		// This search was deleted before these results, delete contacts and abort
		for (TagList::const_iterator itTagList = plistInfo->begin(); itTagList != plistInfo->end(); ++itTagList)
			delete *itTagList;
		delete plistInfo;
	}
}

bool CSearchManager::FindNodeSpecial(const CUInt128 &uID, CKadClientSearcher *pRequester)
{
	// Do a node lookup.
	CString strDbgID;
	uID.ToHexString(&strDbgID);
	DebugLog(_T("Starting NODESPECIAL Kad Search for %s"), (LPCTSTR)strDbgID);
	CSearch *pSearch = new CSearch;
	pSearch->SetSearchType(CSearch::NODESPECIAL);
	pSearch->m_uTarget = uID;
	pSearch->SetNodeSpecialSearchRequester(pRequester);
	return StartSearch(pSearch);
}

bool CSearchManager::FindNodeFWCheckUDP()
{
	CancelNodeFWCheckUDPSearch();
	CUInt128 uID;
	uID.SetValueRandom();
	DebugLog(_T("Starting NODEFWCHECKUDP Kad Search"));
	CSearch *pSearch = new CSearch;
	pSearch->SetSearchType(CSearch::NODEFWCHECKUDP);
	pSearch->m_uTarget = uID;
	return StartSearch(pSearch);
}

void CSearchManager::CancelNodeSpecial(CKadClientSearcher *pRequester)
{
	// Stop a specific nodespecial search
	for (SearchMap::const_iterator itSearchMap = m_mapSearches.begin(); itSearchMap != m_mapSearches.end(); ++itSearchMap) {
		CSearch *pSearch = itSearchMap->second;
		if (pSearch->GetNodeSpecialSearchRequester() == pRequester) {
			pSearch->SetNodeSpecialSearchRequester(NULL);
			pSearch->PrepareToStop();
			return;
		}
	}
}

void CSearchManager::CancelNodeFWCheckUDPSearch()
{
	// Stop node searches; done for udp firewall check
	for (SearchMap::const_iterator itSearchMap = m_mapSearches.begin(); itSearchMap != m_mapSearches.end(); ++itSearchMap)
		if (itSearchMap->second->GetSearchType() == CSearch::NODEFWCHECKUDP)
			itSearchMap->second->PrepareToStop();
}