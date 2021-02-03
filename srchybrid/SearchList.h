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
#pragma once
#include "KnownFile.h"
#include "SearchFile.h"
#include "QArray.h"
#include "Mapkey.h"
#include "SearchParams.h"

enum ESearchType : uint8;

typedef struct
{
	CString	m_strFileName;
	CString	m_strFileType;
	CString	m_strFileHash;
	CString	m_strIndex;
	uint64	m_uFileSize;
	uint32	m_uSourceCount;
	uint32	m_dwCompleteSourceCount;
} SearchFileStruct;

typedef CTypedPtrList<CPtrList, CSearchFile*> SearchList;

typedef struct
{
	uint32 m_nSearchID;
	SearchList m_listSearchFiles;
} SearchListsStruct;

typedef struct
{
	uint32	m_nResults;
	uint32	m_nSpamResults;
} UDPServerRecord;



class CFileDataIO;
class CAbstractFile;
struct SSearchTerm;

class CSearchList
{
	friend class CSearchListCtrl;

public:
	CSearchList();
	~CSearchList();

	void	Clear();
	void	NewSearch(CSearchListCtrl *pWnd, const CString &strResultFileType, SSearchParams *pParams);
	UINT	ProcessSearchAnswer(const uchar *in_packet, uint32 size, const CUpDownClient &sender, bool *pbMoreResultsAvailable, LPCTSTR pszDirectory = NULL);
	UINT	ProcessSearchAnswer(const uchar *in_packet, uint32 size, bool bOptUTF8, uint32 nServerIP, uint16 nServerPort, bool *pbMoreResultsAvailable);
	UINT	ProcessUDPSearchAnswer(CFileDataIO &packet, bool bOptUTF8, uint32 nServerIP, uint16 nServerPort);
	UINT	GetED2KResultCount() const;
	UINT	GetResultCount(uint32 nSearchID) const;
	void	AddResultCount(uint32 nSearchID, const uchar *hash, UINT nCount, bool bSpam);
	void	SetOutputWnd(CSearchListCtrl *in_wnd)
	{
		outputwnd = in_wnd;
	}
	void	RemoveResults(uint32 nSearchID);
	void	RemoveResult(CSearchFile *todel);
	void	ShowResults(uint32 nSearchID);
	void	GetWebList(CQArray<SearchFileStruct, SearchFileStruct> *SearchFileArray, int iSortBy) const;
	void	AddFileToDownloadByHash(const uchar *hash)
	{
		AddFileToDownloadByHash(hash, 0);
	}
	void	AddFileToDownloadByHash(const uchar *hash, int cat);
	bool	AddToList(CSearchFile *toadd, bool bClientResponse = false, uint32 dwFromUDPServerIP = 0);
	CSearchFile* GetSearchFileByHash(const uchar *hash) const;
	void	KademliaSearchKeyword(uint32 searchID, const Kademlia::CUInt128 *pFileID, LPCTSTR name, uint64 size, LPCTSTR type, UINT uKadPublishInfo, CArray<CAICHHash> &raAICHHashes, CArray<uint8> &raAICHHashPopularity, SSearchTerm *pQueriedSearchTerm, UINT numProperties, ...);
	bool	AddNotes(Kademlia::CEntry *entry, const uchar *hash);
	void	SetNotesSearchStatus(const uchar *pFileHash, bool bSearchRunning);
	void	SentUDPRequestNotification(uint32 nSearchID, uint32 dwServerIP);

	void	StoreSearches();
	void	LoadSearches();

	void	DoSpamRating(CSearchFile *pSearchFile, bool bIsClientFile = false, bool bMarkAsNoSpam = false, bool bRecalculateAll = false, bool bUpdate = false, uint32 dwFromUDPServerIP = 0);
	void	MarkFileAsSpam(CSearchFile *pSpamFile, bool bRecalculateAll = false, bool bUpdate = false);
	void	MarkFileAsNotSpam(CSearchFile *pSpamFile, bool bRecalculateAll = false, bool bUpdate = false)
	{
		DoSpamRating(pSpamFile, false, true, bRecalculateAll, bUpdate);
	}
	void	RecalculateSpamRatings(uint32 nSearchID, bool bExpectHigher, bool bExpectLower, bool bUpdate);
	void	SaveSpamFilter();

	UINT GetFoundFiles(uint32 searchID) const
	{
		UINT returnVal;
		return m_foundFilesCount.Lookup(searchID, returnVal) ? returnVal : 0;
	}
protected:
	SearchList*		GetSearchListForID(uint32 nSearchID);
	uint32			GetSpamFilenameRatings(const CSearchFile *pSearchFile, bool bMarkAsNoSpam);
	void			LoadSpamFilter();



private:
	CTypedPtrList<CPtrList, SearchListsStruct*> m_listFileLists;
	CMap<uint32, uint32, UINT, UINT> m_foundFilesCount;
	CMap<uint32, uint32, UINT, UINT> m_foundSourcesCount;
	CMap<uint32, uint32, UINT, UINT> m_ReceivedUDPAnswersCount;
	CMap<uint32, uint32, UINT, UINT> m_RequestedUDPAnswersCount;
	CSearchListCtrl *outputwnd;
	CString			m_strResultFileType;

	uint32			m_nCurED2KSearchID;

	// spam filter
	CStringArray							m_astrSpamCheckCurSearchExp;
	CStringArray							m_astrKnownSpamNames;
	CStringArray							m_astrKnownSimilarSpamNames;
	CMap<uint32, uint32, bool, bool>		m_mapKnownSpamServerIPs;
	CMap<uint32, uint32, bool, bool>		m_mapKnownSpamSourcesIPs;
	CMap<CSKey, const CSKey&, bool, bool>	m_mapKnownSpamHashes;
	CArray<uint64>							m_aui64KnownSpamSizes;
	CArray<uint32, uint32>					m_aCurED2KSentRequestsIPs;
	CArray<uint32, uint32>					m_aCurED2KSentReceivedIPs;
	CMap<uint32, uint32, UDPServerRecord*, UDPServerRecord*>	m_aUDPServerRecords;
	bool									m_bSpamFilterLoaded;
};