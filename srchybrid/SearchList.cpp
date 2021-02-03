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
#include "SearchFile.h"
#include "SearchList.h"
#include "SearchParams.h"
#include "SearchResultsWnd.h"
#include "Packets.h"
#include "Preferences.h"
#include "UpDownClient.h"
#include "SafeFile.h"
#include "SharedFileList.h"
#include "KnownFileList.h"
#include "DownloadQueue.h"
#include "PartFile.h"
#include "CxImage/xImage.h"
#include "kademlia/utils/uint128.h"
#include "Kademlia/Kademlia/Entry.h"
#include "Kademlia/Kademlia/SearchManager.h"
#include "emuledlg.h"
#include "SearchDlg.h"
#include "SearchListCtrl.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define SPAMFILTER_FILENAME		_T("SearchSpam.met")
#define STOREDSEARCHES_FILENAME	_T("StoredSearches.met")
#define STOREDSEARCHES_VERSION	1
///////////////////////////////////////////////////////////////////////////////
// CSearchList

CSearchList::CSearchList()
	: outputwnd()
	, m_nCurED2KSearchID()
	, m_bSpamFilterLoaded()
{
}

CSearchList::~CSearchList()
{
	Clear();
	for (POSITION pos = m_aUDPServerRecords.GetStartPosition(); pos != NULL;) {
		uint32 dwIP;
		UDPServerRecord *pRecord;
		m_aUDPServerRecords.GetNextAssoc(pos, dwIP, pRecord);
		delete pRecord;
	}
	//m_aUDPServerRecords.RemoveAll(); will be called in the next destructor
}

void CSearchList::Clear()
{
	for (POSITION pos = m_listFileLists.GetHeadPosition(); pos != NULL;) {
		POSITION posLast = pos;
		SearchListsStruct *listCur = m_listFileLists.GetNext(pos);
		while (!listCur->m_listSearchFiles.IsEmpty())
			delete listCur->m_listSearchFiles.RemoveHead();
		m_listFileLists.RemoveAt(posLast);
		delete listCur;
	}
}

void CSearchList::RemoveResults(uint32 nSearchID)
{
	// this will not delete the item from the window, make sure your code does it if you call this
	for (POSITION pos = m_listFileLists.GetHeadPosition(); pos != NULL;) {
		POSITION posLast = pos;
		SearchListsStruct *listCur = m_listFileLists.GetNext(pos);
		if (listCur->m_nSearchID == nSearchID) {
			while (!listCur->m_listSearchFiles.IsEmpty())
				delete listCur->m_listSearchFiles.RemoveHead();
			m_listFileLists.RemoveAt(posLast);
			delete listCur;
			return;
		}
	}
}

void CSearchList::ShowResults(uint32 nSearchID)
{
	ASSERT(outputwnd);
	outputwnd->SetRedraw(FALSE);
	CMuleListCtrl::EUpdateMode bCurUpdateMode = outputwnd->SetUpdateMode(CMuleListCtrl::none/*direct*/);

	SearchList *list = GetSearchListForID(nSearchID);
	for (POSITION pos = list->GetHeadPosition(); pos != NULL;) {
		const CSearchFile *cur_file = list->GetNext(pos);
		ASSERT(cur_file->GetSearchID() == nSearchID);
		if (cur_file->GetListParent() == NULL && !cur_file->m_flags.noshow) {
			outputwnd->AddResult(cur_file);
			if (cur_file->IsListExpanded() && cur_file->GetListChildCount() > 0)
				outputwnd->UpdateSources(cur_file);
		}
	}

	outputwnd->SetUpdateMode(bCurUpdateMode);
	outputwnd->SetRedraw(TRUE);
}

void CSearchList::RemoveResult(CSearchFile *todel)
{
	SearchList *list = GetSearchListForID(todel->GetSearchID());
	POSITION pos = list->Find(todel);
	if (pos != NULL) {
		theApp.emuledlg->searchwnd->RemoveResult(todel);
		list->RemoveAt(pos);
		delete todel;
	}
}

void CSearchList::NewSearch(CSearchListCtrl *pWnd, const CString &strResultFileType, SSearchParams *pParams)
{
	if (pWnd)
		outputwnd = pWnd;

	m_strResultFileType = strResultFileType;
	ASSERT(pParams->eType != SearchTypeAutomatic);
	if (pParams->eType == SearchTypeEd2kServer || pParams->eType == SearchTypeEd2kGlobal) {
		m_nCurED2KSearchID = pParams->dwSearchID;
		m_aCurED2KSentRequestsIPs.RemoveAll();
		m_aCurED2KSentReceivedIPs.RemoveAll();
	}
	m_foundFilesCount.SetAt(pParams->dwSearchID, 0);
	m_foundSourcesCount.SetAt(pParams->dwSearchID, 0);
	m_ReceivedUDPAnswersCount.SetAt(pParams->dwSearchID, 0);
	m_RequestedUDPAnswersCount.SetAt(pParams->dwSearchID, 0);

	// convert the expression into an array of search keywords which the user has typed in
	// this is used for the spam filter later and not at all semantically equal to
	// the actual search expression any more
	m_astrSpamCheckCurSearchExp.RemoveAll();
	pParams->strExpression.MakeLower();
	if (pParams->strExpression.Find(_T("related:"), 0) != 0) { // ignore special searches
		int nPos, nPos2;
		while ((nPos = pParams->strExpression.Find(_T('"'))) >= 0 && (nPos2 = pParams->strExpression.Find(_T('"'), nPos + 1)) >= 0) {
			const CString &strQuoted(pParams->strExpression.Mid(nPos + 1, (nPos2 - nPos) - 1));
			m_astrSpamCheckCurSearchExp.Add(strQuoted);
			pParams->strExpression.Delete(nPos, (nPos2 - nPos) + 1);
		}
		for (int iPos = 0; iPos >= 0;) {
			const CString &sToken(pParams->strExpression.Tokenize(_T(".[]()!-'_ "), iPos));
			if (!sToken.IsEmpty() && sToken != "and" && sToken != "or" && sToken != "not")
				m_astrSpamCheckCurSearchExp.Add(sToken);
		}
	}
}

UINT CSearchList::ProcessSearchAnswer(const uchar *in_packet, uint32 size
	, const CUpDownClient &sender, bool *pbMoreResultsAvailable, LPCTSTR pszDirectory)
{
	uint32 uSearchID = theApp.emuledlg->searchwnd->m_pwndResults->GetNextSearchID();
	SSearchParams *pParams = new SSearchParams;
	pParams->strExpression = sender.GetUserName();
	pParams->dwSearchID = uSearchID;
	pParams->bClientSharedFiles = true;
	if (theApp.emuledlg->searchwnd->CreateNewTab(pParams)) {
		m_foundFilesCount.SetAt(uSearchID, 0);
		m_foundSourcesCount.SetAt(uSearchID, 0);
	} else
		delete pParams;

	CSafeMemFile packet(in_packet, size);
	for (uint32 results = packet.ReadUInt32(); results > 0; --results) {
		CSearchFile *toadd = new CSearchFile(&packet, sender.GetUnicodeSupport() != UTF8strNone, uSearchID, 0, 0, pszDirectory);
		if (toadd->IsLargeFile() && !sender.SupportsLargeFiles()) {
			DebugLogWarning(_T("Client offers large file (%s) but did not announce support for it - ignoring file"), (LPCTSTR)toadd->GetFileName());
			delete toadd;
			continue;
		}
		toadd->SetClientID(sender.GetIP());
		toadd->SetClientPort(sender.GetUserPort());
		toadd->SetClientServerIP(sender.GetServerIP());
		toadd->SetClientServerPort(sender.GetServerPort());
		if (sender.GetServerIP() && sender.GetServerPort()) {
			CSearchFile::SServer server(sender.GetServerIP(), sender.GetServerPort(), false);
			server.m_uAvail = 1;
			toadd->AddServer(server);
		}
		toadd->SetPreviewPossible(sender.GetPreviewSupport() && ED2KFT_VIDEO == GetED2KFileTypeID(toadd->GetFileName()));
		AddToList(toadd, true);
	}

	if (pbMoreResultsAvailable)
		*pbMoreResultsAvailable = false;
	int iAddData = static_cast<int>(packet.GetLength() - packet.GetPosition());
	if (iAddData == 1) {
		uint8 ucMore = packet.ReadUInt8();
		if (ucMore <= 0x01) {
			if (pbMoreResultsAvailable)
				*pbMoreResultsAvailable = ucMore != 0;
			if (thePrefs.GetDebugClientTCPLevel() > 0)
				Debug(_T("  Client search answer(%s): More=%u\n"), sender.GetUserName(), ucMore);
		} else if (thePrefs.GetDebugClientTCPLevel() > 0)
			Debug(_T("*** NOTE: Client ProcessSearchAnswer(%s): ***AddData: 1 byte: 0x%02x\n"), sender.GetUserName(), ucMore);

	} else if (iAddData > 0) {
		if (thePrefs.GetDebugClientTCPLevel() > 0) {
			Debug(_T("*** NOTE: Client ProcessSearchAnswer(%s): ***AddData: %u bytes\n"), sender.GetUserName(), iAddData);
			DebugHexDump(in_packet + packet.GetPosition(), iAddData);
		}
	}

	packet.Close();
	return GetResultCount(uSearchID);
}

UINT CSearchList::ProcessSearchAnswer(const uchar *in_packet, uint32 size, bool bOptUTF8
	, uint32 nServerIP, uint16 nServerPort, bool *pbMoreResultsAvailable)
{
	CSafeMemFile packet(in_packet, size);
	for (uint32 i = packet.ReadUInt32(); i > 0; --i) {
		CSearchFile *toadd = new CSearchFile(&packet, bOptUTF8, m_nCurED2KSearchID);
		toadd->SetClientServerIP(nServerIP);
		toadd->SetClientServerPort(nServerPort);
		if (nServerIP && nServerPort) {
			CSearchFile::SServer server(nServerIP, nServerPort, false);
			server.m_uAvail = toadd->GetIntTagValue(FT_SOURCES);
			toadd->AddServer(server);
		}
		AddToList(toadd, false);
	}

	if (pbMoreResultsAvailable)
		*pbMoreResultsAvailable = false;
	int iAddData = (int)(packet.GetLength() - packet.GetPosition());
	if (iAddData == 1) {
		uint8 ucMore = packet.ReadUInt8();
		if (ucMore == 0x00 || ucMore == 0x01) {
			if (pbMoreResultsAvailable)
				*pbMoreResultsAvailable = ucMore != 0;
			if (thePrefs.GetDebugServerTCPLevel() > 0)
				Debug(_T("  Search answer(Server %s:%u): More=%u\n"), (LPCTSTR)ipstr(nServerIP), nServerPort, ucMore);
		} else if (thePrefs.GetDebugServerTCPLevel() > 0)
			Debug(_T("*** NOTE: ProcessSearchAnswer(Server %s:%u): ***AddData: 1 byte: 0x%02x\n"), (LPCTSTR)ipstr(nServerIP), nServerPort, ucMore);
	} else if (iAddData > 0) {
		if (thePrefs.GetDebugServerTCPLevel() > 0) {
			Debug(_T("*** NOTE: ProcessSearchAnswer(Server %s:%u): ***AddData: %u bytes\n"), (LPCTSTR)ipstr(nServerIP), nServerPort, iAddData);
			DebugHexDump(in_packet + packet.GetPosition(), iAddData);
		}
	}

	packet.Close();
	return GetED2KResultCount();
}

UINT CSearchList::ProcessUDPSearchAnswer(CFileDataIO &packet, bool bOptUTF8, uint32 nServerIP, uint16 nServerPort)
{
	CSearchFile *toadd = new CSearchFile(&packet, bOptUTF8, m_nCurED2KSearchID, nServerIP, nServerPort, NULL, false, true);

	bool bFound = false;
	for (INT_PTR i = m_aCurED2KSentRequestsIPs.GetCount(); --i >= 0;)
		if (m_aCurED2KSentRequestsIPs[i] == nServerIP) {
			bFound = true;
			break;
		}

	if (!bFound) {
		DebugLogError(_T("Unrequested or delayed Server UDP Searchresult received from IP %s, ignoring"), (LPCTSTR)ipstr(nServerIP));
		delete toadd;
		return 0;
	}

	bool bNewResponse = true;
	for (INT_PTR i = m_aCurED2KSentReceivedIPs.GetCount(); --i >= 0;)
		if (m_aCurED2KSentReceivedIPs[i] == nServerIP) {
			bNewResponse = false;
			break;
		}

	if (bNewResponse) {
		uint32 nResponses;
		if (!m_ReceivedUDPAnswersCount.Lookup(m_nCurED2KSearchID, nResponses))
			nResponses = 0;
		m_ReceivedUDPAnswersCount.SetAt(m_nCurED2KSearchID, nResponses + 1);
		m_aCurED2KSentReceivedIPs.Add(nServerIP);
	}

	UDPServerRecord *pRecord;
	if (m_aUDPServerRecords.Lookup(nServerIP, pRecord))
		pRecord->m_nResults++;
	else {
		pRecord = new UDPServerRecord;
		pRecord->m_nResults = 1;
		pRecord->m_nSpamResults = 0;
		m_aUDPServerRecords.SetAt(nServerIP, pRecord);
	}

	AddToList(toadd, false, nServerIP);
	return GetED2KResultCount();
}

UINT CSearchList::GetResultCount(uint32 nSearchID) const
{
	UINT nSources;
	return m_foundSourcesCount.Lookup(nSearchID, nSources) ? nSources : 0;
}

UINT CSearchList::GetED2KResultCount() const
{
	return GetResultCount(m_nCurED2KSearchID);
}

void CSearchList::GetWebList(CQArray<SearchFileStruct, SearchFileStruct> *SearchFileArray, int iSortBy) const
{
	for (POSITION pos = m_listFileLists.GetHeadPosition(); pos != NULL;) {
		SearchListsStruct *listCur = m_listFileLists.GetNext(pos);
		for (POSITION pos2 = listCur->m_listSearchFiles.GetHeadPosition(); pos2 != NULL;) {
			const CSearchFile *pFile = listCur->m_listSearchFiles.GetNext(pos2);
			if (pFile == NULL || pFile->GetListParent() != NULL || !(uint64)pFile->GetFileSize() || pFile->GetFileName().IsEmpty() || pFile->m_flags.noshow)
				continue;

			SearchFileStruct structFile;
			structFile.m_strFileName = pFile->GetFileName();
			structFile.m_strFileType = pFile->GetFileTypeDisplayStr();
			structFile.m_strFileHash = md4str(pFile->GetFileHash());
			structFile.m_uSourceCount = pFile->GetSourceCount();
			structFile.m_dwCompleteSourceCount = pFile->GetCompleteSourceCount();
			structFile.m_uFileSize = pFile->GetFileSize();

			switch (iSortBy) {
			case 0:
				structFile.m_strIndex = structFile.m_strFileName;
				break;
			case 1:
				structFile.m_strIndex.Format(_T("%10I64u"), structFile.m_uFileSize);
				break;
			case 2:
				structFile.m_strIndex = structFile.m_strFileHash;
				break;
			case 3:
				structFile.m_strIndex.Format(_T("%09u"), structFile.m_uSourceCount);
				break;
			case 4:
				structFile.m_strIndex = structFile.m_strFileType;
				break;
			default:
				structFile.m_strIndex.Empty();
			}
			SearchFileArray->Add(structFile);
		}
	}
}

void CSearchList::AddFileToDownloadByHash(const uchar *hash, int cat)
{
	for (POSITION pos = m_listFileLists.GetHeadPosition(); pos != NULL;) {
		const SearchListsStruct *listCur = m_listFileLists.GetNext(pos);
		for (POSITION pos2 = listCur->m_listSearchFiles.GetHeadPosition(); pos2 != NULL;) {
			CSearchFile *sf = listCur->m_listSearchFiles.GetNext(pos2);
			if (md4equ(hash, sf->GetFileHash())) {
				theApp.downloadqueue->AddSearchToDownload(sf, 2, cat);
				return;
			}
		}
	}
}

bool CSearchList::AddToList(CSearchFile *toadd, bool bClientResponse, uint32 dwFromUDPServerIP)
{
	if (!bClientResponse && !m_strResultFileType.IsEmpty() && m_strResultFileType != toadd->GetFileType()) {
		delete toadd;
		return false;
	}
	SearchList *list = GetSearchListForID(toadd->GetSearchID());

	// Spam filter: Calculate the filename without any used keywords (and separators) for later use
	CString strNameWithoutKeyword;
	CString strName(toadd->GetFileName());
	strName.MakeLower();

	for (int iPos = 0; iPos >= 0;) {
		const CString &sToken(strName.Tokenize(_T(".[]()!-'_ "), iPos));
		if (sToken.IsEmpty())
			continue;
		bool bFound = false;
		if (!bClientResponse && toadd->GetSearchID() == m_nCurED2KSearchID) {
			for (INT_PTR i = m_astrSpamCheckCurSearchExp.GetCount(); --i >= 0;) {
				if (sToken == m_astrSpamCheckCurSearchExp[i]) {
					bFound = true;
					break;
				}
			}
		}
		if (!bFound) {
			if (!strNameWithoutKeyword.IsEmpty())
				strNameWithoutKeyword += _T(' ');
			strNameWithoutKeyword += sToken;
		}
	}
	toadd->SetNameWithoutKeyword(strNameWithoutKeyword);

	// search for a 'parent' with same file hash and search-id as the new search result entry
	for (POSITION pos = list->GetHeadPosition(); pos != NULL;) {
		CSearchFile *parent = list->GetNext(pos);
		if (parent->GetListParent() == NULL
			&& md4equ(parent->GetFileHash(), toadd->GetFileHash()))
		{
			// if this parent does not yet have any child entries, create one child entry
			// which is equal to the current parent entry (needed for GUI when expanding the child list).
			if (!parent->GetListChildCount()) {
				CSearchFile *child = new CSearchFile(parent);
				child->m_flags.nowrite = 1; //will not save
				child->SetListParent(parent);
				int iSources = parent->GetSourceCount();
				if (iSources == 0)
					iSources = 1;
				child->SetListChildCount(iSources);
				list->AddTail(child);
				parent->SetListChildCount(1);
			}

			// get the 'Availability' of the new search result entry
			uint32 uAvail = toadd->GetSourceCount();
			if (bClientResponse && !uAvail)
				// If this is a response from a client ("View Shared Files"), we set the "Availability" at least to 1.
				uAvail = 1;

			// get 'Complete Sources' of the new search result entry
			uint32 uCompleteSources = toadd->GetCompleteSourceCount();

			bool bFound = false;
			if (thePrefs.GetDebugSearchResultDetailLevel() >= 1)
				; // for debugging: do not merge search results
			else {
				// check if that parent already has a child with same filename as the new search result entry
				for (POSITION pos2 = list->GetHeadPosition(); pos2 != NULL && !bFound; ) {
					CSearchFile *child = list->GetNext(pos2);
					if (child != toadd														// not the same object
						&& child->GetListParent() == parent									// is a child of our result (one file hash)
						&& toadd->GetFileName().CompareNoCase(child->GetFileName()) == 0)	// same name
					{
						bFound = true;

						// add properties of new search result entry to the already available child entry (with same filename)
						// ed2k: use the sum of all values, kad: use the max. values
						if (toadd->IsKademlia()) {
							if (uAvail > child->GetListChildCount())
								child->SetListChildCount(uAvail);
						} else
							child->AddListChildCount(uAvail);

						child->AddSources(uAvail);
						child->AddCompleteSources(uCompleteSources);

						// Check AICH Hash - if they differ, clear it (see KademliaSearchKeyword)
						//					 if we didn't have a hash yet, take it over
						if (toadd->GetFileIdentifier().HasAICHHash()) {
							if (child->GetFileIdentifier().HasAICHHash()) {
								if (parent->GetFileIdentifier().GetAICHHash() != toadd->GetFileIdentifier().GetAICHHash()) {
									DEBUG_ONLY(DebugLogWarning(_T("Kad: SearchList: AddToList: Received searchresult with different AICH hash than existing one, ignoring AICH for result %s"), (LPCTSTR)child->GetFileName()));
									child->SetFoundMultipleAICH();
									child->GetFileIdentifier().ClearAICHHash();
								}
							} else if (!child->DidFoundMultipleAICH()) {
								DEBUG_ONLY(DebugLog(_T("Kad: SearchList: AddToList: Received searchresult with new AICH hash %s, taking over to existing result. Entry: %s"), (LPCTSTR)toadd->GetFileIdentifier().GetAICHHash().GetString(), (LPCTSTR)child->GetFileName()));
								child->GetFileIdentifier().SetAICHHash(toadd->GetFileIdentifier().GetAICHHash());
							}
						}
						break;
					}
				}
			}
			if (!bFound) {
				// the parent which we had found does not yet have a child with that new search result's entry name,
				// add the new entry as a new child
				//
				toadd->SetListParent(parent);
				toadd->SetListChildCount(uAvail);
				if (!toadd->m_flags.noshow)
					parent->AddListChildCount(1);
				list->AddTail(toadd);
			}

			// copy possible available sources from new search result entry to parent
			if (IsValidSearchResultClientIPPort(toadd->GetClientID(), toadd->GetClientPort())) {
				// pre-filter sources which would be dropped in CPartFile::AddSources
				if (CPartFile::CanAddSource(toadd->GetClientID(), toadd->GetClientPort(), toadd->GetClientServerIP(), toadd->GetClientServerPort())) {
					CSearchFile::SClient client(toadd->GetClientID(), toadd->GetClientPort(), toadd->GetClientServerIP(), toadd->GetClientServerPort());
					if (parent->GetClients().Find(client) < 0)
						parent->AddClient(client);
				}
			} else {
				if (thePrefs.GetDebugServerSearchesLevel() > 1)
					Debug(_T("Filtered source from search result %s:%u\n"), (LPCTSTR)DbgGetClientID(toadd->GetClientID()), toadd->GetClientPort());
			}

			// copy possible available servers from new search result entry to parent
			// will be used in future
			if (toadd->GetClientServerIP() && toadd->GetClientServerPort()) {
				CSearchFile::SServer server(toadd->GetClientServerIP(), toadd->GetClientServerPort(), toadd->IsServerUDPAnswer());
				int iFound = parent->GetServers().Find(server);
				if (iFound == -1) {
					server.m_uAvail = uAvail;
					parent->AddServer(server);
				} else
					parent->GetServerAt(iFound).m_uAvail += uAvail;
			}

			UINT uAllChildrenSourceCount = 0;			// ed2k: sum of all sources, kad: the max. sources found
			UINT uAllChildrenCompleteSourceCount = 0; // ed2k: sum of all sources, kad: the max. sources found
			UINT uDifferentNames = 0; // max known different names
			UINT uPublishersKnown = 0; // max publishers known (might be changed to median)
			UINT uTrustValue = 0; // average trust value (might be changed to median)
			uint32 nPublishInfoTags = 0;
			const CSearchFile *bestEntry = NULL;
			bool bHasMultipleAICHHashes = false;
			CAICHHash aichHash;
			bool bAICHHashValid = false;
			for (POSITION pos2 = list->GetHeadPosition(); pos2 != NULL;) {
				const CSearchFile *child = list->GetNext(pos2);
				if (child->GetListParent() == parent) {
					const CFileIdentifier &fileid = child->GetFileIdentifierC();
					// figure out if the children of different AICH hashes
					if (fileid.HasAICHHash()) {
						if (bAICHHashValid && aichHash != fileid.GetAICHHash())
							bHasMultipleAICHHashes = true;
						else if (!bAICHHashValid) {
							aichHash = fileid.GetAICHHash();
							bAICHHashValid = true;
						}
					} else if (child->DidFoundMultipleAICH())
						bHasMultipleAICHHashes = true;

					if (parent->IsKademlia()) {
						if (child->GetListChildCount() > uAllChildrenSourceCount)
							uAllChildrenSourceCount = child->GetListChildCount();
						/*if (child->GetCompleteSourceCount() > uAllChildrenCompleteSourceCount) // not yet supported
							uAllChildrenCompleteSourceCount = child->GetCompleteSourceCount();*/
						uint32 u = child->GetKadPublishInfo();
						if (u != 0) {
							++nPublishInfoTags;
							uDifferentNames = max(uDifferentNames, (u >> 24) & 0xFF);
							uPublishersKnown = max(uPublishersKnown, (u >> 16) & 0xFF);
							uTrustValue += u & 0x0000FFFF;
						}
					} else {
						uAllChildrenSourceCount += child->GetListChildCount();
						uAllChildrenCompleteSourceCount += child->GetCompleteSourceCount();
					}

					if (bestEntry == NULL || child->GetListChildCount() > bestEntry->GetListChildCount())
						bestEntry = child;
				}
			}
			if (bestEntry) {
				parent->SetFileSize(bestEntry->GetFileSize());
				parent->SetFileName(bestEntry->GetFileName());
				parent->SetFileType(bestEntry->GetFileType());
				parent->SetSourceCount(uAllChildrenSourceCount);
				parent->SetCompleteSourceCount(uAllChildrenCompleteSourceCount);
				if (nPublishInfoTags > 0)
					uTrustValue /= nPublishInfoTags;
				parent->SetKadPublishInfo(((uDifferentNames & 0xff) << 24) | ((uPublishersKnown & 0xff) << 16) | ((uTrustValue & 0xffff) << 0));
				// if all children have the same AICH hash (or none), set the parent hash to it, otherwise clear it (see KademliaSearchKeyword)
				if (bHasMultipleAICHHashes || !bAICHHashValid)
					parent->GetFileIdentifier().ClearAICHHash();
				else //if (bAICHHashValid) always true
					parent->GetFileIdentifier().SetAICHHash(aichHash);
			}
			// recalculate spam rating
			DoSpamRating(parent, bClientResponse, false, false, false, dwFromUDPServerIP);

			// add the 'Availability' of the new search result entry to the total search result count for this search
			AddResultCount(parent->GetSearchID(), parent->GetFileHash(), uAvail, parent->IsConsideredSpam());

			// update parent in GUI
			if (outputwnd)
				outputwnd->UpdateSources(parent);

			if (bFound) {
				toadd->m_flags.noshow = 1; //hide in GUI
				list->AddTail(toadd);
			}
			return true;
		}
	}

	// no bounded result found yet -> add as parent to list
	toadd->SetListParent(NULL);
	UINT uAvail = toadd->GetSourceCount();
	if (list->AddTail(toadd)) {
		UINT tempValue;
		if (!m_foundFilesCount.Lookup(toadd->GetSearchID(), tempValue))
			tempValue = 0;
		m_foundFilesCount.SetAt(toadd->GetSearchID(), tempValue + 1);

		// get the 'Availability' of this new search result entry

		if (bClientResponse)
			// If this is a response from a client ("View Shared Files"), we set the "Availability" at least to 1.
			toadd->AddSources(uAvail ? uAvail : 1);
	}

	if (thePrefs.GetDebugSearchResultDetailLevel() >= 1)
		toadd->SetListExpanded(true);

	// calculate spam rating
	DoSpamRating(toadd, bClientResponse, false, false, false, dwFromUDPServerIP);

	// add the 'Availability' of this new search result entry to the total search result count for this search
	AddResultCount(toadd->GetSearchID(), toadd->GetFileHash(), uAvail, toadd->IsConsideredSpam());

	// add parent in GUI
	if (outputwnd)
		outputwnd->AddResult(toadd);

	return true;
}

CSearchFile* CSearchList::GetSearchFileByHash(const uchar *hash) const
{
	for (POSITION pos = m_listFileLists.GetHeadPosition(); pos != NULL;) {
		const SearchListsStruct *listCur = m_listFileLists.GetNext(pos);
		for (POSITION pos2 = listCur->m_listSearchFiles.GetHeadPosition(); pos2 != NULL;) {
			CSearchFile *sf = listCur->m_listSearchFiles.GetNext(pos2);
			if (md4equ(hash, sf->GetFileHash()))
				return sf;
		}
	}
	return NULL;
}

bool CSearchList::AddNotes(Kademlia::CEntry *entry, const uchar *hash)
{
	bool flag = false;
	for (POSITION pos = m_listFileLists.GetHeadPosition(); pos != NULL;) {
		const SearchListsStruct *listCur = m_listFileLists.GetNext(pos);
		for (POSITION pos2 = listCur->m_listSearchFiles.GetHeadPosition(); pos2 != NULL;) {
			CSearchFile *sf = listCur->m_listSearchFiles.GetNext(pos2);
			if (md4equ(hash, sf->GetFileHash())) {
				Kademlia::CEntry *entryClone = entry->Copy();
				if (sf->AddNote(entryClone))
					flag = true;
				else
					delete entryClone;
			}
		}
	}
	return flag;
}

void CSearchList::SetNotesSearchStatus(const uchar *pFileHash, bool bSearchRunning)
{
	for (POSITION pos = m_listFileLists.GetHeadPosition(); pos != NULL;) {
		const SearchListsStruct *listCur = m_listFileLists.GetNext(pos);
		for (POSITION pos2 = listCur->m_listSearchFiles.GetHeadPosition(); pos2 != NULL;) {
			CSearchFile *sf = listCur->m_listSearchFiles.GetNext(pos2);
			if (md4equ(pFileHash, sf->GetFileHash()))
				sf->SetKadCommentSearchRunning(bSearchRunning);
		}
	}
}

void CSearchList::AddResultCount(uint32 nSearchID, const uchar *hash, UINT nCount, bool bSpam)
{
	// do not count already available or downloading files for the search result limit
	if (theApp.sharedfiles->GetFileByID(hash) || theApp.downloadqueue->GetFileByID(hash))
		return;

	UINT tempValue;
	if (!m_foundSourcesCount.Lookup(nSearchID, tempValue))
		tempValue = 0;

	// spam files count as max 5 availability
	m_foundSourcesCount.SetAt(nSearchID, tempValue
		+ ((bSpam && thePrefs.IsSearchSpamFilterEnabled()) ? min(nCount, 5) : nCount));
}

// FIXME LARGE FILES
void CSearchList::KademliaSearchKeyword(uint32 searchID, const Kademlia::CUInt128 *pFileID, LPCTSTR name
	, uint64 size, LPCTSTR type, UINT uKadPublishInfo
	, CArray<CAICHHash> &raAICHHashes, CArray<uint8> &raAICHHashPopularity
	, SSearchTerm *pQueriedSearchTerm, UINT numProperties, ...)
{
	va_list args;
	va_start(args, numProperties);

	EUTF8str eStrEncode = UTF8strRaw;
	Kademlia::CKeyEntry verifierEntry;

	verifierEntry.m_uKeyID.SetValue(*pFileID);
	uchar fileid[16];
	pFileID->ToByteArray(fileid);

	CSafeMemFile temp(250);
	temp.WriteHash16(fileid);
	temp.WriteUInt32(0);	// client IP
	temp.WriteUInt16(0);	// client port

	// write tag list
	UINT uFilePosTagCount = (UINT)temp.GetPosition();
	temp.WriteUInt32(0); // dummy tag count, will be filled later

	uint32 tagcount = 0;
	// standard tags
	CTag tagName(FT_FILENAME, name);
	tagName.WriteTagToFile(&temp, eStrEncode);
	++tagcount;
	verifierEntry.SetFileName(Kademlia::CKadTagValueString(name));

	CTag tagSize(FT_FILESIZE, size, true);
	tagSize.WriteTagToFile(&temp, eStrEncode);
	++tagcount;
	verifierEntry.m_uSize = size;

	if (type != NULL && type[0] != _T('\0')) {
		CTag tagType(FT_FILETYPE, type);
		tagType.WriteTagToFile(&temp, eStrEncode);
		++tagcount;
		verifierEntry.AddTag(new Kademlia::CKadTagStr(TAG_FILETYPE, type));
	}

	// additional tags
	for (; numProperties > 0; --numProperties) {
		UINT uPropType = va_arg(args, UINT);
		LPCSTR pszPropName = va_arg(args, LPCSTR);
		LPVOID pvPropValue = va_arg(args, LPVOID);
		if (uPropType == TAGTYPE_STRING) {
			if ((LPCTSTR)pvPropValue != NULL && ((LPCTSTR)pvPropValue)[0] != _T('\0')) {
				if (strlen(pszPropName) == 1) {
					CTag tagProp((uint8)*pszPropName, (LPCTSTR)pvPropValue);
					tagProp.WriteTagToFile(&temp, eStrEncode);
				} else {
					CTag tagProp(pszPropName, (LPCTSTR)pvPropValue);
					tagProp.WriteTagToFile(&temp, eStrEncode);
				}
				verifierEntry.AddTag(new Kademlia::CKadTagStr(pszPropName, (LPCTSTR)pvPropValue));
				++tagcount;
			}
		} else if (uPropType == TAGTYPE_UINT32) {
			if ((uint32)pvPropValue != 0) {
				CTag tagProp(pszPropName, (uint32)pvPropValue);
				tagProp.WriteTagToFile(&temp, eStrEncode);
				++tagcount;
				verifierEntry.AddTag(new Kademlia::CKadTagUInt(pszPropName, (uint32)pvPropValue));
			}
		} else
			ASSERT(0);
	}
	va_end(args);
	temp.Seek(uFilePosTagCount, SEEK_SET);
	temp.WriteUInt32(tagcount);

	if (pQueriedSearchTerm == NULL || verifierEntry.StartSearchTermsMatch(*pQueriedSearchTerm)) {
		temp.SeekToBegin();
		CSearchFile *tempFile = new CSearchFile(&temp, eStrEncode == UTF8strRaw, searchID, 0, 0, NULL, true);
		tempFile->SetKadPublishInfo(uKadPublishInfo);
		// About the AICH hash: We received a list of possible AICH hashes for this file and now have to decide what to do
		// If it wasn't for backwards compatibility, the choice would be easy: Each different md4+aich+size is its own result,
		// but we can't do this alone for the fact that for the next years we will always have publishers which don't report
		// the AICH hash at all (which would mean having a different entry, which leads to double files in search results).
		// So here is what we do for now:
		// If we have exactly 1 AICH hash and more than 1/3 of the publishers reported it, we set it as verified AICH hash for
		// the file (which is as good as using an ed2k link with an AICH hash attached). If less publishers reported it or if we
		// have multiple AICH hashes, we ignore them and use the MD4 only.
		// This isn't a perfect solution, but it makes sure not to open any new attack vectors (a wrong AICH hash means we cannot
		// download the file successfully) nor to confuse users by requiring them to select an entry out of several equal looking results.
		// Once the majority of nodes in the network publishes AICH hashes, this might get reworked to make the AICH hash more sticky
		if (raAICHHashes.GetCount() == 1 && raAICHHashPopularity.GetCount() == 1) {
			uint8 byPublishers = (uint8)((uKadPublishInfo >> 16) & 0xFF);
			if (byPublishers > 0 && raAICHHashPopularity[0] > 0 && byPublishers / raAICHHashPopularity[0] <= 3) {
				DEBUG_ONLY(DebugLog(_T("Received accepted AICH Hash for search result %s, %u out of %u Publishers, Hash: %s")
					, (LPCTSTR)tempFile->GetFileName(), raAICHHashPopularity[0], byPublishers, (LPCTSTR)raAICHHashes[0].GetString()));
				tempFile->GetFileIdentifier().SetAICHHash(raAICHHashes[0]);
			} else
				DEBUG_ONLY(DebugLog(_T("Received unaccepted AICH Hash for search result %s, %u out of %u Publishers, Hash: %s")
					, (LPCTSTR)tempFile->GetFileName(), raAICHHashPopularity[0], byPublishers, (LPCTSTR)raAICHHashes[0].GetString()));
		} else if (raAICHHashes.GetCount() > 1)
			DEBUG_ONLY(DebugLog(_T("Received multiple (%u) AICH hashes for search result %s, ignoring AICH"), raAICHHashes.GetCount(), (LPCTSTR)tempFile->GetFileName()));
		AddToList(tempFile);
	} else
		DebugLogWarning(_T("Kad Searchresult failed sanitize check against search query, ignoring. (%s)"), name);
}


// default spam threshold = 60
#define SPAM_FILEHASH_HIT					100

#define SPAM_FULLNAME_HIT					80
#define	SPAM_SMALLFULLNAME_HIT				50
#define SPAM_SIMILARNAME_HIT				60
#define SPAM_SMALLSIMILARNAME_HIT			40
#define SPAM_SIMILARNAME_NEARHIT			50
#define SPAM_SIMILARNAME_FARHIT				40

#define SPAM_SIMILARSIZE_HIT				10

#define SPAM_UDPSERVERRES_HIT				21
#define SPAM_UDPSERVERRES_NEARHIT			15
#define SPAM_UDPSERVERRES_FARHIT			10

#define SPAM_ONLYUDPSPAMSERVERS_HIT			30

#define SPAM_SOURCE_HIT						39

#define SPAM_HEURISTIC_BASEHIT				39
#define SPAM_HEURISTIC_MAXHIT				60


#define UDP_SPAMRATIO_THRESHOLD				50


void CSearchList::DoSpamRating(CSearchFile *pSearchFile, bool bIsClientFile, bool bMarkAsNoSpam, bool bRecalculateAll, bool bUpdate, uint32 dwFromUDPServerIP)
{
	/* This spam filter uses two simple approaches to try to identify spam search results:
	1 - detect general characteristics of fake results - not very reliable
		which are (each hit increases the score)
		* high availability from one udp server, but none from others
		* archive or program + size between 0,1 and 10 MB
		* 100% complete sources together with high availability
		Apparently, those characteristics target for current spyware fake results, other fake results like videos
		and so on will not be detectable, because only the first point is more or less common for server fake results,
		which would produce too many false positives

	2 - learn characteristics of files a user has marked as spam
		remembered data is:
		* FileHash (of course, a hit will always lead to a full score rating)
		* Equal filename
		* Equal or similar name after removing the search keywords and separators
			(if search for "emule", "blubby!! emule foo.rar" is remembered as "blubby foo rar")
		* Similar size (+- 5% but max 5MB) as other spam files
		* Equal search source server (UDP only)
		* Equal initial source clients
		* Ratio (Spam / NotSpam) of UDP Servers
	Both detection methods add to the same score rating.

	bMarkAsNoSpam = true: Will remove all stored characteristics which would add to a positive spam score for this file
	*/
	ASSERT(!bRecalculateAll || bMarkAsNoSpam);

	if (!thePrefs.IsSearchSpamFilterEnabled())
		return;

	if (!m_bSpamFilterLoaded)
		LoadSpamFilter();

	int nSpamScore = 0;
	CString strDebug;
	bool bSureNegative = false;
	int nDbgFileHash, nDbgStrings, nDbgSize, nDbgServer, nDbgSources, nDbgHeuristic, nDbgOnlySpamServer;
	nDbgFileHash = nDbgStrings = nDbgSize = nDbgServer = nDbgSources = nDbgHeuristic = nDbgOnlySpamServer = 0;

	// 1- file hash
	bool bSpam = false;
	if (m_mapKnownSpamHashes.Lookup(CSKey(pSearchFile->GetFileHash()), bSpam)) {
		if (!bMarkAsNoSpam && bSpam) {
			nSpamScore += SPAM_FILEHASH_HIT;
			nDbgFileHash = SPAM_FILEHASH_HIT;
		} else if (bSpam)
			m_mapKnownSpamHashes.RemoveKey(CSKey(pSearchFile->GetFileHash()));
		else
			bSureNegative = true;
	}
	CSearchFile *pParent = NULL;
	if (pSearchFile->GetListParent() != NULL)
		pParent = pSearchFile->GetListParent();
	else if (pSearchFile->GetListChildCount() > 0)
		pParent = pSearchFile;
	CSearchFile *pTempFile = (pSearchFile->GetListParent() != NULL) ? pSearchFile->GetListParent() : pSearchFile;

	if (!bSureNegative && bMarkAsNoSpam)
		m_mapKnownSpamHashes.SetAt(CSKey(pSearchFile->GetFileHash()), false);
#ifndef _DEBUG
	else if (bSureNegative && !bMarkAsNoSpam) {
#endif
		// 2-3 FileNames
		// consider also filenames of children / parents / siblings and take the highest rating

		uint32 nHighestRating;
		if (pParent != NULL) {
			nHighestRating = GetSpamFilenameRatings(pParent, bMarkAsNoSpam);
			SearchList *list = GetSearchListForID(pParent->GetSearchID());
			for (POSITION pos = list->GetHeadPosition(); pos != NULL;) {
				const CSearchFile *pCurFile = list->GetNext(pos);
				if (pCurFile->GetListParent() == pParent) {
					uint32 nRating = GetSpamFilenameRatings(pCurFile, bMarkAsNoSpam);
					nHighestRating = max(nHighestRating, nRating);
				}
			}
		} else
			nHighestRating = GetSpamFilenameRatings(pSearchFile, bMarkAsNoSpam);
		nSpamScore += nHighestRating;
		nDbgStrings = nHighestRating;

		//4 - Sizes
		for (INT_PTR i = m_aui64KnownSpamSizes.GetCount(); --i >= 0;) {
			uint64 fsize = (uint64)pSearchFile->GetFileSize();
			if (fsize != 0 && _abs64(fsize - m_aui64KnownSpamSizes[i]) < 5242880
				&& ((_abs64(fsize - m_aui64KnownSpamSizes[i]) * 100) / fsize) < 5)
			{
				if (!bMarkAsNoSpam) {
					nSpamScore += SPAM_SIMILARSIZE_HIT;
					nDbgSize = SPAM_SIMILARSIZE_HIT;
					break;
				}
				m_aui64KnownSpamSizes.RemoveAt(i);
			}
		}
		if (!bIsClientFile) { // only to skip some useless calculations
			const CSimpleArray<CSearchFile::SServer> &aservers = pTempFile->GetServers();
			//5 Servers
			for (int i = 0; i != aservers.GetSize(); ++i) {
				bool bFound = false;
				if (aservers[i].m_nIP != 0 && aservers[i].m_bUDPAnswer && m_mapKnownSpamServerIPs.Lookup(aservers[i].m_nIP, bFound)) {
					if (!bMarkAsNoSpam) {
						strDebug.AppendFormat(_T(" (Serverhit: %s)"), (LPCTSTR)ipstr(aservers[i].m_nIP));
						if (pSearchFile->GetServers().GetSize() == 1 && m_mapKnownSpamServerIPs.GetCount() <= 10) {
							// source only from one server
							nSpamScore += SPAM_UDPSERVERRES_HIT;
							nDbgServer = SPAM_UDPSERVERRES_HIT;
						} else if (pSearchFile->GetServers().GetSize() == 1) {
							// source only from one server but the users seems to be a bit careless with the mark as spam option
							// and has already added a lot UDP servers. To avoid false positives, we give a lower rating
							nSpamScore += SPAM_UDPSERVERRES_NEARHIT;
							nDbgServer = SPAM_UDPSERVERRES_NEARHIT;
						} else {
							// file was given by more than one server, lowest spam rating for server hits
							nSpamScore += SPAM_UDPSERVERRES_FARHIT;
							nDbgServer = SPAM_UDPSERVERRES_FARHIT;
						}
						break;
					}
					m_mapKnownSpamServerIPs.RemoveKey(aservers[i].m_nIP);
				}
			}

			// partial heuristics - only udp spam servers have this file
			// at least one server as origin which is not rated for spam or UDP
			// or not a result from a server at all
			bool bNormalServerWithoutCurrentPresent = (aservers.GetSize() == 0);
			bool bNormalServerPresent = bNormalServerWithoutCurrentPresent;
			for (int i = 0; i < aservers.GetSize(); ++i) {
				UDPServerRecord *pRecord = NULL;
				if (!bMarkAsNoSpam && aservers[i].m_bUDPAnswer && m_aUDPServerRecords.Lookup(aservers[i].m_nIP, pRecord) && pRecord != NULL) {
					ASSERT(pRecord->m_nResults >= pRecord->m_nSpamResults);
					int nRatio;
					if (pRecord->m_nResults >= pRecord->m_nSpamResults && pRecord->m_nResults != 0) {
						nRatio = (pRecord->m_nSpamResults * 100) / pRecord->m_nResults;
						if (nRatio < 50) {
							bNormalServerWithoutCurrentPresent |= (dwFromUDPServerIP != aservers[i].m_nIP);
							bNormalServerPresent = true;
						}
					}  else
						nRatio = 100;
				} else if (!aservers[i].m_bUDPAnswer) {
					bNormalServerWithoutCurrentPresent = true;
					bNormalServerPresent = true;
					break;
				}
				if (!bMarkAsNoSpam)
					ASSERT(pRecord != NULL);
			}
			if (!bNormalServerPresent && !bMarkAsNoSpam) {
				nDbgOnlySpamServer = SPAM_ONLYUDPSPAMSERVERS_HIT;
				nSpamScore += SPAM_ONLYUDPSPAMSERVERS_HIT;
				strDebug += _T(" (AllSpamServers)");
			} else if (!bNormalServerWithoutCurrentPresent && !bMarkAsNoSpam)
				strDebug += _T(" (AllSpamServersWoCurrent)");


			// 7 Heuristic (UDP Results)
			uint32 nResponses;
			if (!m_ReceivedUDPAnswersCount.Lookup(pTempFile->GetSearchID(), nResponses))
				nResponses = 0;
			uint32 nRequests;
			if (!m_RequestedUDPAnswersCount.Lookup(pTempFile->GetSearchID(), nRequests))
				nRequests = 0;
			if (!bNormalServerWithoutCurrentPresent
				&& (nResponses >= 3 || nRequests >= 5) && pTempFile->GetSourceCount() > 100)
			{
				// check if the one of the files sources are in the same ip subnet as a udp server
				// which indicates that the server is advertising its own files
				bool bSourceServer = false;
				for (int i = 0; i < aservers.GetSize(); ++i) {
					if (aservers[i].m_nIP != 0) {
						if ((aservers[i].m_nIP & 0x00FFFFFF) == (pTempFile->GetClientID() & 0x00FFFFFF)) {
							bSourceServer = true;
							strDebug.AppendFormat(_T(" (Server: %s - Source: %s Hit)"), (LPCTSTR)ipstr(aservers[i].m_nIP), (LPCTSTR)ipstr(pTempFile->GetClientID()));
							break;
						}
						for (int j = 0; j < pTempFile->GetClients().GetSize(); ++j) {
							if ((aservers[i].m_nIP & 0x00FFFFFF) == (pTempFile->GetClients()[j].m_nIP & 0x00FFFFFF)) {
								bSourceServer = true;
								strDebug.AppendFormat(_T(" (Server: %s - Source: %s Hit)"), (LPCTSTR)ipstr(aservers[i].m_nIP), (LPCTSTR)ipstr(pTempFile->GetClients()[j].m_nIP));
								break;
							}
						}
					}
				}

				if (((GetED2KFileTypeID(pTempFile->GetFileName()) == ED2KFT_PROGRAM || GetED2KFileTypeID(pTempFile->GetFileName()) == ED2KFT_ARCHIVE)
					&& (uint64)pTempFile->GetFileSize() > 102400 && (uint64)pTempFile->GetFileSize() < 10485760
					&& !bMarkAsNoSpam
					)
					|| bSourceServer)
				{
					nSpamScore += SPAM_HEURISTIC_MAXHIT;
					nDbgHeuristic = SPAM_HEURISTIC_MAXHIT;
				} else if (!bMarkAsNoSpam) {
					nSpamScore += SPAM_HEURISTIC_BASEHIT;
					nDbgHeuristic = SPAM_HEURISTIC_BASEHIT;
				}
			}
		}
		// 6 Sources
		bool bFound = false;
		if (IsValidSearchResultClientIPPort(pTempFile->GetClientID(), pTempFile->GetClientPort())
			&& !::IsLowID(pTempFile->GetClientID())
			&& m_mapKnownSpamSourcesIPs.Lookup(pTempFile->GetClientID(), bFound))
		{
			if (!bMarkAsNoSpam) {
				strDebug.AppendFormat(_T(" (Sourceshit: %s)"), (LPCTSTR)ipstr(pTempFile->GetClientID()));
				nSpamScore += SPAM_SOURCE_HIT;
				nDbgSources = SPAM_SOURCE_HIT;
			} else
				m_mapKnownSpamSourcesIPs.RemoveKey(pTempFile->GetClientID());
		} else {
			for (int i = 0; i != pTempFile->GetClients().GetSize(); ++i)
				if (pTempFile->GetClients()[i].m_nIP != 0
					&& m_mapKnownSpamSourcesIPs.Lookup(pTempFile->GetClients()[i].m_nIP, bFound))
				{
					if (!bMarkAsNoSpam) {
						strDebug.AppendFormat(_T(" (Sources: %s)"), (LPCTSTR)ipstr(pTempFile->GetClients()[i].m_nIP));
						nSpamScore += SPAM_SOURCE_HIT;
						nDbgSources = SPAM_SOURCE_HIT;
						break;
					}
					m_mapKnownSpamSourcesIPs.RemoveKey(pTempFile->GetClients()[i].m_nIP);
				}
		}
#ifndef _DEBUG
	}
#endif

	if (!bMarkAsNoSpam) {
		if (nSpamScore > 0)
			DebugLog(_T("Spamrating Result: %u. Details: Hash: %u, Name: %u, Size: %u, Server: %u, Sources: %u, Heuristic: %u, OnlySpamServers: %u. %s Filename: %s")
				, bSureNegative ? 0 : nSpamScore, nDbgFileHash, nDbgStrings, nDbgSize, nDbgServer, nDbgSources, nDbgHeuristic, nDbgOnlySpamServer, (LPCTSTR)strDebug, (LPCTSTR)pSearchFile->GetFileName());
	} else
		DebugLog(_T("Marked file as No Spam, Old Rating: %u."), pSearchFile->GetSpamRating());

	bool bOldSpamStatus = pSearchFile->IsConsideredSpam();

	pParent = NULL;
	if (pSearchFile->GetListParent() != NULL)
		pParent = pSearchFile->GetListParent();
	else if (pSearchFile->GetListChildCount() > 0)
		pParent = pSearchFile;

	if (pParent != NULL) {
		pParent->SetSpamRating(bMarkAsNoSpam ? 0 : nSpamScore);
		SearchList *list = GetSearchListForID(pParent->GetSearchID());
		for (POSITION pos = list->GetHeadPosition(); pos != NULL;) {
			CSearchFile *pCurFile = list->GetNext(pos);
			if (pCurFile->GetListParent() == pParent)
				pCurFile->SetSpamRating((bMarkAsNoSpam || bSureNegative) ? 0 : nSpamScore);
		}
	} else
		pSearchFile->SetSpamRating(bMarkAsNoSpam ? 0 : nSpamScore);

	// keep record about ratio of spam in UDP server results
	if (bOldSpamStatus != pSearchFile->IsConsideredSpam()) {
		const CSimpleArray<CSearchFile::SServer> &aservers = pTempFile->GetServers();
		for (int i = 0; i < aservers.GetSize(); ++i) {
			UDPServerRecord *pRecord;
			if (aservers[i].m_bUDPAnswer && m_aUDPServerRecords.Lookup(aservers[i].m_nIP, pRecord) && pRecord != NULL) {
				if (pSearchFile->IsConsideredSpam())
					pRecord->m_nSpamResults++;
				else {
					ASSERT(pRecord->m_nSpamResults > 0);
					pRecord->m_nSpamResults--;
				}
			}
		}
	} else if (dwFromUDPServerIP != 0 && pSearchFile->IsConsideredSpam()) {
		// files was already spam, but a new server also gave it as result, add it to his spam stats
		UDPServerRecord *pRecord = NULL;
		if (m_aUDPServerRecords.Lookup(dwFromUDPServerIP, pRecord) && pRecord != NULL)
			pRecord->m_nSpamResults++;
	}

	if (bUpdate && outputwnd != NULL)
		outputwnd->UpdateSources((pParent != NULL) ? pParent : pSearchFile);
	if (bRecalculateAll)
		RecalculateSpamRatings(pSearchFile->GetSearchID(), false, true, bUpdate);
}

uint32 CSearchList::GetSpamFilenameRatings(const CSearchFile *pSearchFile, bool bMarkAsNoSpam)
{
	for (int i = 0; i < m_astrKnownSpamNames.GetCount(); ++i) {
		if (pSearchFile->GetFileName().CompareNoCase(m_astrKnownSpamNames[i]) == 0) {
			if (!bMarkAsNoSpam)
				return (pSearchFile->GetFileName().GetLength() <= 10) ? SPAM_SMALLFULLNAME_HIT : SPAM_FULLNAME_HIT;

			m_astrKnownSpamNames.RemoveAt(i);
			--i;
		}
	}
	uint32 nResult = 0;
	if (!m_astrKnownSimilarSpamNames.IsEmpty() && !pSearchFile->GetNameWithoutKeyword().IsEmpty()) {
		const CString &cname = pSearchFile->GetNameWithoutKeyword();
		for (int i = 0; i < m_astrKnownSimilarSpamNames.GetCount(); ++i) {
			bool bRemove = false;
			if (cname == m_astrKnownSimilarSpamNames[i]) {
				if (!bMarkAsNoSpam)
					return (cname.GetLength() <= 10) ? SPAM_SMALLSIMILARNAME_HIT : SPAM_SIMILARNAME_HIT;

				bRemove = true;
			} else if (cname.GetLength() > 10
				&& (cname.GetLength() == m_astrKnownSimilarSpamNames[i].GetLength()
					|| cname.GetLength() / abs(cname.GetLength() - m_astrKnownSimilarSpamNames[i].GetLength()) >= 3))
			{
				uint32 nStringComp = LevenshteinDistance(cname, m_astrKnownSimilarSpamNames[i]);
				if (nStringComp != 0)
					nStringComp = cname.GetLength() / nStringComp;
				if (nStringComp >= 3) {
					if (!bMarkAsNoSpam) {
						if (nStringComp >= 6)
							nResult = SPAM_SIMILARNAME_NEARHIT;
						else
							nResult = max(nResult, SPAM_SIMILARNAME_FARHIT);
					} else
						bRemove = true;
				}
			}
			if (bRemove) {
				m_astrKnownSimilarSpamNames.RemoveAt(i);
				--i;
			}
		}
	}
	return nResult;
}


SearchList* CSearchList::GetSearchListForID(uint32 nSearchID)
{
	for (POSITION pos = m_listFileLists.GetHeadPosition(); pos != NULL;) {
		SearchListsStruct *list = m_listFileLists.GetNext(pos);
		if (list->m_nSearchID == nSearchID)
			return &list->m_listSearchFiles;
	}
	SearchListsStruct *list = new SearchListsStruct;
	list->m_nSearchID = nSearchID;
	m_listFileLists.AddTail(list);
	return &list->m_listSearchFiles;
}

void CSearchList::SentUDPRequestNotification(uint32 nSearchID, uint32 dwServerIP)
{
	if (nSearchID == m_nCurED2KSearchID)
		m_RequestedUDPAnswersCount.SetAt(nSearchID, (uint32)m_aCurED2KSentRequestsIPs.Add(dwServerIP) + 1);
	else
		ASSERT(0);

}

void CSearchList::MarkFileAsSpam(CSearchFile *pSpamFile, bool bRecalculateAll, bool bUpdate)
{
	if (!m_bSpamFilterLoaded)
		LoadSpamFilter();

	m_astrKnownSpamNames.Add(pSpamFile->GetFileName());
	m_astrKnownSimilarSpamNames.Add(pSpamFile->GetNameWithoutKeyword());
	m_mapKnownSpamHashes.SetAt(CSKey(pSpamFile->GetFileHash()), true);
	m_aui64KnownSpamSizes.Add((uint64)pSpamFile->GetFileSize());

	if (IsValidSearchResultClientIPPort(pSpamFile->GetClientID(), pSpamFile->GetClientPort())
		&& !::IsLowID(pSpamFile->GetClientID()))
	{
		m_mapKnownSpamSourcesIPs.SetAt(pSpamFile->GetClientID(), true);
	}
	for (int i = 0; i < pSpamFile->GetClients().GetSize(); ++i)
		if (pSpamFile->GetClients()[i].m_nIP != 0)
			m_mapKnownSpamSourcesIPs.SetAt(pSpamFile->GetClients()[i].m_nIP, true);

	for (int i = 0; i < pSpamFile->GetServers().GetSize(); ++i)
		if (pSpamFile->GetServers()[i].m_nIP != 0 && pSpamFile->GetServers()[i].m_bUDPAnswer)
			m_mapKnownSpamServerIPs.SetAt(pSpamFile->GetServers()[i].m_nIP, true);


	if (bRecalculateAll)
		RecalculateSpamRatings(pSpamFile->GetSearchID(), true, false, bUpdate);
	else
		DoSpamRating(pSpamFile);

	if (bUpdate && outputwnd != NULL)
		outputwnd->UpdateSources(pSpamFile);
}

void CSearchList::RecalculateSpamRatings(uint32 nSearchID, bool bExpectHigher, bool bExpectLower, bool bUpdate)
{
	ASSERT(!(bExpectHigher && bExpectLower));
	ASSERT(m_bSpamFilterLoaded);

	SearchList *list = GetSearchListForID(nSearchID);
	for (POSITION pos = list->GetHeadPosition(); pos != NULL;) {
		CSearchFile *pCurFile = list->GetNext(pos);
		// check only parents and only if we expect a status change
		if (pCurFile->GetListParent() == NULL && !(pCurFile->IsConsideredSpam() && bExpectHigher)
			&& !(!pCurFile->IsConsideredSpam() && bExpectLower))
		{
			DoSpamRating(pCurFile, false, false, false, false);
			if (bUpdate && outputwnd != NULL)
				outputwnd->UpdateSources(pCurFile);
		}
	}
}

void CSearchList::LoadSpamFilter()
{
	m_astrKnownSpamNames.RemoveAll();
	m_astrKnownSimilarSpamNames.RemoveAll();
	m_mapKnownSpamServerIPs.RemoveAll();
	m_mapKnownSpamSourcesIPs.RemoveAll();
	m_mapKnownSpamHashes.RemoveAll();
	m_aui64KnownSpamSizes.RemoveAll();
	unsigned nDbgFileHashPos = 0;

	m_bSpamFilterLoaded = true;

	const CString &fullpath(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + SPAMFILTER_FILENAME);
	CSafeBufferedFile file;
	CFileException fexp;
	if (!file.Open(fullpath, CFile::modeRead | CFile::osSequentialScan | CFile::typeBinary | CFile::shareDenyWrite, &fexp)) {
		if (fexp.m_cause != CFileException::fileNotFound) {
			CString strError(_T("Failed to load ") SPAMFILTER_FILENAME _T(" file"));
			TCHAR szError[MAX_CFEXP_ERRORMSG];
			if (GetExceptionMessage(fexp, szError, _countof(szError)))
				strError.AppendFormat(_T(" - %s"), szError);
			DebugLogError(_T("%s"), (LPCTSTR)strError);
		}
		return;
	}
	setvbuf(file.m_pStream, NULL, _IOFBF, 16384);

	try {
		uint8 header = file.ReadUInt8();
		if (header != MET_HEADER_I64TAGS) {
			file.Close();
			DebugLogError(_T("Failed to load searchspam.met, invalid first byte"));
			return;
		}

		for (uint32 i = file.ReadUInt32(); i > 0; --i) { //number of records
			CTag tag(&file, false);
			switch (tag.GetNameID()) {
			case SP_FILEHASHSPAM:
				ASSERT(tag.IsHash());
				if (tag.IsHash())
					m_mapKnownSpamHashes.SetAt(CSKey(tag.GetHash()), true);
				break;
			case SP_FILEHASHNOSPAM:
				ASSERT(tag.IsHash());
				if (tag.IsHash()) {
					m_mapKnownSpamHashes.SetAt(CSKey(tag.GetHash()), false);
					++nDbgFileHashPos;
				}
				break;
			case SP_FILEFULLNAME:
				ASSERT(tag.IsStr());
				if (tag.IsStr())
					m_astrKnownSpamNames.Add(tag.GetStr());
				break;
			case SP_FILESIMILARNAME:
				ASSERT(tag.IsStr());
				if (tag.IsStr())
					m_astrKnownSimilarSpamNames.Add(tag.GetStr());
				break;
			case SP_FILESOURCEIP:
				ASSERT(tag.IsInt());
				if (tag.IsInt())
					m_mapKnownSpamSourcesIPs.SetAt(tag.GetInt(), true);
				break;
			case SP_FILESERVERIP:
				ASSERT(tag.IsInt());
				if (tag.IsInt())
					m_mapKnownSpamServerIPs.SetAt(tag.GetInt(), true);
				break;
			case SP_FILESIZE:
				ASSERT(tag.IsInt64());
				if (tag.IsInt64())
					m_aui64KnownSpamSizes.Add(tag.GetInt64());
				break;
			case SP_UDPSERVERSPAMRATIO:
				ASSERT(tag.IsBlob() && tag.GetBlobSize() == 12);
				if (tag.IsBlob() && tag.GetBlobSize() == 12) {
					const BYTE *pBuffer = tag.GetBlob();
					UDPServerRecord *pRecord = new UDPServerRecord;
					pRecord->m_nResults = PeekUInt32(&pBuffer[4]);
					pRecord->m_nSpamResults = PeekUInt32(&pBuffer[8]);
					m_aUDPServerRecords.SetAt(PeekUInt32(&pBuffer[0]), pRecord);
					int nRatio;
					if (pRecord->m_nResults >= pRecord->m_nSpamResults && pRecord->m_nResults != 0)
						nRatio = (pRecord->m_nSpamResults * 100) / pRecord->m_nResults;
					else
						nRatio = 100;
					DEBUG_ONLY(DebugLog(_T("UDP Server Spam Record: IP: %s, Results: %u, SpamResults: %u, Ratio: %u")
						, (LPCTSTR)ipstr(PeekUInt32(&pBuffer[0])), pRecord->m_nResults, pRecord->m_nSpamResults, nRatio));
				}
				break;
			default:
				ASSERT(0);
			}
		}
		file.Close();
	} catch (CFileException *error) {
		if (error->m_cause == CFileException::endOfFile)
			DebugLogError(_T("Failed to load searchspam.met, corrupt"));
		else {
			TCHAR buffer[MAX_CFEXP_ERRORMSG];
			GetExceptionMessage(*error, buffer, _countof(buffer));
			DebugLogError(_T("Failed to load searchspam.met, %s"), buffer);
		}
		error->Delete();
		return;
	}

	DebugLog(_T("Loaded search Spam Filter. Entries - ServerIPs: %u, SourceIPs, %u, hashes: %u, PositiveHashes: %i, FileSizes: %u, FullNames: %u, SimilarNames: %u")
		, (unsigned)m_mapKnownSpamSourcesIPs.GetCount()
		, (unsigned)m_mapKnownSpamServerIPs.GetCount()
		, (unsigned)m_mapKnownSpamHashes.GetCount() - nDbgFileHashPos
		, nDbgFileHashPos
		, (unsigned)m_aui64KnownSpamSizes.GetCount()
		, (unsigned)m_astrKnownSpamNames.GetCount()
		, (unsigned)m_astrKnownSimilarSpamNames.GetCount());
}

void CSearchList::SaveSpamFilter()
{
	if (!m_bSpamFilterLoaded)
		return;

	const CString &fullpath(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + SPAMFILTER_FILENAME);
	CSafeBufferedFile file;
	CFileException fexp;
	if (!file.Open(fullpath, CFile::modeWrite | CFile::modeCreate | CFile::typeBinary | CFile::shareDenyWrite, &fexp)) {
		if (fexp.m_cause != CFileException::fileNotFound) {
			CString strError(_T("Failed to load ") SPAMFILTER_FILENAME _T(" file"));
			TCHAR szError[MAX_CFEXP_ERRORMSG];
			if (GetExceptionMessage(fexp, szError, _countof(szError)))
				strError.AppendFormat(_T(" - %s"), szError);
			DebugLogError(_T("%s"), (LPCTSTR)strError);
		}
		return;
	}
	setvbuf(file.m_pStream, NULL, _IOFBF, 16384);
	uint32 nCount = 0;
	try {
		file.WriteUInt8(MET_HEADER_I64TAGS);
		file.WriteUInt32(nCount);

		for (int i = 0; i < m_astrKnownSpamNames.GetCount(); ++i) {
			CTag tag(SP_FILEFULLNAME, m_astrKnownSpamNames[i]);
			tag.WriteNewEd2kTag(&file, UTF8strOptBOM);
			++nCount;
		}

		for (int i = 0; i < m_astrKnownSimilarSpamNames.GetCount(); ++i) {
			CTag tag(SP_FILESIMILARNAME, m_astrKnownSimilarSpamNames[i]);
			tag.WriteNewEd2kTag(&file, UTF8strOptBOM);
			++nCount;
		}

		for (int i = 0; i < m_aui64KnownSpamSizes.GetCount(); ++i) {
			CTag tag(SP_FILESIZE, m_aui64KnownSpamSizes[i], true);
			tag.WriteNewEd2kTag(&file);
			++nCount;
		}

		CSKey key;
		for (POSITION pos = m_mapKnownSpamHashes.GetStartPosition(); pos != NULL;) {
			bool bSpam;
			m_mapKnownSpamHashes.GetNextAssoc(pos, key, bSpam);
			if (bSpam) {
				CTag tag(SP_FILEHASHSPAM, (BYTE*)key.m_key);
				tag.WriteNewEd2kTag(&file);
			} else {
				CTag tag(SP_FILEHASHNOSPAM, (BYTE*)key.m_key);
				tag.WriteNewEd2kTag(&file);
			}
			++nCount;
		}

		for (POSITION pos = m_mapKnownSpamServerIPs.GetStartPosition(); pos != NULL;) {
			uint32 dwIP;
			bool bTmp;
			m_mapKnownSpamServerIPs.GetNextAssoc(pos, dwIP, bTmp);
			CTag tag(SP_FILESERVERIP, dwIP);
			tag.WriteNewEd2kTag(&file);
			++nCount;
		}

		for (POSITION pos = m_mapKnownSpamSourcesIPs.GetStartPosition(); pos != NULL;) {
			uint32 dwIP;
			bool bTmp;
			m_mapKnownSpamSourcesIPs.GetNextAssoc(pos, dwIP, bTmp);
			CTag tag(SP_FILESOURCEIP, dwIP);
			tag.WriteNewEd2kTag(&file);
			++nCount;
		}

		for (POSITION pos = m_aUDPServerRecords.GetStartPosition(); pos != NULL;) {
			uint32 dwIP;
			UDPServerRecord *pRecord;
			m_aUDPServerRecords.GetNextAssoc(pos, dwIP, pRecord);
			BYTE abyBuffer[12];
			PokeUInt32(&abyBuffer[0], dwIP);
			PokeUInt32(&abyBuffer[4], pRecord->m_nResults);
			PokeUInt32(&abyBuffer[8], pRecord->m_nSpamResults);
			CTag tag(SP_UDPSERVERSPAMRATIO, sizeof(abyBuffer), abyBuffer);
			tag.WriteNewEd2kTag(&file);
			++nCount;
		}

		file.Seek(1ull, CFile::begin);
		file.WriteUInt32(nCount);
		file.Close();
	} catch (CFileException *error) {
		TCHAR buffer[MAX_CFEXP_ERRORMSG];
		GetExceptionMessage(*error, buffer, _countof(buffer));
		DebugLogError(_T("Failed to save searchspam.met, %s"), buffer);
		error->Delete();
		return;
	}
	DebugLog(_T("Stored searchspam.met, wrote %u records"), nCount);
}

void CSearchList::StoreSearches()
{
	// store open searches on shutdown to restore them on the next startup
	const CString &fullpath(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + STOREDSEARCHES_FILENAME);

	CSafeBufferedFile file;
	CFileException fexp;
	if (!file.Open(fullpath, CFile::modeWrite | CFile::modeCreate | CFile::typeBinary | CFile::shareDenyWrite, &fexp)) {
		if (fexp.m_cause != CFileException::fileNotFound) {
			CString strError(_T("Failed to load ") STOREDSEARCHES_FILENAME _T(" file"));
			TCHAR szError[MAX_CFEXP_ERRORMSG];
			if (GetExceptionMessage(fexp, szError, _countof(szError)))
				strError.AppendFormat(_T(" - %s"), szError);
			DebugLogError(_T("%s"), (LPCTSTR)strError);
		}
		return;
	}
	setvbuf(file.m_pStream, NULL, _IOFBF, 16384);
	uint16 nCount = 0;
	try {
		file.WriteUInt8(MET_HEADER_I64TAGS);
		file.WriteUInt8(STOREDSEARCHES_VERSION);
		// count how many (if any) open searches we have which are GUI related
		for (POSITION pos = m_listFileLists.GetHeadPosition(); pos != NULL;) {
			SearchListsStruct *pSl = m_listFileLists.GetNext(pos);
			nCount += static_cast<uint16>(theApp.emuledlg->searchwnd->GetSearchParamsBySearchID(pSl->m_nSearchID) != NULL);
		}
		file.WriteUInt16(nCount);
		if (nCount > 0)
			for (POSITION pos = m_listFileLists.GetHeadPosition(); pos != NULL;) {
				SearchListsStruct *pSl = m_listFileLists.GetNext(pos);
				SSearchParams *pParams = theApp.emuledlg->searchwnd->GetSearchParamsBySearchID(pSl->m_nSearchID);
				if (pParams != NULL) {
					pParams->StorePartially(file);
					uint32 uCount = 0;
					for (POSITION pos2 = pSl->m_listSearchFiles.GetHeadPosition(); pos2 != NULL;)
						uCount += static_cast<uint32>(!pSl->m_listSearchFiles.GetNext(pos2)->m_flags.nowrite);

					file.WriteUInt32(uCount);
					for (POSITION pos2 = pSl->m_listSearchFiles.GetHeadPosition(); pos2 != NULL;) {
						CSearchFile *f = pSl->m_listSearchFiles.GetNext(pos2);
						if (!f->m_flags.nowrite)
							f->StoreToFile(file);
					}
				}
			}

		file.Close();
	} catch (CFileException *error) {
		TCHAR buffer[MAX_CFEXP_ERRORMSG];
		GetExceptionMessage(*error, buffer, _countof(buffer));
		DebugLogError(_T("Failed to save %s, %s"), STOREDSEARCHES_FILENAME, buffer);
		error->Delete();
		return;
	}
	DebugLog(_T("Stored %u open searches for restoring on next start"), nCount);
}

void CSearchList::LoadSearches()
{
	ASSERT(m_listFileLists.IsEmpty());
	const CString &fullpath(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + STOREDSEARCHES_FILENAME);
	CSafeBufferedFile file;
	CFileException fexp;
	if (!file.Open(fullpath, CFile::modeRead | CFile::osSequentialScan | CFile::typeBinary | CFile::shareDenyWrite, &fexp)) {
		if (fexp.m_cause != CFileException::fileNotFound) {
			CString strError(_T("Failed to load ") STOREDSEARCHES_FILENAME _T(" file"));
			TCHAR szError[MAX_CFEXP_ERRORMSG];
			if (GetExceptionMessage(fexp, szError, _countof(szError)))
				strError.AppendFormat(_T(" - %s"), szError);
			DebugLogError(_T("%s"), (LPCTSTR)strError);
		}
		return;
	}
	setvbuf(file.m_pStream, NULL, _IOFBF, 16384);

	try {
		uint8 header = file.ReadUInt8();
		if (header != MET_HEADER_I64TAGS) {
			file.Close();
			DebugLogError(_T("Failed to load %s, invalid first byte"), STOREDSEARCHES_FILENAME);
			return;
		}
		uint8 byVersion = file.ReadUInt8();
		if (byVersion != STOREDSEARCHES_VERSION) {
			file.Close();
			return;
		}

		uint32 nID = (uint32)-1;
		for (unsigned nCount = (unsigned)file.ReadUInt16(); nCount > 0; --nCount) {
			SSearchParams *pParams = new SSearchParams(file);
			pParams->dwSearchID = ++nID; //renumber

			// create a new tab
			const CString &strResultType = pParams->strFileType;
			NewSearch(NULL, (strResultType == _T(ED2KFTSTR_PROGRAM) ? CString() : strResultType), pParams);

			bool bDeleteParams = !theApp.emuledlg->searchwnd->CreateNewTab(pParams, false);
			if (!bDeleteParams) {
				m_foundFilesCount.SetAt(pParams->dwSearchID, 0);
				m_foundSourcesCount.SetAt(pParams->dwSearchID, 0);
			} else
				ASSERT(0); //failed to create tab

			// fill the list with stored results
			for (uint32 nFileCount = file.ReadUInt32(); nFileCount-- > 0;) {
				CSearchFile *toadd = new CSearchFile(&file, true, pParams->dwSearchID, 0, 0, NULL, pParams->eType == SearchTypeKademlia);
				AddToList(toadd, pParams->bClientSharedFiles);
			}
			if (bDeleteParams)
				delete pParams;
		}
		file.Close();
		// adjust the starting values for search IDs to avoid reused IDs in loaded searches
		Kademlia::CSearchManager::SetNextSearchID(++nID);
		theApp.emuledlg->searchwnd->SetNextSearchID(0x80000000u + nID);
	} catch (CFileException *error) {
		if (error->m_cause == CFileException::endOfFile)
			DebugLogError(_T("Failed to load %s, corrupt"), STOREDSEARCHES_FILENAME);
		else {
			TCHAR buffer[MAX_CFEXP_ERRORMSG];
			GetExceptionMessage(*error, buffer, _countof(buffer));
			DebugLogError(_T("Failed to load %s, %s"), STOREDSEARCHES_FILENAME, buffer);
		}
		error->Delete();
	}
}