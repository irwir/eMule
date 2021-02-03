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
#include "Log.h"
#include "OtherFunctions.h"
#include "SafeFile.h"
#include "kademlia/kademlia/Entry.h"
#include "kademlia/kademlia/Indexed.h"
#include "kademlia/io/DataIO.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace Kademlia;

CMap<uint32, uint32, uint32, uint32> CKeyEntry::s_mapGlobalPublishIPs;

CEntry::CEntry()
	: m_uIP()
	, m_uTCPPort()
	, m_uUDPPort()
	, m_uSize()
	, m_tLifetime()
	, m_bSource()
{
}

CEntry::~CEntry()
{
	while (!m_listTag.empty()) {
		delete *m_listTag.begin();
		m_listTag.pop_front();
	}
}

CEntry* CEntry::Copy()
{
	CEntry *pEntry = new CEntry();
	for (POSITION pos = m_listFileNames.GetHeadPosition(); pos != NULL;)
		pEntry->m_listFileNames.AddTail(m_listFileNames.GetNext(pos));

	pEntry->m_uIP = m_uIP;
	pEntry->m_uKeyID.SetValue(m_uKeyID);
	pEntry->m_tLifetime = m_tLifetime;
	pEntry->m_uSize = m_uSize;
	pEntry->m_bSource = m_bSource;
	pEntry->m_uSourceID.SetValue(m_uSourceID);
	pEntry->m_uTCPPort = m_uTCPPort;
	pEntry->m_uUDPPort = m_uUDPPort;
	for (TagList::const_iterator itTagList = m_listTag.begin(); itTagList != m_listTag.end(); ++itTagList)
		pEntry->m_listTag.push_back((*itTagList)->Copy());
	return pEntry;
}

uint64 CEntry::GetIntTagValue(const CKadTagNameString &strTagName, bool bIncludeVirtualTags) const
{
	uint64 uResult;
	GetIntTagValue(strTagName, uResult, bIncludeVirtualTags);
	return uResult;
}

bool CEntry::GetIntTagValue(const CKadTagNameString &strTagName, uint64 &rValue, bool bIncludeVirtualTags) const
{
	for (TagList::const_iterator itTagList = m_listTag.begin(); itTagList != m_listTag.end(); ++itTagList) {
		const CKadTag *pTag = *itTagList;
		if (pTag->IsInt() && !pTag->m_name.Compare(strTagName)) {
			rValue = pTag->GetInt();
			return true;
		}
	}

	if (bIncludeVirtualTags)
		// SizeTag is not stored any more, but queried in some places
		if (!strTagName.Compare(TAG_FILESIZE)) {
			rValue = m_uSize;
			return true;
		}

	rValue = 0;
	return false;
}

CKadTagValueString CEntry::GetStrTagValue(const CKadTagNameString &strTagName) const
{
	for (TagList::const_iterator itTagList = m_listTag.begin(); itTagList != m_listTag.end(); ++itTagList) {
		const CKadTag *pTag = *itTagList;
		if (!pTag->m_name.Compare(strTagName) && pTag->IsStr())
			return pTag->GetStr();
	}
	return CKadTagValueString();
}

void CEntry::SetFileName(const CKadTagValueString &strName)
{
	if (!m_listFileNames.IsEmpty()) {
		ASSERT(0);
		m_listFileNames.RemoveAll();
	}
	m_listFileNames.AddHead(structFileNameEntry{strName, 1});
}

CKadTagValueString CEntry::GetCommonFileName() const
{
	// return the filename on which most publishers seem to agree on due to the counting.
	// this doesn't have to be exact, we just want to make sure to not use a filename which only few
	// bad publishers used and base our search matching and answering on this instead of the most popular name
	// Note: The Index values are not the actual numbers of publishers, but just a relative number to compare to other entries
	const CKadTagValueString *sResult = NULL;
	uint32 nHighestPopularityIndex = 0;
	for (POSITION pos = m_listFileNames.GetHeadPosition(); pos != NULL;) {
		const structFileNameEntry &rCur = m_listFileNames.GetNext(pos);
		if (rCur.m_uPopularityIndex > nHighestPopularityIndex) {
			nHighestPopularityIndex = rCur.m_uPopularityIndex;
			sResult = &rCur.m_fileName;
		}
	}
	CKadTagValueString strResult(sResult != NULL ? *sResult : CKadTagValueString());
	ASSERT(!strResult.IsEmpty() || m_listFileNames.IsEmpty());
	return strResult;
}

CKadTagValueString CEntry::GetCommonFileNameLowerCase() const
{
	CKadTagValueString strResult = GetCommonFileName();
	if (!strResult.IsEmpty())
		KadTagStrMakeLower(strResult);
	return strResult;
}

uint32 CEntry::GetTagCount() const // Adds filename and size to the count if not empty, even if they are not stored as tags
{
	return (uint32)(m_listTag.size() + static_cast<unsigned>(m_uSize > 0) + static_cast<unsigned>(!GetCommonFileName().IsEmpty()));
}

void CEntry::WriteTagListInc(CDataIO *pData, uint32 nIncreaseTagNumber)
{
	// write taglist and add name + size tag
	if (pData == NULL) {
		ASSERT(0);
		return;
	}

	uint32 uCount = GetTagCount() + nIncreaseTagNumber; // will include name and size tag in the count if needed
	ASSERT(uCount <= 0xFF);
	pData->WriteByte((uint8)uCount);

	const CKadTagValueString &strCommonFileName(GetCommonFileName());
	if (!strCommonFileName.IsEmpty()) {
		ASSERT(uCount > m_listTag.size());
		pData->WriteTag(&CKadTagStr(TAG_FILENAME, strCommonFileName));
	}
	if (m_uSize != 0) {
		ASSERT(uCount > m_listTag.size());
		CKadTagUInt tag(TAG_FILESIZE, m_uSize);
		pData->WriteTag(&tag);
	}

	for (TagList::const_iterator itTagList = m_listTag.begin(); itTagList != m_listTag.end(); ++itTagList)
		pData->WriteTag(*itTagList);
}

void CEntry::AddTag(CKadTag *pTag, uint32 uDbgSourceIP)
{
	// Filter tags which are for sending query results only and should never be stored (or even worse sent within the taglist)
	if (!pTag->m_name.Compare(TAG_KADAICHHASHRESULT)) {
		DebugLogWarning(_T("Received result tag TAG_KADAICHHASHRESULT on publishing, filtered, source %s"), (LPCTSTR)ipstr(htonl(uDbgSourceIP)));
		delete pTag;
	} else if (!pTag->m_name.Compare(TAG_PUBLISHINFO)) {
		DebugLogWarning(_T("Received result tag TAG_PUBLISHINFO on publishing, filtered, source %s"), (LPCTSTR)ipstr(htonl(uDbgSourceIP)));
		delete pTag;
	} else
		m_listTag.push_back(pTag);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////// CKeyEntry
CKeyEntry::CKeyEntry()
	: m_fTrustValue()
	, m_pliPublishingIPs()
	, dwLastTrustValueCalc()
{
}

CKeyEntry::~CKeyEntry()
{
	if (m_pliPublishingIPs != NULL) {
		while (!m_pliPublishingIPs->IsEmpty())
			AdjustGlobalPublishTracking(m_pliPublishingIPs->RemoveHead().m_uIP, false, _T("instance delete"));

		delete m_pliPublishingIPs;
		m_pliPublishingIPs = NULL;
	}
}

bool CKeyEntry::StartSearchTermsMatch(const SSearchTerm &rSearchTerm)
{
	m_strSearchTermCacheCommonFileNameLowerCase = GetCommonFileNameLowerCase();
	bool bResult = SearchTermsMatch(rSearchTerm);
	m_strSearchTermCacheCommonFileNameLowerCase.Empty();
	return bResult;
}

bool CKeyEntry::SearchTermsMatch(const SSearchTerm &rSearchTerm) const
{
	switch (rSearchTerm.m_type) {
	// boolean operators
	case SSearchTerm::AND:
		return SearchTermsMatch(*rSearchTerm.m_pLeft) && SearchTermsMatch(*rSearchTerm.m_pRight);
	case SSearchTerm::OR:
		return SearchTermsMatch(*rSearchTerm.m_pLeft) || SearchTermsMatch(*rSearchTerm.m_pRight);
	case SSearchTerm::NOT:
		return SearchTermsMatch(*rSearchTerm.m_pLeft) && !SearchTermsMatch(*rSearchTerm.m_pRight);

	// word which is to be searched in the file name (and in additional meta data as done by some ed2k servers???)
	case SSearchTerm::String:
		{
			INT_PTR iStrSearchTerms = rSearchTerm.m_pastr->GetCount();
			if (iStrSearchTerms <= 0)
				return false;
			// if there are more than one search strings specified (e.g. "aaa bbb ccc") the entire string is handled
			// like "aaa AND bbb AND ccc". search all strings from the string search term in the tokenized list of
			// the file name. all strings of string search term have to be found (AND)
			while (--iStrSearchTerms >= 0)
				// this will not give the same results as when tokenizing the filename string, but it is 20 times faster.
				if (wcsstr(m_strSearchTermCacheCommonFileNameLowerCase, (*rSearchTerm.m_pastr)[iStrSearchTerms]) == NULL)
					return false;
		}
		return true;
	case SSearchTerm::MetaTag:
		if (rSearchTerm.m_pTag->m_type == TAGTYPE_STRING) { // meta tags with string values
			if (rSearchTerm.m_pTag->m_name.Compare(TAG_FILEFORMAT) == 0) {
				// 21-Sep-2006 []: Special handling for TAG_FILEFORMAT which is already part
				// of the filename and thus does not need to get published nor stored explicitly,
				int iExt = m_strSearchTermCacheCommonFileNameLowerCase.ReverseFind(_T('.'));
				if (iExt >= 0) {
					if (KadTagStrCompareNoCase((LPCWSTR)m_strSearchTermCacheCommonFileNameLowerCase + iExt + 1, rSearchTerm.m_pTag->GetStr()) == 0)
						return true;
				}
			} else {
				for (TagList::const_iterator itTagList = m_listTag.begin(); itTagList != m_listTag.end(); ++itTagList) {
					const CKadTag *pTag = *itTagList;
					if (pTag->IsStr() && rSearchTerm.m_pTag->m_name.Compare(pTag->m_name) == 0)
						return KadTagStrCompareNoCase(pTag->GetStr(), rSearchTerm.m_pTag->GetStr()) == 0;
				}
			}
		}
		return false;
	}

	//Comparison operations for numbers
	if (rSearchTerm.m_pTag->IsInt()) { // meta tags with integer values
		uint64 uValue;
		if (GetIntTagValue(rSearchTerm.m_pTag->m_name, uValue, true)) {
			const uint64 tInt = rSearchTerm.m_pTag->GetInt();
			switch (rSearchTerm.m_type) {
			case SSearchTerm::OpGreaterEqual:
				return uValue >= tInt;
			case SSearchTerm::OpLessEqual:
				return uValue <= tInt;
			case SSearchTerm::OpGreater:
				return uValue > tInt;
			case SSearchTerm::OpLess:
				return uValue < tInt;
			case SSearchTerm::OpEqual:
				return uValue == tInt;
			case SSearchTerm::OpNotEqual:
				return uValue != tInt;
			}
		}
	} else if (rSearchTerm.m_pTag->IsFloat()) { // meta tags with float values
		const float tFloat = rSearchTerm.m_pTag->GetFloat();
		for (TagList::const_iterator itTagList = m_listTag.begin(); itTagList != m_listTag.end(); ++itTagList) {
			const Kademlia::CKadTag *pTag = *itTagList;
			if (pTag->IsFloat() && rSearchTerm.m_pTag->m_name.Compare(pTag->m_name) == 0) {
				const float fValue = pTag->GetFloat();
				switch (rSearchTerm.m_type) {
				case SSearchTerm::OpGreaterEqual:
					return fValue >= tFloat;
				case SSearchTerm::OpLessEqual:
					return fValue <= tFloat;
				case SSearchTerm::OpGreater:
					return fValue > tFloat;
				case SSearchTerm::OpLess:
					return fValue < tFloat;
				case SSearchTerm::OpEqual:
					return fValue == tFloat;
				case SSearchTerm::OpNotEqual:
					return fValue != tFloat;
				default:
					return false;
				}
			}
		}
	}
	return false;
}

void CKeyEntry::AdjustGlobalPublishTracking(uint32 uIP, bool bIncrease, const CString& /*strDbgReason*/)
{
	uint32 nCount;
	BOOL bFound = s_mapGlobalPublishIPs.Lookup(uIP & ~0xFF, nCount); // /24 block; take care of endianness if needed
	if (!bFound)
		nCount = 0;
	if (bIncrease)
		++nCount;
	else
		--nCount;
	if (bFound || bIncrease)
		s_mapGlobalPublishIPs.SetAt(uIP & ~0xFF, nCount);
	else
		ASSERT(0);
	//LOGTODO
	//if (!strDbgReason.IsEmpty())
	//	DebugLog(_T("KadEntryTack: %s %s (%s) - (%s), new count %u"), (bIncrease ? _T("Adding") : _T("Removing")), (LPCTSTR)ipstr(htonl(uIP & ~0xFF)), (LPCTSTR)ipstr(htonl(uIP)), (LPCTSTR)strDbgReason, nCount);
}

void CKeyEntry::MergeIPsAndFilenames(CKeyEntry *pFromEntry)
{
	// this is called when replacing a stored entry with a refreshed one.
	// we want to take over the tracked IPs, AICHHash and the different file names from the old entry,
	// the rest is still "overwritten" with the refreshed values. This might be not perfect
	// for the tag list in some cases, but we cant afford to store hundreds of tag lists
	// to figure out the best one like we do for the filenames now
	if (m_pliPublishingIPs != NULL) { // This instance needs to be a new entry, otherwise we don't want/need to merge
		ASSERT(pFromEntry == NULL);
		ASSERT(!m_pliPublishingIPs->IsEmpty());
		ASSERT(!m_listFileNames.IsEmpty());
		return;
	}
	ASSERT(m_aAICHHashes.GetCount() <= 1);
	//fetch the "new" AICH hash if any
	CAICHHash *pNewAICHHash;
	if (m_aAICHHashes.IsEmpty())
		pNewAICHHash = NULL;
	else {
		pNewAICHHash = new CAICHHash(m_aAICHHashes[0]);
		m_aAICHHashes.RemoveAll();
		m_anAICHHashPopularity.RemoveAll();
	}
	bool bRefresh = false;
	if (pFromEntry == NULL || pFromEntry->m_pliPublishingIPs == NULL) {
		ASSERT(pFromEntry == NULL);
		// if called with NULL, this is a complete new entry and we need to initialize our lists
		if (m_pliPublishingIPs == NULL)
			m_pliPublishingIPs = new CList<structPublishingIP>();
		// update the global track map below
	} else {
		delete m_pliPublishingIPs; // should be always NULL, already ASSERTed above if not

		//  copy over the existing ones.
		m_aAICHHashes.Copy(pFromEntry->m_aAICHHashes);
		m_anAICHHashPopularity.Copy(pFromEntry->m_anAICHHashPopularity);

		// merge the tracked IPs, add this one if not already on the list
		m_pliPublishingIPs = pFromEntry->m_pliPublishingIPs;
		pFromEntry->m_pliPublishingIPs = NULL;
		bool bFastRefresh = false;
		for (POSITION pos = m_pliPublishingIPs->GetHeadPosition(); pos != NULL;) {
			POSITION pos2 = pos;
			structPublishingIP Cur = m_pliPublishingIPs->GetNext(pos);
			if (Cur.m_uIP == m_uIP) {
				bRefresh = true;
				if ((time(NULL) < Cur.m_tLastPublish) + (KADEMLIAREPUBLISHTIMES - HR2S(1))) {
					DEBUG_ONLY(DebugLog(_T("KadEntryTracking: FastRefresh publish, ip: %s"), (LPCTSTR)ipstr(htonl(m_uIP))));
					bFastRefresh = true; // refreshed faster than expected, will not count into filename popularity index
				}
				Cur.m_tLastPublish = time(NULL);
				m_pliPublishingIPs->RemoveAt(pos2);
				m_pliPublishingIPs->AddTail(Cur);
				// Has the AICH Hash this publisher reported changed?
				if (pNewAICHHash != NULL) {
					if (Cur.m_wAICHHashIdx == _UI16_MAX) {
						DEBUG_ONLY(DebugLog(_T("KadEntryTracking: New AICH Hash during publishing (publisher reported none before), publisher ip: %s"), (LPCTSTR)ipstr(htonl(m_uIP))));
						Cur.m_wAICHHashIdx = AddRemoveAICHHash(*pNewAICHHash, true);
					} else if (m_aAICHHashes[Cur.m_wAICHHashIdx] != *pNewAICHHash) {
						DebugLogWarning(_T("KadEntryTracking: AICH Hash changed, publisher ip: %s"), (LPCTSTR)ipstr(htonl(m_uIP)));
						AddRemoveAICHHash(m_aAICHHashes[Cur.m_wAICHHashIdx], false);
						Cur.m_wAICHHashIdx = AddRemoveAICHHash(*pNewAICHHash, true);
					}
				} else if (Cur.m_wAICHHashIdx != _UI16_MAX) {
					DebugLogWarning(_T("KadEntryTracking: AICH Hash removed, publisher ip: %s"), (LPCTSTR)ipstr(htonl(m_uIP)));
					AddRemoveAICHHash(m_aAICHHashes[Cur.m_wAICHHashIdx], false);
					Cur.m_wAICHHashIdx = _UI16_MAX;
				}
				break;
			}
		}
		// copy over trust value in case we don't want to recalculate
		m_fTrustValue = pFromEntry->m_fTrustValue;
		dwLastTrustValueCalc = pFromEntry->dwLastTrustValueCalc;

		// copy over the different names, if they differ from the one we have right now
		ASSERT(m_listFileNames.GetCount() == 1); // we should have only one name here, since its the entry from one sinlge source
		const structFileNameEntry &structCurrentName(m_listFileNames.IsEmpty() ? structFileNameEntry() : m_listFileNames.RemoveHead());
		bool bDuplicate = false;
		for (POSITION pos = pFromEntry->m_listFileNames.GetHeadPosition(); pos != NULL;) {
			structFileNameEntry structNameToCopy = pFromEntry->m_listFileNames.GetNext(pos);
			if (KadTagStrCompareNoCase(structCurrentName.m_fileName, structNameToCopy.m_fileName) == 0) {
				// the filename of our new entry matches with our old, increase the popularity index for the old one
				bDuplicate = true;
				if (!bFastRefresh)
					structNameToCopy.m_uPopularityIndex++;
			}
			m_listFileNames.AddTail(structNameToCopy);
		}
		if (!bDuplicate)
			m_listFileNames.AddTail(structCurrentName);
	}
	// it's done if this was a refresh, otherwise update the global track map
	if (!bRefresh) {
		ASSERT(m_uIP != 0);
		uint16 nAICHHashIdx = pNewAICHHash ? AddRemoveAICHHash(*pNewAICHHash, true) : _UI16_MAX;
		structPublishingIP add = {time(NULL), m_uIP, nAICHHashIdx};
		m_pliPublishingIPs->AddTail(add);

		// add the publisher to the tacking list
		AdjustGlobalPublishTracking(m_uIP, true, _T("new publisher"));

		// we keep track of max 100 IPs in order to avoid too much time for calculation/storing/loading.
		if (m_pliPublishingIPs->GetCount() > 100) {
			structPublishingIP curEntry = m_pliPublishingIPs->RemoveHead();
			if (curEntry.m_wAICHHashIdx != _UI16_MAX)
				VERIFY(AddRemoveAICHHash(m_aAICHHashes[curEntry.m_wAICHHashIdx], false) == curEntry.m_wAICHHashIdx);
			AdjustGlobalPublishTracking(curEntry.m_uIP, false, _T("more than 100 publishers purge"));
		}
		// since we added a new publisher, we want to (re)calculate the trust value for this entry
		RecalcualteTrustValue();
	}
	delete pNewAICHHash;
	/*//DEBUG_ONLY(
		DebugLog(_T("Kad: EntryTrack: Indexed Keyword, Refresh: %s, Current Publisher: %s, Total Publishers: %u, Total different Names: %u,TrustValue: %.2f, file: %s"),
			(bRefresh ? _T("Yes") : _T("No")), (LPCTSTR)ipstr(htonl(m_uIP)), m_pliPublishingIPs->GetCount(), m_listFileNames.GetCount(), m_fTrustValue, m_uSourceID.ToHexString());
		//);*/
	/*if (m_aAICHHashes.GetCount() == 1) {
			DebugLog(_T("Kad: EntryTrack: Indexed Keyword, Refresh: %s, Current Publisher: %s, Total Publishers: %u, Total different Names: %u,TrustValue: %.2f, file: %s, AICH Hash: %s, Popularity: %u"),
			(bRefresh ? _T("Yes") : _T("No")), (LPCTSTR)ipstr(htonl(m_uIP)), m_pliPublishingIPs->GetCount(), m_listFileNames.GetCount(), m_fTrustValue, m_uSourceID.ToHexString(), m_aAICHHashes[0].GetString(), m_anAICHHashPopularity[0]);
	} else if (m_aAICHHashes.GetCount() > 1) {
			DebugLog(_T("Kad: EntryTrack: Indexed Keyword, Refresh: %s, Current Publisher: %s, Total Publishers: %u, Total different Names: %u,TrustValue: %.2f, file: %s, AICH Hash: %u - dumping"),
			(bRefresh ? _T("Yes") : _T("No")), (LPCTSTR)ipstr(htonl(m_uIP)), m_pliPublishingIPs->GetCount(), m_listFileNames.GetCount(), m_fTrustValue, m_uSourceID.ToHexString(), m_aAICHHashes.GetCount());
			for (int i = 0; i < m_aAICHHashes.GetCount(); ++i)
				DebugLog(_T("Hash: %s, Popularity: %u"),  m_aAICHHashes[i].GetString(), m_anAICHHashPopularity[i]);
	}*/
}

void CKeyEntry::RecalcualteTrustValue()
{
#define		PUBLISHPOINTSSPERSUBNET			10.0f
	// The trust value is supposed to be an indicator how trustworthy/important (or spammy) this entry is and lies between 0 and ~10000,
	// but mostly we say everything below 1 is bad, everything above 1 is good. It is calculated by looking at how many different
	// IPs/24 have published this entry and how many entries each of those IPs have.
	// Each IP/24 has x (say 3) points. This means if one IP publishes 3 different entries without any other IP publishing those entries,
	// each of those entries will have 3 / 3 = 1 Trust value. That's fine. If it publishes 6 alone, each entry has 3 / 6 = 0.5 trust value - not so good
	// However if there is another publisher for entry 5, which only publishes this entry then we have 3/6 + 3/1 = 3.5 trust value for this entry
	//
	// Whats the point? With this rating we try to avoid getting spammed with entries for a given keyword by a small IP range, which blends out
	// all other entries for this keyword do to its amount as well as giving an indicator for the searcher. So if we are the node to index "Knoppix", and someone
	// from 1 IP publishes 500 times "knoppix casino 500% bonus.txt", all those entries will have a trust value of 0.006 and we make sure that
	// on search requests for knoppix, those entries are only returned after all entries with a trust value > 1 were sent (if there is still space).
	//
	// Its important to note that entry with < 1 do NOT get ignored or singled out, this only comes into play if we have 300 more results for
	// a search request rating > 1
	if (m_pliPublishingIPs == NULL) {
		ASSERT(0);
		return;
	}
	dwLastTrustValueCalc = ::GetTickCount();
	m_fTrustValue = 0;
	ASSERT(!m_pliPublishingIPs->IsEmpty());
	for (POSITION pos = m_pliPublishingIPs->GetHeadPosition(); pos != NULL;) {
		uint32 nCount;
		if (!s_mapGlobalPublishIPs.Lookup(m_pliPublishingIPs->GetNext(pos).m_uIP & ~0xFF, nCount)) // /24 block; take care of endianness if needed
			nCount = 0;
		if (nCount > 0)
			m_fTrustValue += PUBLISHPOINTSSPERSUBNET / nCount;
		else {
			DebugLogError(_T("Kad: EntryTrack: Inconsistency RecalcualteTrustValue()"));
			ASSERT(0);
		}
	}
}

float CKeyEntry::GetTrustValue()
{
	// update if last calculation is too old, will assert if this entry is not supposed to have a trust value
	if (::GetTickCount() >= dwLastTrustValueCalc + MIN2MS(10))
		RecalcualteTrustValue();
	return m_fTrustValue;
}

void CKeyEntry::CleanUpTrackedPublishers()
{
	if (m_pliPublishingIPs == NULL)
		return;
	time_t tNow = time(NULL);
	while (!m_pliPublishingIPs->IsEmpty()) {
		// entries are ordered, older ones first
		const structPublishingIP &curEntry = m_pliPublishingIPs->GetHead();
		if (tNow < curEntry.m_tLastPublish + KADEMLIAREPUBLISHTIMEK)
			break;
		AdjustGlobalPublishTracking(curEntry.m_uIP, false, _T("cleanup"));
		m_pliPublishingIPs->RemoveHead();
	}
}

CEntry*	CKeyEntry::Copy()
{
	return CEntry::Copy();
}

void CKeyEntry::WritePublishTrackingDataToFile(CDataIO *pData)
{
	// format: <AICH HashCount 2><{AICH Hash Indexed} HashCount> <Names_Count 4><{<Name string><PopularityIndex 4>} Names_Count>
	//		   <PublisherCount 4><{<IP 4><Time 4><AICH Idx 2>} PublisherCount>

	// Write AICH Hashes and map them to a new cleaned up index without unreferenced hashes
	uint16 nNewIdxPos = 0;
	INT_PTR asize = m_aAICHHashes.GetCount();
	CArray<uint16> aNewIndexes;
	aNewIndexes.SetSize(asize);
	for (int i = 0; i < asize; ++i)
		aNewIndexes[i] = (m_anAICHHashPopularity[i] > 0) ? nNewIdxPos++ : _UI16_MAX;
	pData->WriteUInt16(nNewIdxPos);
	for (int i = 0; i < asize; ++i)
		if (m_anAICHHashPopularity[i] > 0)
			pData->WriteArray(m_aAICHHashes[i].GetRawHashC(), CAICHHash::GetHashSize());

	pData->WriteUInt32((uint32)m_listFileNames.GetCount());
	for (POSITION pos = m_listFileNames.GetHeadPosition(); pos != NULL;) {
		const structFileNameEntry &rCur = m_listFileNames.GetNext(pos);
		pData->WriteString(rCur.m_fileName);
		pData->WriteUInt32(rCur.m_uPopularityIndex);
	}
	if (m_pliPublishingIPs != NULL) {
		pData->WriteUInt32((uint32)m_pliPublishingIPs->GetCount());
		for (POSITION pos = m_pliPublishingIPs->GetHeadPosition(); pos != NULL;) {
			const structPublishingIP &rCur = m_pliPublishingIPs->GetNext(pos);
			ASSERT(rCur.m_uIP != 0);
			pData->WriteUInt32(rCur.m_uIP);
			pData->WriteUInt32((uint32)rCur.m_tLastPublish);
			uint16 nIdx = _UI16_MAX;
			if (rCur.m_wAICHHashIdx != _UI16_MAX) {
				nIdx = aNewIndexes[rCur.m_wAICHHashIdx];
				ASSERT(nIdx != _UI16_MAX || m_anAICHHashPopularity[0] <= 0);
			}
			pData->WriteUInt16(nIdx);
		}
	} else {
		ASSERT(0);
		pData->WriteUInt32(0);
	}
}

void CKeyEntry::ReadPublishTrackingDataFromFile(CDataIO *pData, bool bIncludesAICH)
{
	// format: <AICH HashCount 2><{AICH Hash Indexed} HashCount> <Names_Count 4><{<Name string><PopularityIndex 4>} Names_Count>
	//		   <PublisherCount 4><{<IP 4><Time 4><AICH Idx 2>} PublisherCount>
	ASSERT(m_aAICHHashes.IsEmpty());
	ASSERT(m_anAICHHashPopularity.IsEmpty());
	if (bIncludesAICH) {
		CAICHHash hash;
		for (uint16 i = pData->ReadUInt16(); i-- > 0;) { //hash count
			pData->ReadArray(hash.GetRawHash(), CAICHHash::GetHashSize());
			m_aAICHHashes.Add(hash);
			m_anAICHHashPopularity.Add(0);
		}
	}

	ASSERT(m_listFileNames.IsEmpty());
	uint32 nNameCount = pData->ReadUInt32();
	for (uint32 i = 0; i < nNameCount; ++i) {
		structFileNameEntry sToAdd;
		sToAdd.m_fileName = Kademlia::CKadTagValueString(pData->ReadStringUTF8());
		sToAdd.m_uPopularityIndex = pData->ReadUInt32();
		m_listFileNames.AddTail(sToAdd);
	}

	ASSERT(m_pliPublishingIPs == NULL);
	m_pliPublishingIPs = new CList<structPublishingIP>();
	uint32 nIPCount = pData->ReadUInt32();
	time_t nDbgLastTime = 0;
	for (uint32 i = 0; i < nIPCount; ++i) {
		structPublishingIP sToAdd;
		sToAdd.m_uIP = pData->ReadUInt32();
		ASSERT(sToAdd.m_uIP != 0);
		sToAdd.m_tLastPublish = pData->ReadUInt32();
		ASSERT(nDbgLastTime <= sToAdd.m_tLastPublish); // should always be sorted oldest first
		nDbgLastTime = sToAdd.m_tLastPublish;
		// read hash index and update popularity index
		if (bIncludesAICH) {
			sToAdd.m_wAICHHashIdx = pData->ReadUInt16();
			if (sToAdd.m_wAICHHashIdx != _UI16_MAX) {
				if (sToAdd.m_wAICHHashIdx >= m_aAICHHashes.GetCount()) {
					// should never happen
					ASSERT(0);
					DebugLogError(_T("CKeyEntry::ReadPublishTrackingDataFromFile - Out of Index AICH Hash index value while loading keywords"));
					sToAdd.m_wAICHHashIdx = _UI16_MAX;
				} else
					m_anAICHHashPopularity[sToAdd.m_wAICHHashIdx]++;
			}
		} else
			sToAdd.m_wAICHHashIdx = _UI16_MAX;

		AdjustGlobalPublishTracking(sToAdd.m_uIP, true, CString());

		m_pliPublishingIPs->AddTail(sToAdd);
	}
	RecalcualteTrustValue();
#ifdef _DEBUG
	if (m_aAICHHashes.GetCount() == 1)
		DebugLog(_T("Loaded 1 AICH Hash (%s, publishers %u of %u) for file %s"), (LPCTSTR)m_aAICHHashes[0].GetString(), m_anAICHHashPopularity[0], m_pliPublishingIPs->GetCount(), (LPCTSTR)m_uSourceID.ToHexString());
	else if (m_aAICHHashes.GetCount() > 1) {
		DebugLogWarning(_T("Loaded multiple (%u) AICH Hashes for file %s, dumping..."), m_aAICHHashes.GetCount(), (LPCTSTR)m_uSourceID.ToHexString());
		for (int i = 0; i < m_aAICHHashes.GetCount(); ++i)
			DebugLog(_T("%s - %u out of %u publishers"), (LPCTSTR)m_aAICHHashes[i].GetString(), m_anAICHHashPopularity[i], m_pliPublishingIPs->GetCount());
	}
	//if (GetTrustValue() < 1.0f)
		//DEBUG_ONLY( DebugLog(_T("Loaded %u different names, %u different publishIPs (trustvalue = %.2f) for file %s"), nNameCount, nIPCount, GetTrustValue(), m_uSourceID.ToHexString()) );
#endif
}

void CKeyEntry::DirtyDeletePublishData()
{
	// instead of deleting our publishers properly in the destructor with decreasing the count in the global map
	// we just remove them, and trust that the caller in the end also resets the global map, so the
	// kad shutdown is sped up a bit
	delete m_pliPublishingIPs;
	m_pliPublishingIPs = NULL;
}

void CKeyEntry::WriteTagListWithPublishInfo(CDataIO *pData)
{
	if (m_pliPublishingIPs == NULL || m_pliPublishingIPs->IsEmpty()) {
		ASSERT(0);
		WriteTagList(pData);
		return;
	}

	uint32 nAdditionalTags = 1 + static_cast<uint32>(!m_aAICHHashes.IsEmpty());

	WriteTagListInc(pData, nAdditionalTags); // write the standard tag list but increase the tag count by the count we want to add

	// here we add a tag including how many publishers this entry has, the trust value and how many different names are known
	// this is supposed to get used in later versions as an indicator for the user how valid this result is (of course this tag
	// alone cannot be trusted 100%, because we could be a bad node, but it's a part of the puzzle)
	uint32 uTrust = (uint16)(GetTrustValue() * 100);
	uint32 uPublishers = m_pliPublishingIPs->GetCount() % 256;
	uint32 uNames = m_listFileNames.GetCount() % 256;
	// 32 bit tag: <namecount uint8><publishers uint8><trustvalue*100 uint16>
	uint32 uTagValue = (uNames << 24) | (uPublishers << 16) | (uTrust << 0);
	const CKadTagUInt tag(TAG_PUBLISHINFO, uTagValue);
	pData->WriteTag(&tag);

	// Last but not least the AICH Hash tag, containing all reported (hopefully, exactly 1) AICH hashes
	// for this file together with the count of publishers who reported it
	if (!m_aAICHHashes.IsEmpty()) {
		CSafeMemFile fileAICHTag(100);
		uint8 byCount = 0;
		// get count of AICH tags with popularity > 0
		for (int i = 0; i < m_aAICHHashes.GetCount(); ++i) {
			byCount += static_cast<uint8>(m_anAICHHashPopularity[i] > 0);
			// bsob tags in kad are limited to 255 bytes, so no more than 12 AICH hashes can be written
			// that shouldn't be an issue however, as the normal AICH hash count is 1, if we have more than
			// 10 for some reason we can't use it most likely anyway
			if (1 + (CAICHHash::GetHashSize() * (byCount + 1)) + (1 * (byCount + 1)) > 250) {
				DebugLogWarning(_T("More than 12(!) AICH Hashes to send for search answer, have to truncate, entry: %s"), (LPCTSTR)m_uSourceID.ToHexString());
				break;
			}

		}
		// write tag even on 0 count now
		fileAICHTag.WriteUInt8(byCount);
		uint8 nWritten = 0;
		uint8 j;
		for (j = 0; nWritten < byCount && j < m_aAICHHashes.GetCount(); ++j) {
			if (m_anAICHHashPopularity[j] > 0) {
				fileAICHTag.WriteUInt8(m_anAICHHashPopularity[j]);
				m_aAICHHashes[j].Write(&fileAICHTag);
				++nWritten;
			}
		}
		ASSERT(nWritten == byCount && nWritten <= j);
		ASSERT(fileAICHTag.GetLength() <= 255);
		uint8 nSize = (uint8)fileAICHTag.GetLength();
		BYTE *byBuffer = fileAICHTag.Detach();
		const CKadTagBsob tag1(TAG_KADAICHHASHRESULT, byBuffer, nSize);
		pData->WriteTag(&tag);
		free(byBuffer);
	}
}

uint16 CKeyEntry::AddRemoveAICHHash(const CAICHHash &hash, bool bAdd)
{
	ASSERT(m_aAICHHashes.GetCount() == m_anAICHHashPopularity.GetCount());
	for (int i = (int)m_aAICHHashes.GetCount(); --i >= 0;)
		if (m_aAICHHashes[i] == hash) {
			if (bAdd)
				++m_anAICHHashPopularity[i];
			else {
				if (m_anAICHHashPopularity[i] > 0)
					--m_anAICHHashPopularity[i];
				//else
				//	ASSERT( false );
			}
			return (uint16)i;
		}

	if (bAdd) {
		m_aAICHHashes.Add(hash);
		m_anAICHHashPopularity.Add(1);
		return (uint16)(m_aAICHHashes.GetCount() - 1);
	}
	ASSERT(0);
	return _UI16_MAX;
}