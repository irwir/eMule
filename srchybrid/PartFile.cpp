//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
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
#include <sys/stat.h>
#include <io.h>
#include <winioctl.h>
#ifdef _DEBUG
#include "DebugHelpers.h"
#endif
#include "emule.h"
#include "PartFile.h"
#include "UpDownClient.h"
#include "UrlClient.h"
#include "ED2KLink.h"
#include "Preview.h"
#include "ArchiveRecovery.h"
#include "SearchFile.h"
#include "Kademlia/Kademlia/Kademlia.h"
#include "kademlia/kademlia/search.h"
#include "kademlia/kademlia/SearchManager.h"
#include "kademlia/kademlia/Entry.h"
#include "DownloadQueue.h"
#include "IPFilter.h"
#include "Packets.h"
#include "Preferences.h"
#include "SafeFile.h"
#include "SharedFileList.h"
#include "ListenSocket.h"
#include "ServerConnect.h"
#include "Server.h"
#include "KnownFileList.h"
#include "emuledlg.h"
#include "TransferDlg.h"
#include "TaskbarNotifier.h"
#include "ClientList.h"
#include "shahashset.h"
#include "Log.h"
#include "Collection.h"
#include "CollectionViewDialog.h"
#include "uploaddiskiothread.h"
#include "PartFileWriteThread.h"
#ifndef XP_BUILD
#include <urlmon.h>
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


// Barry - use this constant for both places
#define PROGRESS_HEIGHT 3

CBarShader CPartFile::s_LoadBar(PROGRESS_HEIGHT); // Barry - was 5
CBarShader CPartFile::s_ChunkBar(16);

IMPLEMENT_DYNAMIC(CPartFile, CKnownFile)

CPartFile::CPartFile(UINT cat)
{
	Init();
	m_category = cat;
}

CPartFile::CPartFile(CSearchFile *searchresult, UINT cat)
{
	Init();

	const CTypedPtrList<CPtrList, Kademlia::CEntry*> &list = searchresult->getNotes();
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;)
		m_kadNotes.AddTail(list.GetNext(pos)->Copy());
	CPartFile::UpdateFileRatingCommentAvail();

	m_FileIdentifier.SetMD4Hash(searchresult->GetFileHash());
	const CFileIdentifier &fileid = searchresult->GetFileIdentifierC();
	if (fileid.HasAICHHash()) {
		m_FileIdentifier.SetAICHHash(fileid.GetAICHHash());
		m_pAICHRecoveryHashSet->SetMasterHash(fileid.GetAICHHash(), AICH_VERIFIED);
	}

	for (INT_PTR i = 0; i < searchresult->m_taglist.GetCount(); ++i) {
		const CTag *pTag = searchresult->m_taglist[i];
		switch (pTag->GetNameID()) {
		case FT_FILENAME:
			ASSERT(pTag->IsStr());
			if (pTag->IsStr() && GetFileName().IsEmpty())
				CPartFile::SetFileName(pTag->GetStr(), true, true);
			break;
		case FT_FILESIZE:
			ASSERT(pTag->IsInt64(true));
			if (pTag->IsInt64(true))
				CPartFile::SetFileSize(pTag->GetInt64());
			break;
		default:
			{
				bool bTagAdded = false;
				if (pTag->GetNameID() != 0 && !pTag->HasName() && (pTag->IsStr() || pTag->IsInt())) {
					static const struct
					{
						uint8	nName;
						uint8	nType;
					} _aMetaTags[] =
					{
						{ FT_MEDIA_ARTIST,  TAGTYPE_STRING },
						{ FT_MEDIA_ALBUM,   TAGTYPE_STRING },
						{ FT_MEDIA_TITLE,   TAGTYPE_STRING },
						{ FT_MEDIA_LENGTH,  TAGTYPE_UINT32 },
						{ FT_MEDIA_BITRATE, TAGTYPE_UINT32 },
						{ FT_MEDIA_CODEC,   TAGTYPE_STRING },
						{ FT_FILETYPE,		TAGTYPE_STRING },
						{ FT_FILEFORMAT,	TAGTYPE_STRING }
					};
					for (unsigned t = 0; t < _countof(_aMetaTags); ++t) {
						if (pTag->GetType() == _aMetaTags[t].nType && pTag->GetNameID() == _aMetaTags[t].nName) {
							// skip string tags with empty string values
							if (pTag->IsStr() && pTag->GetStr().IsEmpty())
								break;

							// skip integer tags with zero values
							if (pTag->IsInt() && pTag->GetInt() == 0)
								break;

							TRACE(_T("CPartFile::CPartFile(CSearchFile*): added tag %s\n"), (LPCTSTR)pTag->GetFullInfo(DbgGetFileMetaTagName));
							CTag *newtag = new CTag(*pTag);
							m_taglist.Add(newtag);
							bTagAdded = true;
							break;
						}
					}
				}

				if (!bTagAdded)
					TRACE(_T("CPartFile::CPartFile(CSearchFile*): ignored tag %s\n"), (LPCTSTR)pTag->GetFullInfo(DbgGetFileMetaTagName));
			}
		}
	}
	CreatePartFile(cat);
	m_category = cat;
}

CPartFile::CPartFile(const CString &edonkeylink, UINT cat)
{
	CED2KLink *pLink = NULL;
	try {
		pLink = CED2KLink::CreateLinkFromUrl(edonkeylink);
		ASSERT(pLink);
		CED2KFileLink *pFileLink = pLink->GetFileLink();
		if (!pFileLink)
			throw GetResString(IDS_ERR_NOTAFILELINK);
		InitializeFromLink(*pFileLink, cat);
	} catch (const CString &error) {
		CString strMsg;
		strMsg.Format(GetResString(IDS_ERR_INVALIDLINK), (LPCTSTR)error);
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_LINKERROR), (LPCTSTR)strMsg);
		SetStatus(PS_ERROR);
	}
	delete pLink;
}

void CPartFile::InitializeFromLink(const CED2KFileLink &fileLink, UINT cat)
{
	Init();

	CPartFile::SetFileName(fileLink.GetName(), true, true);
	CPartFile::SetFileSize(fileLink.GetSize());
	m_FileIdentifier.SetMD4Hash(fileLink.GetHashKey());
	if (fileLink.HasValidAICHHash()) {
		m_FileIdentifier.SetAICHHash(fileLink.GetAICHHash());
		m_pAICHRecoveryHashSet->SetMasterHash(fileLink.GetAICHHash(), AICH_VERIFIED);
	}
	if (!theApp.downloadqueue->IsFileExisting(m_FileIdentifier.GetMD4Hash())) {
		if (fileLink.m_hashset && fileLink.m_hashset->GetLength() > 0) {
			try {
				if (!m_FileIdentifier.LoadMD4HashsetFromFile(*fileLink.m_hashset, true)) {
					ASSERT(m_FileIdentifier.GetRawMD4HashSet().IsEmpty());
					AddDebugLogLine(false, _T("eD2K link \"%s\" specified with invalid hashset"), fileLink.GetName());
				} else
					m_bMD4HashsetNeeded = false;
			} catch (CFileException *ex) {
				AddDebugLogLine(false, _T("Error: Failed to process hashset for eD2K link \"%s\"%s"), fileLink.GetName(), (LPCTSTR)CExceptionStrDash(*ex));
				ex->Delete();
			}
		}
		CreatePartFile(cat);
		m_category = cat;
	} else
		SetStatus(PS_ERROR);
}

CPartFile::CPartFile(const CED2KFileLink &fileLink, UINT cat)
{
	try {
		InitializeFromLink(fileLink, cat);
	} catch (const CString &error) {
		CString strMsg;
		strMsg.Format(GetResString(IDS_ERR_INVALIDLINK), (LPCTSTR)error);
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_LINKERROR), (LPCTSTR)strMsg);
		SetStatus(PS_ERROR);
	}
}

void CPartFile::Init()
{
	m_uRating = 0;

	m_tUtcLastModified = (time_t)-1;
	m_tCompleteSourcesTime = 0; //time(NULL);
	m_nCompleteSourcesCount = 0;
	m_nCompleteSourcesCountLo = 0;
	m_nCompleteSourcesCountHi = 0;

	m_DeadSourceList.Init(false);
	lastseencomplete = 0;
	m_hWrite = INVALID_HANDLE_VALUE;
	m_iWrites = 0;
	m_LastSearchTime = 0;
	m_LastSearchTimeKad = 0;
	memset(src_stats, 0, sizeof src_stats);
	memset(net_stats, 0, sizeof net_stats);
	m_TotalSearchesKad = 0;
	m_bPreviewing = false;
	m_bRecoveringArchive = false;
	m_bLocalSrcReqQueued = false;
	srcarevisible = false;
	m_bMD4HashsetNeeded = true;

	m_pAICHRecoveryHashSet = new CAICHRecoveryHashSet(this);
	m_completedsize = 0ull;
	m_uTransferred = 0;
	m_uCorruptionLoss = 0;
	m_uCompressionGain = 0;
	m_nTotalBufferData = 0;
	m_iLastPausePurge = time(NULL);
	m_tActivated = 0;
	m_nDlActiveTime = 0;
	m_tLastModified = (time_t)-1;
	m_tCreated = 0;
	m_uFileOpProgress = 0;
	lastSwapForSourceExchangeTick = m_lastpurgetime = ::GetTickCount();
	m_lastRefreshedDLDisplay = 0;
	m_nLastBufferFlushTime = 0;
	m_nNextMetFlushTime = 0;
	m_nFileFlushTime = 0; //nothing to flush
	m_dwFileAttributes = 0;
	m_random_update_wait = (DWORD)(rand() % SEC2MS(1));
	memset(m_anStates, 0, sizeof m_anStates);
	m_category = 0;
	m_uMaxSources = 0;
	availablePartsCount = 0;
	m_ClientSrcAnswered = 0;
	m_LastNoNeededCheck = 0;
	m_uPartsSavedDueICH = 0;
	m_datarate = 0;
	m_percentcompleted = 0;
	status = PS_EMPTY;
	m_eFileOp = PFOP_NONE;
	m_refresh = 0;
	m_iDownPriority = m_iDownPriority ? PR_HIGH : PR_NORMAL;
	m_paused = false;
	m_stopped = false;
	m_bPauseOnPreview = false;
	m_insufficient = false;
	m_bCompletionError = false;
	m_bAICHPartHashsetNeeded = true;
	m_bAutoDownPriority = thePrefs.GetNewAutoDown();
	m_bDelayDelete = false;
	m_bpreviewprio = false;
	m_bUpdateMet = false;
}

CPartFile::~CPartFile()
{
	// Barry - Ensure all buffered data is written
	if ((HANDLE)m_hpartfile != INVALID_HANDLE_VALUE) {
		// commit file and directory entry
		FlushBuffer(false, true);
		CPartFileWriteThread::RemFile(this);
		m_hpartfile.Close();
		// Update met file (with the current directory entry)
		SavePartFile();
	} else
		CPartFileWriteThread::RemFile(this);

	while (!m_BufferedData_list.IsEmpty()) {
		const PartFileBufferedData *item = m_BufferedData_list.RemoveHead();
		delete[] item->data;
		delete item;
	}
	delete m_pAICHRecoveryHashSet;
}

#ifdef _DEBUG
void CPartFile::AssertValid() const
{
	CKnownFile::AssertValid();

	srclist.AssertValid();
	A4AFsrclist.AssertValid();
	(void)lastseencomplete;
	m_hpartfile.AssertValid();
	m_FileCompleteMutex.AssertValid();
	(void)m_LastSearchTime;
	(void)m_LastSearchTimeKad;
	(void)src_stats;
	(void)net_stats;
	(void)m_TotalSearchesKad;
	CHECK_BOOL(m_bPreviewing);
	CHECK_BOOL(m_bRecoveringArchive);
	CHECK_BOOL(m_bLocalSrcReqQueued);
	CHECK_BOOL(srcarevisible);
	CHECK_BOOL(m_bMD4HashsetNeeded);
	(void)s_LoadBar;
	(void)s_ChunkBar;
	m_gaplist.AssertValid();
	requestedblocks_list.AssertValid();
	m_BufferedData_list.AssertValid();
	m_SrcPartFrequency.AssertValid();
	corrupted_list.AssertValid();
	m_aChangedPart.AssertValid();
	m_downloadingSourceList.AssertValid();
	(void)m_fullname;
	(void)m_partmetfilename;
	ASSERT(m_completedsize <= (uint64)m_nFileSize);
	(void)m_uTransferred;
	(void)m_uCorruptionLoss;
	(void)m_uCompressionGain;
	(void)m_nTotalBufferData;
	(void)m_iLastPausePurge;
	(void)m_lastRefreshedDLDisplay;
	(void)m_nLastBufferFlushTime;
	(void)m_dwFileAttributes;
	(void)m_anStates;
	(void)m_category;
	(void)availablePartsCount;
	(void)m_ClientSrcAnswered;
	(void)m_lastpurgetime;
	(void)m_LastNoNeededCheck;
	(void)m_uPartsSavedDueICH;
	(void)m_datarate;
	ASSERT(m_percentcompleted >= 0.0F && m_percentcompleted <= 100.0F);
	ASSERT(status == PS_READY || status == PS_EMPTY || status == PS_WAITINGFORHASH || status == PS_ERROR || status == PS_COMPLETING || status == PS_COMPLETE);
	(void)m_refresh;
	ASSERT(m_iDownPriority == PR_LOW || m_iDownPriority == PR_NORMAL || m_iDownPriority == PR_HIGH);
	CHECK_BOOL(m_paused);
	CHECK_BOOL(m_stopped);
	CHECK_BOOL(m_insufficient);
	CHECK_BOOL(m_bCompletionError);
	CHECK_BOOL(m_bAICHPartHashsetNeeded);
	CHECK_BOOL(m_bAutoDownPriority);
}

void CPartFile::Dump(CDumpContext &dc) const
{
	CKnownFile::Dump(dc);
}
#endif

void CPartFile::CreatePartFile(UINT cat)
{
	if ((uint64)m_nFileSize > MAX_EMULE_FILE_SIZE) {
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_CREATEPARTFILE));
		SetStatus(PS_ERROR);
		return;
	}

	// decide which temp folder to use
	const CString &tempdirtouse(theApp.downloadqueue->GetOptimalTempDir(cat, m_nFileSize));

	// use the lowest free part file number for a file name (InterCeptor)
	CString filename;
	int i = 0;
	do
		filename.Format(_T("%s%03i.part"), (LPCTSTR)tempdirtouse, ++i);
	while (::PathFileExists(filename));
	SetPath(tempdirtouse);
	m_partmetfilename.Format(_T("%03i.part.met"), i);
	m_fullname.Format(_T("%s%s"), (LPCTSTR)tempdirtouse, (LPCTSTR)m_partmetfilename);
	const CString &partfull(RemoveFileExtension(m_fullname));
	SetFilePath(partfull);

	if (!m_hpartfile.Open(partfull, CFile::modeCreate | CFile::modeReadWrite | CFile::shareDenyNone | CFile::osSequentialScan)) {
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_CREATEPARTFILE));
		SetStatus(PS_ERROR);
		return;
	}

	CTag *partnametag = new CTag(FT_PARTFILENAME, RemoveFileExtension(m_partmetfilename));
	m_taglist.Add(partnametag);

	AddGap(0, (uint64)m_nFileSize - 1);

	if (thePrefs.GetSparsePartFiles()) {
		DWORD dwReturnedBytes;
		if (!DeviceIoControl((HANDLE)m_hpartfile, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &dwReturnedBytes, NULL)) {
			// Errors:
			// ERROR_INVALID_FUNCTION	returned by WinXP when attempting to create a sparse file on a FAT32 partition
			DWORD dwError = ::GetLastError();
			if (dwError != ERROR_INVALID_FUNCTION && thePrefs.GetVerboseLogPriority() <= DLP_VERYLOW)
				DebugLogError(_T("Failed to apply NTFS sparse file attribute to file \"%s\" - %s"), (LPCTSTR)partfull, (LPCTSTR)GetErrorMessage(dwError, 1));
		}
	}

	FILETIME ft_ctime, ft_mtime;
	if (GetFileTime((HANDLE)m_hpartfile, &ft_ctime, (LPFILETIME)NULL, &ft_mtime)) {
		m_tCreated = (time_t)FileTimeToUnixTime(ft_ctime);
		m_tLastModified = (time_t)FileTimeToUnixTime(ft_mtime);
		if (m_tLastModified - m_tCreated > 1) { //tunnelling!
			m_tCreated = m_tLastModified;
			//fix creation time
			VERIFY(SetFileTime((HANDLE)m_hpartfile, &ft_mtime, (LPFILETIME)NULL, (LPFILETIME)NULL));
		}
	} else {
		m_tCreated = m_tLastModified = time(NULL);
		AddDebugLogLine(false, _T("Failed to get file date for \"%s\" - %s"), (LPCTSTR)partfull, _tcserror(errno));
	}

	m_dwFileAttributes = ::GetFileAttributes(partfull);
	if (m_dwFileAttributes == INVALID_FILE_ATTRIBUTES)
		m_dwFileAttributes = 0;

	if (m_FileIdentifier.GetTheoreticalMD4PartHashCount() == 0)
		m_bMD4HashsetNeeded = false;
	if (m_FileIdentifier.GetTheoreticalAICHPartHashCount() == 0)
		m_bAICHPartHashsetNeeded = false;

	m_SrcPartFrequency.SetSize(GetPartCount());
	if (GetPartCount())
		memset(&m_SrcPartFrequency[0], 0, GetPartCount() * sizeof m_SrcPartFrequency[0]);
	m_paused = false;

	if (thePrefs.AutoFilenameCleanup())
		SetFileName(CleanupFilename(GetFileName()));

	SavePartFile();
	m_CorruptionBlackBox.Init(m_nFileSize);
	SetActive(theApp.IsConnected());
}

/*
* David: Lets try to import a Shareaza download...
*
* The first part to get filename size and hash is easy
* the second part to get the hashset and the gap List
* is much more complicated.
*
* We could parse the whole *.sd file but I chose another tricky way:
* To find the hashset we will search for the ed2k hash,
* it is repeated on the begin of the hashset
* To get the gap list we will process analog
* but now we will search for the file size.
*
*
* The *.sd file format for version 32
* [S][D][L] <-- File ID
* [20][0][0][0] <-- Version
* [FF][FE][FF][BYTE]NAME <-- len;Name
* [QWORD] <-- Size
* [BYTE][0][0][0]SHA(20)[BYTE][0][0][0] <-- SHA Hash
* [BYTE][0][0][0]TIGER(24)[BYTE][0][0][0] <-- TIGER Hash
* [BYTE][0][0][0]MD5(16)[BYTE][0][0][0] <-- MD4 Hash
* [BYTE][0][0][0]ED2K(16)[BYTE][0][0][0] <-- ED2K Hash
* [...] <-- Saved Sources
* [QWORD][QWORD][DWORD]GAP(QWORD:QWORD)<-- Gap List: Total;Left;count;gap1(begin:length),gap2,Gap3,...
* [...] <-- Bittorent Info
* [...] <-- Tiger Tree
* [DWORD]ED2K(16)HASH1(16)HASH2(16)... <-- ED2K Hash Set: count;ed2k hash;hash1,hash2,hash3,...
* [...] <-- Comments
*/
EPartFileLoadResult CPartFile::ImportShareazaTempfile(LPCTSTR in_directory, LPCTSTR in_filename, EPartFileFormat *pOutCheckFileFormat)
{
	CString fullname;
	fullname.Format(_T("%s%s"), in_directory, in_filename);

	// open the file
	CFile sdFile;
	CFileException fex;
	if (!sdFile.Open(fullname, CFile::modeRead | CFile::osSequentialScan | CFile::typeBinary | CFile::shareDenyWrite, &fex)) {
		CString s;
		s.Format(GetResString(IDS_ERR_OPENMET), in_filename, _T(""));
		LogError(LOG_STATUSBAR, _T("%s%s"), (LPCTSTR)s, (LPCTSTR)CExceptionStrDash(fex));
		return PLR_FAILED_METFILE_NOACCESS;
	}
	//::setvbuf(sdFile.m_pStream, NULL, _IOFBF, 16384);

	try {
		CArchive ar(&sdFile, CArchive::load);

		// Is it a valid Shareaza temp file?
		CHAR szID[3];
		ar.Read(szID, 3);
		if (memcmp(szID, "SDL", 3) != 0) {
			ar.Close();
			if (pOutCheckFileFormat != NULL)
				*pOutCheckFileFormat = PMT_UNKNOWN;
			return PLR_FAILED_OTHER;
		}

		// Get the version
		int nVersion;
		ar >> nVersion;

		// Get the File Name
		CString sRemoteName;
		ar >> sRemoteName;
		SetFileName(sRemoteName);

		// Get the File Size
		unsigned __int64 lSize;
		ar >> lSize;

		SetFileSize(EMFileSize(lSize));

		// Get the ed2k hash
		BOOL bSHA1, bTiger, Trusted, bED2K = false;
		BYTE pED2K[MDX_DIGEST_SIZE];

		ar >> bSHA1;
		if (bSHA1) {
			BYTE pSHA1[20];
			ar.Read(pSHA1, sizeof pSHA1);
		}
		if (nVersion >= 31)
			ar >> Trusted;

		ar >> bTiger;
		if (bTiger) {
			BYTE pTiger[24];
			ar.Read(pTiger, sizeof pTiger);
		}
		if (nVersion >= 31)
			ar >> Trusted;

		if (nVersion >= 22) {
			BOOL bMD5;
			ar >> bMD5;
			if (bMD5) {
				BYTE pMD5[MDX_DIGEST_SIZE];
				ar.Read(pMD5, sizeof pMD5);
			}
		}
		if (nVersion >= 31)
			ar >> Trusted;

		if (nVersion >= 13)
			ar >> bED2K;
		if (bED2K)
			ar.Read(pED2K, sizeof pED2K);
		if (nVersion >= 31)
			ar >> Trusted;

		ar.Close();

		if (bED2K)
			m_FileIdentifier.SetMD4Hash(pED2K);
		else {
			Log(LOG_ERROR, GetResString(IDS_X_SHAREAZA_IMPORT_NO_HASH), in_filename);
			return PLR_FAILED_OTHER;
		}

		if (pOutCheckFileFormat != NULL) {
			*pOutCheckFileFormat = PMT_SHAREAZA;
			return PLR_CHECKSUCCESS;
		}

		// Now the tricky part
		LONGLONG basePos = sdFile.GetPosition();

		// Try to get the gap list
		if (gotostring(sdFile, (uchar*)&lSize, nVersion >= 29 ? 8 : 4)) { // search the gap list
			sdFile.Seek(sdFile.GetPosition() - (nVersion >= 29 ? 8 : 4), CFile::begin); // - file size
			CArchive ar1(&sdFile, CArchive::load);

			bool badGapList = false;

			if (nVersion >= 29) {
				uint64 nTotal, nRemaining;
				DWORD nFragments;
				ar1 >> nTotal >> nRemaining >> nFragments;

				if (nTotal >= nRemaining) {
					uint64 begin, length;
					while (nFragments--) {
						ar1 >> begin >> length;
						if (begin + length > nTotal) {
							badGapList = true;
							break;
						}
						AddGap(begin, min(begin + length - 1, lSize - 1));
					}
				} else
					badGapList = true;
			} else {
				DWORD nTotal, nRemaining;
				DWORD nFragments;
				ar1 >> nTotal >> nRemaining >> nFragments;

				if (nTotal >= nRemaining) {
					DWORD begin, length;
					while (nFragments--) {
						ar1 >> begin >> length;
						if (begin + length > nTotal) {
							badGapList = true;
							break;
						}
						AddGap(begin, min(begin + (length - 1ull), lSize - 1));
					}
				} else
					badGapList = true;
			}

			if (badGapList) {
				m_gaplist.RemoveAll();
				Log(LOG_WARNING, GetResString(IDS_X_SHAREAZA_IMPORT_GAP_LIST_CORRUPT), in_filename);
			}

			ar1.Close();
		} else {
			Log(LOG_WARNING, GetResString(IDS_X_SHAREAZA_IMPORT_NO_GAP_LIST), in_filename);
			sdFile.Seek(basePos, CFile::begin); // not found, reset start position
		}

		// Try to get the complete hashset
		if (gotostring(sdFile, m_FileIdentifier.GetMD4Hash(), 16)) { // search the hashset
			sdFile.Seek(sdFile.GetPosition() - 16 - 4, CFile::begin); // - list size - hash length
			CArchive ar2(&sdFile, CArchive::load);

			DWORD nCount;
			ar2 >> nCount;

			BYTE pMD4[MDX_DIGEST_SIZE];
			ar2.Read(pMD4, sizeof pMD4); // read the hash again

			// read the hashset
			for (DWORD i = 0; i < nCount; ++i) {
				uchar *curhash = new uchar[MDX_DIGEST_SIZE];
				ar2.Read(curhash, 16);
				m_FileIdentifier.GetRawMD4HashSet().Add(curhash);
			}

			if (m_FileIdentifier.GetAvailableMD4PartHashCount() > 1) {
				if (!m_FileIdentifier.CalculateMD4HashByHashSet(true, true))
					Log(LOG_WARNING, GetResString(IDS_X_SHAREAZA_IMPORT_HASH_SET_CORRUPT), in_filename);
			} else if (m_FileIdentifier.GetTheoreticalMD4PartHashCount() != m_FileIdentifier.GetAvailableMD4PartHashCount()) {
				Log(LOG_WARNING, GetResString(IDS_X_SHAREAZA_IMPORT_HASH_SET_CORRUPT), in_filename);
				m_FileIdentifier.DeleteMD4Hashset();
			}

			ar2.Close();
		} else {
			Log(LOG_WARNING, GetResString(IDS_X_SHAREAZA_IMPORT_NO_HASH_SET), in_filename);
			//sdFile.Seek(basePos, CFile::begin); // not found, reset start position
		}

		// Close the file
		sdFile.Close();
	} catch (CArchiveException *ex) {
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_FILEERROR), in_filename, (LPCTSTR)GetFileName(), (LPCTSTR)CExceptionStr(*ex));
		ex->Delete();
		return PLR_FAILED_OTHER;
	} catch (CFileException *ex) {
		if (ex->m_cause == CFileException::endOfFile)
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_METCORRUPT), in_filename, (LPCTSTR)GetFileName());
		else
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_FILEERROR), in_filename, (LPCTSTR)GetFileName(), (LPCTSTR)CExceptionStr(*ex));
		ex->Delete();
		return PLR_FAILED_OTHER;
#ifndef _DEBUG
	} catch (...) {
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_METCORRUPT), in_filename, (LPCTSTR)GetFileName());
		ASSERT(0);
		return PLR_FAILED_OTHER;
#endif
	}

	// The part below would be a copy of the CPartFile::LoadPartFile,
	// so it is smarter to save and reload the file instead of duplicating the whole stuff
	if (!SavePartFile())
		return PLR_FAILED_OTHER;

	m_FileIdentifier.DeleteMD4Hashset();
	m_gaplist.RemoveAll();

	return LoadPartFile(in_directory, in_filename);
}

EPartFileLoadResult CPartFile::LoadPartFile(LPCTSTR in_directory, LPCTSTR in_filename, EPartFileFormat *pOutCheckFileFormat)
{
	bool isnewstyle;
	EPartFileFormat partmettype = PMT_UNKNOWN;

	typedef CMap<UINT, UINT, Gap_Struct, Gap_Struct&> CGapMap;
	CGapMap gap_map; // Slugfiller
	m_uTransferred = 0;
	m_partmetfilename = in_filename;
	SetPath(in_directory);
	ASSERT(GetPath().Right(1) == _T("\\"));
	m_fullname.Format(_T("%s%s"), (LPCTSTR)GetPath(), (LPCTSTR)m_partmetfilename);

	// read data from part.met file
	CSafeBufferedFile metFile;
	CFileException fex;
	if (!metFile.Open(m_fullname, CFile::modeRead | CFile::osSequentialScan | CFile::typeBinary | CFile::shareDenyWrite, &fex)) {
		CString s;
		s.Format(GetResString(IDS_ERR_OPENMET), (LPCTSTR)m_partmetfilename, _T(""));
		LogError(LOG_STATUSBAR, _T("%s%s"), (LPCTSTR)s, (LPCTSTR)CExceptionStrDash(fex));
		return PLR_FAILED_METFILE_NOACCESS;
	}
	::setvbuf(metFile.m_pStream, NULL, _IOFBF, 16384);

	try {
		uint8 version = metFile.ReadUInt8();

		if (version != PARTFILE_VERSION && version != PARTFILE_SPLITTEDVERSION && version != PARTFILE_VERSION_LARGEFILE) {
			metFile.Close();
			if (version == 83)
				return ImportShareazaTempfile(in_directory, in_filename, pOutCheckFileFormat);

			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_BADMETVERSION), (LPCTSTR)m_partmetfilename, (LPCTSTR)GetFileName());
			return PLR_FAILED_METFILE_CORRUPT;
		}

		isnewstyle = (version == PARTFILE_SPLITTEDVERSION);
		partmettype = isnewstyle ? PMT_SPLITTED : PMT_DEFAULTOLD;
		if (!isnewstyle) {
			uint32 test;
			metFile.Seek(24, CFile::begin);
			metFile.Read(&test, sizeof test);

			metFile.Seek(1, CFile::begin);

			if (test == 0x01020000u) {
				isnewstyle = true;	// edonkey's so called "old part style"
				partmettype = PMT_NEWOLD;
			}
		}

		if (isnewstyle) {
			uint32 temp;
			metFile.Read(&temp, sizeof temp);

			if (temp == 0) // 0.48 partmets - different again
				m_FileIdentifier.LoadMD4HashsetFromFile(metFile, false);
			else {
				uchar gethash[MDX_DIGEST_SIZE];
				metFile.Seek(2, CFile::begin);
				LoadDateFromFile(metFile);
				metFile.Read(gethash, sizeof gethash);
				m_FileIdentifier.SetMD4Hash(gethash);
			}
		} else {
			LoadDateFromFile(metFile);
			m_FileIdentifier.LoadMD4HashsetFromFile(metFile, false);
		}

		bool bHadAICHHashSetTag = false;
		for (uint32 tagcount = metFile.ReadUInt32(); tagcount > 0; --tagcount) {
			CTag *newtag = new CTag(metFile, false);
			if (pOutCheckFileFormat == NULL || newtag->GetNameID() == FT_FILESIZE || newtag->GetNameID() == FT_FILENAME) {
				switch (newtag->GetNameID()) {
				case FT_FILENAME:
					if (!newtag->IsStr()) {
						LogError(LOG_STATUSBAR, GetResString(IDS_ERR_METCORRUPT), (LPCTSTR)m_partmetfilename, (LPCTSTR)GetFileName());
						delete newtag;
						return PLR_FAILED_METFILE_CORRUPT;
					}
					if (GetFileName().IsEmpty())
						SetFileName(newtag->GetStr());
					break;
				case FT_LASTSEENCOMPLETE:
					ASSERT(newtag->IsInt());
					if (newtag->IsInt())
						lastseencomplete = newtag->GetInt();
					break;
				case FT_FILESIZE:
					ASSERT(newtag->IsInt64(true));
					if (newtag->IsInt64(true))
						SetFileSize(newtag->GetInt64());
					break;
				case FT_TRANSFERRED:
					ASSERT(newtag->IsInt64(true));
					if (newtag->IsInt64(true))
						m_uTransferred = newtag->GetInt64();
					break;
				case FT_COMPRESSION:
					ASSERT(newtag->IsInt64(true));
					if (newtag->IsInt64(true))
						m_uCompressionGain = newtag->GetInt64();
					break;
				case FT_CORRUPTED:
					ASSERT(newtag->IsInt64());
					if (newtag->IsInt64())
						m_uCorruptionLoss = newtag->GetInt64();
					break;
				case FT_FILETYPE:
					ASSERT(newtag->IsStr());
					if (newtag->IsStr())
						SetFileType(newtag->GetStr());
					break;
				case FT_CATEGORY:
					ASSERT(newtag->IsInt());
					if (newtag->IsInt())
						m_category = newtag->GetInt();
					break;
				case FT_MAXSOURCES:
					ASSERT(newtag->IsInt());
					if (newtag->IsInt())
						m_uMaxSources = newtag->GetInt();
					break;
				case FT_DLPRIORITY:
					ASSERT(newtag->IsInt());
					if (newtag->IsInt()) {
						if (!isnewstyle) {
							m_iDownPriority = (uint8)newtag->GetInt();
							if (m_iDownPriority == PR_AUTO) {
								m_iDownPriority = PR_HIGH;
								SetAutoDownPriority(true);
							} else {
								if (m_iDownPriority != PR_LOW && m_iDownPriority != PR_NORMAL && m_iDownPriority != PR_HIGH)
									m_iDownPriority = PR_NORMAL;
								SetAutoDownPriority(false);
							}
						}
					}
					break;
				case FT_STATUS:
					ASSERT(newtag->IsInt());
					if (newtag->IsInt())
						m_paused = m_stopped = (newtag->GetInt() != 0);

					break;
				case FT_ULPRIORITY:
					ASSERT(newtag->IsInt());
					if (newtag->IsInt()) {
						if (!isnewstyle) {
							int iUpPriority = newtag->GetInt();
							if (iUpPriority == PR_AUTO) {
								SetUpPriority(PR_HIGH, false);
								SetAutoUpPriority(true);
							} else {
								if (iUpPriority != PR_VERYLOW && iUpPriority != PR_LOW && iUpPriority != PR_NORMAL && iUpPriority != PR_HIGH && iUpPriority != PR_VERYHIGH)
									iUpPriority = PR_NORMAL;
								SetUpPriority((uint8)iUpPriority, false);
								SetAutoUpPriority(false);
							}
						}
					}
					break;
				case FT_KADLASTPUBLISHSRC:
					ASSERT(newtag->IsInt());
					if (newtag->IsInt()) {
						SetLastPublishTimeKadSrc(newtag->GetInt(), 0);
						if (GetLastPublishTimeKadSrc() > time(NULL) + KADEMLIAREPUBLISHTIMES) {
							//There may be a possibility of an older client that saved a random number here. This will check for that.
							SetLastPublishTimeKadSrc(0, 0);
						}
					}
					break;
				case FT_KADLASTPUBLISHNOTES:
					ASSERT(newtag->IsInt());
					if (newtag->IsInt())
						SetLastPublishTimeKadNotes(newtag->GetInt());
					break;
				case FT_DL_PREVIEW:
					ASSERT(newtag->IsInt());
					SetPreviewPrio(((newtag->GetInt() >> 0) & 0x01) == 1);
					SetPauseOnPreview(((newtag->GetInt() >> 1) & 0x01) == 1);
					break;

				// statistics
				case FT_ATTRANSFERRED:
					ASSERT(newtag->IsInt());
					if (newtag->IsInt())
						statistic.SetAllTimeTransferred(newtag->GetInt());
					break;
				case FT_ATTRANSFERREDHI:
					ASSERT(newtag->IsInt());
					if (newtag->IsInt()) {
						uint32 low = (uint32)statistic.GetAllTimeTransferred();
						uint64 hi = newtag->GetInt();
						statistic.SetAllTimeTransferred((hi << 32) + low);
					}
					break;
				case FT_ATREQUESTED:
					ASSERT(newtag->IsInt());
					if (newtag->IsInt())
						statistic.SetAllTimeRequests(newtag->GetInt());
					break;
				case FT_ATACCEPTED:
					ASSERT(newtag->IsInt());
					if (newtag->IsInt())
						statistic.SetAllTimeAccepts(newtag->GetInt());
					break;

				// old tags: as long as they are not needed, take the chance to purge them
				case FT_PERMISSIONS:
				case FT_KADLASTPUBLISHKEY:
					ASSERT(newtag->IsInt());
					break;
				case FT_DL_ACTIVE_TIME:
					ASSERT(newtag->IsInt());
					if (newtag->IsInt())
						m_nDlActiveTime = newtag->GetInt();
					break;
				case FT_CORRUPTEDPARTS:
					ASSERT(newtag->IsStr());
					if (newtag->IsStr()) {
						ASSERT(corrupted_list.GetHeadPosition() == NULL);
						const CString &strCorruptedParts(newtag->GetStr());
						for (int iPos = 0; iPos >= 0;) {
							const CString &strPart(strCorruptedParts.Tokenize(_T(","), iPos));
							uint16 uPart;
							if (!strPart.IsEmpty()
								&& _stscanf(strPart, _T("%hu"), &uPart) == 1
								&& uPart < GetPartCount()
								&& !IsCorruptedPart(uPart))
							{
								corrupted_list.AddTail(uPart);
							}
						}
					}
					break;
				case FT_AICH_HASH:
					{
						ASSERT(newtag->IsStr());
						CAICHHash hash;
						if (DecodeBase32(newtag->GetStr(), hash) == CAICHHash::GetHashSize()) {
							m_FileIdentifier.SetAICHHash(hash);
							m_pAICHRecoveryHashSet->SetMasterHash(hash, AICH_VERIFIED);
						} else
							ASSERT(0);
					}
					break;
				case FT_AICHHASHSET:
					if (newtag->IsBlob()) {
						CSafeMemFile aichHashSetFile(newtag->GetBlob(), newtag->GetBlobSize());
						m_FileIdentifier.LoadAICHHashsetFromFile(aichHashSetFile, false);
						aichHashSetFile.Detach();
						bHadAICHHashSetTag = true;
					} else
						ASSERT(0);
					break;
				default:
					if (newtag->GetNameID() == 0 && (newtag->GetName()[0] == FT_GAPSTART || newtag->GetName()[0] == FT_GAPEND)) {
						ASSERT(newtag->IsInt64(true));
						if (newtag->IsInt64(true)) {
							UINT gapkey = atoi(&newtag->GetName()[1]);
							CGapMap::CPair *pair = gap_map.PLookup(gapkey);
							if (!pair) {
								gap_map[gapkey] = Gap_Struct{_UI64_MAX, _UI64_MAX};
								pair = gap_map.PLookup(gapkey);
								ASSERT(pair);
							}
							if (newtag->GetName()[0] == FT_GAPSTART)
								pair->value.start = newtag->GetInt64();
							else if (newtag->GetName()[0] == FT_GAPEND)
								pair->value.end = newtag->GetInt64() - 1;
						}
					} else {
						m_taglist.Add(newtag);
						newtag = NULL;
					}
				}
			}
			delete newtag;
		}

		//m_bAICHPartHashsetNeeded = m_FileIdentifier.GetTheoreticalAICHPartHashCount() > 0;
		if (bHadAICHHashSetTag)
			if (!m_FileIdentifier.VerifyAICHHashSet())
				DebugLogError(_T("Failed to load AICH Part HashSet for part file %s"), (LPCTSTR)GetFileName());
			else {
			//	DebugLog(_T("Succeeded to load AICH Part HashSet for file %s"), (LPCTSTR)GetFileName());
				m_bAICHPartHashsetNeeded = false;
			}

		// load the hash sets from the hybrid style .part.met
		if (isnewstyle && pOutCheckFileFormat == NULL && (metFile.GetPosition() < metFile.GetLength())) {
			uint8 temp;
			metFile.Read(&temp, sizeof temp);

			// assuming we will get all hash sets
			for (uint32 i = GetPartCount(); i && (metFile.GetPosition() + 16 < metFile.GetLength()); --i) {
				uchar *cur_hash = new uchar[MDX_DIGEST_SIZE];
				metFile.Read(cur_hash, MDX_DIGEST_SIZE);
				m_FileIdentifier.GetRawMD4HashSet().Add(cur_hash);
			}
			m_FileIdentifier.CalculateMD4HashByHashSet(true, true);
		}

		metFile.Close();
	} catch (CFileException *ex) {
		if (ex->m_cause == CFileException::endOfFile)
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_METCORRUPT), (LPCTSTR)m_partmetfilename, (LPCTSTR)GetFileName());
		else
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_FILEERROR), (LPCTSTR)m_partmetfilename, (LPCTSTR)GetFileName(), (LPCTSTR)CExceptionStr(*ex));
		ex->Delete();
		return PLR_FAILED_METFILE_CORRUPT;
#ifndef _DEBUG
	} catch (...) {
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_METCORRUPT), (LPCTSTR)m_partmetfilename, (LPCTSTR)GetFileName());
		ASSERT(0);
		return PLR_FAILED_METFILE_CORRUPT;
#endif
	}

	if ((uint64)m_nFileSize > MAX_EMULE_FILE_SIZE) {
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_FILEERROR), (LPCTSTR)m_partmetfilename, (LPCTSTR)GetFileName(), _T("File size exceeds supported limit"));
		return PLR_FAILED_OTHER;
	}

	if (pOutCheckFileFormat != NULL) {
		// AAARGGGHH!!!....
		*pOutCheckFileFormat = partmettype;
		return PLR_CHECKSUCCESS;
	}

	// Now flush the map into the list (Slugfiller)
	for (const CGapMap::CPair *pair = gap_map.PGetFirstAssoc(); pair != NULL; pair = gap_map.PGetNextAssoc(pair)) {
		const Gap_Struct &gap = pair->value;
		// SLUGFILLER: SafeHash - revised code, and extra safety
		if (gap.start != _UI64_MAX && gap.end != _UI64_MAX && gap.start <= gap.end && gap.start < (uint64)m_nFileSize)
			AddGap(gap.start, min(gap.end, (uint64)m_nFileSize - 1)); // use safe adding
		// SLUGFILLER: SafeHash
	}

	// verify corrupted parts list
	for (POSITION posCorruptedPart = corrupted_list.GetHeadPosition(); posCorruptedPart != NULL;) {
		POSITION posLast = posCorruptedPart;
		if (IsCompleteBD(corrupted_list.GetNext(posCorruptedPart)))
			corrupted_list.RemoveAt(posLast);
	}

	//check if this is a backup
	if (_tcsicmp(_tcsrchr(m_fullname, _T('.')), PARTMET_TMP_EXT) == 0 || _tcsicmp(_tcsrchr(m_fullname, _T('.')), PARTMET_BAK_EXT) == 0)
		m_fullname = RemoveFileExtension(m_fullname);

	// open permanent handle
	const CString &searchpath(RemoveFileExtension(m_fullname));
	ASSERT(searchpath.Right(5) == _T(".part"));
	//CFileException fex;
	if (!m_hpartfile.Open(searchpath, CFile::modeReadWrite | CFile::shareDenyNone | CFile::osSequentialScan, &fex)) {
		CString s;
		s.Format(GetResString(IDS_ERR_FILEOPEN), (LPCTSTR)searchpath, (LPCTSTR)GetFileName());
		LogError(LOG_STATUSBAR, _T("%s%s"), (LPCTSTR)s, (LPCTSTR)CExceptionStrDash(fex));
		return PLR_FAILED_OTHER;
	}

	// get part file's time
	struct _stat64 st;
	if (statUTC((HANDLE)m_hpartfile, st) == 0) {
		m_tCreated = (time_t)st.st_ctime;
		m_tLastModified = (time_t)st.st_mtime;
	} else
		AddDebugLogLine(false, _T("Failed to get file date for \"%s\" - %s"), (LPCTSTR)searchpath, _tcserror(errno));

	try {
		SetFilePath(searchpath);
		m_dwFileAttributes = ::GetFileAttributes(GetFilePath());
		if (m_dwFileAttributes == INVALID_FILE_ATTRIBUTES)
			m_dwFileAttributes = 0;

		// SLUGFILLER: SafeHash - final safety, make sure any missing part of the file is a gap
		if (m_hpartfile.GetLength() < (uint64)m_nFileSize)
			AddGap(m_hpartfile.GetLength(), (uint64)m_nFileSize - 1);
		else if (m_hpartfile.GetLength() > (uint64)m_nFileSize) {
			// Goes both ways - Partfile should never be too large
			TRACE(_T("Partfile \"%s\" is too large! Truncating %I64u bytes.\n"), (LPCTSTR)GetFileName(), m_hpartfile.GetLength() - (uint64)m_nFileSize);
			m_hpartfile.SetLength((uint64)m_nFileSize);
		}
		// SLUGFILLER: SafeHash

		m_SrcPartFrequency.SetSize(GetPartCount());
		if (GetPartCount())
			memset(&m_SrcPartFrequency[0], 0, GetPartCount() * sizeof m_SrcPartFrequency[0]);
		SetStatus(PS_EMPTY);
		m_CorruptionBlackBox.Init(m_nFileSize);
		// check hash count, file status etc
		m_bMD4HashsetNeeded = !m_FileIdentifier.HasExpectedMD4HashCount();
		if (m_bMD4HashsetNeeded) {
			ASSERT(m_FileIdentifier.GetRawMD4HashSet().GetCount() == 0);
			return PLR_LOADSUCCESS;
		}

		for (UINT i = min(m_FileIdentifier.GetAvailableMD4PartHashCount(), GetPartCount()); i-- > 0;)
			if (IsCompleteBD(i)) {
				SetStatus(PS_READY);
				break;
			}

		if (m_gaplist.IsEmpty()) {	// is this file complete already?
			CompleteFile(false);
			return PLR_LOADSUCCESS;
		}

		if (!isnewstyle) { // not for importing
			// check date of .part file - if it's wrong, rehash file
			CFileStatus filestatus;
			try {
				m_hpartfile.GetStatus(filestatus);
			} catch (CException *ex) {
				ex->Delete();
			}
			time_t fdate = (time_t)filestatus.m_mtime.GetTime();
			if (fdate == 0)
				fdate = (time_t)-1;
			if (fdate == (time_t)-1) {
				if (thePrefs.GetVerbose())
					AddDebugLogLine(false, _T("Failed to get file date of \"%s\" (%s)"), filestatus.m_szFullName, (LPCTSTR)GetFileName());
			} else
				AdjustNTFSDaylightFileTime(fdate, filestatus.m_szFullName);

			if (m_tUtcLastModified != fdate) {
				CString strFileInfo(GetFilePath());
				strFileInfo.AppendFormat(_T(" (%s)"), (LPCTSTR)GetFileName());
				LogError(LOG_STATUSBAR, GetResString(IDS_ERR_REHASH), (LPCTSTR)strFileInfo);
				// rehash
				SetStatus(PS_WAITINGFORHASH);
				CAddFileThread *addfilethread = static_cast<CAddFileThread*>(AfxBeginThread(RUNTIME_CLASS(CAddFileThread), THREAD_PRIORITY_BELOW_NORMAL, 0, CREATE_SUSPENDED));
				if (addfilethread) {
					SetFileOp(PFOP_HASHING);
					addfilethread->SetValues(0, GetPath(), m_hpartfile.GetFileName(), _T(""), this);
					SetFileOpProgress(0);
					SetStatus(PS_HASHING);
					addfilethread->ResumeThread();
				} else
					SetStatus(PS_ERROR);
			}
		}
	} catch (CFileException *ex) {
		LogError(LOG_STATUSBAR, _T("Failed to initialize part file \"%s\" (%s)%s")
			, (LPCTSTR)m_hpartfile.GetFilePath()
			, (LPCTSTR)GetFileName()
			, (LPCTSTR)CExceptionStrDash(*ex));
		ex->Delete();
		return PLR_FAILED_OTHER;
	}

	UpdateCompletedInfos();
	return PLR_LOADSUCCESS;
}

bool CPartFile::SavePartFile(bool bDontOverrideBak)
{
	if (status == PS_WAITINGFORHASH || status == PS_HASHING)
		return false;
	// search part file
	const CString &searchpath(RemoveFileExtension(m_fullname));
	CFileFind ff;
	BOOL bFound = ff.FindFile(searchpath);
	if (bFound)
		ff.FindNextFile();
	if (!bFound || ff.IsDirectory()) {
		LogError(GetResString(IDS_ERR_SAVEMET) + _T(" - %s"), (LPCTSTR)m_partmetfilename, (LPCTSTR)GetFileName(), (LPCTSTR)GetResString(IDS_ERR_PART_FNF));
		return false;
	}
	// get file date
	FILETIME lwtime;
	ff.GetLastWriteTime(&lwtime);
	m_tLastModified = (time_t)FileTimeToUnixTime(lwtime);
	if (m_tLastModified <= 0)
		m_tLastModified = (time_t)-1;
	m_tUtcLastModified = m_tLastModified;
	if (m_tUtcLastModified == (time_t)-1) {
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("Failed to get file date of \"%s\" (%s)"), (LPCTSTR)m_partmetfilename, (LPCTSTR)GetFileName());
	} else
		AdjustNTFSDaylightFileTime(m_tUtcLastModified, ff.GetFilePath());
	ff.Close();

	const CString &strTmpFile(m_fullname + PARTMET_TMP_EXT);

	// save file data to part.met file
	CSafeBufferedFile file;
	CFileException fex;
	if (!file.Open(strTmpFile, CFile::modeWrite | CFile::modeCreate | CFile::typeBinary | CFile::shareDenyWrite, &fex)) {
		CString s;
		s.Format(GetResString(IDS_ERR_SAVEMET), (LPCTSTR)m_partmetfilename, (LPCTSTR)GetFileName());
		LogError(_T("%s%s"), (LPCTSTR)s, (LPCTSTR)CExceptionStrDash(fex));
		return false;
	}
	::setvbuf(file.m_pStream, NULL, _IOFBF, 16384);

	try {
		//version
		// only use 64 bit tags, when PARTFILE_VERSION_LARGEFILE is set!
		file.WriteUInt8(IsLargeFile() ? PARTFILE_VERSION_LARGEFILE : PARTFILE_VERSION);

		//date
		file.WriteUInt32((uint32)m_tUtcLastModified);

		//hash
		m_FileIdentifier.WriteMD4HashsetToFile(file);

		UINT uTagCount = 0;
		ULONG uTagCountFilePos = (ULONG)file.GetPosition();
		file.WriteUInt32(uTagCount);

		CTag nametag(FT_FILENAME, GetFileName());
		nametag.WriteTagToFile(file, UTF8strOptBOM);
		++uTagCount;

		CTag sizetag(FT_FILESIZE, (uint64)m_nFileSize, IsLargeFile());
		sizetag.WriteTagToFile(file);
		++uTagCount;

		if (m_uTransferred) {
			CTag transtag(FT_TRANSFERRED, m_uTransferred, IsLargeFile());
			transtag.WriteTagToFile(file);
			++uTagCount;
		}
		if (m_uCompressionGain) {
			CTag transtag(FT_COMPRESSION, m_uCompressionGain, IsLargeFile());
			transtag.WriteTagToFile(file);
			++uTagCount;
		}
		if (m_uCorruptionLoss) {
			CTag transtag(FT_CORRUPTED, m_uCorruptionLoss, IsLargeFile());
			transtag.WriteTagToFile(file);
			++uTagCount;
		}

		if (m_paused) {
			CTag statustag(FT_STATUS, 1);
			statustag.WriteTagToFile(file);
			++uTagCount;
		}

		CTag prioritytag(FT_DLPRIORITY, IsAutoDownPriority() ? PR_AUTO : m_iDownPriority);
		prioritytag.WriteTagToFile(file);
		++uTagCount;

		CTag ulprioritytag(FT_ULPRIORITY, IsAutoUpPriority() ? PR_AUTO : GetUpPriority());
		ulprioritytag.WriteTagToFile(file);
		++uTagCount;

		if (lastseencomplete.GetTime()) {
			CTag lsctag(FT_LASTSEENCOMPLETE, (UINT)lastseencomplete.GetTime());
			lsctag.WriteTagToFile(file);
			++uTagCount;
		}

		if (m_category) {
			CTag categorytag(FT_CATEGORY, m_category);
			categorytag.WriteTagToFile(file);
			++uTagCount;
		}

		if (GetLastPublishTimeKadSrc()) {
			CTag kadLastPubSrc(FT_KADLASTPUBLISHSRC, (uint32)GetLastPublishTimeKadSrc());
			kadLastPubSrc.WriteTagToFile(file);
			++uTagCount;
		}

		if (GetLastPublishTimeKadNotes()) {
			CTag kadLastPubNotes(FT_KADLASTPUBLISHNOTES, (uint32)GetLastPublishTimeKadNotes());
			kadLastPubNotes.WriteTagToFile(file);
			++uTagCount;
		}

		if (GetDlActiveTime()) {
			CTag tagDlActiveTime(FT_DL_ACTIVE_TIME, GetDlActiveTime());
			tagDlActiveTime.WriteTagToFile(file);
			++uTagCount;
		}

		if (GetPreviewPrio() || IsPausingOnPreview()) {
			UINT uTagValue = (static_cast<UINT>(IsPausingOnPreview()) << 1) | (static_cast<UINT>(GetPreviewPrio()) << 0);
			CTag tagDlPreview(FT_DL_PREVIEW, uTagValue);
			tagDlPreview.WriteTagToFile(file);
			++uTagCount;
		}

		// statistics
		if (statistic.GetAllTimeTransferred()) {
			CTag attag1(FT_ATTRANSFERRED, (uint32)statistic.GetAllTimeTransferred());
			attag1.WriteTagToFile(file);
			++uTagCount;

			CTag attag4(FT_ATTRANSFERREDHI, (uint32)(statistic.GetAllTimeTransferred() >> 32));
			attag4.WriteTagToFile(file);
			++uTagCount;
		}

		if (statistic.GetAllTimeRequests()) {
			CTag attag2(FT_ATREQUESTED, statistic.GetAllTimeRequests());
			attag2.WriteTagToFile(file);
			++uTagCount;
		}

		if (statistic.GetAllTimeAccepts()) {
			CTag attag3(FT_ATACCEPTED, statistic.GetAllTimeAccepts());
			attag3.WriteTagToFile(file);
			++uTagCount;
		}

		if (m_uMaxSources) {
			CTag attag3(FT_MAXSOURCES, m_uMaxSources);
			attag3.WriteTagToFile(file);
			++uTagCount;
		}

		// corrupt part infos
		POSITION posCorruptedPart = corrupted_list.GetHeadPosition();
		if (posCorruptedPart) {
			CString strCorruptedParts;
			while (posCorruptedPart) {
				UINT uCorruptedPart = (UINT)corrupted_list.GetNext(posCorruptedPart);
				strCorruptedParts.AppendFormat(&_T(",%u")[static_cast<size_t>(strCorruptedParts.IsEmpty())], uCorruptedPart);
			}
			ASSERT(!strCorruptedParts.IsEmpty());
			CTag tagCorruptedParts(FT_CORRUPTEDPARTS, strCorruptedParts);
			tagCorruptedParts.WriteTagToFile(file);
			++uTagCount;
		}

		//AICH File hash
		if (m_FileIdentifier.HasAICHHash()) {
			CTag aichtag(FT_AICH_HASH, m_FileIdentifier.GetAICHHash().GetString());
			aichtag.WriteTagToFile(file);
			++uTagCount;

			// AICH Part HashSet
			// no point in permanently storing the AICH part hashset if we need to rehash the file anyway to fetch the full recovery hashset
			// the tag will make the known.met incompatible with emule version prior 0.44a - but that one is nearly 6 years old
			if (m_FileIdentifier.HasExpectedAICHHashCount()) {
				uint32 nAICHHashSetSize = (CAICHHash::GetHashSize() * (m_FileIdentifier.GetAvailableAICHPartHashCount() + 1)) + 2;
				BYTE *pHashBuffer = new BYTE[nAICHHashSetSize];
				CSafeMemFile hashSetFile(pHashBuffer, nAICHHashSetSize);
				bool bWriteHashSet = true;
				try {
					m_FileIdentifier.WriteAICHHashsetToFile(hashSetFile);
				} catch (CFileException *pError) {
					ASSERT(0);
					DebugLogError(_T("Memfile Error while storing AICH Part HashSet"));
					bWriteHashSet = false;
					delete[] hashSetFile.Detach();
					pError->Delete();
				}
				if (bWriteHashSet) {
					CTag tagAICHHashSet(FT_AICHHASHSET, hashSetFile.Detach(), nAICHHashSetSize);
					tagAICHHashSet.WriteTagToFile(file);
					++uTagCount;
				}
			}
		}

		for (INT_PTR j = 0; j < m_taglist.GetCount(); ++j)
			if (m_taglist[j]->IsStr() || m_taglist[j]->IsInt()) {
				m_taglist[j]->WriteTagToFile(file, UTF8strOptBOM);
				++uTagCount;
			}

		//gaps
		char namebuffer[10];
		char *number = &namebuffer[1];
		UINT i_pos = 0;
		for (POSITION pos = m_gaplist.GetHeadPosition(); pos != NULL;) {
			const Gap_Struct &gap = m_gaplist.GetNext(pos);
			_itoa(i_pos, number, 10);
			namebuffer[0] = FT_GAPSTART;
			CTag gapstarttag(namebuffer, gap.start, IsLargeFile());
			gapstarttag.WriteTagToFile(file);
			++uTagCount;

			// gap start = first missing byte but gap ends = first non-missing byte in edonkey
			// but I think its easier to use the real limits
			namebuffer[0] = FT_GAPEND;
			CTag gapendtag(namebuffer, gap.end + 1, IsLargeFile());
			gapendtag.WriteTagToFile(file);
			++uTagCount;

			++i_pos;
		}
		// Add buffered data as gap too - at the time of writing the file, this data
		// does not exist on the disk, so not adding it as gaps leads to inconsistencies
		// which cause problems in case of failing to write the buffered data
		// (for example, on disk full errors)
		// don't bother to merge everything, we do this on the next loading
//		uint32 dbgMerged = 0;
		for (POSITION pos = m_BufferedData_list.GetHeadPosition(); pos != NULL;) {
			POSITION pos2 = pos;
			const PartFileBufferedData *item = m_BufferedData_list.GetNext(pos);
			if (item->flushed == PB_WRITTEN) {
				DeleteWrittenItem(pos2);
				continue;
			}
			const uint64 nStart = item->start;
			uint64 nEnd = item->end;
			while (pos != NULL) { // merge if obvious
				pos2 = pos;
				item = m_BufferedData_list.GetNext(pos);
				if (item->flushed == PB_WRITTEN) {
					pos = pos2; //step back; the outer loop will delete this item
					break;
				}
				if (item->start != nEnd + 1)
					break;
//				++dbgMerged;
				nEnd = item->end;
			}

			_itoa(i_pos, number, 10);
			namebuffer[0] = FT_GAPSTART;
			CTag gapstarttag(namebuffer, nStart, IsLargeFile());
			gapstarttag.WriteTagToFile(file);
			++uTagCount;

			// gap start = first missing byte; but gap ends = first non-missing byte in edonkey
			// but I think its easier to user the real limits
			namebuffer[0] = FT_GAPEND;
			CTag gapendtag(namebuffer, nEnd + 1, IsLargeFile());
			gapendtag.WriteTagToFile(file);
			++uTagCount;
			++i_pos;
		}
//		DEBUG_ONLY( DebugLog(_T("Wrote %u buffered gaps (%u merged) for file %s"), m_BufferedData_list.GetCount(), dbgMerged, (LPCTSTR)GetFileName()) );

		file.Seek(uTagCountFilePos, CFile::begin);
		file.WriteUInt32(uTagCount);
		file.SeekToEnd();
		CommitAndClose(file);
	} catch (CFileException *ex) {
		CString strError;
		strError.Format(GetResString(IDS_ERR_SAVEMET), (LPCTSTR)m_partmetfilename, (LPCTSTR)GetFileName());
		LogError(_T("%s%s"), (LPCTSTR)strError, (LPCTSTR)CExceptionStrDash(*ex));
		ex->Delete();

		// remove the partially written or otherwise damaged temporary file,
		// need to close the file before removing it.
		file.Abort(); //Call 'Abort' instead of 'Close' to avoid ASSERT.
		(void)_tremove(strTmpFile);
		return false;
	}

	// after successfully writing the temporary part.met file...
	if (_tremove(m_fullname) != 0 && errno != ENOENT) {
		if (thePrefs.GetVerbose())
			DebugLogError(_T("Failed to remove \"%s\" - %s"), (LPCTSTR)m_fullname, _tcserror(errno));
	}

	if (_trename(strTmpFile, m_fullname) != 0) {
		int iErrno = errno;
		if (thePrefs.GetVerbose())
			DebugLogError(_T("Failed to move temporary part.met file \"%s\" to \"%s\" - %s"), (LPCTSTR)strTmpFile, (LPCTSTR)m_fullname, _tcserror(iErrno));

		CString strError;
		strError.Format(GetResString(IDS_ERR_SAVEMET), (LPCTSTR)m_partmetfilename, (LPCTSTR)GetFileName());
		strError.AppendFormat(_T(" - %s"), _tcserror(iErrno));
		LogError(_T("%s"), (LPCTSTR)strError);
		return false;
	}

	// create a backup of the successfully written part.met file
	if (!::CopyFile(m_fullname, m_fullname + PARTMET_BAK_EXT, static_cast<BOOL>(bDontOverrideBak)))
		if (!bDontOverrideBak)
			DebugLogError(_T("Failed to create backup of %s (%s) - %s"), (LPCTSTR)m_fullname, (LPCTSTR)GetFileName(), (LPCTSTR)GetErrorMessage(::GetLastError()));

	return true;
}

void CPartFile::PartFileHashFinished(CKnownFile *result)
{
	ASSERT(result->GetFileIdentifier().GetTheoreticalMD4PartHashCount() == m_FileIdentifier.GetTheoreticalMD4PartHashCount());
	ASSERT(result->GetFileIdentifier().GetTheoreticalAICHPartHashCount() == m_FileIdentifier.GetTheoreticalAICHPartHashCount());
	bool errorfound = false;
	bool bToShare = false; // add to the shared files list if a complete part was found
	// check each part
	for (UINT nPart = 0; nPart < GetPartCount(); ++nPart) {
		ASSERT(IsCompleteBD(nPart) == IsComplete(nPart));
		if (IsComplete(nPart)) {
			bool bMD4Error = false;
			bool bAICHError = false;
			// MD4
			bool bMD4Checked;
			if (nPart == 0 && m_FileIdentifier.GetTheoreticalMD4PartHashCount() == 0) {
				bMD4Checked = true;
				bMD4Error = !md4equ(result->GetFileIdentifier().GetMD4Hash(), GetFileIdentifier().GetMD4Hash());
			} else if (m_FileIdentifier.HasExpectedMD4HashCount()) {
				bMD4Checked = true;
				if (result->GetFileIdentifier().GetMD4PartHash(nPart) && GetFileIdentifier().GetMD4PartHash(nPart))
					bMD4Error = !md4equ(result->GetFileIdentifier().GetMD4PartHash(nPart), m_FileIdentifier.GetMD4PartHash(nPart));
				else
					ASSERT(0);
			} else
				bMD4Checked = false;
			// AICH
			bool bAICHChecked = false;
			if (GetFileIdentifier().HasAICHHash()) {
				if (nPart == 0 && m_FileIdentifier.GetTheoreticalAICHPartHashCount() == 0) {
					bAICHChecked = true;
					bAICHError = result->GetFileIdentifier().GetAICHHash() != GetFileIdentifier().GetAICHHash();
				} else if (m_FileIdentifier.HasExpectedAICHHashCount()) {
					bAICHChecked = true;
					if (result->GetFileIdentifier().GetAvailableAICHPartHashCount() > nPart && GetFileIdentifier().GetAvailableAICHPartHashCount() > nPart)
						bAICHError = result->GetFileIdentifier().GetRawAICHHashSet()[nPart] != GetFileIdentifier().GetRawAICHHashSet()[nPart];
					else
						ASSERT(0);
				}
			}
			if (bMD4Error || bAICHError) {
				errorfound = true;
				LogWarning(GetResString(IDS_ERR_FOUNDCORRUPTION), nPart, (LPCTSTR)GetFileName());
				const uint64 nPartStart = nPart * PARTSIZE;
				AddGap(nPartStart, min(nPartStart + PARTSIZE, (uint64)m_nFileSize) - 1);
				if (bMD4Checked && bAICHChecked && bMD4Error != bAICHError)
					DebugLogError(_T("AICH and MD4 HashSet disagree on verifying part %u for file %s. MD4: %s - AICH: %s"), nPart
						, (LPCTSTR)GetFileName(), bMD4Error ? _T("Corrupt") : _T("OK"), bAICHError ? _T("Corrupt") : _T("OK"));
			} else
				bToShare = true;
		}
	}
	// missing md4 hashset?
	if (!m_FileIdentifier.HasExpectedMD4HashCount()) {
		DebugLogError(_T("Final hashing/rehashing without valid MD4 HashSet for file %s"), (LPCTSTR)GetFileName());
		// if finished we can copy over the hashset from our hash result
		if (m_gaplist.IsEmpty()
			&& md4equ(result->GetFileIdentifier().GetMD4Hash(), GetFileIdentifier().GetMD4Hash())
			&& m_FileIdentifier.SetMD4HashSet(result->GetFileIdentifier().GetRawMD4HashSet()))
		{
			m_bMD4HashsetNeeded = false;
		}
	}

	if (!errorfound && status == PS_COMPLETING) {
		if (!result->GetFileIdentifier().HasAICHHash())
			AddDebugLogLine(false, _T("Failed to store new AICH recovery and Part Hashset for completed file %s"), (LPCTSTR)GetFileName());
		else {
			m_FileIdentifier.SetAICHHash(result->GetFileIdentifier().GetAICHHash());
			m_FileIdentifier.SetAICHHashSet(result->GetFileIdentifier());
			SetAICHRecoverHashSetAvailable(true);
		}
		m_pAICHRecoveryHashSet->FreeHashSet();
	}

	delete result;
	if (errorfound) {
		SetStatus(PS_READY);
		if (thePrefs.GetVerbose())
			DebugLogError(LOG_STATUSBAR, _T("File hashing failed for \"%s\""), (LPCTSTR)GetFileName());
		SavePartFile();
		return;
	}
	if (thePrefs.GetVerbose())
		AddDebugLogLine(true, _T("Completed hashing file \"%s\""), (LPCTSTR)GetFileName());
	if (status == PS_COMPLETING) {
		if (theApp.sharedfiles->GetFileByID(GetFileHash()) == NULL)
			theApp.sharedfiles->SafeAddKFile(this);
		CompleteFile(true);
	} else {
		AddLogLine(false, GetResString(IDS_HASHINGDONE), (LPCTSTR)GetFileName());
		SetStatus(PS_READY);
		SavePartFile();
		if (bToShare)
			theApp.sharedfiles->SafeAddKFile(this);
	}
}

void CPartFile::AddGap(uint64 start, uint64 end) //keep the list ordered!
{
	ASSERT(end < (uint64)m_nFileSize && start <= end);
	POSITION before = NULL;
	for (POSITION pos = m_gaplist.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		const Gap_Struct &gap = m_gaplist.GetNext(pos);
		if (gap.start > end) {
			before = pos2; //no intersections, insert the new gap
			break;
		}
		if (gap.end >= start) {
			if (gap.start <= start && gap.end >= end)
				return; //this gap contains the whole new gap
			//either start or end of this gap may be outside of the new gap, but not both
			if (gap.end > end)
				end = gap.end; //extend the tail
			else if (gap.start < start)
				start = gap.start; //extend the head
			//this gap is fully contained in the new gap
			m_gaplist.RemoveAt(pos2);
		}
	}
	if (before)
		m_gaplist.InsertBefore(before, Gap_Struct{start, end});
	else
		m_gaplist.AddTail(Gap_Struct{start, end});
	UpdateDisplayedInfo();
}

bool CPartFile::IsComplete(uint64 start, uint64 end) const
{
	ASSERT(end < (uint64)m_nFileSize && start <= end);

	for (POSITION pos = m_gaplist.GetHeadPosition(); pos != NULL;) {
		const Gap_Struct &gap = m_gaplist.GetNext(pos);
		if (gap.start > end)
			break; //no intersections
		if (gap.end >= start)
			return false;
	}
	return true;
}

bool CPartFile::IsCompleteSafe(uint64 start, uint64 end) const
{
	return IsComplete(start, min(end, (uint64)m_nFileSize - 1));
}

bool CPartFile::IsComplete(UINT uPart) const
{
	return IsCompleteSafe(uPart * PARTSIZE, uPart * PARTSIZE + PARTSIZE - 1);
}

//take into account unwritten buffered data
bool CPartFile::IsCompleteBD(uint64 start, uint64 end) const
{
	ASSERT(end < (uint64)m_nFileSize && start <= end);

	if (!IsComplete(start, end))
		return false;

	for (POSITION pos = m_BufferedData_list.GetHeadPosition(); pos != NULL;) {
		const PartFileBufferedData *cur_block = m_BufferedData_list.GetNext(pos);
		if (cur_block->start > end)
			break; //no intersections
		if (cur_block->end >= start)
			return false;
	}
	return true;
}

bool CPartFile::IsCompleteBDSafe(uint64 start, uint64 end) const
{
	return IsCompleteBD(start, min(end, (uint64)m_nFileSize - 1));
}

bool CPartFile::IsCompleteBD(UINT uPart) const
{
	return IsCompleteBDSafe(uPart * PARTSIZE, uPart * PARTSIZE + PARTSIZE - 1);
}

bool CPartFile::IsPureGap(uint64 start, uint64 end) const
{
	if (end >= (uint64)m_nFileSize)
		end = (uint64)m_nFileSize - 1;
	ASSERT(start <= end);

	for (POSITION pos = m_gaplist.GetHeadPosition(); pos != NULL;) {
		const Gap_Struct &gap = m_gaplist.GetNext(pos);
		if (gap.start > end)
			break;
		if (gap.start <= start && gap.end >= end) //fully inside this gap
			return true;
	}
	return false;
}

bool CPartFile::IsAlreadyRequested(uint64 start, uint64 end, bool bCheckBuffers) const
{
	ASSERT(start <= end);
	// check our request list
	for (POSITION pos = requestedblocks_list.GetHeadPosition(); pos != NULL;) {
		const Requested_Block_Struct *cur_block = requestedblocks_list.GetNext(pos);
		if (cur_block->EndOffset >= start && cur_block->StartOffset <= end)
			return true;
	}
	// check our buffers
	if (bCheckBuffers)
		for (POSITION pos = m_BufferedData_list.GetHeadPosition(); pos != NULL;) {
			const PartFileBufferedData *item = m_BufferedData_list.GetNext(pos);
			if (item->start > end)
				break; //no intersections
			if (item->end >= start) {
				DebugLogWarning(_T("CPartFile::IsAlreadyRequested, collision with buffered data found"));
				return true;
			}
		}

	return false;
}

bool CPartFile::ShrinkToAvoidAlreadyRequested(uint64 &start, uint64 &end) const
{
	ASSERT(start <= end);
#ifdef _DEBUG
	uint64 startOrig = start;
	uint64 endOrig = end;
#endif
	for (POSITION pos = requestedblocks_list.GetHeadPosition(); pos != NULL;) {
		const Requested_Block_Struct *cur_block = requestedblocks_list.GetNext(pos);
		if (cur_block->StartOffset <= end && cur_block->EndOffset >= start) {
			if (cur_block->StartOffset > start)
				end = cur_block->StartOffset - 1;
			else if (cur_block->EndOffset < end)
				start = cur_block->EndOffset + 1;
			else
				return false;

			if (start > end)
				return false;
		}
	}

	// has been shrunk to fit requested, might need more shrinking to not collide with buffered data
	// check our buffers
	for (POSITION pos = m_BufferedData_list.GetHeadPosition(); pos != NULL;) {
		const PartFileBufferedData *item = m_BufferedData_list.GetNext(pos);
		if (item->start > end)
			break;
		if (item->end >= start) {
			if (item->start > start)
				end = item->start - 1;
			else if (item->end < end)
				start = item->end + 1;
			else
				return false;

			if (start > end)
				return false;
		}
	}
	ASSERT(start >= startOrig && start <= endOrig);
	ASSERT(end >= startOrig && end <= endOrig);
	return true;
}

uint64 CPartFile::GetTotalGapSizeInRange(uint64 uRangeStart, uint64 uRangeEnd) const
{
	ASSERT(uRangeStart <= uRangeEnd);

	uint64 uTotalGapSize = 0;

	if (uRangeEnd >= (uint64)m_nFileSize)
		uRangeEnd = (uint64)m_nFileSize - 1;

	for (POSITION pos = m_gaplist.GetHeadPosition(); pos != NULL;) {
		const Gap_Struct &gap = m_gaplist.GetNext(pos);
		if (gap.start > uRangeEnd)
			break;
		//here gap.start <= uRangeEnd
		if (gap.start <= uRangeStart && gap.end >= uRangeEnd) {
			uTotalGapSize = uRangeEnd - uRangeStart + 1;
			break;
		}
		if (gap.start >= uRangeStart) {
			uint64 uEnd = (gap.end > uRangeEnd) ? uRangeEnd : gap.end;
			uTotalGapSize += uEnd - gap.start + 1;
		} else if (gap.end >= uRangeStart && gap.end <= uRangeEnd)
			uTotalGapSize += gap.end - uRangeStart + 1;
	}
	ASSERT(uTotalGapSize <= uRangeEnd - uRangeStart + 1);
	return uTotalGapSize;
}

uint64 CPartFile::GetTotalGapSizeInPart(UINT uPart) const
{
	//the called method will adjust the range end
	return GetTotalGapSizeInRange(uPart * PARTSIZE, uPart * PARTSIZE + PARTSIZE - 1);
}

bool CPartFile::GetNextEmptyBlockInPart(UINT partNumber, Requested_Block_Struct *pReqBlock) const
{
	// Find start of this part
	uint64 partStart = PARTSIZE * partNumber;
	uint64 start = partStart;

	// The end of the part must be within file size
	uint64 partEnd = min(partStart + PARTSIZE, (uint64)m_nFileSize) - 1;
	ASSERT(partStart <= partEnd);

	// Loop until a suitable gap is found and return true, or no more gaps and return false
	for (;;) {
		const Gap_Struct *firstGap = NULL;
		// Find the first gap from the start position
		for (POSITION pos = m_gaplist.GetHeadPosition(); pos != NULL;) {
			const Gap_Struct &gap = m_gaplist.GetNext(pos);
			// Want gaps that overlap start<->partEnd
			if (gap.start > partEnd)
				break;
			if (gap.end >= start) {
				firstGap = &gap; //here gap.start <= partEnd
				break;
			}
		}
		// If no gaps after start, exit
		if (firstGap == NULL)
			return false;

		// Update start position if gap starts after the current pos
		if (start < firstGap->start)
			start = firstGap->start;

		// Find end, keeping within the max block size and the part limit
		uint64 end = min(firstGap->end, partEnd);
		uint64 blockLimit = partStart + ((start - partStart) / EMBLOCKSIZE + 1) * EMBLOCKSIZE - 1;
		if (end > blockLimit)
			end = blockLimit;

		// If this gap has not already been requested, we have found a valid entry
		if (!IsAlreadyRequested(start, end, true)) {
			// Was this block to be returned
			if (pReqBlock != NULL) {
				pReqBlock->StartOffset = start;
				pReqBlock->EndOffset = end;
				md4cpy(pReqBlock->FileID, GetFileHash());
				pReqBlock->transferred = 0;
			}
			return true;
		}

		uint64 tempStart = start;
		uint64 tempEnd = end;
		if (ShrinkToAvoidAlreadyRequested(tempStart, tempEnd)) {
			AddDebugLogLine(false, _T("Shrunk interval to prevent collision with already requested block: Old interval %I64u-%I64u. New interval: %I64u-%I64u. File %s."), start, end, tempStart, tempEnd, (LPCTSTR)GetFileName());

			// Was this block to be returned
			if (pReqBlock != NULL) {
				pReqBlock->StartOffset = tempStart;
				pReqBlock->EndOffset = tempEnd;
				md4cpy(pReqBlock->FileID, GetFileHash());
				pReqBlock->transferred = 0;
			}
			return true;
		}

		// Break from the loop if tried all gaps
		if (end >= partEnd)
			break;
		// Reposition to the next gap
		start = end + 1;
	}

	// No suitable gap found
	return false;
}

void CPartFile::FillGap(uint64 start, uint64 end)
{
	ASSERT(end < (uint64)m_nFileSize && start <= end);

	for (POSITION pos = m_gaplist.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		Gap_Struct &gap = m_gaplist.GetNext(pos);
		if (gap.start > end)
			break;
		if (gap.end >= start) {
			if (gap.start >= start && gap.end <= end) {
				m_gaplist.RemoveAt(pos2); //this gap is fully filled
			} else if (gap.start >= start)
				gap.start = end + 1; //cut the head of this gap
			else if (gap.end <= end)
				gap.end = start - 1; //cut the tail of this gap
			else {
				uint64 prev = gap.end; //this gap fully includes the filler
				gap.end = start - 1;   //cut the tail, then add the rest
				m_gaplist.InsertAfter(pos2, Gap_Struct{end + 1, prev});
				break; // [Lord KiRon]
			}
		}
	}

	UpdateCompletedInfos();
	UpdateDisplayedInfo();
}

void CPartFile::UpdateCompletedInfos()
{
	uint64 allgaps = 0;
	for (POSITION pos = m_gaplist.GetHeadPosition(); pos != NULL;) {
		const Gap_Struct &gap = m_gaplist.GetNext(pos);
		allgaps += gap.end - gap.start + 1;
	}

	UpdateCompletedInfos(allgaps);
}

void CPartFile::UpdateCompletedInfos(uint64 uTotalGaps)
{
	if (uTotalGaps > (uint64)m_nFileSize) {
		ASSERT(0);
		uTotalGaps = (uint64)m_nFileSize;
	}

	if (m_gaplist.IsEmpty()) {
		m_percentcompleted = 100.0F;
		m_completedsize = (uint64)m_nFileSize;
	} else {
		// 'm_percentcompleted' is only used in GUI, round down to avoid showing "100%" in case
		// we actually have only "99.9%"
		m_percentcompleted = (float)(floor((1.0 - (double)uTotalGaps / (uint64)m_nFileSize) * 1000.0) / 10.0);
		m_completedsize = (uint64)m_nFileSize - uTotalGaps;
	}
}

void CPartFile::DrawShareStatusBar(CDC &dc, LPCRECT rect, bool onlygreyrect, bool bFlat) const
{
	if (!IsPartFile()) {
		CKnownFile::DrawShareStatusBar(dc, rect, onlygreyrect, bFlat);
		return;
	}

	static const COLORREF crNotShared = RGB(224, 224, 224);
	s_ChunkBar.SetFileSize(m_nFileSize);
	s_ChunkBar.SetRect(rect);
	s_ChunkBar.Fill(crNotShared);

	if (!onlygreyrect) {
		static const COLORREF crMissing = RGB(255, 0, 0);
		const COLORREF crNooneAsked(bFlat ? RGB(0, 0, 0) : RGB(104, 104, 104));
		for (UINT i = GetPartCount(); i-- > 0;) {
			uint64 uBegin = PARTSIZE * i;
			if (IsCompleteBDSafe(uBegin, uBegin + PARTSIZE - 1)) {
				COLORREF colour;
				bool bActive = (GetStatus() != PS_PAUSED);
				if (bActive || !m_ClientUploadList.IsEmpty() || m_nCompleteSourcesCountHi > 0) {
					uint16 frequency;
					if (bActive && i < (UINT)m_SrcPartFrequency.GetSize())
						frequency = m_SrcPartFrequency[i];
					else if (m_AvailPartFrequency.IsEmpty())
						frequency = m_nCompleteSourcesCountLo;
					else
						frequency = max(m_AvailPartFrequency[i], m_nCompleteSourcesCountLo);

					if (frequency > 0)
						colour = RGB(0, (22 * frequency >= 232 ? 0 : 232 - 22 * frequency), 255);
					else
						colour = crMissing;
				} else
					colour = crNooneAsked;
				s_ChunkBar.FillRange(uBegin, uBegin + PARTSIZE, colour);
			}
		}
	}
	s_ChunkBar.Draw(dc, rect->left, rect->top, bFlat);
}

void CPartFile::DrawStatusBar(CDC &dc, const CRect &rect, bool bFlat) /*const*/
{
	COLORREF crProgress, crProgressBk, crHave, crPending, crMissing;

	EPartFileStatus eVirtualState = GetStatus();
	bool bNotGrey = eVirtualState == PS_EMPTY || eVirtualState == PS_COMPLETE || eVirtualState == PS_READY;

	if (g_bLowColorDesktop) {
		bFlat = true;
		// use straight Windows colors
		crProgress = RGB(0, 255, 0);
		crProgressBk = RGB(192, 192, 192);
		if (bNotGrey) {
			crMissing = RGB(255, 0, 0);
			crHave = RGB(0, 0, 0);
			crPending = RGB(255, 255, 0);
		} else {
			crMissing = RGB(128, 0, 0);
			crHave = RGB(128, 128, 128);
			crPending = RGB(128, 128, 0);
		}
	} else {
		crProgress = RGB(0, (bFlat ? 150 : 224), 0);
		crProgressBk = RGB(224, 224, 224);
		if (bNotGrey) {
			crMissing = RGB(255, 0, 0);
			crHave = bFlat ? RGB(0, 0, 0) : RGB(104, 104, 104);
			crPending = RGB(255, 208, 0);
		} else {
			crMissing = RGB(191, 64, 64);
			crHave = bFlat ? RGB(64, 64, 64) : RGB(116, 116, 116);
			crPending = RGB(191, 168, 64);
		}
	}

	s_ChunkBar.SetRect(rect);
	s_ChunkBar.SetFileSize((uint64)m_nFileSize);
	s_ChunkBar.Fill(crHave);

	if (status == PS_COMPLETE || status == PS_COMPLETING) {
		m_percentcompleted = 100.0F;
		m_completedsize = (uint64)m_nFileSize;
		s_ChunkBar.FillRange(0, m_completedsize, crProgress);
		s_ChunkBar.Draw(dc, rect.left, rect.top, bFlat);
	} else if (eVirtualState == PS_INSUFFICIENT || status == PS_ERROR) {
		int iOldBkColor = dc.SetBkColor(RGB(255, 255, 0));
		if (theApp.m_brushBackwardDiagonal.m_hObject)
			dc.FillRect(rect, &theApp.m_brushBackwardDiagonal);
		else
			dc.FillSolidRect(rect, RGB(255, 255, 0));
		dc.SetBkColor(iOldBkColor);

		UpdateCompletedInfos();
	} else {
		// red gaps
		uint64 allgaps = 0;
		UINT i = 0;
		for (POSITION pos = m_gaplist.GetHeadPosition(); pos != NULL;) {
			const Gap_Struct &gap = m_gaplist.GetNext(pos);
			allgaps += gap.end - gap.start + 1;
			uint64 start = gap.start;
			uint64 end = gap.end;
			for (; i < GetPartCount(); ++i) {
				const uint64 uEnd = i * PARTSIZE + PARTSIZE - 1;
				if (start >= PARTSIZE * i && start <= uEnd) { // is in this part?
					bool gapdone = (end <= uEnd);
					if (!gapdone)
						end = uEnd; // and next part

					// paint
					COLORREF color;
					if (i < (UINT)m_SrcPartFrequency.GetCount() && m_SrcPartFrequency[i]) {
						uint16 freq = m_SrcPartFrequency[i] - 1;
						if (g_bLowColorDesktop) {
							if (bNotGrey)
								color = RGB(0, (freq < 5 ? 255 : 0), 255);
							else
								color = RGB(0, 128, 128);
						} else {
							if (bNotGrey)
								color = RGB(0, (210 - 22 * freq < 0) ? 0 : 210 - 22 * freq, 255);
							else
								color = RGB(64, (169 - 11 * freq < 64) ? 64 : 169 - 11 * freq, 191);
						}
					} else
						color = crMissing;
					s_ChunkBar.FillRange(start, end + 1, color);

					if (gapdone) // finished?
						break;

					start = end + 1;
					end = gap.end;
				}
			}
		}

		// yellow pending parts
		for (POSITION pos = requestedblocks_list.GetHeadPosition(); pos != NULL;) {
			const Requested_Block_Struct *block = requestedblocks_list.GetNext(pos);
			s_ChunkBar.FillRange(block->StartOffset + block->transferred, block->EndOffset + 1, crPending);
		}

		s_ChunkBar.Draw(dc, rect.left, rect.top, bFlat);

		// green progress
		float blockpixel = (float)rect.Width() / (uint64)m_nFileSize;
		float width = ((uint64)m_nFileSize - allgaps) * blockpixel + 0.5f;
		if (!bFlat) {
			s_LoadBar.SetWidth((int)width);
			s_LoadBar.Fill(crProgress);
			s_LoadBar.Draw(dc, rect.left, rect.top, false);
		} else {
			RECT gaprect{rect.left, rect.top, rect.left + (LONG)width, rect.top + PROGRESS_HEIGHT};
			dc.FillSolidRect(&gaprect, crProgress);
			//draw gray progress only if flat
			gaprect.left = gaprect.right;
			gaprect.right = rect.right;
			dc.FillSolidRect(&gaprect, crProgressBk);
		}

		UpdateCompletedInfos(allgaps);
	}

	// additionally show any file op progress (needed for PS_COMPLETING and PS_WAITINGFORHASH)
	if (GetFileOp() != PFOP_NONE) {
		CRect rcFileOpProgress(rect);
		LONG width = (LONG)(GetFileOpProgress() * rcFileOpProgress.Width() / 100.0F + 0.5F);
		rcFileOpProgress.bottom = rcFileOpProgress.top + PROGRESS_HEIGHT;
		if (!bFlat) {
			s_LoadBar.SetWidth((int)width);
			s_LoadBar.Fill(RGB(255, 208, 0));
			s_LoadBar.Draw(dc, rcFileOpProgress.left, rcFileOpProgress.top, false);
		} else {
			rcFileOpProgress.right = rcFileOpProgress.left + width;
			dc.FillSolidRect(&rcFileOpProgress, RGB(255, 208, 0));
			rcFileOpProgress.left = rcFileOpProgress.right;
			rcFileOpProgress.right = rect.right;
			dc.FillSolidRect(&rcFileOpProgress, crProgressBk);
		}
	}
}

void CPartFile::WritePartStatus(CSafeMemFile &file) const
{
	uint16 uED2KPartCount = GetED2KPartCount();
	file.WriteUInt16(uED2KPartCount);

	for (UINT uPart = 0; uPart < uED2KPartCount;) {
		uint8 towrite = 0;
		for (UINT i = 0; i < 8 && uPart < uED2KPartCount; ++i) {
			if (IsCompleteBD(uPart))
				towrite |= 1 << i;
			++uPart;
		}
		file.WriteUInt8(towrite);
	}
}

void CPartFile::WriteCompleteSourcesCount(CSafeMemFile &file) const
{
	file.WriteUInt16(m_nCompleteSourcesCount);
}

int CPartFile::GetValidSourcesCount() const
{
	int counter = 0;
	for (POSITION pos = srclist.GetHeadPosition(); pos != NULL;)
		switch (srclist.GetNext(pos)->GetDownloadState()) {
		case DS_ONQUEUE:
		case DS_DOWNLOADING:
		case DS_CONNECTED:
		case DS_REMOTEQUEUEFULL:
			++counter;
		}

	return counter;
}

UINT CPartFile::GetNotCurrentSourcesCount() const
{
	UINT counter = 0;
	for (POSITION pos = srclist.GetHeadPosition(); pos != NULL;)
		switch (srclist.GetNext(pos)->GetDownloadState()) {
		case DS_ONQUEUE:
		case DS_DOWNLOADING:
			continue;
		default:
			++counter;
		}

	return counter;
}

uint64 CPartFile::GetNeededSpace() const
{
	// Do a safety check, though it should never happen
	return ((uint64)m_nFileSize > m_hpartfile.GetLength()) ? (uint64)m_nFileSize - m_hpartfile.GetLength() : 0;
}

EPartFileStatus CPartFile::GetStatus(bool ignorepause) const
{
	if ((!m_paused && !m_insufficient) || status == PS_ERROR || status == PS_COMPLETING || status == PS_COMPLETE || ignorepause)
		return status;
	return m_paused ? PS_PAUSED : PS_INSUFFICIENT;
}

void CPartFile::AddDownloadingSource(CUpDownClient *client)
{
	POSITION pos = m_downloadingSourceList.Find(client); // to be sure
	if (pos == NULL) {
		m_downloadingSourceList.AddTail(client);
		theApp.emuledlg->transferwnd->GetDownloadClientsList()->AddClient(client);
	}
}

void CPartFile::RemoveDownloadingSource(CUpDownClient *client)
{
	POSITION pos = m_downloadingSourceList.Find(client); // to be sure
	if (pos != NULL) {
		m_downloadingSourceList.RemoveAt(pos);
		theApp.emuledlg->transferwnd->GetDownloadClientsList()->RemoveClient(client);
	}
}

uint32 CPartFile::Process(uint32 reducedownload, UINT icounter/*in percent*/)
{
	if (thePrefs.m_iDbgHeap >= 2)
		ASSERT_VALID(this);

	UINT nOldTransSourceCount = GetSrcStatisticsValue(DS_DOWNLOADING);
	const DWORD curTick = ::GetTickCount();
	if (curTick < m_nLastBufferFlushTime) {
		ASSERT(0);
		m_nLastBufferFlushTime = curTick;
	}

	// If buffer size exceeds limit, or if not written within time limit, flush data
	if (m_nTotalBufferData > thePrefs.GetFileBufferSize() || curTick >= m_nLastBufferFlushTime + thePrefs.GetFileBufferTimeLimit())
		FlushBuffer();
	//If data keeps arriving, flush to disk sometimes for extra safety
	if (m_nFileFlushTime && curTick >= m_nFileFlushTime + SEC2MS(31) && m_hWrite != INVALID_HANDLE_VALUE) {
		::FlushFileBuffers(m_hWrite);
		m_nFileFlushTime = 0;
	}

	m_datarate = 0;

	// calculate data rate, set limit etc.
	if (icounter < 10) {
		uint32 cur_datarate;
		for (POSITION pos = m_downloadingSourceList.GetHeadPosition(); pos != NULL;) {
			CUpDownClient *cur_src = m_downloadingSourceList.GetNext(pos);
			if (thePrefs.m_iDbgHeap >= 2)
				ASSERT_VALID(cur_src);
			if (cur_src && cur_src->GetDownloadState() == DS_DOWNLOADING) {
				ASSERT(cur_src->socket);
				if (cur_src->socket) {
					cur_src->CheckDownloadTimeout();
					cur_datarate = cur_src->CalculateDownloadRate();
					m_datarate += cur_datarate;
					if (reducedownload) {
						uint32 limit = reducedownload * cur_datarate / 1000;
						if (limit < 1000 && reducedownload >= 200)
							limit += 1000;
						else if (limit < 200 && cur_datarate == 0 && reducedownload >= 100)
							limit = 200;
						else if (limit < 60 && cur_datarate < 600 && reducedownload >= 97)
							limit = 60;
						else if (limit < 20 && cur_datarate < 200 && reducedownload >= 93)
							limit = 20;
						else if (limit < 1)
							limit = 1;
						cur_src->socket->SetDownloadLimit(limit);
					}
				}
			}
		}
	} else {
		bool downloadingbefore = m_anStates[DS_DOWNLOADING] > 0;
		// -khaos--+++> Moved this here, otherwise we were setting our permanent variables to 0 every tenth of a second...
		memset(m_anStates, 0, sizeof m_anStates);
		memset(src_stats, 0, sizeof src_stats);
		memset(net_stats, 0, sizeof net_stats);

		for (POSITION pos = srclist.GetHeadPosition(); pos != NULL;) {
			CUpDownClient *cur_src = srclist.GetNext(pos);
			if (thePrefs.m_iDbgHeap >= 2)
				ASSERT_VALID(cur_src);

			// BEGIN -rewritten- refreshing statistics (no need for temp vars since it is not multi-threaded)
			UINT nCountForState = cur_src->GetDownloadState();
			//special case which is not yet set as download state
			if (nCountForState == DS_ONQUEUE && cur_src->IsRemoteQueueFull())
				nCountForState = DS_REMOTEQUEUEFULL;

			// this is a performance killer -> avoid calling 'IsBanned' for gathering stats
			//if (cur_src->IsBanned())
			//	nCountForState = DS_BANNED;
			if (cur_src->GetUploadState() == US_BANNED) // not as accurate as 'IsBanned', but way faster and good enough for stats.
				nCountForState = DS_BANNED;

			if (cur_src->GetSourceFrom() >= SF_SERVER && cur_src->GetSourceFrom() <= SF_PASSIVE)
				++src_stats[cur_src->GetSourceFrom()];

			if (cur_src->GetServerIP() && cur_src->GetServerPort()) {
				++net_stats[0];
				if (cur_src->GetKadPort())
					++net_stats[2];
			}
			if (cur_src->GetKadPort())
				++net_stats[1];

			ASSERT(nCountForState < _countof(m_anStates));
			++m_anStates[nCountForState];

			switch (cur_src->GetDownloadState()) {
			case DS_DOWNLOADING:
				ASSERT(cur_src->socket);
				if (cur_src->socket) {
					cur_src->CheckDownloadTimeout();
					uint32 cur_datarate = cur_src->CalculateDownloadRate();
					m_datarate += cur_datarate;
					if (reducedownload && cur_src->GetDownloadState() == DS_DOWNLOADING) {
						uint32 limit = reducedownload * cur_datarate / 1000; //(uint32)(((float)reducedownload/100) * cur_datarate)/10;
						if (limit < 1000 && reducedownload == 200)
							limit += 1000;
						else if (limit < 200 && cur_datarate == 0 && reducedownload >= 100)
							limit = 200;
						else if (limit < 60 && cur_datarate < 600 && reducedownload >= 97)
							limit = 60;
						else if (limit < 20 && cur_datarate < 200 && reducedownload >= 93)
							limit = 20;
						else if (limit < 1)
							limit = 1;
						cur_src->socket->SetDownloadLimit(limit);
					} else
						cur_src->socket->DisableDownloadLimit();
				}
				break;
			case DS_BANNED: // Do nothing with this client.
				break;
			case DS_LOWTOLOWIP: // Check if something has changed with our or their ID state.
				// To Mods, please stop instantly removing these sources.
				// This causes sources to pop in and out creating extra overhead!
				//Make sure this source is still a LowID Client, and we still cannot callback to this Client.
				if (cur_src->HasLowID() && !theApp.CanDoCallback(cur_src)) {
					//If we are almost maxed on sources, slowly remove these client to see if we can find a better source.
					if ((curTick >= m_lastpurgetime + SEC2MS(30)) && (GetSourceCount() >= GetMaxSources() * 4 / 5)) {
						theApp.downloadqueue->RemoveSource(cur_src);
						m_lastpurgetime = curTick;
					}
					break;
				}
				// This should no longer be a LOWTOLOWIP.
				cur_src->SetDownloadState(DS_ONQUEUE);
				break;
			case DS_NONEEDEDPARTS:
				// To Mods, please stop instantly removing these sources.
				// This causes sources to pop in and out creating extra overhead!
				if (curTick >= m_lastpurgetime + SEC2MS(40)) {
					m_lastpurgetime = curTick;
					// we only delete them if reaching the limit
					if (GetSourceCount() >= (GetMaxSources() * 4 / 5)) {
						theApp.downloadqueue->RemoveSource(cur_src);
						break;
					}
				}
				// doubled re-ask time for no needed parts - save connections and traffic
				if (cur_src->GetTimeUntilReask() == 0) {
					cur_src->SwapToAnotherFile(_T("A4AF for NNP file. CPartFile::Process()"), true, false, false, NULL, true, true); // ZZ:DownloadManager
					// Recheck this client to see if still NNP. Set to DS_NONE so that we force a TCP re-ask next time.
					cur_src->SetDownloadState(DS_ONQUEUE); //DS_NONE could be deleted in CleanUpClientList
				}
				break;
			case DS_ONQUEUE:
				// To Mods, please stop instantly removing these sources.
				// This causes sources to pop in and out creating extra overhead!
				if (cur_src->IsRemoteQueueFull()) {
					if ((curTick >= m_lastpurgetime + MIN2MS(1)) && (GetSourceCount() >= (GetMaxSources() * 4 / 5))) {
						theApp.downloadqueue->RemoveSource(cur_src);
						m_lastpurgetime = curTick;
						break;
					}
				}
				//Allow up to 1 min for UDP to respond. If we are within one min of TCP re-ask, do not try.
				if (theApp.IsConnected() && cur_src->GetTimeUntilReask() < MIN2MS(2) && cur_src->GetTimeUntilReask() > SEC2MS(1) && curTick >= cur_src->GetLastTriedToConnectTime() + MIN2MS(20)) // ZZ:DownloadManager (one re-ask timestamp for each file)
					cur_src->UDPReaskForDownload();

			case DS_CONNECTING:
			case DS_TOOMANYCONNS:
			case DS_TOOMANYCONNSKAD:
			case DS_NONE:
			case DS_WAITCALLBACK:
			case DS_WAITCALLBACKKAD:
				if (theApp.IsConnected() && cur_src->GetTimeUntilReask() == 0) { // ZZ:DownloadManager (one re-ask timestamp for each file)
					if (cur_src->socket && cur_src->socket->IsConnected() && cur_src->CheckHandshakeFinished() && cur_src->GetUploadState() != US_BANNED) {
						// netfinity: Ask immediately if already connected and if allowed, or we may lose the source
						cur_src->SetDownloadState(DS_CONNECTED);
						cur_src->SetLastTriedToConnectTime();
						cur_src->SendFileRequest();
					} else if (curTick >= cur_src->GetLastTriedToConnectTime() + MIN2MS(20)) {
						if (!cur_src->AskForDownload()) // NOTE: This may *delete* the client!!
							break; //I left this break here as a reminder in case of re-arranging things.
					}
				}
			}
		}
		if (downloadingbefore != (m_anStates[DS_DOWNLOADING] > 0))
			NotifyStatusChange();

		if (GetMaxSourcePerFileUDP() > GetSourceCount()) {
			if (theApp.downloadqueue->DoKademliaFileRequest() && (Kademlia::CKademlia::GetTotalFile() < KADEMLIATOTALFILE) && (curTick >= m_LastSearchTimeKad) && Kademlia::CKademlia::IsConnected() && theApp.IsConnected() && !m_stopped) { //Once we can handle lowID users in Kad, we remove the second IsConnected
				//Kademlia
				theApp.downloadqueue->SetLastKademliaFileRequest();
				if (!GetKadFileSearchID()) {
					Kademlia::CSearch *pSearch = Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::FILE, true, Kademlia::CUInt128(GetFileHash()));
					if (pSearch) {
						if (m_TotalSearchesKad < 7)
							++m_TotalSearchesKad;
						m_LastSearchTimeKad = curTick + (KADEMLIAREASKTIME * m_TotalSearchesKad);
						pSearch->SetGUIName((CStringW)GetFileName());
						SetKadFileSearchID(pSearch->GetSearchID());
					} else
						SetKadFileSearchID(0);
				}
			}
		} else if (GetKadFileSearchID())
			Kademlia::CSearchManager::StopSearch(GetKadFileSearchID(), true);

		// check if we want new sources from server
		if (!m_bLocalSrcReqQueued && (!m_LastSearchTime || curTick >= m_LastSearchTime + SERVERREASKTIME) && theApp.serverconnect->IsConnected()
			&& GetMaxSourcePerFileSoft() > GetSourceCount() && !m_stopped
			&& (!IsLargeFile() || (theApp.serverconnect->GetCurrentServer() != NULL && theApp.serverconnect->GetCurrentServer()->SupportsLargeFilesTCP())))
		{
			m_bLocalSrcReqQueued = true;
			theApp.downloadqueue->SendLocalSrcRequest(this);
		}

		if (++m_refresh >= 3) {
			m_refresh = 0;
			UpdateAutoDownPriority();
			UpdateDisplayedInfo();
			UpdateCompletedInfos();
		}
	}

	if (GetSrcStatisticsValue(DS_DOWNLOADING) != nOldTransSourceCount) {
		if (theApp.emuledlg->transferwnd->GetDownloadList()->m_curTab == 0)
			theApp.emuledlg->transferwnd->GetDownloadList()->ChangeCategory(0);
		else
			UpdateDisplayedInfo(true);
		if (thePrefs.ShowCatTabInfos())
			theApp.emuledlg->transferwnd->UpdateCatTabTitles();
	}

	return m_datarate;
}

bool CPartFile::CanAddSource(uint32 userid, uint16 port, uint32 serverip, uint16 serverport, UINT *pdebug_lowiddropped, bool ed2kID)
{
	//The incoming ID could have the userid in the Hybrid format.
	uint32 hybridID;
	if (ed2kID)
		hybridID = ::IsLowID(userid) ? userid : htonl(userid);
	else {
		hybridID = userid;
		if (!::IsLowID(userid))
			userid = htonl(userid);
	}

	// MOD Note: Do not change this part - Merkur
	if (theApp.serverconnect->IsConnected()) {
		if (theApp.serverconnect->IsLowID()) {
			if (theApp.serverconnect->GetClientID() == userid && theApp.serverconnect->GetCurrentServer()->GetIP() == serverip && theApp.serverconnect->GetCurrentServer()->GetPort() == serverport)
				return false;
			if (theApp.serverconnect->GetLocalIP() == userid)
				return false;
		} else if (theApp.serverconnect->GetClientID() == userid && thePrefs.GetPort() == port)
			return false;
	}
	if (Kademlia::CKademlia::IsConnected())
		if (!Kademlia::CKademlia::IsFirewalled())
			if (Kademlia::CKademlia::GetIPAddress() == hybridID && thePrefs.GetPort() == port)
				return false;

	//This allows *.*.*.0 clients to not be removed if Ed2kID == false
	if (::IsLowID(hybridID) && theApp.IsFirewalled()) {
		if (pdebug_lowiddropped)
			++(*pdebug_lowiddropped);
		return false;
	}
	// MOD Note - end
	return true;
}

void CPartFile::AddSources(CSafeMemFile *sources, uint32 serverip, uint16 serverport, bool bWithObfuscationAndHash)
{
	UINT ucount = sources->ReadUInt8();

	UINT debug_lowiddropped = 0;
	UINT debug_possiblesources = 0;
	uchar achUserHash[MDX_DIGEST_SIZE];
	bool bSkip = false;
	for (UINT i = 0; i < ucount; ++i) {
		uint32 userid = sources->ReadUInt32();
		uint16 port = sources->ReadUInt16();
		uint8 byCryptOptions = 0;
		if (bWithObfuscationAndHash) {
			byCryptOptions = sources->ReadUInt8();
			if ((byCryptOptions & 0x80) != 0)
				sources->ReadHash16(achUserHash);

			if ((thePrefs.IsCryptLayerPreferred() && (byCryptOptions & 0x01/*supported*/) > 0 && (byCryptOptions & 0x80) == 0)
				|| (thePrefs.IsCryptLayerEnabled() && (byCryptOptions & 0x02/*requested*/) > 0 && (byCryptOptions & 0x80) == 0))
			{
				DebugLogWarning(_T("Server didn't provide UserHash for source %u, even if it was expected to (or local obfuscation settings changed during server connect)"), userid);
			} else if (!thePrefs.IsCryptLayerPreferred() && (byCryptOptions & 0x02/*requested*/) == 0 && (byCryptOptions & 0x80) != 0)
				DebugLogWarning(_T("Server provided UserHash for source %u, even if it wasn't expected to (or local obfuscation settings changed during server connect)"), userid);
		}

		// since we may receive multiple UDP source search results, we have to "consume" all data of that packet
		if (m_stopped || bSkip)
			continue;

		// check the HighID(IP) - "Filter LAN IPs" and "IPfilter" the received sources IP addresses
		if (!::IsLowID(userid)) {
			if (!IsGoodIP(userid)) {
				// check for 0-IP, localhost and optionally for LAN addresses
				//if (thePrefs.GetLogFilteredIPs())
				//	AddDebugLogLine(false, _T("Ignored source (IP=%s) received from server - bad IP"), (LPCTSTR)ipstr(userid));
				continue;
			}
			if (theApp.ipfilter->IsFiltered(userid)) {
				if (thePrefs.GetLogFilteredIPs())
					AddDebugLogLine(false, _T("Ignored source (IP=%s) received from server - IP filter (%s)"), (LPCTSTR)ipstr(userid), (LPCTSTR)theApp.ipfilter->GetLastHit());
				continue;
			}
			if (theApp.clientlist->IsBannedClient(userid)) {
#ifdef _DEBUG
				if (thePrefs.GetLogBannedClients()) {
					CUpDownClient *pClient = theApp.clientlist->FindClientByIP(userid);
					if (pClient)
						AddDebugLogLine(false, _T("Ignored source (IP=%s) received from server - banned client %s"), (LPCTSTR)ipstr(userid), (LPCTSTR)pClient->DbgGetClientInfo());
				}
#endif
				continue;
			}
		}

		// additionally check for LowID and own IP
		if (!CanAddSource(userid, port, serverip, serverport, &debug_lowiddropped)) {
			//if (thePrefs.GetLogFilteredIPs())
			//	AddDebugLogLine(false, _T("Ignored source (IP=%s) received from server"), (LPCTSTR)ipstr(userid));
			continue;
		}

		if (GetMaxSources() > GetSourceCount()) {
			++debug_possiblesources;
			CUpDownClient *newsource = new CUpDownClient(this, port, userid, serverip, serverport, true);
			newsource->SetConnectOptions(byCryptOptions, true, false);

			if ((byCryptOptions & 0x80) != 0)
				newsource->SetUserHash(achUserHash);
			theApp.downloadqueue->CheckAndAddSource(this, newsource);
		} else {
			// since we may received multiple search source UDP results we have to "consume" all data of that packet
			bSkip = true;
			if (GetKadFileSearchID())
				Kademlia::CSearchManager::StopSearch(GetKadFileSearchID(), false);
			continue;
		}
	}
	if (thePrefs.GetDebugSourceExchange())
		AddDebugLogLine(false, _T("SXRecv: Server source response; Count=%u, Dropped=%u, PossibleSources=%u, File=\"%s\""), ucount, debug_lowiddropped, debug_possiblesources, (LPCTSTR)GetFileName());
}

void CPartFile::AddSource(LPCTSTR pszURL, uint32 nIP)
{
	if (m_stopped)
		return;

	if (!IsGoodIP(nIP)) {
		// check for 0-IP, localhost and optionally for LAN addresses
		//if (thePrefs.GetLogFilteredIPs())
		//	AddDebugLogLine(false, _T("Ignored URL source (IP=%s) \"%s\" - bad IP"), (LPCTSTR)ipstr(nIP), pszURL);
		return;
	}
	if (theApp.ipfilter->IsFiltered(nIP)) {
		if (thePrefs.GetLogFilteredIPs())
			AddDebugLogLine(false, _T("Ignored URL source (IP=%s) \"%s\" - IP filter (%s)"), (LPCTSTR)ipstr(nIP), pszURL, (LPCTSTR)theApp.ipfilter->GetLastHit());
		return;
	}

	CUrlClient *client = new CUrlClient;
	if (!client->SetUrl(pszURL, nIP)) {
		LogError(LOG_STATUSBAR, _T("Failed to process URL source \"%s\""), pszURL);
		delete client;
		return;
	}
	client->SetRequestFile(this);
	client->SetSourceFrom(SF_LINK);
	if (theApp.downloadqueue->CheckAndAddSource(this, client))
		UpdatePartsInfo();
}

void CPartFile::UpdatePartsInfo()
{
	if (!IsPartFile()) {
		CKnownFile::UpdatePartsInfo();
		return;
	}
	time_t tNow = time(NULL);
	bool bRefresh = (tNow - m_tCompleteSourcesTime > 0);

	// Reset part counters
	m_SrcPartFrequency.SetSize(GetPartCount());
	memset(&m_SrcPartFrequency[0], 0, GetPartCount() * sizeof m_SrcPartFrequency[0]);

	CArray<uint16, uint16> acount;
	if (bRefresh)
		acount.SetSize(0, srclist.GetCount());
	for (POSITION pos = srclist.GetHeadPosition(); pos != NULL;) {
		const CUpDownClient *cur_src = srclist.GetNext(pos);
		if (cur_src->GetPartStatus()) {
			for (INT_PTR i = GetPartCount(); --i >= 0;)
				m_SrcPartFrequency[i] += static_cast<uint16>(cur_src->IsPartAvailable((UINT)i));

			if (bRefresh)
				acount.Add(cur_src->GetUpCompleteSourcesCount());
		}
	}

	if (bRefresh) {
		m_nCompleteSourcesCountLo = m_nCompleteSourcesCountHi = 0;
		if (!GetPartCount())
			m_nCompleteSourcesCount = 0;
		else {
			m_nCompleteSourcesCount = _UI16_MAX;
			for (INT_PTR i = GetPartCount(); --i >= 0;)
				if (m_nCompleteSourcesCount > m_SrcPartFrequency[i])
					m_nCompleteSourcesCount = m_SrcPartFrequency[i];
		}
		acount.Add(m_nCompleteSourcesCount);

		int n = (int)acount.GetCount();
		if (n > 0) {
			// SLUGFILLER: heapsortCompletesrc
			for (int r = n / 2; --r >= 0;)
				HeapSort(acount, r, n - 1);
			for (int r = n; --r;) {
				uint16 t = acount[r];
				acount[r] = acount[0];
				acount[0] = t;
				HeapSort(acount, 0, r - 1);
			}
			// SLUGFILLER: heapsortCompletesrc

			// calculate range
			int i = n >> 1;			// (n / 2)
			int j = (n * 3) >> 2;	// (n * 3) / 4
			int k = (n * 7) >> 3;	// (n * 7) / 8

			//When still a part file, adjust your guesses by 20% to what you see.

			if (n < 5) {
				//Not many sources, so just use what you see.
				//m_nCompleteSourcesCount;
				m_nCompleteSourcesCountHi = m_nCompleteSourcesCountLo = m_nCompleteSourcesCount;
			} else if (n < 20) {
				//For low guess and normal guess count
				//	If we see more sources then the guessed low and normal, use what we see.
				//	If we see less sources then the guessed low, adjust network accounts for 80%, we account for 20% with what we see and make sure we are still above the normal.
				//For high guess
				//  Adjust 80% network and 20% what we see.
				if (acount[i] < m_nCompleteSourcesCount)
					m_nCompleteSourcesCountLo = m_nCompleteSourcesCount;
				else
					m_nCompleteSourcesCountLo = (acount[i] * 4 + m_nCompleteSourcesCount) / 5;
				m_nCompleteSourcesCount = m_nCompleteSourcesCountLo;
				m_nCompleteSourcesCountHi = (acount[j] * 4 + m_nCompleteSourcesCount) / 5;
				if (m_nCompleteSourcesCountHi < m_nCompleteSourcesCount)
					m_nCompleteSourcesCountHi = m_nCompleteSourcesCount;
			} else {
				//Many sources.
				//For low guess
				//	Use what we see.
				//For normal guess
				//	Adjust network accounts for 80%, we account for 20% with what we see and make sure we are still above the low.
				//For high guess
				//  Adjust network accounts for 80%, we account for 20% with what we see and make sure we are still above the normal.
				m_nCompleteSourcesCountLo = m_nCompleteSourcesCount;
				m_nCompleteSourcesCount = (acount[j] * 4 + m_nCompleteSourcesCount) / 5;
				if (m_nCompleteSourcesCount < m_nCompleteSourcesCountLo)
					m_nCompleteSourcesCount = m_nCompleteSourcesCountLo;
				m_nCompleteSourcesCountHi = (acount[k] * 4 + m_nCompleteSourcesCount) / 5;
				if (m_nCompleteSourcesCountHi < m_nCompleteSourcesCount)
					m_nCompleteSourcesCountHi = m_nCompleteSourcesCount;
			}
		}
		m_tCompleteSourcesTime = tNow + MIN2S(1);
	}
	UpdateDisplayedInfo();
}

bool CPartFile::RemoveBlockFromList(uint64 start, uint64 end)
{
	ASSERT(start <= end);

	for (POSITION pos = requestedblocks_list.GetHeadPosition(); pos != NULL;) {
		POSITION posLast = pos;
		const Requested_Block_Struct *block = requestedblocks_list.GetNext(pos);
		if (block->StartOffset <= start && block->EndOffset >= end) {
			requestedblocks_list.RemoveAt(posLast);
			return true;
		}
	}
	return false;
}

void CPartFile::CompleteFile(bool bIsHashingDone)
{
	ASSERT(m_iWrites <= 0 && m_gaplist.IsEmpty() && m_BufferedData_list.IsEmpty());
	CPartFileWriteThread::RemFile(this);
	m_nFileFlushTime = 0;

	theApp.downloadqueue->RemoveLocalServerRequest(this);
	if (GetKadFileSearchID())
		Kademlia::CSearchManager::StopSearch(GetKadFileSearchID(), false);

	if (srcarevisible)
		theApp.emuledlg->transferwnd->GetDownloadList()->HideSources(this);

	if (!bIsHashingDone) {
		SetStatus(PS_COMPLETING);
		m_datarate = 0;
		CAddFileThread *addfilethread = static_cast<CAddFileThread*>(AfxBeginThread(RUNTIME_CLASS(CAddFileThread), THREAD_PRIORITY_BELOW_NORMAL, 0, CREATE_SUSPENDED));
		if (addfilethread) {
			const CString mytemppath(m_fullname, m_fullname.GetLength() - m_partmetfilename.GetLength());
			addfilethread->SetValues(NULL, mytemppath, RemoveFileExtension(m_partmetfilename), _T(""), this);
			SetFileOp(PFOP_HASHING);
			SetFileOpProgress(0);
			addfilethread->ResumeThread();
		} else {
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_FILECOMPLETIONTHREAD));
			SetStatus(PS_ERROR);
		}
	} else {
		StopFile();
		SetStatus(PS_COMPLETING);
		CWinThread *pThread = AfxBeginThread(CompleteThreadProc, this, THREAD_PRIORITY_BELOW_NORMAL, 0, CREATE_SUSPENDED); // Lord KiRon - using threads for file completion
		if (pThread) {
			SetFileOp(PFOP_COPYING);
			SetFileOpProgress(0);
			pThread->ResumeThread();

			theApp.emuledlg->transferwnd->GetDownloadList()->ShowFilesCount();
			if (thePrefs.ShowCatTabInfos())
				theApp.emuledlg->transferwnd->UpdateCatTabTitles();
			UpdateDisplayedInfo(true);
		} else {
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_FILECOMPLETIONTHREAD));
			SetStatus(PS_ERROR);
		}
	}
}

UINT CPartFile::CompleteThreadProc(LPVOID pvParams)
{
	DbgSetThreadName("PartFileComplete");
	InitThreadLocale();
	CPartFile *pFile = reinterpret_cast<CPartFile*>(pvParams);
	if (!pFile)
		return UINT_MAX;
	(void)::CoInitialize(NULL);
	pFile->PerformFileComplete();
	::CoUninitialize();
	return 0;
}

void UncompressFile(LPCTSTR pszFilePath, CPartFile *pPartFile)
{
	// check, if it's a compressed file
	DWORD dwAttr = ::GetFileAttributes(pszFilePath);
	if (dwAttr == INVALID_FILE_ATTRIBUTES || (dwAttr & FILE_ATTRIBUTE_COMPRESSED) == 0)
		return;

	CString strDir(pszFilePath);
	::PathRemoveFileSpec(strDir.GetBuffer());
	strDir.ReleaseBuffer();

	// If the directory of the file has the 'Compress' attribute, do not uncompress the file
	dwAttr = ::GetFileAttributes(strDir);
	if (dwAttr == INVALID_FILE_ATTRIBUTES || (dwAttr & FILE_ATTRIBUTE_COMPRESSED) != 0)
		return;

	HANDLE hFile = ::CreateFile(pszFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		if (thePrefs.GetVerbose())
			theApp.QueueDebugLogLine(true, _T("Failed to open file \"%s\" for decompressing - %s"), pszFilePath, (LPCTSTR)GetErrorMessage(::GetLastError(), 1));
		return;
	}

	if (pPartFile)
		pPartFile->SetFileOp(PFOP_UNCOMPRESSING);

	USHORT usInData = COMPRESSION_FORMAT_NONE;
	DWORD dwReturned = 0;
	if (!DeviceIoControl(hFile, FSCTL_SET_COMPRESSION, &usInData, sizeof usInData, NULL, 0, &dwReturned, NULL))
		if (thePrefs.GetVerbose())
			theApp.QueueDebugLogLine(true, _T("Failed to decompress file \"%s\" - %s"), pszFilePath, (LPCTSTR)GetErrorMessage(::GetLastError(), 1));

	::CloseHandle(hFile);
}

#ifdef XP_BUILD
#ifndef __IZoneIdentifier_INTERFACE_DEFINED__
MIDL_INTERFACE("cd45f185-1b21-48e2-967b-ead743a8914e")
IZoneIdentifier : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE GetId(DWORD *pdwZone) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetId(DWORD dwZone) = 0;
	virtual HRESULT STDMETHODCALLTYPE Remove() = 0;
};
#endif //__IZoneIdentifier_INTERFACE_DEFINED__

#ifdef CLSID_PersistentZoneIdentifier
EXTERN_C const IID CLSID_PersistentZoneIdentifier;
#elif _WIN32_WINNT<_WIN32_WINNT_VISTA
const GUID CLSID_PersistentZoneIdentifier = {0x0968E258, 0x16C7, 0x4DBA, { 0xAA, 0x86, 0x46, 0x2D, 0xD6, 0x1E, 0x31, 0xA3 }};
#endif
#endif

void SetZoneIdentifier(LPCTSTR pszFilePath)
{
	if (!thePrefs.GetCheckFileOpen())
		return;
	CComPtr<IZoneIdentifier> pZoneIdentifier;
	if (SUCCEEDED(pZoneIdentifier.CoCreateInstance(CLSID_PersistentZoneIdentifier, NULL, CLSCTX_INPROC_SERVER))) {
		CComQIPtr<IPersistFile> pPersistFile(pZoneIdentifier);
		if (pPersistFile) {
			// Specify the 'zone identifier' which has to be committed with 'IPersistFile::Save'
			if (SUCCEEDED(pZoneIdentifier->SetId(URLZONE_INTERNET))) {
				// Save the 'zone identifier'
				// NOTE: This does not modify the file content in any way,
				// *but* it modifies the "Last Modified" file time!
				VERIFY(SUCCEEDED(pPersistFile->Save(pszFilePath, FALSE)));
			}
		}
	}
}

DWORD CALLBACK CopyProgressRoutine(LARGE_INTEGER TotalFileSize, LARGE_INTEGER TotalBytesTransferred,
	LARGE_INTEGER /*StreamSize*/, LARGE_INTEGER /*StreamBytesTransferred*/, DWORD /*dwStreamNumber*/,
	DWORD /*dwCallbackReason*/, HANDLE /*hSourceFile*/, HANDLE /*hDestinationFile*/,
	LPVOID lpData)
{
	CPartFile *pPartFile = static_cast<CPartFile*>(lpData);
	if (TotalFileSize.QuadPart && pPartFile && pPartFile->IsKindOf(RUNTIME_CLASS(CPartFile))) {
		WPARAM uProgress = (WPARAM)(TotalBytesTransferred.QuadPart * 100 / TotalFileSize.QuadPart);
		if (uProgress != pPartFile->GetFileOpProgress()) {
			ASSERT(uProgress <= 100);
			VERIFY(theApp.emuledlg->PostMessage(TM_FILEOPPROGRESS, uProgress, (LPARAM)pPartFile));
		}
	} else
		ASSERT(0);

	return PROGRESS_CONTINUE;
}

// Lord KiRon - using threads for file completion
// NOTE: This function is executed within a separate thread, do *NOT* use any lists/queues
// of the main thread without synchronization. Even access to members of the CPartFile
// (e.g. filename) would need proper synchronization to achieve full multi-threading compliance.
BOOL CPartFile::PerformFileComplete()
{
	// If that function is invoked from within the file completion thread, it's OK if we wait (and block) the thread.
	CSingleLock sLock(&m_FileCompleteMutex, TRUE);

	TCHAR *newfilename = _tcsdup(GetFileName());
	if (!newfilename)
		return FALSE;

	const CString &strPartfilename(RemoveFileExtension(m_fullname));
	_tcscpy(newfilename, (LPCTSTR)StripInvalidFilenameChars(newfilename));

	CString indir;
	if (::PathFileExists(thePrefs.GetCategory(GetCategory())->strIncomingPath))
		indir = thePrefs.GetCategory(GetCategory())->strIncomingPath;
	else
		indir = thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR);
	ASSERT(indir.Right(1) == _T("\\"));
	// close permanent handle
	try {
		if ((HANDLE)m_hpartfile != INVALID_HANDLE_VALUE)
			m_hpartfile.Close();
	} catch (CFileException *ex) {
		theApp.QueueLogLine(true, GetResString(IDS_ERR_FILEERROR), (LPCTSTR)m_partmetfilename, (LPCTSTR)GetFileName(), (LPCTSTR)CExceptionStr(*ex));
		ex->Delete();
	}

	CString strNewname(indir);
	strNewname += newfilename;
	bool renamed = ::PathFileExists(strNewname);
	if (renamed) {
		size_t length = _tcslen(newfilename);
		ASSERT(length); //name should never be empty

		//the file extension
		TCHAR *ext = _tcsrchr(newfilename, _T('.'));
		if (ext == NULL)
			ext = &newfilename[length];

		TCHAR *last = ext; //new end is the file name before extension
		*last = _T('\0'); //truncate file name

		int namecount = 0;
		//search for matching ()s and check if it contains a number
		if ((ext != newfilename) && (_tcsrchr(newfilename, _T(')')) + 1 == last)) {
			TCHAR *first = _tcsrchr(newfilename, _T('('));
			if (first != NULL) {
				++first;
				bool found = true;
				for (TCHAR *step = first; step < last - 1; ++step)
					if (!_istdigit(*step)) {
						found = false;
						break;
					}
				if (found) {
					namecount = _tstoi(first);
					first[-1] = _T('\0'); //truncate again
				}
			}
		}

		ext = min(ext + 1, newfilename + length);
		CString strTestName;
		do
			strTestName.Format(_T("%s%s(%d).%s"), (LPCTSTR)indir, newfilename, ++namecount, ext);
		while (::PathFileExists(strTestName));
		strNewname = strTestName;
	}
	free(newfilename);

	bNoNewReads = true; //this file is on the move and cannot be read
	for (bool bFirstTry = true; ;) {
		if (::MoveFileWithProgress(strPartfilename, strNewname, CopyProgressRoutine, this, MOVEFILE_COPY_ALLOWED))
			break;
		DWORD dwMoveResult = ::GetLastError();
		if (dwMoveResult != ERROR_SHARING_VIOLATION || !bFirstTry) {
			theApp.QueueLogLine(true, GetResString(IDS_ERR_COMPLETIONFAILED) + _T(" - \"%s\": %s")
				, (LPCTSTR)GetFileName(), (LPCTSTR)strNewname, (LPCTSTR)GetErrorMessage(dwMoveResult));
			// If the destination file path is too long, the default system error message may be unhelpful for user to know what failed.
			if (strNewname.GetLength() >= MAX_PATH)
				theApp.QueueLogLine(true, GetResString(IDS_ERR_COMPLETIONFAILED) + _T(" - \"%s\": Path too long")
					, (LPCTSTR)GetFileName(), (LPCTSTR)strNewname);

			m_paused = m_stopped = true;
			SetStatus(PS_ERROR);
			m_bCompletionError = true;
			SetFileOp(PFOP_NONE);
			if (!theApp.IsClosing())
				VERIFY(theApp.emuledlg->PostMessage(TM_FILECOMPLETED, FILE_COMPLETION_THREAD_FAILED, (LPARAM)this));
			bNoNewReads = false; //re-enable reading till next completion attempt
			return FALSE;
		}
		// The UploadDiskIOThread might have an open handle to this file due to ongoing uploads
		// On old Windows versions this might result in a sharing violation (new versions have FILE_SHARE_DELETE)
		// So wait a few seconds and try again. Due to the lock we set, the UploadDiskIOThread will close the file ASAP
		bFirstTry = false;
		theApp.QueueDebugLogLine(false, _T("Sharing violation while finishing partfile, might be due to ongoing upload. Locked and trying again soon. File %s")
			, (LPCTSTR)GetFileName());
		::Sleep(SEC2MS(5)); // we can sleep here, because we are threaded
	}

	UncompressFile(strNewname, this);
	SetZoneIdentifier(strNewname);	// may modify the file's "Last Modified" time

	// to have the accurate date stored in known.met we have to update the 'date' of a just completed file.
	// if we don't update the file date here (after committing the file and before adding the record to known.met),
	// that file will be rehashed at next startup and there would also be a duplicate entry (hash+size)
	// in known.met because of a different file date!
	ASSERT((HANDLE)m_hpartfile == INVALID_HANDLE_VALUE); // the file must be closed/committed!
	struct _stat64 st;
	if (statUTC(strNewname, st) == 0) {
		m_tUtcLastModified = m_tLastModified = (time_t)st.st_mtime;
		AdjustNTFSDaylightFileTime(m_tUtcLastModified, strNewname);
	}

	static LPCTSTR const pszErrfmt = _T(" - %s");
	// remove part.met file
	if (!::DeleteFile(m_fullname)) {
		CString sFmt(GetResString(IDS_ERR_DELETEFAILED));
		sFmt.AppendFormat(pszErrfmt, (LPCTSTR)GetErrorMessage(::GetLastError()));
		theApp.QueueLogLine(true, sFmt, (LPCTSTR)m_fullname);
	}

	// remove backup files
	const CString &BAKName(m_fullname + PARTMET_BAK_EXT);
	if (!_taccess(BAKName, 0) && !::DeleteFile(BAKName)) {
		CString sFmt(GetResString(IDS_ERR_DELETE));
		sFmt.AppendFormat(pszErrfmt, (LPCTSTR)GetErrorMessage(::GetLastError()));
		theApp.QueueLogLine(true, sFmt, (LPCTSTR)BAKName);
	}

	const CString &TMPName(m_fullname + PARTMET_TMP_EXT);
	if (!_taccess(TMPName, 0) && !::DeleteFile(TMPName)) {
		CString sFmt(GetResString(IDS_ERR_DELETE));
		sFmt.AppendFormat(pszErrfmt, (LPCTSTR)GetErrorMessage(::GetLastError()));
		theApp.QueueLogLine(true, sFmt, (LPCTSTR)TMPName);
	}

	// initialize 'this' part file for being a 'complete' file, this is to be done *before* releasing the file mutex.
	m_fullname = strNewname;
	SetPath(indir);
	SetFilePath(m_fullname);
	_SetStatus(PS_COMPLETE); // set status of CPartFile object, but do not update GUI (to avoid multi-threading problems)
	m_paused = false;
	SetFileOp(PFOP_NONE);

	// clear the blackbox to free up memory
	m_CorruptionBlackBox.Free();
	m_aChangedPart.SetSize(0);

	// explicitly unlock the file before posting something to the main thread.
	sLock.Unlock();

	if (!theApp.IsClosing())
		VERIFY(theApp.emuledlg->PostMessage(TM_FILECOMPLETED, FILE_COMPLETION_THREAD_SUCCESS | (renamed ? FILE_COMPLETION_THREAD_RENAMED : 0), (LPARAM)this));
	return TRUE;
}

// 'End' of file completion. To avoid multi-threading synchronization problems,
// this is to be invoked in the main thread only!
void CPartFile::PerformFileCompleteEnd(DWORD dwResult)
{
	if (dwResult & FILE_COMPLETION_THREAD_SUCCESS) {
		if (!m_nCompleteSourcesCount)
			m_nCompleteSourcesCountHi = m_nCompleteSourcesCountLo = m_nCompleteSourcesCount = 1;
		m_tCompleteSourcesTime = 0; //force update in Shared Files

		SetStatus(PS_COMPLETE); // (set status and) update status-related modification of GUI elements
		theApp.knownfiles->SafeAddKFile(this);
		ASSERT(!nInUse);
		theApp.downloadqueue->RemoveFile(this);
		CUploadDiskIOThread::DissociateFile(this); //file has moved, a new handle would be required
		bNoNewReads = false; //ready for uploading - enable reading
		if (thePrefs.GetRemoveFinishedDownloads())
			theApp.emuledlg->transferwnd->GetDownloadList()->RemoveFile(this);
		else
			UpdateDisplayedInfo(true);

		theApp.emuledlg->transferwnd->GetDownloadList()->ShowFilesCount();

		thePrefs.Add2DownCompletedFiles();
		thePrefs.Add2DownSessionCompletedFiles();
		thePrefs.SaveCompletedDownloadsStat();

		// 05-Jän-2004 [bc]: ed2k and Kad are already full of totally wrong and/or not properly
		// attached meta data. Take the chance to clean any available meta data tags and provide
		// only tags which were determined by us.
		UpdateMetaDataTags();

		// republish that file to the ed2k-server to update the 'FT_COMPLETE_SOURCES' counter on the server.
		theApp.sharedfiles->RepublishFile(this);

		// give visual response
		Log(LOG_SUCCESS | LOG_STATUSBAR, GetResString(IDS_DOWNLOADDONE), (LPCTSTR)GetFileName());
		theApp.emuledlg->ShowNotifier(GetResString(IDS_TBN_DOWNLOADDONE) + _T('\n') + GetFileName(), TBN_DOWNLOADFINISHED, GetFilePath());
		if (dwResult & FILE_COMPLETION_THREAD_RENAMED) {
			CString strFilePath(GetFullName());
			::PathStripPath(strFilePath.GetBuffer());
			strFilePath.ReleaseBuffer();
			Log(LOG_STATUSBAR, GetResString(IDS_DOWNLOADRENAMED), (LPCTSTR)strFilePath);
		}
		if (!m_pCollection && CCollection::HasCollectionExtention(GetFileName())) {
			m_pCollection = new CCollection();
			if (!m_pCollection->InitCollectionFromFile(GetFilePath(), GetFileName())) {
				delete m_pCollection;
				m_pCollection = NULL;
			}
		}
	}

	theApp.downloadqueue->StartNextFileIfPrefs(GetCategory());
}

void  CPartFile::RemoveAllSources(bool bTryToSwap)
{
	for (POSITION pos = srclist.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cli = srclist.GetNext(pos);
		if (!bTryToSwap || !cli->SwapToAnotherFile(_T("Removing source. CPartFile::RemoveAllSources()"), true, true, true, NULL, false, false)) // ZZ:DownloadManager
			theApp.downloadqueue->RemoveSource(cli, false);
	}
	UpdatePartsInfo();
	UpdateAvailablePartsCount();

	//[enkeyDEV(Ottavio84) -A4AF-]
	// remove all links A4AF in sources to this file
	while (!A4AFsrclist.IsEmpty()) {
		CUpDownClient *cli = A4AFsrclist.RemoveHead();
		POSITION pos = cli->m_OtherRequests_list.Find(this);
		if (pos)
			cli->m_OtherRequests_list.RemoveAt(pos);
		else {
			pos = cli->m_OtherNoNeeded_list.Find(this);
			if (pos)
				cli->m_OtherNoNeeded_list.RemoveAt(pos);
		}
		if (pos)
			theApp.emuledlg->transferwnd->GetDownloadList()->RemoveSource(cli, this);
	}
	UpdateFileRatingCommentAvail();
}

void CPartFile::DeletePartFile()
{
	ASSERT(!m_bPreviewing);

	// Barry - Need to tell any connected clients to stop sending the file
	StopFile(true);

	if (GetFileOp() != PFOP_NONE) { //hashing, copying, uncompressing
		if (!m_bDelayDelete) {
			LogWarning(LOG_STATUSBAR, GetResString(IDS_DELETEAFTERFILEOP), (LPCTSTR)GetFileName());
			m_bDelayDelete = true; //signal to hashing thread
		}
		return;
	}

	theApp.sharedfiles->RemoveFile(this, true);
	theApp.downloadqueue->RemoveFile(this);
	theApp.emuledlg->transferwnd->GetDownloadList()->RemoveFile(this);
	theApp.knownfiles->AddCancelledFileID(GetFileHash());

	if ((HANDLE)m_hpartfile != INVALID_HANDLE_VALUE)
		m_hpartfile.Close();

	static LPCTSTR const pszErrfmt = _T(" - %s");
	if (_tremove(m_fullname)) {
		CString sFmt(GetResString(IDS_ERR_DELETE));
		sFmt.AppendFormat(pszErrfmt, _tcserror(errno));
		LogError(LOG_STATUSBAR, sFmt, (LPCTSTR)m_fullname);
	}

	const CString &partfilename(RemoveFileExtension(m_fullname));
	if (_tremove(partfilename)) {
		CString sFmt(GetResString(IDS_ERR_DELETE));
		sFmt.AppendFormat(pszErrfmt, _tcserror(errno));
		LogError(LOG_STATUSBAR, sFmt, (LPCTSTR)partfilename);
	}

	const CString &BAKName(m_fullname + PARTMET_BAK_EXT);
	if (!_taccess(BAKName, 0) && !::DeleteFile(BAKName)) {
		CString sFmt(GetResString(IDS_ERR_DELETE));
		sFmt.AppendFormat(pszErrfmt, (LPCTSTR)GetErrorMessage(::GetLastError()));
		LogError(LOG_STATUSBAR, sFmt, (LPCTSTR)BAKName);
	}

	const CString &TMPName(m_fullname + PARTMET_TMP_EXT);
	if (!_taccess(TMPName, 0) && !::DeleteFile(TMPName)) {
		CString sFmt(GetResString(IDS_ERR_DELETE));
		sFmt.AppendFormat(pszErrfmt, (LPCTSTR)GetErrorMessage(::GetLastError()));
		LogError(LOG_STATUSBAR, sFmt, (LPCTSTR)TMPName);
	}

	delete this;
}

bool CPartFile::HashSinglePart(UINT partnumber, bool *pbAICHReportedOK)
{
	// Right now we demand that AICH (if we have one) and MD4 agree on a parthash, no matter what
	// This is the most secure way in order to make sure eMule will never deliver a corrupt file,
	// even if one or both of the hash algorithms were broken
	// This however doesn't mean that eMule is guaranteed to be able to finish a file in case one
	// of the algorithms is completely broken, but we will bother about that if it becomes an issue.
	// At least with the current implementation nothing can go horribly wrong (from the security PoV)
	if (pbAICHReportedOK != NULL)
		*pbAICHReportedOK = false;
	if (!m_FileIdentifier.HasExpectedMD4HashCount() && !(m_FileIdentifier.HasAICHHash() && m_FileIdentifier.HasExpectedAICHHashCount())) {
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_HASHERRORWARNING), (LPCTSTR)GetFileName());
		m_bMD4HashsetNeeded = true;
		m_bAICHPartHashsetNeeded = true;
		return true;
	}

	CAICHHashTree *phtAICHPartHash = NULL;
	if (m_FileIdentifier.HasAICHHash() && m_FileIdentifier.HasExpectedAICHHashCount()) {
		const CAICHHashTree *pPartTree = m_pAICHRecoveryHashSet->FindPartHash((uint16)partnumber);
		if (pPartTree != NULL) {
			// use a new part tree, so we don't overwrite any existing recovery data which we might still need later on
			phtAICHPartHash = new CAICHHashTree(pPartTree->m_nDataSize, pPartTree->m_bIsLeftBranch, pPartTree->GetBaseSize());
		} else
			ASSERT(0);
	}

	const ULONGLONG uOff = PARTSIZE * partnumber;
	const uint64 length = mini(m_hpartfile.GetLength() - uOff, PARTSIZE);
	uchar hashresult[MDX_DIGEST_SIZE];
	m_hpartfile.Seek((LONGLONG)uOff, CFile::begin);
	CreateHash(&m_hpartfile, length, hashresult, phtAICHPartHash);

	bool bMD4Error = false;
	bool bMD4Checked = m_FileIdentifier.HasExpectedMD4HashCount();
	if (bMD4Checked) {
		if (GetPartCount() > 1 || m_nFileSize == PARTSIZE) {
			if (m_FileIdentifier.GetAvailableMD4PartHashCount() > partnumber)
				bMD4Error = !md4equ(hashresult, m_FileIdentifier.GetMD4PartHash(partnumber));
			else {
				ASSERT(0);
				m_bMD4HashsetNeeded = true;
			}
		} else
			bMD4Error = !md4equ(hashresult, m_FileIdentifier.GetMD4Hash());
	} else {
		DebugLogError(_T("MD4 HashSet not present while verifying part %u for file %s"), partnumber, (LPCTSTR)GetFileName());
		m_bMD4HashsetNeeded = true;
	}

	bool bAICHError = false;
	bool bAICHChecked = m_FileIdentifier.HasAICHHash() && m_FileIdentifier.HasExpectedAICHHashCount() && phtAICHPartHash != NULL;
	if (bAICHChecked) {
		ASSERT(phtAICHPartHash->m_bHashValid);
		if (GetPartCount() > 1) {
			if (m_FileIdentifier.GetAvailableAICHPartHashCount() > partnumber)
				bAICHError = m_FileIdentifier.GetRawAICHHashSet()[partnumber] != phtAICHPartHash->m_Hash;
			else
				ASSERT(0);
		} else
			bAICHError = m_FileIdentifier.GetAICHHash() != phtAICHPartHash->m_Hash;
	}
	//else
	//	DebugLogWarning(_T("AICH HashSet not present while verifying part %u for file %s"), partnumber, (LPCTSTR)GetFileName());

	delete phtAICHPartHash;
	if (pbAICHReportedOK != NULL && bAICHChecked)
		*pbAICHReportedOK = !bAICHError;
	if (bMD4Checked && bAICHChecked && bMD4Error != bAICHError)
		DebugLogError(_T("AICH and MD4 HashSet disagree on verifying part %u for file %s. MD4: %s - AICH: %s"), partnumber
			, (LPCTSTR)GetFileName(), bMD4Error ? _T("Corrupt") : _T("OK"), bAICHError ? _T("Corrupt") : _T("OK"));
#ifdef _DEBUG
	else
		DebugLog(_T("Verifying part %u for file %s. MD4: %s - AICH: %s"), partnumber, (LPCTSTR)GetFileName()
			, bMD4Checked ? (bMD4Error ? _T("Corrupt") : _T("OK")) : _T("Unavailable"), bAICHChecked ? (bAICHError ? _T("Corrupt") : _T("OK")) : _T("Unavailable"));
#endif
	return !bMD4Error && !bAICHError;
}

// Barry - Also want to preview zip/rar files
bool CPartFile::IsArchive(bool onlyPreviewable) const
{
	if (onlyPreviewable)
		switch (GetFileTypeEx(const_cast<CPartFile*>(this))) {
		case ARCHIVE_ZIP:
		case ARCHIVE_RAR:
		case ARCHIVE_ACE:
		case ARCHIVE_7Z:
		case IMAGE_ISO:
			return true;
		}

	return ED2KFT_ARCHIVE == GetED2KFileTypeID(GetFileName());
}

bool CPartFile::IsPreviewableFileType() const
{
	return IsArchive(true) || IsMovie();
}

void CPartFile::SetDownPriority(uint8 NewPriority, bool resort)
{
	//Changed the default re-sort to true. As it was, we almost never sorted the download list when a priority changed.
	//If we don't keep the download list sorted, priority means nothing in downloadqueue.cpp->process().
	//Also, if we call this method with the same priority, don't do anything to help use less CPU cycles.
	if (m_iDownPriority != NewPriority) {
		//We have a new priority
		if (NewPriority != PR_LOW && NewPriority != PR_NORMAL && NewPriority != PR_HIGH) {
			//This should never happen. Default to Normal.
			ASSERT(0);
			NewPriority = PR_NORMAL;
		}

		m_iDownPriority = NewPriority;
		//Some methods will change a batch of priorities then call these methods.
		if (resort) {
			//Sort the download queue so contacting sources work correctly.
			theApp.downloadqueue->SortByPriority();
			theApp.downloadqueue->CheckDiskspaceTimed();
		}
		//Update our display to show the new info based on our new priority.
		UpdateDisplayedInfo(true);
		//Save the partfile. We do this so that if we restart eMule before this files does
		//any transfers, it will remember the new priority.
		SavePartFile();
	}
}

bool CPartFile::CanOpenFile() const
{
	return GetStatus() == PS_COMPLETE;
}

void CPartFile::OpenFile() const
{
	if (m_pCollection) {
		CCollectionViewDialog dialog;
		dialog.SetCollection(m_pCollection);
		dialog.DoModal();
	} else
		ShellDefaultVerb(GetFullName());
}

bool CPartFile::CanStopFile() const
{
	switch (GetStatus()) {
	case PS_ERROR:
	case PS_COMPLETE:
	case PS_COMPLETING:
		return false;
	}
	return !IsStopped();
}

void CPartFile::StopFile(bool bCancel, bool resort)
{
	// Barry - Need to tell any connected clients to stop sending the file
	PauseFile(false, resort);
	m_LastSearchTimeKad = 0;
	m_TotalSearchesKad = 0;
	RemoveAllSources(true);
	m_paused = m_stopped = true;
	m_insufficient = false;
	m_datarate = 0;
	memset(m_anStates, 0, sizeof m_anStates);
	memset(src_stats, 0, sizeof src_stats);	//Xman Bugfix
	memset(net_stats, 0, sizeof net_stats);	//Xman Bugfix

	if (!bCancel)
		FlushBuffer();
	if (resort) {
		theApp.downloadqueue->SortByPriority();
		theApp.downloadqueue->CheckDiskspace();
	}
	UpdateDisplayedInfo(true);
}

void CPartFile::StopPausedFile()
{
	//Once an hour, remove any sources for files which are no longer active downloads
	if (inSet(GetStatus(), PS_PAUSED, PS_INSUFFICIENT, PS_ERROR) && !m_stopped && time(NULL) >= m_iLastPausePurge + HR2S(1))
		StopFile();
	else if (m_bDelayDelete)
		DeletePartFile();
}

bool CPartFile::CanPauseFile() const
{
	switch (GetStatus()) {
	case PS_PAUSED:
	case PS_ERROR:
	case PS_COMPLETE:
	case PS_COMPLETING:
		return false;
	}
	return true;
}

void CPartFile::PauseFile(bool bInsufficient, bool resort)
{
	// If file is already in 'insufficient' state, don't set it again to insufficient. This may happen
	// if a disk full condition is thrown before the automatic periodic check of free disk space was done.
	if (bInsufficient && m_insufficient)
		return;

	// if file is already in 'paused' or 'insufficient' state, do not refresh the purge time
	if (!m_paused && !m_insufficient)
		m_iLastPausePurge = time(NULL);
	theApp.downloadqueue->RemoveLocalServerRequest(this);

	if (GetKadFileSearchID()) {
		Kademlia::CSearchManager::StopSearch(GetKadFileSearchID(), true);
		m_LastSearchTimeKad = 0; //If we were in the middle of searching, reset timer so they can resume searching.
	}

	SetActive(false);

	if (status == PS_COMPLETE || status == PS_COMPLETING)
		return;

	for (POSITION pos = srclist.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_src = srclist.GetNext(pos);
		if (cur_src->GetDownloadState() == DS_DOWNLOADING) {
			cur_src->SendCancelTransfer();
			cur_src->SetDownloadState(DS_ONQUEUE, _T("You cancelled the download. Sending OP_CANCELTRANSFER"));
		}
	}

	if (bInsufficient)
		LogError(LOG_STATUSBAR, _T("Insufficient disk space - pausing download of \"%s\""), (LPCTSTR)GetFileName());
	else
		m_paused = true;
	m_insufficient = bInsufficient;

	NotifyStatusChange();
	m_datarate = 0;
	m_anStates[DS_DOWNLOADING] = 0; // -khaos--+++> Renamed var.
	if (!bInsufficient) {
		if (resort) {
			theApp.downloadqueue->SortByPriority();
			theApp.downloadqueue->CheckDiskspace();
		}
		SavePartFile();
	}
	UpdateDisplayedInfo(true);
}

bool CPartFile::CanResumeFile() const
{
	switch (GetStatus()) {
	case PS_ERROR:
		return GetCompletionError();
	case PS_INSUFFICIENT:
	case PS_PAUSED:
		return true;
	}
	return false;
}

void CPartFile::ResumeFile(bool resort)
{
	if (status == PS_COMPLETE || status == PS_COMPLETING)
		return;
	if (status == PS_ERROR && m_bCompletionError) {
		if (m_gaplist.IsEmpty() && m_BufferedData_list.IsEmpty()) {
			// file rehashing could probably be avoided, but better be on the safe side.
			m_bCompletionError = false;
			CompleteFile(false);
		} else
			ASSERT(0);
	} else {
		m_paused = m_stopped = false;
		SetActive(theApp.IsConnected());
		m_LastSearchTime = 0;
		if (resort) {
			theApp.downloadqueue->SortByPriority();
			theApp.downloadqueue->CheckDiskspace();
		}
		SavePartFile();
		NotifyStatusChange();

		UpdateDisplayedInfo(true);
	}
}

void CPartFile::ResumeFileInsufficient()
{
	if (status != PS_COMPLETE && status != PS_COMPLETING && m_insufficient) {
		AddLogLine(false, _T("Resuming download of \"%s\""), (LPCTSTR)GetFileName());
		m_insufficient = false;
		SetActive(theApp.IsConnected());
		m_LastSearchTime = 0;
		UpdateDisplayedInfo(true);
	}
}

CString CPartFile::getPartfileStatus() const
{
	if (GetFileOp() == PFOP_IMPORTPARTS)
		return _T("Importing part");

	UINT uid;
	switch (GetStatus()) {
	case PS_HASHING:
	case PS_WAITINGFORHASH:
		uid = IDS_HASHING;
		break;
	case PS_COMPLETING:
		{
			CString strState(GetResString(IDS_COMPLETING));
			switch (GetFileOp()) {
			case PFOP_HASHING:
				strState.AppendFormat(_T(" (%s)"), (LPCTSTR)GetResString(IDS_HASHING));
				break;
			case PFOP_COPYING:
				strState += _T(" (Copying)");
				break;
			case PFOP_UNCOMPRESSING:
				strState += _T(" (Uncompressing)");
			}
			return strState;
		}
	case PS_COMPLETE:
		uid = IDS_COMPLETE;
		break;
	case PS_PAUSED:
		uid = m_stopped ? IDS_STOPPED : IDS_PAUSED;
		break;
	case PS_INSUFFICIENT:
		uid = IDS_INSUFFICIENT;
		break;
	case PS_ERROR:
		uid = m_bCompletionError ? IDS_INSUFFICIENT : IDS_ERRORLIKE;
		break;
	default:
		uid = GetSrcStatisticsValue(DS_DOWNLOADING) > 0 ? IDS_DOWNLOADING : IDS_WAITING;
	}
	return GetResString(uid);
}

int CPartFile::getPartfileStatusRank() const
{
	switch (GetStatus()) {
	case PS_HASHING:
	case PS_WAITINGFORHASH:
		return 7;
	case PS_COMPLETING:
		return 1;
	case PS_COMPLETE:
		return 0;
	case PS_PAUSED:
		return IsStopped() ? 6 : 5;
	case PS_INSUFFICIENT:
		return 4;
	case PS_ERROR:
		return 8;
	}
	if (GetSrcStatisticsValue(DS_DOWNLOADING) == 0)
		return 3; // waiting?
	return 2; // downloading?
}

time_t CPartFile::getTimeRemaining() const
{
	uint64 completesize = (uint64)GetCompletedSize();
	time_t simple = (time_t)(GetDatarate() ? ((uint64)m_nFileSize - completesize) / GetDatarate() : -1);
	if (thePrefs.UseSimpleTimeRemainingComputation())
		return simple;
	time_t tActive = GetDlActiveTime();
	time_t estimate = (time_t)((tActive && completesize >= 512000)
		? ((uint64)m_nFileSize - completesize) / ((double)completesize / tActive) : -1);

	if (estimate == (time_t)-1 || (simple > 0 && simple < estimate))
		return simple; //not enough data to guess; no matter if we are transferring or not
	//-1 if the estimate is too high.
	return (estimate < DAY2S(15)) ? estimate : (time_t)-1;
}

void CPartFile::PreviewFile()
{
	if (thePreviewApps.Preview(this))
		return;

	if (IsArchive(true)) {
		if (!m_bRecoveringArchive && !m_bPreviewing)
			CArchiveRecovery::recover(this, true, thePrefs.GetPreviewCopiedArchives());
		return;
	}

	if (!IsReadyForPreview()) {
		ASSERT(0);
		return;
	}

	if (thePrefs.IsMoviePreviewBackup()) {
		if (!CheckFileOpen(GetFilePath(), GetFileName()))
			return;
		m_bPreviewing = true;
		CPreviewThread *pThread = static_cast<CPreviewThread*>(AfxBeginThread(RUNTIME_CLASS(CPreviewThread), THREAD_PRIORITY_BELOW_NORMAL, 0, CREATE_SUSPENDED));
		if (pThread == NULL) {
			m_bPreviewing = false;
			return;
		}
		pThread->SetValues(this, thePrefs.GetVideoPlayer(), thePrefs.GetVideoPlayerArgs());
		pThread->ResumeThread();
	} else
		ExecutePartFile(this, thePrefs.GetVideoPlayer(), thePrefs.GetVideoPlayerArgs());
}

bool CPartFile::IsReadyForPreview() const
{
	CPreviewApps::ECanPreviewRes ePreviewAppsRes = thePreviewApps.CanPreview(this);
	if (ePreviewAppsRes != CPreviewApps::NotHandled)
		return ePreviewAppsRes == CPreviewApps::Yes;

	EPartFileStatus uStatus = GetStatus();
	// Barry - Allow preview of archives only if length > 1k
	if (IsArchive(true)) {
		// check if we are already trying archive recovery on this part file
		if (m_bRecoveringArchive)
			return false;

		// check part file state
		if (inSet(uStatus, PS_COMPLETE, PS_COMPLETING))
			return false;

		// check available data size
		if ((uint64)GetCompletedSize() < 1024)
			return false;

		// check free disk space
		uint64 uMinFreeDiskSpace = (thePrefs.IsCheckDiskspaceEnabled() && thePrefs.GetMinFreeDiskSpace() > 0)
			? thePrefs.GetMinFreeDiskSpace()
			: 20 * 1024 * 1024;
		if (thePrefs.GetPreviewCopiedArchives())
			uMinFreeDiskSpace += (uint64)m_nFileSize * 2;
		else
			uMinFreeDiskSpace += (uint64)GetCompletedSize() + 16 * 1024;
		return GetFreeDiskSpaceX(GetTmpPath()) >= uMinFreeDiskSpace;
	}

	if (m_bPreviewing)
		return false;

	if (thePrefs.IsMoviePreviewBackup())
		return inSet(uStatus, PS_READY, PS_PAUSED)
			&& GetPartCount() >= 5
			&& IsMovie()
			&& (uint64)m_nFileSize < GetFreeDiskSpaceX(GetTmpPath()) + 100000000
			&& IsComplete(0)
			&& IsComplete(GetPartCount() - 1);

	TCHAR szVideoPlayerFileName[_MAX_FNAME];
	_tsplitpath(thePrefs.GetVideoPlayer(), NULL, NULL, szVideoPlayerFileName, NULL);

	// enable the preview command if 'PreviewSmallBlocks' option is enabled
	// or if VideoLAN client is specified
	if (thePrefs.GetPreviewSmallBlocks() || _tcsicmp(szVideoPlayerFileName, _T("vlc")) == 0) {
		switch (uStatus) {
		case PS_READY:
		case PS_EMPTY:
		case PS_PAUSED:
		case PS_INSUFFICIENT:
			if ((uint64)GetCompletedSize() >= 16 * 1024) //minimum data size check
				break;
		default:
			return false;
		}

		// default: check the ED2K file format to be of type audio, video or CD image.
		// but because this could disable the preview command for some file types which eMule does not know,
		// this test can be avoided by specifying 'PreviewSmallBlocks=2'
		if (thePrefs.GetPreviewSmallBlocks() <= 1) {
			// check the file extension
			EED2KFileType eFileType = GetED2KFileTypeID(GetFileName());
			if (!(eFileType == ED2KFT_VIDEO || eFileType == ED2KFT_AUDIO || eFileType == ED2KFT_CDIMAGE)) {
				// check the ED2K file type
				const CString &rstrED2KFileType(GetStrTagValue(FT_FILETYPE));
				if (rstrED2KFileType.IsEmpty() || (rstrED2KFileType == _T(ED2KFTSTR_AUDIO) && rstrED2KFileType == _T(ED2KFTSTR_VIDEO)))
					return false;
			}
		}

		// If it's an MPEG file, VLC is even capable to show parts of the file with an empty beginning!
		bool bMPEG;
		LPCTSTR pszExt = _tcsrchr(GetFileName(), _T('.'));
		if (pszExt != NULL) {
			CString strExt(pszExt + 1);
			strExt.MakeLower();
			bMPEG = (strExt == _T("mpg") || strExt == _T("mpeg") || strExt == _T("mpe") || strExt == _T("mp3") || strExt == _T("mp2") || strExt == _T("mpa"));
		} else
			bMPEG = false;

		// TODO: for MPEG to search a block which is at least 16K (Audio) or 256K (Video)
		if (!bMPEG) {
			// For AVI files it depends on the used codec
			if ((uint64)GetCompletedSize() < 256 * 1024) //minimum data size
				return false;
			if (thePrefs.GetPreviewSmallBlocks() < 2 && !IsComplete(0, 256 * 1024))
				return false;
		}

		return true;
	}

	return (uStatus == PS_READY || uStatus == PS_PAUSED)
		&& GetPartCount() >= 2
		&& IsMovie()
		&& IsComplete(0);
}

void CPartFile::UpdateAvailablePartsCount()
{
	UINT availablecounter = 0;
	for (UINT nPart = GetPartCount(); nPart-- > 0;)
		for (POSITION pos = srclist.GetHeadPosition(); pos != NULL;)
			if (srclist.GetNext(pos)->IsPartAvailable(nPart)) {
				++availablecounter;
				break;
			}

	if (GetPartCount() == availablecounter && availablePartsCount <= availablecounter) //set lastseencomplete to the latest time, not to the earliest
		lastseencomplete = CTime::GetCurrentTime();
	availablePartsCount = availablecounter;
}

Packet* CPartFile::CreateSrcInfoPacket(const CUpDownClient *forClient, uint8 byRequestedVersion, uint16 nRequestedOptions) const
{
	if (!IsPartFile() || srclist.IsEmpty())
		return CKnownFile::CreateSrcInfoPacket(forClient, byRequestedVersion, nRequestedOptions);

	if (!md4equ(forClient->GetUploadFileID(), GetFileHash())) {
		// should never happen
		DEBUG_ONLY(DebugLogError(_T("*** %hs - client (%s) upload file \"%s\" does not match file \"%s\""), __FUNCTION__, (LPCTSTR)forClient->DbgGetClientInfo(), (LPCTSTR)DbgGetFileInfo(forClient->GetUploadFileID()), (LPCTSTR)GetFileName()));
		ASSERT(0);
		return NULL;
	}

	// check whether client has either no download status at all or a download status which is valid for this file
	if (!(forClient->GetUpPartCount() == 0 && forClient->GetUpPartStatus() == NULL)
		&& !(forClient->GetUpPartCount() == GetPartCount() && forClient->GetUpPartStatus() != NULL))
	{
		// should never happen
		DEBUG_ONLY(DebugLogError(_T("*** %hs - part count (%u) of client (%s) does not match part count (%u) of file \"%s\""), __FUNCTION__, forClient->GetUpPartCount(), (LPCTSTR)forClient->DbgGetClientInfo(), GetPartCount(), (LPCTSTR)GetFileName()));
		ASSERT(0);
		return NULL;
	}

	if (inSet(GetStatus(), PS_READY, PS_EMPTY))
		return NULL;

	CSafeMemFile data(1024);

	uint8 byUsedVersion;
	bool bIsSX2Packet;
	if (forClient->SupportsSourceExchange2() && byRequestedVersion > 0) {
		// the client uses SourceExchange2 and requested the highest version he knows
		// and we send the highest version we know, but of course not higher than his request
		byUsedVersion = min(byRequestedVersion, (uint8)SOURCEEXCHANGE2_VERSION);
		bIsSX2Packet = true;
		data.WriteUInt8(byUsedVersion);

		// we don't support any special SX2 options yet, reserved for later use
		if (nRequestedOptions != 0)
			DebugLogWarning(_T("Client requested unknown options for SourceExchange2: %u (%s)"), nRequestedOptions, (LPCTSTR)forClient->DbgGetClientInfo());
	} else {
		byUsedVersion = forClient->GetSourceExchange1Version();
		bIsSX2Packet = false;
		if (forClient->SupportsSourceExchange2())
			DebugLogWarning(_T("Client which announced to support SX2 sent SX1 packet instead (%s)"), (LPCTSTR)forClient->DbgGetClientInfo());
	}

	UINT nCount = 0;
	data.WriteHash16(m_FileIdentifier.GetMD4Hash());
	data.WriteUInt16(0); //reserve place for the number of sources

	const uint8 *reqstatus = forClient->GetUpPartStatus();
	for (POSITION pos = srclist.GetHeadPosition(); pos != NULL;) {
		const CUpDownClient *cur_src = srclist.GetNext(pos);
		if (cur_src->HasLowID() || !cur_src->IsValidSource())
			continue;
		const uint8 *srcstatus = cur_src->GetPartStatus();
		if (srcstatus && cur_src->GetPartCount() == GetPartCount()) {
			ASSERT(!reqstatus || forClient->GetUpPartCount() == GetPartCount());
			//If reqstatus is known, only send sources which have needed parts for this client
			//Otherwise, we know this client is valid, but don't know the part count status,
			//and currently we just send them all.
			int i = GetPartCount();
			while (--i >= 0)
				if (srcstatus[i] && (!reqstatus || !reqstatus[i]))
					break;

			if (i >= 0) { //found a needed part
				++nCount;
				uint32 dwID = cur_src->GetUserIDHybrid();
				if (byUsedVersion < 3)
					dwID = htonl(dwID);
				data.WriteUInt32(dwID);
				data.WriteUInt16(cur_src->GetUserPort());
				data.WriteUInt32(cur_src->GetServerIP());
				data.WriteUInt16(cur_src->GetServerPort());
				if (byUsedVersion >= 2)
					data.WriteHash16(cur_src->GetUserHash());
				if (byUsedVersion >= 4) {
					// ConnectSettings - SourceExchange V4
					// 4 Reserved (!)
					// 1 DirectCallback Supported/Available
					// 1 CryptLayer Required
					// 1 CryptLayer Requested
					// 1 CryptLayer Supported
					const uint8 uSupportsCryptLayer = static_cast<uint8>(cur_src->SupportsCryptLayer());
					const uint8 uRequestsCryptLayer = static_cast<uint8>(cur_src->RequestsCryptLayer());
					const uint8 uRequiresCryptLayer = static_cast<uint8>(cur_src->RequiresCryptLayer());
					//const uint8 uDirectUDPCallback = static_cast<uint8>(cur_src->SupportsDirectUDPCallback());
					const uint8 byCryptOptions = /*(uDirectUDPCallback << 3) |*/ (uRequiresCryptLayer << 2) | (uRequestsCryptLayer << 1) | (uSupportsCryptLayer << 0);
					data.WriteUInt8(byCryptOptions);
				}
				if (nCount > 500)
					break;
			}
		} else if (thePrefs.GetVerbose()) // should never happen
			DEBUG_ONLY(DebugLogError(_T("*** %hs - found source (%s) with wrong partcount (%u) attached to partfile \"%s\" (partcount=%u)"), __FUNCTION__, (LPCTSTR)cur_src->DbgGetClientInfo(), cur_src->GetPartCount(), (LPCTSTR)GetFileName(), GetPartCount()));
	}
	if (!nCount)
		return 0;
	data.Seek(bIsSX2Packet ? 17 : 16, CFile::begin);
	data.WriteUInt16((uint16)nCount);

	Packet *result = new Packet(data, OP_EMULEPROT);
	result->opcode = bIsSX2Packet ? OP_ANSWERSOURCES2 : OP_ANSWERSOURCES;
	// (1+)16+2+501*(4+2+4+2+16+1) = 14547 (14548) bytes max.
	if (result->size > 354)
		result->PackPacket();
	if (thePrefs.GetDebugSourceExchange())
		AddDebugLogLine(false, _T("SXSend: Client source response SX2=%s, Version=%u; Count=%u, %s, File=\"%s\""), bIsSX2Packet ? _T("Yes") : _T("No"), byUsedVersion, nCount, (LPCTSTR)forClient->DbgGetClientInfo(), (LPCTSTR)GetFileName());
	return result;
}

void CPartFile::AddClientSources(CSafeMemFile *sources, uint8 uClientSXVersion, bool bSourceExchange2, const CUpDownClient *pClient)
{
	if (m_stopped)
		return;

	if (thePrefs.GetDebugSourceExchange()) {
		CString strDbgClientInfo;
		if (pClient)
			strDbgClientInfo.Format(_T("%s, "), (LPCTSTR)pClient->DbgGetClientInfo());
		AddDebugLogLine(false, _T("SXRecv: Client source response; SX2=%s, Ver=%u, %sFile=\"%s\""), bSourceExchange2 ? _T("Yes") : _T("No"), uClientSXVersion, (LPCTSTR)strDbgClientInfo, (LPCTSTR)GetFileName());
	}

	UINT nCount;
	UINT uPacketSXVersion;
	if (!bSourceExchange2) {
		// for SX1 (deprecated):
		// Check if the data size matches the 'nCount' for v1 or v2 and eventually correct the source
		// exchange version while reading the packet data. Otherwise we could experience a higher
		// chance in dealing with wrong source data, userhashes and finally duplicate sources.
		nCount = sources->ReadUInt16();
		UINT uDataSize = (UINT)(sources->GetLength() - sources->GetPosition());
		// Checks if version 1 packet is correct size
		if (nCount * (4 + 2 + 4 + 2) == uDataSize) {
			// Received v1 packet: Check if remote client supports at least v1
			if (uClientSXVersion < 1) {
				if (thePrefs.GetVerbose()) {
					CString strDbgClientInfo;
					if (pClient)
						strDbgClientInfo.Format(_T("%s, "), (LPCTSTR)pClient->DbgGetClientInfo());
					DebugLogWarning(_T("Received invalid SX packet (v%u, count=%u, size=%u), %sFile=\"%s\""), uClientSXVersion, nCount, uDataSize, (LPCTSTR)strDbgClientInfo, (LPCTSTR)GetFileName());
				}
				return;
			}
			uPacketSXVersion = 1;
		} else if (nCount * (4 + 2 + 4 + 2 + 16) == uDataSize) {	// Checks if version 2&3 packet is correct size
			// Received v2,v3 packet: Check if remote client supports at least v2
			if (uClientSXVersion < 2) {
				if (thePrefs.GetVerbose()) {
					CString strDbgClientInfo;
					if (pClient)
						strDbgClientInfo.Format(_T("%s, "), (LPCTSTR)pClient->DbgGetClientInfo());
					DebugLogWarning(_T("Received invalid SX packet (v%u, count=%u, size=%u), %sFile=\"%s\""), uClientSXVersion, nCount, uDataSize, (LPCTSTR)strDbgClientInfo, (LPCTSTR)GetFileName());
				}
				return;
			}
			uPacketSXVersion = (uClientSXVersion == 2) ? 2 : 3;
		} else if (nCount * (4 + 2 + 4 + 2 + 16 + 1) == uDataSize) {	// v4 packets
			// Received v4 packet: Check if remote client supports at least v4
			if (uClientSXVersion < 4) {
				if (thePrefs.GetVerbose()) {
					CString strDbgClientInfo;
					if (pClient)
						strDbgClientInfo.Format(_T("%s, "), (LPCTSTR)pClient->DbgGetClientInfo());
					DebugLogWarning(_T("Received invalid SX packet (v%u, count=%u, size=%u), %sFile=\"%s\""), uClientSXVersion, nCount, uDataSize, (LPCTSTR)strDbgClientInfo, (LPCTSTR)GetFileName());
				}
				return;
			}
			uPacketSXVersion = 4;
		} else {
			// If v5+ inserts additional data (like v2), the above code will correctly filter those packets.
			// If v5+ appends additional data after <count>(<Sources>)[count], we are in trouble with the
			// above code. Though a client which does not understand v5+ should never receive such a packet.
			if (thePrefs.GetVerbose()) {
				CString strDbgClientInfo;
				if (pClient)
					strDbgClientInfo.Format(_T("%s, "), (LPCTSTR)pClient->DbgGetClientInfo());
				DebugLogWarning(_T("Received invalid SX packet (v%u, count=%u, size=%u), %sFile=\"%s\""), uClientSXVersion, nCount, uDataSize, (LPCTSTR)strDbgClientInfo, (LPCTSTR)GetFileName());
			}
			return;
		}
	} else {
		// for SX2:
		// We only check if the version is known by us and do a quick sanitize check on known version
		// other then SX1, the packet will be ignored in case of error, since it can't be a "misunderstanding" any more
		if (uClientSXVersion > SOURCEEXCHANGE2_VERSION || uClientSXVersion == 0) {
			if (thePrefs.GetVerbose()) {
				CString strDbgClientInfo;
				if (pClient)
					strDbgClientInfo.Format(_T("%s, "), (LPCTSTR)pClient->DbgGetClientInfo());
				DebugLogWarning(_T("Received invalid SX2 packet - Version unknown (v%u), %sFile=\"%s\""), uClientSXVersion, (LPCTSTR)strDbgClientInfo, (LPCTSTR)GetFileName());
			}
			return;
		}
		// all known versions use the first 2 bytes as count, and unknown versions have been already filtered above
		nCount = sources->ReadUInt16();
		UINT uDataSize = (UINT)(sources->GetLength() - sources->GetPosition());
		bool bError;
		switch (uClientSXVersion) {
		case 1:
			bError = nCount * (4 + 2 + 4 + 2) != uDataSize;
			break;
		case 2:
		case 3:
			bError = nCount * (4 + 2 + 4 + 2 + 16) != uDataSize;
			break;
		case 4:
			bError = nCount * (4 + 2 + 4 + 2 + 16 + 1) != uDataSize;
			break;
		default:
			bError = false;
			ASSERT(0);
		}

		if (bError) {
			ASSERT(0);
			if (thePrefs.GetVerbose()) {
				CString strDbgClientInfo;
				if (pClient)
					strDbgClientInfo.Format(_T("%s, "), (LPCTSTR)pClient->DbgGetClientInfo());
				DebugLogWarning(_T("Received invalid/corrupt SX2 packet (v%u, count=%u, size=%u), %sFile=\"%s\""), uClientSXVersion, nCount, uDataSize, (LPCTSTR)strDbgClientInfo, (LPCTSTR)GetFileName());
			}
			return;
		}
		uPacketSXVersion = uClientSXVersion;
	}

	for (; nCount > 0; --nCount) {
		uint32 dwID = sources->ReadUInt32();
		uint16 nPort = sources->ReadUInt16();
		uint32 dwServerIP = sources->ReadUInt32();
		uint16 nServerPort = sources->ReadUInt16();

		uchar achUserHash[MDX_DIGEST_SIZE];
		if (uPacketSXVersion >= 2)
			sources->ReadHash16(achUserHash);

		uint8 byCryptOptions = (uPacketSXVersion >= 4) ? sources->ReadUInt8() : 0;

		// Clients send ID's in the Hybrid format so highID clients with *.*.*.0 won't be falsely switched to a lowID.
		uint32 dwIDED2K = (uPacketSXVersion < 3) ? dwID : htonl(dwID);

		// check the HighID(IP) - "Filter LAN IPs" and "IPfilter" the received sources IP addresses
		if (!::IsLowID(dwIDED2K)) {
			if (!IsGoodIP(dwIDED2K)) {
				// check for 0-IP, localhost and optionally for LAN addresses
				//if (thePrefs.GetLogFilteredIPs())
				//	AddDebugLogLine(false, _T("Ignored source (IP=%s) received via source exchange - bad IP"), (LPCTSTR)ipstr(dwIDED2K));
				continue;
			}
			if (theApp.ipfilter->IsFiltered(dwIDED2K)) {
				if (thePrefs.GetLogFilteredIPs())
					AddDebugLogLine(false, _T("Ignored source (IP=%s) received via source exchange - IP filter (%s)"), (LPCTSTR)ipstr(dwIDED2K), (LPCTSTR)theApp.ipfilter->GetLastHit());
				continue;
			}
			if (theApp.clientlist->IsBannedClient(dwIDED2K)) {
#ifdef _DEBUG
				if (thePrefs.GetLogBannedClients()) {
					CUpDownClient *pClient1 = theApp.clientlist->FindClientByIP(dwIDED2K);
					if (pClient1)
						AddDebugLogLine(false, _T("Ignored source (IP=%s) received via source exchange - banned client %s"), (LPCTSTR)ipstr(dwIDED2K), (LPCTSTR)pClient1->DbgGetClientInfo());
				}
#endif
				continue;
			}
		}

		// additionally check for LowID and own IP
		if (!CanAddSource(dwID, nPort, dwServerIP, nServerPort, NULL, (uPacketSXVersion < 3))) {
			//if (thePrefs.GetLogFilteredIPs())
			//	AddDebugLogLine(false, _T("Ignored source (IP=%s) received via source exchange"), (LPCTSTR)ipstr(dwIDED2K));
			continue;
		}

		if (GetSourceCount() >= GetMaxSources())
			break;

		CUpDownClient *newsource = new CUpDownClient(this, nPort, dwID, dwServerIP, nServerPort, (uPacketSXVersion < 3));
		if (uPacketSXVersion >= 2)
			newsource->SetUserHash(achUserHash);
		if (uPacketSXVersion >= 4) {
			newsource->SetConnectOptions(byCryptOptions, true, false);
			//if (thePrefs.GetDebugSourceExchange()) // remove this log later
			//	AddDebugLogLine(false, _T("Received CryptLayer aware (%u) source from V4 Sourceexchange (%s)"), byCryptOptions, (LPCTSTR)newsource->DbgGetClientInfo());
		}
		newsource->SetSourceFrom(SF_SOURCE_EXCHANGE);
		theApp.downloadqueue->CheckAndAddSource(this, newsource);
	}
}

/* Barry - Replaces BlockReceived()

	Originally this only wrote to disk when a full 180k block
	had been received from a client, and only asked for data in
	180k blocks.

	This meant that on average 90k was lost for every connection
	to a client data source. This is a lot of wasted data.

	To reduce data waste, packets are now written to buffers
	and flushed to disk regularly regardless of size downloaded.
	This includes compressed packets.

	Data is also requested only where gaps are, not in 180k blocks.
	The requested amount still will not exceed 180k, but may be
	smaller to fill a gap.
*/
uint32 CPartFile::WriteToBuffer(uint64 transize, const BYTE *data, uint64 start, uint64 end
	, Requested_Block_Struct *block, const CUpDownClient *client, bool bCopyData)
{
	ASSERT((sint64)transize > 0 && end < (uint64)m_nFileSize && start <= end);
	// Increment transferred bytes counter for this file
	if (client) //Imported Parts are not counted as transferred
		m_uTransferred += transize;

	// This is needed a few times
	const size_t lenData = static_cast<size_t>(end - start + 1);
	ASSERT(end - start + 1 <= INT32_MAX);

	if (lenData > transize) {
		m_uCompressionGain += lenData - transize;
		thePrefs.Add2SavedFromCompression(lenData - transize);
	}

	static LPCTSTR const sImport = _T("(import)");
	// Occasionally packets are duplicated, no point to write it twice
	if (IsComplete(start, end)) {
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("PrcBlkPkt: Already written block %s; File=%s; %s"), (LPCTSTR)DbgGetBlockInfo(start, end), (LPCTSTR)GetFileName(), client ? (LPCTSTR)client->DbgGetClientInfo() : sImport);
		return 0;
	}
	// security sanitize check to make sure we do not write anything into already hashed complete chunks
	const UINT nStartPart = (unsigned)(start / PARTSIZE);
	if (IsComplete(nStartPart)) {
		DebugLogError(_T("PrcBlkPkt: Received data touches already hashed chunk - ignored (start) %s; File=%s; %s"), (LPCTSTR)DbgGetBlockInfo(start, end), (LPCTSTR)GetFileName(), client ? (LPCTSTR)client->DbgGetClientInfo() : sImport);
		return 0;
	}
	const UINT nEndPart = (unsigned)(end / PARTSIZE);
	if (nStartPart != nEndPart) {
		if (IsComplete(nEndPart)) {
			DebugLogError(_T("PrcBlkPkt: Received data touches already hashed chunk - ignored (end) %s; File=%s; %s"), (LPCTSTR)DbgGetBlockInfo(start, end), (LPCTSTR)GetFileName(), client ? (LPCTSTR)client->DbgGetClientInfo() : sImport);
			return 0;
		}
		DEBUG_ONLY(DebugLogWarning(_T("PrcBlkPkt: Received data crosses chunk boundaries %s; File=%s; %s"), (LPCTSTR)DbgGetBlockInfo(start, end), (LPCTSTR)GetFileName(), client ? (LPCTSTR)client->DbgGetClientInfo() : sImport));
	}

	// log transfer information in our "blackbox"
	m_CorruptionBlackBox.ReceivedData(start, end, client);

	// Copy data to a new buffer if necessary
	BYTE *buffer;
	if (bCopyData) {
		buffer = new BYTE[lenData];
		memcpy(buffer, data, lenData);
	} else
		buffer = const_cast<BYTE*>(data);

	// Create a new buffered queue entry
	PartFileBufferedData *newitem = new PartFileBufferedData{start, end, buffer, block};

	// Add to the queue at the correct position (most likely the end)
	POSITION after = NULL;
	for (POSITION pos = m_BufferedData_list.GetTailPosition(); pos != NULL;) {
		POSITION posLast = pos;
		const PartFileBufferedData *item = m_BufferedData_list.GetPrev(pos);
		if (item->end < newitem->end) {
			ASSERT(item->end < newitem->start); //the list is ordered, no overlaps
			//if (item->end >= newitem->start)
			//	DebugLogError(_T("WriteToBuffer: (%I64u >= %I64u)"), item->end, newitem->start);
			after = posLast;
			break;
		}
	}
	if (after)
		m_BufferedData_list.InsertAfter(after, newitem);
	else
		m_BufferedData_list.AddHead(newitem);

	// Increment buffer size marker
	m_nTotalBufferData += lenData;

	// Mark this small section of the file as filled
	FillGap(newitem->start, newitem->end);

	// If client is known, the caller updates the flush mark on the requested block
	// Otherwise, update here (for imported parts)
	if (!client && requestedblocks_list.Find(block) != NULL)
		block->transferred += lenData;
	// We prefer to flush the buffer on timer, but if we get over our limit too far
	// (high speed upload), flush here to save memory and time on list processing
	if (m_gaplist.IsEmpty()
		|| !inSet(GetStatus(), PS_READY, PS_EMPTY) //import parts
		|| (m_nTotalBufferData > thePrefs.GetFileBufferSize() * 2ull))
	{
		FlushBuffer();
	}

	// Return the length of data written to the buffer
	return static_cast<uint32>(lenData);
}

void CPartFile::FlushBuffer(bool bForceICH, bool bNoAICH)
{
	m_nLastBufferFlushTime = ::GetTickCount();
	if (GetPartCount() <= 0) //&& m_BufferedData_list.IsEmpty())
		return;

	//if (thePrefs.GetVerbose())
	//	AddDebugLogLine(false, _T("Flushing file %s - buffer size = %ld bytes (%ld queued items) transferred = %ld [time = %ld]"), (LPCTSTR)GetFileName(), m_nTotalBufferData, m_BufferedData_list.GetCount(), m_uTransferred, m_nLastBufferFlushTime);

	try {
		ULONGLONG cursize = m_hpartfile.GetLength();
		bool bCheckDiskspace = thePrefs.IsCheckDiskspaceEnabled() && thePrefs.GetMinFreeDiskSpace() > 0;
		//Previously full file allocation was performed in a special thread. That thread was writing
		// 1 byte at the end of file. Overlapped I/O is already in a separate thread, and synchronising
		// 3 threads could be fun. This fun may be avoided by using overlapped I/O for allocation too,
		// but the catch is that writing at the end of file may damage already written valid data due to
		// unpredictable order of overlapped operations.
		// The solution would be this:
		// If the very last byte of the file is buffered, use that buffer for allocation.
		// Otherwise, write 1 byte after the real end of file and truncate after the write.

		//Full file allocation flag: 0 - none; 1 - write an extra byte; 2 - use buffered data
		byte uAllocate = static_cast<byte>(cursize <= 0 && m_nTotalBufferData > 0 && IsNormalFile() && thePrefs.GetAllocCompleteMode());

		ULONGLONG newsize;
		if (IsNormalFile() && !m_BufferedData_list.IsEmpty()) {
			// Get an offset closest to the end
			newsize = m_BufferedData_list.GetTail()->end + 1;
			if (uAllocate) {
				if (newsize == (uint64)m_nFileSize)
					uAllocate = 2; //using buffered data for allocation
				else
					newsize = (uint64)m_nFileSize; //write an extra byte
			} else if (newsize < cursize)
				newsize = 0;
		} else
			newsize = 0; // not calling SetLength

		// Check free disk space if required
		if (bCheckDiskspace) {
			ULONGLONG uFreeDiskSpace = GetFreeDiskSpaceX(GetTmpPath());
			ULONGLONG uIncrease = thePrefs.GetMinFreeDiskSpace();
			if (IsNormalFile()) {
				if (newsize > cursize)
					uIncrease += newsize - cursize;
			} else {
				// Check free disk space for compressed/sparse files before possibly increasing the file size
				// regardless whether the file is increased in size, use the amount of data which will be written
				// Would need to use disk cluster sizes for more accuracy
				uIncrease += m_nTotalBufferData;
			}
			// See if increasing the file would reduce the amount of min. free space below the limit.
			// Would need to use disk cluster sizes for more accuracy
			if (uIncrease >= uFreeDiskSpace)
				AfxThrowFileException(CFileException::diskFull, 0, m_hpartfile.GetFileName());
		}
		// Ensure file is big enough for asynchronous writes
		if (newsize)
			m_hpartfile.SetLength(newsize); // may throw 'diskFull'

		//pass data to the writing thread
		CPartFileWriteThread *pThread = theApp.m_pPartFileWriteThread;
		if (pThread && pThread->IsRunning()) {
			bool bLocked = false;
			for (POSITION pos = m_BufferedData_list.GetHeadPosition(); pos != NULL;) {
				PartFileBufferedData *item = m_BufferedData_list.GetNext(pos);
				if (item->flushed == PB_READY) {
					if (!bLocked) {
						bLocked = true;
						pThread->m_lockFlushList.Lock();
						if (uAllocate == 1) //an extra byte to allocate
							pThread->m_FlushList.AddHead(ToWrite{ this, new PartFileBufferedData{(uint64)m_nFileSize, (uint64)m_nFileSize} });
					}
					if (uAllocate == 2 && pos == NULL) //using the last item for allocation
						pThread->m_FlushList.AddHead(ToWrite{this, item});
					else
						pThread->m_FlushList.AddTail(ToWrite{this, item});
					item->dwError = 0; //reset error (this could be a retry)
					item->flushed = PB_PENDING;
				}
			}
			if (bLocked)
				pThread->m_lockFlushList.Unlock();
			if (!pThread->m_FlushList.IsEmpty()) //let it sleep if nothing to do
				pThread->WakeUpCall();
		}

		//process data from the writing thread
		for (POSITION pos = m_BufferedData_list.GetHeadPosition(); pos != NULL;) {
			POSITION pos2 = pos;
			PartFileBufferedData *item = m_BufferedData_list.GetNext(pos);
			switch (item->flushed) {
			case PB_READY:
				ASSERT(!pThread || !pThread->IsRunning());
			case PB_PENDING:
				continue;
			case PB_ERROR:
				item->flushed = PB_READY; //prepare for resend
				CFileException::ThrowOsError((LONG)item->dwError, m_hpartfile.GetFileName());
			//default:
			case PB_WRITTEN: //success
				DeleteWrittenItem(pos2);
			}
		}

		// Partfile should never be too large
		if (m_hpartfile.GetLength() > (uint64)m_nFileSize) {
			// "last chance" correction. the real bugfix had to be applied elsewhere
			TRACE(_T("Partfile \"%s\" is too large! Truncating %I64u byte(s).\n"), (LPCTSTR)GetFileName(), m_hpartfile.GetLength() - (uint64)m_nFileSize);
			m_hpartfile.SetLength((uint64)m_nFileSize);
		}

		// Check every changed part of the file
		for (UINT uPartNumber = 0; uPartNumber < GetPartCount(); ++uPartNumber) {
			if (!m_aChangedPart[uPartNumber])
				continue;
			m_aChangedPart[uPartNumber] = false;

			const uint64 uStart = PARTSIZE * uPartNumber;
			const uint64 uEnd = min(uStart + PARTSIZE, (uint64)m_nFileSize) - 1;
			// Is this 9MB part complete
			if (IsCompleteBD(uStart, uEnd)) {
				// Is part corrupt
				bool bAICHAgreed;
				if (!HashSinglePart(uPartNumber, &bAICHAgreed)) {
					LogWarning(LOG_STATUSBAR, GetResString(IDS_ERR_PARTCORRUPT), uPartNumber, (LPCTSTR)GetFileName());
					AddGap(uStart, uEnd);

					// add part to corrupted list, if not already there
					if (!IsCorruptedPart(uPartNumber))
						corrupted_list.AddTail((uint16)uPartNumber);

					// request AICH recovery data, except if AICH already agreed anyway or we explicitly don't want to
					if (!bNoAICH && !bAICHAgreed)
						RequestAICHRecovery(uPartNumber);

					// update stats
					uint64 lost = uEnd - uStart + 1;
					m_uCorruptionLoss += lost;
					thePrefs.Add2LostFromCorruption(lost);
				} else {
					if (!m_bMD4HashsetNeeded && thePrefs.GetVerbose())
						AddDebugLogLine(DLP_VERYLOW, false, _T("Finished part %u of \"%s\""), uPartNumber, (LPCTSTR)GetFileName());

					// tell the blackbox about the verified data
					m_CorruptionBlackBox.VerifiedData(uStart, uEnd);

					// if this part was successfully completed (although ICH is active), remove from corrupted list
					POSITION posCorrupted = corrupted_list.Find((uint16)uPartNumber);
					if (posCorrupted)
						corrupted_list.RemoveAt(posCorrupted);

					AddToSharedFiles(); // Successfully completed part, make it available for sharing
				}
			} else if (IsCorruptedPart(uPartNumber) && (thePrefs.IsICHEnabled() || bForceICH)) {
				// Try to recover with minimal loss
				if (HashSinglePart(uPartNumber)) {
					++m_uPartsSavedDueICH;
					thePrefs.Add2SessionPartsSavedByICH(1);

					uint32 uRecovered = (uint32)GetTotalGapSizeInPart(uPartNumber);
					FillGap(uStart, uEnd);
					RemoveBlockFromList(uStart, uEnd);

					// tell the blackbox about the verified data
					m_CorruptionBlackBox.VerifiedData(uStart, uEnd);

					// remove from corrupted list
					POSITION posCorrupted = corrupted_list.Find((uint16)uPartNumber);
					if (posCorrupted)
						corrupted_list.RemoveAt(posCorrupted);

					AddLogLine(true, GetResString(IDS_ICHWORKED), uPartNumber, (LPCTSTR)GetFileName(), (LPCTSTR)CastItoXBytes(uRecovered));

					// update file stats
					if (m_uCorruptionLoss >= uRecovered) // check if the tag existed in part.met
						m_uCorruptionLoss -= uRecovered;
					// here we can't know if we have to subtract the amount of recovered data from
					// the session stats or the cumulative stats, so we subtract from where we can
					// which leads eventually to the correct total stats
					if (thePrefs.sesLostFromCorruption >= uRecovered)
						thePrefs.sesLostFromCorruption -= uRecovered;
					else if (thePrefs.cumLostFromCorruption >= uRecovered)
						thePrefs.cumLostFromCorruption -= uRecovered;

					AddToSharedFiles(); // Successfully recovered part, make it available for sharing
				}
			}
		}

		if (m_bUpdateMet) {
			if (m_nNextMetFlushTime < m_nLastBufferFlushTime) {
				SavePartFile();	// Update met file
				m_bUpdateMet = false;
				m_nNextMetFlushTime = m_nLastBufferFlushTime + SEC2MS(29);
			}
		} else
			m_bUpdateMet = (m_nTotalBufferData > 0);

		if (!theApp.IsClosing()) { // may be called during shutdown!
			// Is this file finished?
			if (m_gaplist.IsEmpty()) {
				if (m_iWrites <= 0 && m_BufferedData_list.IsEmpty() && GetStatus() != PS_COMPLETING)
					CompleteFile(false);
				else
					m_nLastBufferFlushTime -= thePrefs.GetFileBufferTimeLimit() - 211; //try again shortly
			}  else if (m_bPauseOnPreview && IsReadyForPreview()) {
				m_bPauseOnPreview = false;
				PauseFile();
			}

			// Check free disk space
			//
			// Checking the free disk space again after the file was written could most likely be avoided,
			// but because we do not use real physical disk allocation units for the free disk computations,
			// it should be more safe and accurate to check the free disk space again, after file was written
			// and buffers were flushed to disk.
			//
			// If using a normal file, we could avoid disk space check if the file was not increased.
			// If using a compressed or sparse file, we always have to check the space
			// regardless whether the file was increased in size or not.
			if (bCheckDiskspace && !IsNormalFile()) {
				switch (GetStatus()) {
				case PS_PAUSED:
				case PS_ERROR:
				case PS_COMPLETING:
				case PS_COMPLETE:
					break;
				default:
					if (GetFreeDiskSpaceX(GetTmpPath()) < thePrefs.GetMinFreeDiskSpace()) {
						// Compressed/sparse files: always pause the file
						// Normal files: pause the file only if it would still grow
						//if (!IsNormalFile() || GetNeededSpace() > 0)
							PauseFile(true);
					}
				}
			}
		}
	} catch (CFileException *ex) {
		FlushBuffersExceptionHandler(ex);
#ifndef _DEBUG
	} catch (...) {
		FlushBuffersExceptionHandler();
#endif
	}
}

void CPartFile::FlushBuffersExceptionHandler(CFileException *ex)
{
	if (thePrefs.IsCheckDiskspaceEnabled() && ex->m_cause == CFileException::diskFull) {
		CString msg;
		msg.Format(GetResString(IDS_ERR_OUTOFSPACE), (LPCTSTR)GetFileName());
		LogError(LOG_STATUSBAR, msg);
		if (theApp.IsRunning() && thePrefs.GetNotifierOnImportantError())
			theApp.emuledlg->ShowNotifier(msg, TBN_IMPORTANTEVENT);

		// 'CFileException::diskFull' is also used for 'not enough min. free space'
		if (!theApp.IsClosing())
			if (thePrefs.IsCheckDiskspaceEnabled() && thePrefs.GetMinFreeDiskSpace() == 0)
				theApp.downloadqueue->CheckDiskspace(true);
			else
				PauseFile(true);
	} else {
		if (thePrefs.IsErrorBeepEnabled())
			Beep(800, 200);

		if (ex->m_cause == CFileException::diskFull) {
			CString msg;
			msg.Format(GetResString(IDS_ERR_OUTOFSPACE), (LPCTSTR)GetFileName());
			LogError(LOG_STATUSBAR, msg);
			// may be called during shutdown!
			if (thePrefs.GetNotifierOnImportantError() && !theApp.IsClosing())
				theApp.emuledlg->ShowNotifier(msg, TBN_IMPORTANTEVENT);
		} else {
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_WRITEERROR), (LPCTSTR)GetFileName(), (LPCTSTR)CExceptionStr(*ex));
			SetStatus(PS_ERROR);
		}
		m_paused = true;
		m_iLastPausePurge = time(NULL);
		theApp.downloadqueue->RemoveLocalServerRequest(this);
		m_datarate = 0;
		m_anStates[DS_DOWNLOADING] = 0;
	}

	if (!theApp.IsClosing()) { // may be called during shutdown!
		if (GetStatus() == PS_ERROR && srcarevisible)
			theApp.emuledlg->transferwnd->GetDownloadList()->HideSources(this);
		UpdateDisplayedInfo();
	}

	ex->Delete();
}

void CPartFile::FlushBuffersExceptionHandler()
{
	ASSERT(0);
	LogError(LOG_STATUSBAR, GetResString(IDS_ERR_WRITEERROR), (LPCTSTR)GetFileName(), (LPCTSTR)GetResString(IDS_UNKNOWN));
	SetStatus(PS_ERROR);
	m_paused = true;
	m_iLastPausePurge = time(NULL);
	theApp.downloadqueue->RemoveLocalServerRequest(this);
	m_datarate = 0;
	m_anStates[DS_DOWNLOADING] = 0;
	if (!theApp.IsClosing()) // may be called during shutdown!
		UpdateDisplayedInfo();
}

// Barry - This will invert the gap list, the caller have to delete gaps when done
// 'Gaps' returned is an ordered array of the filled areas (for archive recovery and backup copies).
// Note, that end values are not decremented by 1 as it was for ordinary gaps.
//
//Different usage of 'end' for 'fills' and 'gaps' is annoying and might get fixed later
void CPartFile::GetFilledArray(CArray<Gap_Struct> &filled) const
{
	filled.RemoveAll();
	if (m_gaplist.IsEmpty())
		return;
	uint64 uEnd = (uint64)m_nFileSize;
	INT_PTR iCnt = m_gaplist.GetCount() + static_cast<INT_PTR>(m_gaplist.GetTail().end < uEnd);
	filled.SetSize(iCnt);
	uint64 start = 0;
	INT_PTR i = 0;
	for (POSITION pos = m_gaplist.GetHeadPosition(); pos != NULL;) {
		const Gap_Struct &gap = m_gaplist.GetNext(pos);
		if (gap.start > start)
			filled[i++] = Gap_Struct{start, gap.start};
		start = gap.end + 1;
	}
	if (start < uEnd)
		filled[i] = Gap_Struct{start, uEnd};
}

void CPartFile::UpdateFileRatingCommentAvail(bool bForceUpdate)
{
	bool bOldHasComment = m_bHasComment;
	UINT uOldUserRatings = m_uUserRating;

	m_bHasComment = false;
	UINT uRatings = 0;
	UINT uUserRatings = 0;

	for (POSITION pos = srclist.GetHeadPosition(); pos != NULL;) {
		const CUpDownClient *cur_src = srclist.GetNext(pos);
		if (!m_bHasComment && cur_src->HasFileComment())
			m_bHasComment = true;
		if (cur_src->HasFileRating()) {
			++uRatings;
			uUserRatings += cur_src->GetFileRating();
		}
	}
	for (POSITION pos = m_kadNotes.GetHeadPosition(); pos != NULL;) {
		const Kademlia::CEntry *entry = m_kadNotes.GetNext(pos);
		if (!m_bHasComment && !entry->GetStrTagValue(Kademlia::CKadTagNameString(TAG_DESCRIPTION)).IsEmpty())
			m_bHasComment = true;
		UINT rating = (UINT)entry->GetIntTagValue(Kademlia::CKadTagNameString(TAG_FILERATING));
		if (rating != 0) {
			++uRatings;
			uUserRatings += rating;
		}
	}

	m_uUserRating = uRatings ? (uint32)ROUND(uUserRatings / (float)uRatings) : 0;

	if (bOldHasComment != m_bHasComment || uOldUserRatings != m_uUserRating || bForceUpdate)
		UpdateDisplayedInfo(true);
}

void CPartFile::UpdateDisplayedInfo(bool force)
{
	if (!theApp.IsClosing()) {
		const DWORD curTick = ::GetTickCount();

		if (force || curTick >= m_lastRefreshedDLDisplay + MINWAIT_BEFORE_DLDISPLAY_WINDOWUPDATE + m_random_update_wait) {
			theApp.emuledlg->transferwnd->GetDownloadList()->UpdateItem(this);
			m_lastRefreshedDLDisplay = curTick;
		}
	}
}

void CPartFile::UpdateAutoDownPriority()
{
	if (IsAutoDownPriority()) {
		uint8 priority;
		INT_PTR count = GetSourceCount();
		if (count > 100)
			priority = PR_LOW;
		else if (count > 20)
			priority = PR_NORMAL;
		else
			priority = PR_HIGH;
		SetDownPriority(priority);
	}
}

UINT CPartFile::GetCategory() /*const*/
{
	if (m_category >= (UINT)thePrefs.GetCatCount())
		m_category = 0;
	return m_category;
}

bool CPartFile::HasDefaultCategory() const // extra function for const
{
	return m_category == 0 || m_category >= (UINT)thePrefs.GetCatCount();
}

// Ornis: Creating progressive presentation of the partfilestatuses - for webdisplay
const CStringA CPartFile::GetProgressString(uint16 size) const
{
	static const char crProgress = '0';			//green
	static const char crHave = '1';				//black
	static const char crPending = '2';			//yellow
	//static const char crMissing = '3';			//red
	//static const char crWaiting[7] = "456789";	//blue '4' - few sources, '9' - full sources
	//red '3' - missing (no sources), blue: '4' - few sources, '9' - many sources
	static const char crSources[8] = "3456789";

	CStringA my_ChunkBar(crHave, size + 2); // two more for safety

	float unit = (float)size / (uint64)m_nFileSize;

	if (inSet(GetStatus(), PS_COMPLETE, PS_COMPLETING))
		CharFillRange(my_ChunkBar, 0, (uint32)((uint64)m_nFileSize * unit), crProgress);
	else {
		// red gaps
		UINT i = 0;
		for (POSITION pos = m_gaplist.GetHeadPosition(); pos != NULL;) {
			const Gap_Struct &gap = m_gaplist.GetNext(pos);
			bool gapdone = false;
			uint64 start = gap.start;
			uint64 end = gap.end;
			for (; i < GetPartCount(); ++i) {
				if (start >= i * PARTSIZE && start <= (i + 1) * PARTSIZE) { // is in this part?
					if (end <= (i + 1) * PARTSIZE)
						gapdone = true;
					else
						end = (i + 1) * PARTSIZE; // and next part
					// paint
					uint8 color;
					if (i < (UINT)m_SrcPartFrequency.GetCount())
						color = crSources[min((m_SrcPartFrequency[i] + 1) / 2, (int)sizeof crSources - 1)];
					else
						color = crSources[0]; //crMissing
					CharFillRange(my_ChunkBar, (uint32)(start * unit), (uint32)(end * unit + 1), color);

					if (gapdone) // finished?
						break;

					start = end;
					end = gap.end;
				}
			}
		}
	}
	
	// yellow pending parts
	for (POSITION pos = requestedblocks_list.GetHeadPosition(); pos != NULL;) {
		const Requested_Block_Struct *block = requestedblocks_list.GetNext(pos);
		CharFillRange(my_ChunkBar, (uint32)((block->StartOffset + block->transferred) * unit), (uint32)(block->EndOffset * unit), crPending);
	}

	return my_ChunkBar;
}

void CPartFile::CharFillRange(CStringA &buffer, uint32 start, uint32 end, char color) const
{
	char *p = buffer.GetBuffer();
	memset(&p[start], color, end - start + 1);
	buffer.ReleaseBufferSetLength(buffer.GetLength());
}

void CPartFile::AddToSharedFiles()
{
	if (status == PS_EMPTY
		&& !m_bMD4HashsetNeeded
		&& m_FileIdentifier.HasExpectedMD4HashCount()
		&& !theApp.IsClosing()) // may be called during shutdown!
	{
		SetStatus(PS_READY); //available for sharing
		theApp.sharedfiles->SafeAddKFile(this);
	}
}

void CPartFile::DeleteWrittenItem(const POSITION pos)
{
	const PartFileBufferedData *item = m_BufferedData_list.GetAt(pos);
	ASSERT(!item->dwError);
	m_nTotalBufferData -= item->end - item->start + 1;
	// SLUGFILLER: SafeHash - could be more than one part
	for (INT_PTR i = (INT_PTR)(item->start / PARTSIZE); i <= (INT_PTR)(item->end / PARTSIZE); ++i)
		m_aChangedPart[i] = true;
	// SLUGFILLER: SafeHash

	m_BufferedData_list.RemoveAt(pos);
	delete[] item->data;
	delete item;

	if (!m_nFileFlushTime)
		m_nFileFlushTime = ::GetTickCount();
}

void CPartFile::SetCategory(UINT cat)
{
	m_category = cat;

// ZZ:DownloadManager -->
	// set new prio
	if (IsPartFile())
		SavePartFile();
// <-- ZZ:DownloadManager
}

void CPartFile::_SetStatus(EPartFileStatus eStatus)
{
	// NOTE: This function is meant to be used from *different* threads -> Do *NOT* call
	// any GUI functions from within here!!
	ASSERT(eStatus != PS_PAUSED && eStatus != PS_INSUFFICIENT);
	status = eStatus;
}

void CPartFile::SetStatus(EPartFileStatus eStatus)
{
	_SetStatus(eStatus);
	if (!theApp.IsClosing()) {
		NotifyStatusChange();
		UpdateDisplayedInfo(true);
		if (thePrefs.ShowCatTabInfos())
			theApp.emuledlg->transferwnd->UpdateCatTabTitles();
	}
}

void CPartFile::NotifyStatusChange()
{
	if (!theApp.IsClosing())
		theApp.emuledlg->transferwnd->GetDownloadList()->UpdateCurrentCategoryView(this);
}

UINT CPartFile::GetTransferringSrcCount() const
{
	return GetSrcStatisticsValue(DS_DOWNLOADING);
}

// [Maella -Enhanced Chunk Selection- (based on jicxicmic)]
#pragma pack(push, 1)
struct Chunk
{
	uint16 part;			// Index of the chunk
	union
	{
		uint16 frequency;	// Availability of the chunk
		uint16 rank;		// Download priority factor (highest = 0, lowest = _UI16_MAX)
	};
};
#pragma pack(pop)

bool CPartFile::GetNextRequestedBlock(CUpDownClient *sender, Requested_Block_Struct **newblocks
	, int &iCount) /*const*/
{
	// The purpose of this function is to return a list of blocks (~180KB) to
	// download. To avoid a premature stop of the downloading, all blocks that
	// are requested from the same source must be located within the same
	// chunk (=> part ~9MB).
	//
	// The selection of the chunk to download is one of the CRITICAL parts of the
	// edonkey network. The selection algorithm must ensure the best spreading
	// of files.
	//
	// The selection is based on several criteria:
	//  -   Frequency of the chunk (availability), very rare chunks must be downloaded
	//      as quickly as possible to become a new available source.
	//  -   Parts used for preview (first + last chunk); to preview or check a
	//      file (e.g. movie, mp3)
	//  -   Completion (nearest-to-complete), partially retrieved chunks should be
	//      completed before starting to download an other one.
	//
	// The frequency criterion defines 4 grades of availability: very rare, rare, almost rare,
	// and common. Inside each grade, the criteria have a specific ‘weight’, used
	// to calculate the priority of chunks. The chunk(s) with the highest
	// priority (highest=0, lowest=0xffff) is/are selected first.
	//
	// This algorithm usually selects the rarest chunk(s). However, chunk(s) that is/are
	// close to completion may still get a higher priority (priority inversion).
	// For common chunks to complete faster, it also tries to put the transferring clients
	// onto the same chunk.
	//

	// Check input parameters
	if (iCount <= 0 || sender->GetPartStatus() == NULL)
		return false;

	//AddDebugLogLine(DLP_VERYLOW, false, _T("Evaluating chunks for file: \"%s\" Client: %s"), (LPCTSTR)GetFileName(), (LPCTSTR)sender->DbgGetClientInfo());

	// Define and create the list of the chunks to download
	const UINT partCount = GetPartCount();
	CList<Chunk> chunksList(partCount);

	uint16 tempLastPartAsked = sender->m_lastPartAsked;
	if (tempLastPartAsked != _UI16_MAX && (sender->GetClientSoft() != SO_EMULE || sender->GetVersion() < MAKE_CLIENT_VERSION(0, 43, 1)))
		tempLastPartAsked = _UI16_MAX;

	// Main loop
	int newBlockCount = 0;
	while (newBlockCount < iCount) {
		// Create a request block structure if a chunk has been previously selected
		if (tempLastPartAsked != _UI16_MAX) {
			Requested_Block_Struct *pBlock = new Requested_Block_Struct;
			if (GetNextEmptyBlockInPart(tempLastPartAsked, pBlock)) {
				//AddDebugLogLine(false, _T("Got request block. Interval %i-%i. File %s. Client: %s"), pBlock->StartOffset, pBlock->EndOffset, (LPCTSTR)GetFileName(), (LPCTSTR)sender->DbgGetClientInfo());
				// Keep track of all pending requested blocks
				requestedblocks_list.AddTail(pBlock);
				// Update list of blocks to return
				newblocks[newBlockCount++] = pBlock;
				// Skip the rest of the main loop (=> CPU load)
				continue;
			}
			// All blocks for this chunk have been already requested
			delete pBlock;
			// => Try to select another chunk
			sender->m_lastPartAsked = tempLastPartAsked = _UI16_MAX;
		}
		// A new chunk must be selected (e.g. download starting, previous chunk complete)

		// Quantify all chunks (create list of chunks to download)
		// This is done only one time and only if it is necessary (=> CPU load)
		if (chunksList.IsEmpty()) {
			// Identify the locally missing part(s) that this source has
			for (UINT i = 0; i < partCount; ++i)
				if (sender->IsPartAvailable(i) && GetNextEmptyBlockInPart(i, NULL))
					// Add to the list a new entry for this chunk
					chunksList.AddTail(Chunk{(uint16)i, { m_SrcPartFrequency[i]}});

			// Check if any blocks need to be downloaded
			if (chunksList.IsEmpty())
				break; // Nothing to do; exit the main loop

			// Define the bounds of the zones (very rare, rare etc)
			// more depending on available sources
			const uint16 limit = (uint16)max((GetSourceCount() + 9) / 10, 3);

			const uint16 veryRareBound = limit;
			const uint16 rareBound = 2 * limit;
			const uint16 almostRareBound = 4 * limit;

			// Cache Preview state (Criterion 2)
			const bool isPreviewEnable = (thePrefs.GetPreviewPrio() || (GetPreviewPrio() && thePrefs.IsExtControlsEnabled()))
				&& (uint64)m_nFileSize > 2 * PARTSIZE
				&& IsPreviewableFileType();

			// Collect and calculate criteria for all chunks
			for (POSITION pos = chunksList.GetHeadPosition(); pos != NULL;) {
				Chunk &cur_chunk = chunksList.GetNext(pos);

				// Offsets of chunk
				UINT uCurChunkPart = cur_chunk.part; // help VC71...
				const uint64 uStart = uCurChunkPart * PARTSIZE;
				const uint64 uEnd = min(uStart + PARTSIZE, (uint64)m_nFileSize) - 1;
				ASSERT(uStart <= uEnd);

				// Criterion 2. Parts used for preview
				// Remark: - We need to download the first part and the last part(s).
				//         - When the last part is very small, it's necessary to
				//           download the two last parts.
				bool critPreview = isPreviewEnable
					&& (cur_chunk.part == 0 // First chunk
						|| cur_chunk.part == partCount - 1 // Last chunk
						|| (cur_chunk.part == partCount - 2 && m_nFileSize - uEnd < PARTSIZE / 3));

				// Criterion 3. Request state (downloading in process from other source(s))
				bool critRequested = false; // <--- This is set as a part of the second critCompletion loop below

				// Criterion 4. Completion
				uint64 partSize = uEnd - uStart + 1; //If all is covered by gaps, we have downloaded PARTSIZE, or possibly less for the last chunk
				ASSERT(partSize <= PARTSIZE);
				for (POSITION pos1 = m_gaplist.GetHeadPosition(); pos1 != NULL;) {
					const Gap_Struct &gap = m_gaplist.GetNext(pos1);
					if (gap.start > uEnd)
						break;
					// Check if Gap is within the bounds
					if (gap.start < uStart) {
						if (gap.end > uStart && gap.end < uEnd) {
							ASSERT(partSize >= (gap.end - uStart + 1));
							partSize -= gap.end - uStart + 1;
						} else if (gap.end >= uEnd) {
							partSize = 0;
							break; // exit Criterion 4 loop
						}
					} else { //if (gap.start <= uEnd) {
						if (gap.end < uEnd) {
							ASSERT(partSize >= (gap.end - gap.start + 1));
							partSize -= gap.end - gap.start + 1;
						} else {
							ASSERT(partSize >= (uEnd - gap.start + 1));
							partSize -= uEnd - gap.start + 1;
						}
					}
				}
				//ASSERT(partSize <= PARTSIZE && partSize <= (uEnd - uStart + 1));

				// requested blocks from sources we are currently downloading from are counted as if already downloaded
				// This code will cause bytes that has been requested AND transferred to be counted twice, so we can end
				// up with a completion number > PARTSIZE. That's OK, since it's just a relative number to compare chunks.
				for (POSITION reqPos = requestedblocks_list.GetHeadPosition(); reqPos != NULL;) {
					const Requested_Block_Struct *reqBlock = requestedblocks_list.GetNext(reqPos);
					if (reqBlock->StartOffset < uStart) {
						if (reqBlock->EndOffset > uStart) {
							if (reqBlock->EndOffset < uEnd) {
								//ASSERT(partSize + (reqBlock->EndOffset - uStart + 1) <= (uEnd - uStart + 1));
								partSize += reqBlock->EndOffset - uStart + 1;
							} else {
								//ASSERT(partSize + (uEnd - uStart + 1) <= uEnd - uStart);
								partSize += uEnd - uStart + 1;
							}
							critRequested = true;
						}
					} else if (reqBlock->StartOffset <= uEnd) {
						if (reqBlock->EndOffset < uEnd) {
							//ASSERT(partSize + (reqBlock->EndOffset - reqBlock->StartOffset + 1) <= (uEnd - uStart + 1));
							partSize += reqBlock->EndOffset - reqBlock->StartOffset + 1;
						} else {
							//ASSERT(partSize +  (uEnd - reqBlock->StartOffset + 1) <= (uEnd - uStart + 1));
							partSize += uEnd - reqBlock->StartOffset + 1;
						}
						critRequested = true;
					}
				}
				//Don't check this (see comment above for explanation): ASSERT(partSize <= PARTSIZE && partSize <= (uEnd - uStart + 1));

				if (partSize > PARTSIZE)
					partSize = PARTSIZE;

				//Last chunk is always counted as a full size chunk - to give it no advantage because of smaller size.
				//So 1/3 of PARTSIZE downloaded in the last chunk will give 33% even if there's just one more byte do download to complete the chunk.
				uint16 critCompletion = (uint16)((partSize * 100 + PARTSIZE - 1) / PARTSIZE); // in [%].

				// Criterion 5. Prefer to continue the same chunk
				const bool sameChunk = (cur_chunk.part == sender->m_lastPartAsked);

				// Criterion 6. The more transferring clients that has this part, the better (i.e. lower).
				uint16 transferringClientsScore = (uint16)m_downloadingSourceList.GetCount();

				// Criterion 7. Sooner to completion (how much of a part is completed, how fast can be transferred to this part, if all currently transferring clients with this part are put on it. Lower is better.)
				uint16 bandwidthScore = 2000;

				// Calculate criterion 6 and 7
				if (transferringClientsScore > 1) {
					UINT totalDownloadDatarateForThisPart = 1;
					for (POSITION downloadingClientPos = m_downloadingSourceList.GetHeadPosition(); downloadingClientPos != NULL;) {
						const CUpDownClient *downloadingClient = m_downloadingSourceList.GetNext(downloadingClientPos);
						if (downloadingClient->IsPartAvailable(cur_chunk.part)) {
							--transferringClientsScore;
							totalDownloadDatarateForThisPart += downloadingClient->GetDownloadDatarate() + 500; // + 500 to make sure that a non-started chunk available at two clients will end up just barely below 2000 (max limit)
						}
					}

					bandwidthScore = (uint16)min((UINT)((PARTSIZE - partSize) / (totalDownloadDatarateForThisPart * 5ull)), 2000u);
					//AddDebugLogLine(DLP_VERYLOW, false
					//    , _T("BandwidthScore for chunk %i: bandwidthScore = %u = min((PARTSIZE-partSize)/(totalDownloadDatarateForThisChunk * 5), 2000u) = min((PARTSIZE-%I64u)/(%u*5), 2000u)")
					//    , cur_chunk.part, bandwidthScore, partSize, totalDownloadDatarateForThisChunk);
				}

				//AddDebugLogLine(DLP_VERYLOW, false, _T("Evaluating chunk number: %i, SourceCount: %u/%i, critPreview: %s, critRequested: %s, critCompletion: %i%%, sameChunk: %s"), cur_chunk.part, cur_chunk.frequency, GetSourceCount(), (critPreview ? _T("true") : _T("false")), (critRequested ? _T("true") : _T("false")), critCompletion, (sameChunk ? _T("true") : _T("false")));

				// Calculate priority with all criteria
				if (partSize > 0 && GetSourceCount() <= GetSrcA4AFCount()) {
					// If there are too many a4af sources, the completion of blocks have very high prio
					cur_chunk.rank = (cur_chunk.frequency)           // Criterion 1
						+ (critPreview ? 0 : 200)                    // Criterion 2
						+ static_cast<uint16>(!critRequested)        // Criterion 3
						+ (100 - critCompletion)                     // Criterion 4
						+ static_cast<uint16>(!sameChunk)            // Criterion 5
						+ bandwidthScore;                            // Criterion 7
				} else if (cur_chunk.frequency <= veryRareBound) {
					// 3000..xxxx unrequested + requested very rare chunks
					cur_chunk.rank = (75 * cur_chunk.frequency)      // Criterion 1
						+ static_cast<uint16>(!critRequested)        // Criterion 2
						+ (critRequested ? 3000 : 3001)              // Criterion 3
						+ (100 - critCompletion)                     // Criterion 4
						+ static_cast<uint16>(!sameChunk)            // Criterion 5
						+ transferringClientsScore;                  // Criterion 6
				} else if (critPreview) {
					// 10000..10100  unrequested preview chunks
					// 20000..20100  requested preview chunks
					cur_chunk.rank = ((critRequested && !sameChunk) ? 20000 : 10000) // Criterion 3
						+ (100 - critCompletion);                    // Criterion 4
				} else if (cur_chunk.frequency <= rareBound) {
					// 10101..1xxxx  requested rare chunks
					// 10102..1xxxx  unrequested rare chunks
					//ASSERT(cur_chunk.frequency >= veryRareBound);
					cur_chunk.rank = (25 * cur_chunk.frequency)      // Criterion 1
						+ (critRequested ? 10101 : 10102)            // Criterion 3
						+ (100 - critCompletion)                     // Criterion 4
						+ static_cast<uint16>(!sameChunk)            // Criterion 5
						+ transferringClientsScore;                  // Criterion 6
				} else if (cur_chunk.frequency <= almostRareBound) {
					// 20101..1xxxx  requested almost rare chunks
					// 20150..1xxxx  unrequested almost rare chunks
					//ASSERT(cur_chunk.frequency >= rareBound);

					// used to slightly lessen the importance of frequency
					uint16 randomAdd = (uint16)((3 * RAND_MAX / 2 + (uint32)rand() *(almostRareBound - rareBound)) / RAND_MAX);
					//AddDebugLogLine(DLP_VERYLOW, false, _T("RandomAdd: %i, (%i-%i=%i)"), randomAdd, rareBound, almostRareBound, almostRareBound-rareBound);

					cur_chunk.rank = (cur_chunk.frequency)           // Criterion 1
						+ (critRequested ? 20101 : (20201 + almostRareBound - rareBound)) // Criterion 3
						+ ((partSize > 0) ? 0 : 500)                 // Criterion 4
						+ (5 * (100 - critCompletion))               // Criterion 4
						+ (sameChunk ? 0ui16 : randomAdd)            // Criterion 5
						+ bandwidthScore;                            // Criterion 7
				} else { // common chunk
					// 30000..30100  requested common chunks
					// 30001..30101  unrequested common chunks
					cur_chunk.rank = (critRequested ? 30000 : 30001) // Criterion 3
						+ (100 - critCompletion)                     // Criterion 4
						+ static_cast<uint16>(!sameChunk)            // Criterion 5
						+ bandwidthScore;                            // Criterion 7
				}
				//AddDebugLogLine(DLP_VERYLOW, false, _T("Rank: %u"), cur_chunk.rank);
			}
		}

		//if (chunksList.IsEmpty()) // No chunks remained to download
		//	break; // Exit the main loop

		// Select the next chunk to download
		// Find and count the chunk(s) with the highest priority
		int cnt = 0; // Number of found chunks with the same priority
		uint16 rank = _UI16_MAX; // Highest priority found
		CArray<POSITION> aBest;
		aBest.SetSize(partCount);
		for (POSITION pos = chunksList.GetHeadPosition(); pos != NULL;) {
			POSITION pos2 = pos;
			const Chunk &cur_chunk = chunksList.GetNext(pos);
			if (cur_chunk.rank < rank) {
				cnt = 0;
				rank = cur_chunk.rank;
			}
			if (cur_chunk.rank == rank)
				aBest[cnt++] = pos2;
		}

		// Pick a random chunk to avoid that everybody tries to download the
		// same chunks at the same time (=> spread the selected chunk among clients)
		POSITION pos = aBest[cnt > 1 ? rand() % cnt : 0];
		sender->m_lastPartAsked = tempLastPartAsked = chunksList.GetAt(pos).part;
		chunksList.RemoveAt(pos);
	}
	// Return the number of the blocks
	iCount = newBlockCount;

	// Return
	return newBlockCount > 0;
}
// Maella end

CString CPartFile::GetInfoSummary(bool bNoFormatCommands) const
{
	if (!IsPartFile())
		return CKnownFile::GetInfoSummary();

	CString compl(GetResString(IDS_DL_TRANSFCOMPL));
	compl.AppendFormat(_T(": %s/%s (%.1f%%)")
		, (LPCTSTR)CastItoXBytes((uint64)GetCompletedSize())
		, (LPCTSTR)CastItoXBytes((uint64)m_nFileSize)
		, GetPercentCompleted());

	const CString &lsc((lastseencomplete > 0) ? lastseencomplete.Format(thePrefs.GetDateTimeFormat()) : GetResString(IDS_NEVER));

	float availability = (GetPartCount() > 0) ? GetAvailablePartCount() * 100.0f / GetPartCount() : 0.0f;

	CString avail;
	avail.Format(GetResString(IDS_AVAIL), GetPartCount(), GetAvailablePartCount(), availability);

	const CString &lastdwl(GetFileDate() ? GetCFileDate().Format(thePrefs.GetDateTimeFormat()) : GetResString(IDS_NEVER));

	CString sourcesinfo(GetResString(IDS_DL_SOURCES));
	sourcesinfo += _T(": ");
	sourcesinfo.AppendFormat(GetResString(IDS_SOURCESINFO), GetSourceCount(), GetValidSourcesCount(), GetSrcStatisticsValue(DS_NONEEDEDPARTS), GetSrcA4AFCount());
	sourcesinfo += _T('\n');

	// always show space on disk
	CString sOnDisk;
	sOnDisk.Format(_T("  (%s%s)"), (LPCTSTR)GetResString(IDS_ONDISK), (LPCTSTR)CastItoXBytes(GetRealFileSize()));

	CString sStatus;
	if (GetTransferringSrcCount() > 0)
		sStatus.Format(GetResString(IDS_PARTINFOS2), GetTransferringSrcCount());
	else
		sStatus = getPartfileStatus();

	CString info(GetFileName());
	info.AppendFormat(_T("\n")
		_T("%s %s\n")
		_T("%s %s  %s\n")
		_T("%s\n")
		_T("%s %s\n")
		_T("%s: %s\n")
		_T("%s\n")
		_T("%s%s")
		_T("%s %s\n")
		_T("%s %s")
		, (LPCTSTR)GetResString(IDS_FD_HASH), (LPCTSTR)md4str(GetFileHash())
		, (LPCTSTR)GetResString(IDS_FD_SIZE), (LPCTSTR)CastItoXBytes((uint64)m_nFileSize), (LPCTSTR)sOnDisk
		, bNoFormatCommands ? _T("") : _T("<br_head>")
		, (LPCTSTR)GetResString(IDS_FD_MET), (LPCTSTR)GetPartMetFileName()
		, (LPCTSTR)GetResString(IDS_STATUS), (LPCTSTR)sStatus
		, (LPCTSTR)compl
		, (LPCTSTR)sourcesinfo, (LPCTSTR)avail
		, (LPCTSTR)GetResString(IDS_LASTSEENCOMPL), (LPCTSTR)lsc
		, (LPCTSTR)GetResString(IDS_FD_LASTCHANGE), (LPCTSTR)lastdwl
	);
	return info;
}

//This method is supposed to be called from other threads.
//To prevent the part file from vanishing while copying, a lock on m_FileCompleteMutex
//might be better than currently used m_bPreviewing flag
bool CPartFile::CopyPartFile(CArray<Gap_Struct> &raFilled, const CString &tempFileName)
{
	const INT_PTR iLast = raFilled.GetCount() - 1;
	ASSERT(iLast >= 0);
	try {
		// Create destination file and set length to the last filled end position
		CFile destFile;
		destFile.Open(tempFileName, CFile::modeCreate | CFile::modeWrite | CFile::shareDenyWrite | CFile::osSequentialScan);
		destFile.SetLength(raFilled[iLast].end);

		// Loop through the filled areas and copy data
		m_bPreviewing = true;
		for (INT_PTR i = 0; i <= iLast; ++i) {
			const Gap_Struct &fill = raFilled[i];
			for (uint64 uStart = fill.start; uStart < fill.end;) { //last valid byte was at fill.end-1
				BYTE buffer[16384];
				OVERLAPPED ovr{0, 0, {{LODWORD(uStart), HIDWORD(uStart)}}};
				OVERLAPPED ovw = ovr;
				DWORD lenData = (DWORD)min(uStart - fill.end, sizeof buffer);
				DWORD dwRead;
				if (!::ReadFile((HANDLE)m_hpartfile, buffer, lenData, &dwRead, &ovr))
					CFileException::ThrowOsError((LONG)::GetLastError(), GetFileName());
				if (!::WriteFile((HANDLE)destFile, buffer, dwRead, NULL, &ovw))
					CFileException::ThrowOsError((LONG)::GetLastError(), tempFileName);
				ASSERT(dwRead && dwRead < fill.end);
				uStart += dwRead;
			}
		}
		m_bPreviewing = false;
		return true;
	} catch (CFileException *ex) {
		ex->Delete();
	} catch (...) {
		ASSERT(0);
	}
	m_bPreviewing = false;
	return false;
}

bool CPartFile::GrabImage(uint8 nFramesToGrab, double dStartTime, bool bReduceColor, uint16 nMaxWidth, void *pSender)
{
	if (IsPartFile()) {
		if (!inSet(GetStatus(), PS_READY, PS_PAUSED) || m_bPreviewing || GetPartCount() < 2 || !IsCompleteBD(0))
			return false;
		m_bPreviewing = m_FileCompleteMutex.Lock(100);
		if (!m_bPreviewing)
			return false;
	}
	const CString &sFile(IsPartFile() ? RemoveFileExtension(m_fullname) : GetFilePath());
	return CKnownFile::GrabImage(sFile, nFramesToGrab, dStartTime, bReduceColor, nMaxWidth, pSender);
}

void CPartFile::GrabbingFinished(HBITMAP *imgResults, uint8 nFramesGrabbed, void *pSender)
{
	if (IsPartFile()) {
		m_bPreviewing = false;
		m_FileCompleteMutex.Unlock(); // unlock the file and continue processing
	}
	CKnownFile::GrabbingFinished(imgResults, nFramesGrabbed, pSender);
}

void CPartFile::GetLeftToTransferAndAdditionalNeededSpace(uint64 &rui64LeftToTransfer
	, uint64 &rui64AdditionalNeededSpace) const
{
	uint64 uSizeLastGap = 0;
	for (POSITION pos = m_gaplist.GetHeadPosition(); pos != NULL;) {
		const Gap_Struct &gap = m_gaplist.GetNext(pos);
		uint64 uGapSize = gap.end - gap.start + 1;
		rui64LeftToTransfer += uGapSize;
		if (gap.end == (uint64)m_nFileSize - 1)
			uSizeLastGap = uGapSize;
	}

	if (IsNormalFile()) {
		// File is not NTFS-Compressed nor NTFS-Sparse
		if (m_nFileSize == GetRealFileSize()) // already fully allocated?
			rui64AdditionalNeededSpace = 0;
		else
			rui64AdditionalNeededSpace = uSizeLastGap;
	} else
		// File is NTFS-Compressed or NTFS-Sparse
		rui64AdditionalNeededSpace = rui64LeftToTransfer;
}

/*Checks, if a given item should be shown in a given category
AllcatTypes:
	0	all
	1	all not assigned
	2	not completed
	3	completed
	4	waiting
	5	transferring
	6	erroneous
	7	paused
	8	stopped
	10	Video
	11	Audio
	12	Archive
	13	CDImage
	14  Doc
	15  Pic
	16  Program
*/
bool CPartFile::CheckShowItemInGivenCat(INT_PTR inCategory) /*const*/
{
	// common cases
	if (inCategory >= thePrefs.GetCatCount())
		return false;

	int myfilter = thePrefs.GetCatFilter(inCategory);
	if (((UINT)inCategory == GetCategory() && myfilter == 0))
		return true;
	if (inCategory > 0 && GetCategory() != (UINT)inCategory && !thePrefs.GetCategory(inCategory)->care4all)
		return false;

	bool ret = myfilter <= 0;
	if (!ret && (myfilter < 4 || myfilter > 8 || IsPartFile()))
		switch (myfilter) {
		case 1: //all not assigned
			ret = (GetCategory() == 0);
			break;
		case 2: //not completed
			ret = IsPartFile();
			break;
		case 3: //completed
			ret = !IsPartFile();
			break;
		case 4: //waiting
			ret = inSet(GetStatus(), PS_READY, PS_EMPTY) && GetTransferringSrcCount() == 0;
			break;
		case 5: //transferring
			ret = inSet(GetStatus(), PS_READY, PS_EMPTY) && GetTransferringSrcCount() > 0;
			break;
		case 6: //erroneous
			ret = (GetStatus() == PS_ERROR);
			break;
		case 7: //paused
			ret = m_paused; //GetStatus() == PS_PAUSED || IsStopped(); stop implies pause
			break;
		case 8: //stopped
			ret = (lastseencomplete != 0);
			break;
		case 10: //Video
			ret = IsMovie();
			break;
		case 11: //Audio
			ret = (ED2KFT_AUDIO == GetED2KFileTypeID(GetFileName()));
			break;
		case 12: //Archive
			ret = IsArchive();
			break;
		case 13: //CDImage
			ret = (ED2KFT_CDIMAGE == GetED2KFileTypeID(GetFileName()));
			break;
		case 14: //Doc
			ret = (ED2KFT_DOCUMENT == GetED2KFileTypeID(GetFileName()));
			break;
		case 15: //Pic
			ret = (ED2KFT_IMAGE == GetED2KFileTypeID(GetFileName()));
			break;
		case 16: //Program
			ret = (ED2KFT_PROGRAM == GetED2KFileTypeID(GetFileName()));
			break;
		case 18: //regexp
			ret = RegularExpressionMatch(thePrefs.GetCategory(inCategory)->regexp, GetFileName());
			break;
		case 20: //EmuleCollection
			ret = (ED2KFT_EMULECOLLECTION == GetED2KFileTypeID(GetFileName()));
		}

	return thePrefs.GetCatFilterNeg(inCategory) ? !ret : ret;
}

void CPartFile::SetFileName(LPCTSTR pszFileName, bool bReplaceInvalidFileSystemChars, bool bRemoveControlChars)
{
	CKnownFile::SetFileName(pszFileName, bReplaceInvalidFileSystemChars, bRemoveControlChars);

	UpdateDisplayedInfo(true);
	theApp.emuledlg->transferwnd->GetDownloadList()->UpdateCurrentCategoryView(this);
}

void CPartFile::SetActive(bool bActive)
{
	time_t tNow = time(NULL);
	if (bActive) {
		if (theApp.IsConnected() && m_tActivated == 0)
			m_tActivated = tNow;
	} else if (m_tActivated != 0) {
		m_nDlActiveTime += tNow - m_tActivated;
		m_tActivated = 0;
	}
}

time_t CPartFile::GetDlActiveTime() const
{
	time_t nDlActiveTime = m_nDlActiveTime;
	if (m_tActivated != 0)
		nDlActiveTime += time(NULL) - m_tActivated;
	return nDlActiveTime;
}

bool CPartFile::RightFileHasHigherPrio(CPartFile *left, CPartFile *right)
{
	if (!right)
		return false;
	if (!left)
		return true;
	const UINT lCat = left->GetCategory();
	const UINT rCat = right->GetCategory();
	const Category_Struct *lCatStruct = thePrefs.GetCategory(lCat);
	const Category_Struct *rCatStruct = thePrefs.GetCategory(rCat);

	return rCatStruct->prio > lCatStruct->prio
		|| (rCatStruct->prio == lCatStruct->prio
			&& (right->GetDownPriority() > left->GetDownPriority()
				|| (right->GetDownPriority() == left->GetDownPriority()
					&& rCat != 0 && rCat == lCat
					&& rCatStruct->downloadInAlphabeticalOrder
					&& thePrefs.IsExtControlsEnabled()
					&& !right->GetFileName().IsEmpty() && !left->GetFileName().IsEmpty()
					&& right->GetFileName().CompareNoCase(left->GetFileName()) < 0
				   )
			   )
		   );
}

void CPartFile::RequestAICHRecovery(UINT nPart)
{
	if (!m_pAICHRecoveryHashSet->HasValidMasterHash() || (m_pAICHRecoveryHashSet->GetStatus() != AICH_TRUSTED && m_pAICHRecoveryHashSet->GetStatus() != AICH_VERIFIED)) {
		AddDebugLogLine(DLP_DEFAULT, false, _T("Unable to request AICH recovery data because we have no trusted Masterhash"));
		return;
	}
	if ((uint64)m_nFileSize <= PARTSIZE * nPart + EMBLOCKSIZE)
		return;
	if (CAICHRecoveryHashSet::IsClientRequestPending(this, (uint16)nPart)) {
		AddDebugLogLine(DLP_DEFAULT, false, _T("RequestAICHRecovery: Already a request for this part pending"));
		return;
	}

	// first check if we have already the recovery data, no need to re-request it then
	if (m_pAICHRecoveryHashSet->IsPartDataAvailable(nPart * PARTSIZE)) {
		AddDebugLogLine(DLP_DEFAULT, false, _T("Found PartRecoveryData in memory"));
		AICHRecoveryDataAvailable(nPart);
		return;
	}

	ASSERT(nPart < GetPartCount());
	// find some random client which support AICH to ask for the blocks
	// first lets see how many we have at all, we prefer high id very much
	uint32 cAICHClients = 0;
	uint32 cAICHLowIDClients = 0;
	for (POSITION pos = srclist.GetHeadPosition(); pos != NULL;) {
		const CUpDownClient *pCurClient = srclist.GetNext(pos);
		if (pCurClient->IsSupportingAICH() && pCurClient->GetReqFileAICHHash() != NULL && !pCurClient->IsAICHReqPending()
			&& (*pCurClient->GetReqFileAICHHash()) == m_pAICHRecoveryHashSet->GetMasterHash())
		{
			if (pCurClient->HasLowID())
				++cAICHLowIDClients;
			else
				++cAICHClients;
		}
	}
	if ((cAICHClients | cAICHLowIDClients) == 0) {
		AddDebugLogLine(DLP_DEFAULT, false, _T("Unable to request AICH recovery data because found no client who supports it and has the same hash as the trusted one"));
		return;
	}
	uint32 nSelectedClient = rand() % (cAICHClients > 0 ? cAICHClients : cAICHLowIDClients) + 1;

	CUpDownClient *pClient = NULL;
	for (POSITION pos = srclist.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *pCurClient = srclist.GetNext(pos);
		if (pCurClient->IsSupportingAICH() && pCurClient->GetReqFileAICHHash() != NULL && !pCurClient->IsAICHReqPending()
			&& (*pCurClient->GetReqFileAICHHash()) == m_pAICHRecoveryHashSet->GetMasterHash())
		{
			if (cAICHClients > 0) {
				if (!pCurClient->HasLowID())
					--nSelectedClient;
			} else {
				ASSERT(pCurClient->HasLowID());
				--nSelectedClient;
			}
			if (nSelectedClient == 0) {
				pClient = pCurClient;
				break;
			}
		}
	}
	if (pClient != NULL) {
		AddDebugLogLine(DLP_DEFAULT, false, _T("Requesting AICH Hash (%s) from client %s"), cAICHClients ? _T("HighId") : _T("LowID"), (LPCTSTR)pClient->DbgGetClientInfo());
		pClient->SendAICHRequest(this, (uint16)nPart);
	} else
		ASSERT(0);
}

void CPartFile::AICHRecoveryDataAvailable(UINT nPart)
{
	if (GetPartCount() < nPart) {
		ASSERT(0);
		return;
	}
	FlushBuffer(true, true);
	const uint64 uStart = PARTSIZE * nPart;
	const uint64 length = mini(m_hpartfile.GetLength() - uStart, PARTSIZE);
	// if the part was already OK, it would now be complete
	if (IsCompleteBD(uStart, uStart + length - 1)) {
		AddDebugLogLine(DLP_DEFAULT, false, _T("Processing AICH recovery data: The part (%u) is already complete, canceling"));
		return;
	}

	const CAICHHashTree *pVerifiedHash = m_pAICHRecoveryHashSet->m_pHashTree.FindExistingHash(uStart, length);
	if (pVerifiedHash == NULL || !pVerifiedHash->m_bHashValid) {
		AddDebugLogLine(DLP_DEFAULT, false, _T("Processing AICH recovery data: Unable to get verified hash from hashset (should never happen)"));
		ASSERT(0);
		return;
	}
	CAICHHashTree htOurHash(pVerifiedHash->m_nDataSize, pVerifiedHash->m_bIsLeftBranch, pVerifiedHash->GetBaseSize());
	try {
		m_hpartfile.Seek((LONGLONG)uStart, CFile::begin);
		CreateHash(&m_hpartfile, length, NULL, &htOurHash);
	} catch (...) {
		ASSERT(0);
		return;
	}

	if (!htOurHash.m_bHashValid) {
		AddDebugLogLine(DLP_DEFAULT, false, _T("Processing AICH recovery data: Failed to retrieve AICH Hashset of corrupt part"));
		ASSERT(0);
		return;
	}

	// now compare the hash we just made, to the verified hash and re-add all blocks which are OK
	uint64 nRecovered = 0;
	for (uint64 pos = 0; pos < length; pos += EMBLOCKSIZE) {
		const uint64 nBlockSize = min(EMBLOCKSIZE, length - pos);
		const CAICHHashTree *pVerifiedBlock = pVerifiedHash->FindExistingHash(pos, nBlockSize);
		CAICHHashTree *pOurBlock = htOurHash.FindHash(pos, nBlockSize);
		if (pVerifiedBlock == NULL || pOurBlock == NULL || !pVerifiedBlock->m_bHashValid || !pOurBlock->m_bHashValid) {
			ASSERT(0);
			continue;
		}
		if (pOurBlock->m_Hash == pVerifiedBlock->m_Hash) {
			FillGap(uStart + pos, uStart + pos + (nBlockSize - 1));
			RemoveBlockFromList(uStart + pos, uStart + pos + (nBlockSize - 1));
			nRecovered += nBlockSize;
			// tell the blackbox about the verified data
			m_CorruptionBlackBox.VerifiedData(uStart + pos, uStart + pos + (nBlockSize - 1));
		} else {
			// inform our "blackbox" about the corrupted block which may ban clients who sent it
			m_CorruptionBlackBox.CorruptedData(uStart + pos, uStart + pos + (nBlockSize - 1));
		}
	}
	m_CorruptionBlackBox.EvaluateData((uint16)nPart);

	if (m_uCorruptionLoss >= nRecovered)
		m_uCorruptionLoss -= nRecovered;
	if (thePrefs.sesLostFromCorruption >= nRecovered)
		thePrefs.sesLostFromCorruption -= nRecovered;

	// OK now some sanity checks
	if (IsCompleteBD(uStart, uStart + length - 1)) {
		// this is bad, but it could probably happen under some rare circumstances
		// make sure that HashSinglePart() (MD4 and possibly AICH again) agrees to this fact too,
		// for Verified Hashes problems are handled within that functions, otherwise:
		if (!HashSinglePart(nPart)) {
			AddDebugLogLine(DLP_DEFAULT, false, _T("Processing AICH recovery data: The part (%u) got completed while recovering - but MD4 says it corrupt! Setting hashset to error state, deleting part"), nPart);
			// now we are fu... unhappy
			if (!m_FileIdentifier.HasAICHHash())
				m_pAICHRecoveryHashSet->SetStatus(AICH_ERROR); // set it to error on unverified hashes
			AddGap(uStart, uStart + length - 1);
			ASSERT(0);
			return;
		}
		AddDebugLogLine(DLP_DEFAULT, false, _T("Processing AICH recovery data: The part (%u) got completed while recovering and HashSinglePart() (MD4) agrees"), nPart);
		// alrighty not so bad
		POSITION posCorrupted = corrupted_list.Find((uint16)nPart);
		if (posCorrupted)
			corrupted_list.RemoveAt(posCorrupted);

		AddToSharedFiles(); // Successfully recovered part, make it available for sharing

		if (m_gaplist.IsEmpty() && m_BufferedData_list.IsEmpty() && !theApp.IsClosing()) // Is this file finished?
			CompleteFile(false);

	} // end sanity check
	// Update met file
	SavePartFile();
	// make sure the user appreciates our great recovery work :P
	//AICH successfully recovered %s of %s from part %u for %s
	AddLogLine(true, GetResString(IDS_AICH_WORKED), (LPCTSTR)CastItoXBytes(nRecovered), (LPCTSTR)CastItoXBytes(length), nPart, (LPCTSTR)GetFileName());
}

UINT CPartFile::GetMaxSources() const
{
	// Ignore any specified 'max sources' value if not in 'extended mode' -> don't use a parameter
	// which was once specified in GUI but can not be seen/modified any longer.
	return (!thePrefs.IsExtControlsEnabled() || m_uMaxSources == 0) ? thePrefs.GetMaxSourcePerFileDefault() : m_uMaxSources;
}

UINT CPartFile::GetMaxSourcePerFileSoft() const
{
	UINT temp = (GetMaxSources() * 9) / 10;
	return (temp > MAX_SOURCES_FILE_SOFT) ? MAX_SOURCES_FILE_SOFT : temp;
}

UINT CPartFile::GetMaxSourcePerFileUDP() const
{
	UINT temp = (GetMaxSources() * 3) / 4;
	return (temp > MAX_SOURCES_FILE_UDP) ? MAX_SOURCES_FILE_UDP : temp;
}

CString CPartFile::GetTmpPath() const
{
	return m_fullname.Left(m_fullname.ReverseFind(_T('\\')) + 1);
}

void CPartFile::RefilterFileComments()
{
	const CString &cfilter(thePrefs.GetCommentFilter());
	// check all available comments against our filter again
	if (cfilter.IsEmpty())
		return;
	for (POSITION pos = srclist.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_src = srclist.GetNext(pos);
		if (cur_src->HasFileComment()) {
			CString strCommentLower(cur_src->GetFileComment());
			strCommentLower.MakeLower();

			for (int iPos = 0; iPos >= 0;) {
				const CString &strFilter(cfilter.Tokenize(_T("|"), iPos));
				if (!strFilter.IsEmpty() && strCommentLower.Find(strFilter) >= 0) {
					cur_src->SetFileComment(_T(""));
					cur_src->SetFileRating(0);
					break;
				}
			}
		}
	}
	RefilterKadNotes();
	UpdateFileRatingCommentAvail();
}

void CPartFile::SetFileSize(EMFileSize nFileSize)
{
	ASSERT(m_pAICHRecoveryHashSet != NULL);
	m_pAICHRecoveryHashSet->SetFileSize(nFileSize);
	CKnownFile::SetFileSize(nFileSize);
	m_aChangedPart.SetSize((INT_PTR)(((uint64)nFileSize + PARTSIZE - 1) / PARTSIZE));
}