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
#include <io.h>
#include "emule.h"
#include "SharedFileList.h"
#include "KnownFileList.h"
#include "KnownFile.h"
#include "opcodes.h"
#include "Preferences.h"
#include "SafeFile.h"
#include "OtherFunctions.h"
#include "UpDownClient.h"
#include "DownloadQueue.h"
#include "emuledlg.h"
#include "TransferDlg.h"
#include "Log.h"
#include "packets.h"
#include "MD5Sum.h"
#include "SharedFilesWnd.h"
#include "SharedFilesCtrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define KNOWN_MET_FILENAME		_T("known.met")
#define CANCELLED_MET_FILENAME	_T("cancelled.met")

#define CANCELLED_HEADER_OLD	MET_HEADER
#define CANCELLED_HEADER		(MET_HEADER + 0x01)
#define CANCELLED_VERSION		0x01

CKnownFileList::CKnownFileList()
	: m_nTransferredTotal()
	, m_nRequestedTotal()
	, m_nAcceptedTotal()
	, transferred()
	, m_dwCancelledFilesSeed()
	, requested()
	, accepted()
{
	m_Files_map.InitHashTable(2063);
	m_mapCancelledFiles.InitHashTable(1031);
	m_nLastSaved = ::GetTickCount();
	Init();
}

CKnownFileList::~CKnownFileList()
{
	Clear();
}

bool CKnownFileList::Init()
{
	return LoadKnownFiles() && LoadCancelledFiles();
}

bool CKnownFileList::LoadKnownFiles()
{
	CString fullpath(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + KNOWN_MET_FILENAME);
	CSafeBufferedFile file;
	CFileException fexp;
	if (!file.Open(fullpath, CFile::modeRead | CFile::osSequentialScan | CFile::typeBinary | CFile::shareDenyWrite, &fexp)) {
		if (fexp.m_cause != CFileException::fileNotFound) {
			CString strError(_T("Failed to load ") KNOWN_MET_FILENAME _T(" file"));
			TCHAR szError[MAX_CFEXP_ERRORMSG];
			if (GetExceptionMessage(fexp, szError, _countof(szError)))
				strError.AppendFormat(_T(" - %s"), szError);
			LogError(LOG_STATUSBAR, _T("%s"), (LPCTSTR)strError);
		}
		return false;
	}
	setvbuf(file.m_pStream, NULL, _IOFBF, 16384);

	CKnownFile *pRecord = NULL;
	try {
		uint8 header = file.ReadUInt8();
		if (header != MET_HEADER && header != MET_HEADER_I64TAGS) {
			file.Close();
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_SERVERMET_BAD));
			return false;
		}
		AddDebugLogLine(false, _T("Known.met file version is %u (%s support 64bit tags)"), header, (header == MET_HEADER) ? _T("doesn't") : _T("does"));

		uint32 RecordsNumber = file.ReadUInt32();
		for (uint32 i = 0; i < RecordsNumber; ++i) {
			pRecord = new CKnownFile();
			if (!pRecord->LoadFromFile(&file)) {
				TRACE(_T("*** Failed to load entry %u (name=%s  hash=%s  size=%I64u  parthashes=%u expected parthashes=%u) from known.met\n")
					, i, (LPCTSTR)pRecord->GetFileName(), (LPCTSTR)md4str(pRecord->GetFileHash()), (uint64)pRecord->GetFileSize()
					, pRecord->GetFileIdentifier().GetAvailableMD4PartHashCount(), pRecord->GetFileIdentifier().GetTheoreticalMD4PartHashCount());
				delete pRecord;
			} else
				SafeAddKFile(pRecord);
			pRecord = NULL;
		}
		file.Close();
	} catch (CFileException *error) {
		if (error->m_cause == CFileException::endOfFile)
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_SERVERMET_BAD));
		else {
			TCHAR buffer[MAX_CFEXP_ERRORMSG];
			GetExceptionMessage(*error, buffer, _countof(buffer));
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_SERVERMET_UNKNOWN), buffer);
		}
		error->Delete();
		delete pRecord;
		return false;
	}

	return true;
}

bool CKnownFileList::LoadCancelledFiles()
{
// cancelled.met Format: <Header 1 = CANCELLED_HEADER><Version 1 = CANCELLED_VERSION><Seed 4><Count 4>[<HashHash 16><TagCount 1>[Tags TagCount] Count]
	if (!thePrefs.IsRememberingCancelledFiles())
		return true;
	CString fullpath(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + CANCELLED_MET_FILENAME);
	CSafeBufferedFile file;
	CFileException fexp;
	if (!file.Open(fullpath, CFile::modeRead | CFile::osSequentialScan | CFile::typeBinary | CFile::shareDenyWrite, &fexp)) {
		if (fexp.m_cause != CFileException::fileNotFound) {
			CString strError(_T("Failed to load ") CANCELLED_MET_FILENAME _T(" file"));
			TCHAR szError[MAX_CFEXP_ERRORMSG];
			if (GetExceptionMessage(fexp, szError, _countof(szError)))
				strError.AppendFormat(_T(" - %s"), szError);
			LogError(LOG_STATUSBAR, _T("%s"), (LPCTSTR)strError);
		}
		return false;
	}
	setvbuf(file.m_pStream, NULL, _IOFBF, 16384);
	try {
		bool bOldVersion = false;
		uint8 header = file.ReadUInt8();
		if (header != CANCELLED_HEADER) {
			if (header == CANCELLED_HEADER_OLD) {
				bOldVersion = true;
				DebugLog(_T("Deprecated version of cancelled.met found, converting to new version"));
			} else {
				file.Close();
				return false;
			}
		}
		if (!bOldVersion) {
			if (file.ReadUInt8() > CANCELLED_VERSION) {
				file.Close();
				return false;
			}

			m_dwCancelledFilesSeed = file.ReadUInt32();
		}
		if (m_dwCancelledFilesSeed == 0) {
			ASSERT(bOldVersion || file.GetLength() <= 10);
			m_dwCancelledFilesSeed = (GetRandomUInt32() % 0xFFFFFFFEu) + 1;
		}

		uchar ucHash[16];
		for (uint32 i = file.ReadUInt32(); i > 0; --i) { //number of records
			file.ReadHash16(ucHash);
			// for compatibility with future versions which may add more data than just the hash
			for (uint8 j = file.ReadUInt8(); j > 0; --j) //number of tags
				CTag tag(&file, false);

			if (bOldVersion) {
				// convert old real hash to new hash
				uchar pachSeedHash[20];
				PokeUInt32(pachSeedHash, m_dwCancelledFilesSeed);
				md4cpy(pachSeedHash + 4, ucHash);
				MD5Sum md5(pachSeedHash, sizeof pachSeedHash);
				md4cpy(ucHash, md5.GetRawHash());
			}
			m_mapCancelledFiles.SetAt(CSKey(ucHash), 1);
		}
		file.Close();
		return true;
	} catch (CFileException *error) {
		if (error->m_cause == CFileException::endOfFile)
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_CONFIGCORRUPT), CANCELLED_MET_FILENAME);
		else {
			TCHAR buffer[MAX_CFEXP_ERRORMSG];
			GetExceptionMessage(*error, buffer, _countof(buffer));
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_FAILEDTOLOAD), CANCELLED_MET_FILENAME, buffer);
		}
		error->Delete();
	}
	return false;
}

void CKnownFileList::Save()
{
	if (thePrefs.GetLogFileSaving())
		AddDebugLogLine(false, _T("Saving known files list file \"%s\""), KNOWN_MET_FILENAME);
	m_nLastSaved = ::GetTickCount();
	CString fullpath(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + KNOWN_MET_FILENAME);
	CSafeBufferedFile file;
	CFileException fexp;
	if (!file.Open(fullpath, CFile::modeWrite | CFile::modeCreate | CFile::typeBinary | CFile::shareDenyWrite, &fexp)) {
		CString strError(_T("Failed to save ") KNOWN_MET_FILENAME _T(" file"));
		TCHAR szError[MAX_CFEXP_ERRORMSG];
		if (GetExceptionMessage(fexp, szError, _countof(szError)))
			strError.AppendFormat(_T(" - %s"), szError);
		LogError(LOG_STATUSBAR, _T("%s"), (LPCTSTR)strError);
	} else {
		setvbuf(file.m_pStream, NULL, _IOFBF, 16384);

		try {
			file.WriteUInt8(0); // we will write the version tag later depending if any large files are on the list
			UINT nRecordsNumber = 0;
			bool bContainsAnyLargeFiles = false;
			file.WriteUInt32(nRecordsNumber);
			CCKey key;
			for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
				CKnownFile *pFile;
				m_Files_map.GetNextAssoc(pos, key, pFile);
				if (thePrefs.IsRememberingDownloadedFiles() || theApp.sharedfiles->IsFilePtrInList(pFile)) {
					pFile->WriteToFile(&file);
					++nRecordsNumber;
					if (pFile->IsLargeFile())
						bContainsAnyLargeFiles = true;
				}
			}
			file.SeekToBegin();
			file.WriteUInt8(bContainsAnyLargeFiles ? MET_HEADER_I64TAGS : MET_HEADER);
			file.WriteUInt32(nRecordsNumber);

			if ((theApp.IsClosing() && thePrefs.GetCommitFiles() >= 1) || thePrefs.GetCommitFiles() >= 2) {
				file.Flush(); // flush file stream buffers to disk buffers
				if (_commit(_fileno(file.m_pStream)) != 0) // commit disk buffers to disk
					AfxThrowFileException(CFileException::hardIO, ::GetLastError(), file.GetFileName());
			}
			file.Close();
		} catch (CFileException *error) {
			CString strError(_T("Failed to save ") KNOWN_MET_FILENAME _T(" file"));
			TCHAR szError[MAX_CFEXP_ERRORMSG];
			if (GetExceptionMessage(*error, szError, _countof(szError)))
				strError.AppendFormat(_T(" - %s"), szError);
			LogError(LOG_STATUSBAR, _T("%s"), (LPCTSTR)strError);
			error->Delete();
		}
	}


	if (thePrefs.GetLogFileSaving())
		AddDebugLogLine(false, _T("Saving known files list file \"%s\""), CANCELLED_MET_FILENAME);
	fullpath = thePrefs.GetMuleDirectory(EMULE_CONFIGDIR);
	fullpath += CANCELLED_MET_FILENAME;
	if (!file.Open(fullpath, CFile::modeWrite | CFile::modeCreate | CFile::typeBinary | CFile::shareDenyWrite, &fexp)) {
		CString strError(_T("Failed to save ") CANCELLED_MET_FILENAME _T(" file"));
		TCHAR szError[MAX_CFEXP_ERRORMSG];
		if (GetExceptionMessage(fexp, szError, _countof(szError)))
			strError.AppendFormat(_T(" - %s"), szError);
		LogError(LOG_STATUSBAR, _T("%s"), (LPCTSTR)strError);
	} else {
		setvbuf(file.m_pStream, NULL, _IOFBF, 16384);

		try {
			file.WriteUInt8(CANCELLED_HEADER);
			file.WriteUInt8(CANCELLED_VERSION);
			file.WriteUInt32(m_dwCancelledFilesSeed);
			if (!thePrefs.IsRememberingCancelledFiles())
				file.WriteUInt32(0);
			else {
				file.WriteUInt32((uint32)m_mapCancelledFiles.GetCount());
				CSKey key;
				for (POSITION pos = m_mapCancelledFiles.GetStartPosition(); pos != NULL;) {
					int dwDummy;
					m_mapCancelledFiles.GetNextAssoc(pos, key, dwDummy);
					file.WriteHash16(key.m_key);
					file.WriteUInt8(0);
				}
			}

			if ((theApp.IsClosing() && thePrefs.GetCommitFiles() >= 1) || thePrefs.GetCommitFiles() >= 2) {
				file.Flush(); // flush file stream buffers to disk buffers
				if (_commit(_fileno(file.m_pStream)) != 0) // commit disk buffers to disk
					AfxThrowFileException(CFileException::hardIO, ::GetLastError(), file.GetFileName());
			}
			file.Close();
		} catch (CFileException *error) {
			CString strError(_T("Failed to save ") CANCELLED_MET_FILENAME _T(" file"));
			TCHAR szError[MAX_CFEXP_ERRORMSG];
			if (GetExceptionMessage(*error, szError, _countof(szError)))
				strError.AppendFormat(_T(" - %s"), szError);
			LogError(LOG_STATUSBAR, _T("%s"), (LPCTSTR)strError);
			error->Delete();
		}
	}
}

void CKnownFileList::Clear()
{
	m_mapKnownFilesByAICH.RemoveAll();
	CCKey key;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
		CKnownFile *pFile;
		m_Files_map.GetNextAssoc(pos, key, pFile);
		delete pFile;
	}
	m_Files_map.RemoveAll();
}

void CKnownFileList::Process()
{
	if (::GetTickCount() >= m_nLastSaved + MIN2MS(11))
		Save();
}

bool CKnownFileList::SafeAddKFile(CKnownFile *toadd)
{
	bool bRemovedDuplicateSharedFile = false;
	CCKey key(toadd->GetFileHash());
	CKnownFile *pFileInMap;
	if (m_Files_map.Lookup(key, pFileInMap)) {
		TRACE(_T("%hs: Already in known list:   %s %I64u \"%s\"\n"), __FUNCTION__, (LPCTSTR)md4str(pFileInMap->GetFileHash()), (uint64)pFileInMap->GetFileSize(), (LPCTSTR)pFileInMap->GetFileName());
		TRACE(_T("%hs: Old entry replaced with: %s %I64u \"%s\"\n"), __FUNCTION__, (LPCTSTR)md4str(toadd->GetFileHash()), (uint64)toadd->GetFileSize(), (LPCTSTR)toadd->GetFileName());

		// if we hash files which are already in known file list and add them later (when the hashing thread is finished),
		// we can not delete any already available entry from known files list. that entry can already be used by the
		// shared file list -> crash.

		m_Files_map.RemoveKey(CCKey(pFileInMap->GetFileHash()));
		m_mapKnownFilesByAICH.RemoveKey(pFileInMap->GetFileIdentifier().GetAICHHash());
		//This can happen in a couple of situations.
		//File was renamed outside of eMule.
		//A user decided to re-download a file he has downloaded and unshared.
		if (theApp.sharedfiles) {
			// This solves the problem with dangling ptr in shared files ctrl,
			// but creates a new bug. It may lead to unshared files! Even
			// worse it may lead to files which are 'shared' in GUI but
			// which are not shared 'logically'.
			//
			// To reduce the harm, remove the file from shared files list,
			// only if really needed. Right now this 'harm' applies for files
			// which are re-shared and then completed (again) because they were
			// also in download queue (they were added there when the already
			// available file was not in shared file list).
			if (theApp.sharedfiles->IsFilePtrInList(pFileInMap))
				bRemovedDuplicateSharedFile = theApp.sharedfiles->RemoveFile(pFileInMap);
			ASSERT(!theApp.sharedfiles->IsFilePtrInList(pFileInMap));
		}
		//Double check to make sure this is the same file as it's possible that a two files have
		//the same hash. Maybe in the future we can change the client to not just use Hash as a key
		//throughout the entire client.
		ASSERT(toadd->GetFileSize() == pFileInMap->GetFileSize());
		ASSERT(toadd != pFileInMap);
		if (toadd->GetFileSize() == pFileInMap->GetFileSize())
			toadd->statistic.MergeFileStats(&pFileInMap->statistic);

		ASSERT(theApp.sharedfiles == NULL || !theApp.sharedfiles->IsFilePtrInList(pFileInMap));
		ASSERT(theApp.downloadqueue == NULL || !theApp.downloadqueue->IsPartFile(pFileInMap));

		// Quick fix: If we downloaded already downloaded files again and if those files all had the same
		// file names and were renamed during file completion, we have a pending ptr in transfer window.
		if (theApp.emuledlg->transferwnd && theApp.emuledlg->transferwnd->GetDownloadList()->m_hWnd)
			theApp.emuledlg->transferwnd->GetDownloadList()->RemoveFile(reinterpret_cast<CPartFile*>(pFileInMap));
		// Make sure the file is not used in out sharedfilesctrl any more
		if (theApp.emuledlg->sharedfileswnd && theApp.emuledlg->sharedfileswnd->sharedfilesctrl.m_hWnd)
			theApp.emuledlg->sharedfileswnd->sharedfilesctrl.RemoveFile(pFileInMap, true);
		delete pFileInMap;
	}
	m_Files_map.SetAt(key, toadd);
	if (bRemovedDuplicateSharedFile)
		theApp.sharedfiles->SafeAddKFile(toadd);

	if (toadd->GetFileIdentifier().HasAICHHash())
		m_mapKnownFilesByAICH.SetAt(toadd->GetFileIdentifier().GetAICHHash(), toadd);
	return true;
}

CKnownFile* CKnownFileList::FindKnownFile(LPCTSTR filename, time_t date, uint64 size) const
{
	CCKey key;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
		CKnownFile *cur_file;
		m_Files_map.GetNextAssoc(pos, key, cur_file);
		if (cur_file->GetUtcFileDate() == date && (uint64)cur_file->GetFileSize() == size && filename == cur_file->GetFileName())
			return cur_file;
	}
	return NULL;
}

CKnownFile* CKnownFileList::FindKnownFileByPath(const CString &sFilePath) const
{
	CCKey key;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
		CKnownFile *cur_file;
		m_Files_map.GetNextAssoc(pos, key, cur_file);
		if (!cur_file->GetFilePath().CompareNoCase(sFilePath))
			return cur_file;
	}
	return NULL;
}

CKnownFile* CKnownFileList::FindKnownFileByID(const uchar *hash) const
{
	if (hash) {
		CKnownFile *found_file;
		CCKey key(hash);
		if (m_Files_map.Lookup(key, found_file))
			return found_file;
	}
	return NULL;
}

bool CKnownFileList::IsKnownFile(const CKnownFile *file) const
{
	return file && (FindKnownFileByID(file->GetFileHash()) != NULL);
}

bool CKnownFileList::IsFilePtrInList(const CKnownFile *file) const
{
	if (file) {
		CCKey key;
		for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
			CKnownFile *cur_file;
			m_Files_map.GetNextAssoc(pos, key, cur_file);
			if (file == cur_file)
				return true;
		}
	}
	return false;
}

void CKnownFileList::AddCancelledFileID(const uchar *hash)
{
	if (thePrefs.IsRememberingCancelledFiles()) {
		if (m_dwCancelledFilesSeed == 0)
			m_dwCancelledFilesSeed = (GetRandomUInt32() % 0xFFFFFFFE) + 1;

		uchar pachSeedHash[20];
		PokeUInt32(pachSeedHash, m_dwCancelledFilesSeed);
		md4cpy(pachSeedHash + 4, hash);
		MD5Sum md5(pachSeedHash, sizeof pachSeedHash);
		md4cpy(pachSeedHash, md5.GetRawHash());
		m_mapCancelledFiles.SetAt(CSKey(pachSeedHash), 1);
	}
}

bool CKnownFileList::IsCancelledFileByID(const uchar *hash) const
{
	if (thePrefs.IsRememberingCancelledFiles()) {
		uchar pachSeedHash[20];
		PokeUInt32(pachSeedHash, m_dwCancelledFilesSeed);
		md4cpy(pachSeedHash + 4, hash);
		MD5Sum md5(pachSeedHash, sizeof pachSeedHash);
		md4cpy(pachSeedHash, md5.GetRawHash());

		int dwDummy;
		if (m_mapCancelledFiles.Lookup(CSKey(pachSeedHash), dwDummy))
			return true;
	}
	return false;
}

void CKnownFileList::CopyKnownFileMap(CMap<CCKey, const CCKey&, CKnownFile*, CKnownFile*> &Files_Map)
{
	CCKey key;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
		CKnownFile *cur_file;
		m_Files_map.GetNextAssoc(pos, key, cur_file);
		Files_Map.SetAt(key, cur_file);
	}
}

bool CKnownFileList::ShouldPurgeAICHHashset(const CAICHHash &rAICHHash) const
{
	const CKnownFile *pFile;
	if (m_mapKnownFilesByAICH.Lookup(rAICHHash, pFile)) {
		if (!pFile->ShouldPartiallyPurgeFile())
			return false;
	} else
		ASSERT(0);
	return true;
}

void CKnownFileList::AICHHashChanged(const CAICHHash *pOldAICHHash, const CAICHHash &rNewAICHHash, CKnownFile *pFile)
{
	if (pOldAICHHash != NULL)
		m_mapKnownFilesByAICH.RemoveKey(*pOldAICHHash);
	m_mapKnownFilesByAICH.SetAt(rNewAICHHash, pFile);
}