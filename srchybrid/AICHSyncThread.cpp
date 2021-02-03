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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "StdAfx.h"
#include "aichsyncthread.h"
#include "shahashset.h"
#include "safefile.h"
#include "knownfile.h"
#include "sha.h"
#include "emule.h"
#include "emuledlg.h"
#include "sharedfilelist.h"
#include "knownfilelist.h"
#include "sharedfileswnd.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////////////////
///CAICHSyncThread
IMPLEMENT_DYNCREATE(CAICHSyncThread, CWinThread)

BOOL CAICHSyncThread::InitInstance()
{
	DbgSetThreadName("AICHSyncThread");
	InitThreadLocale();
	return TRUE;
}

int CAICHSyncThread::Run()
{
	if (theApp.IsClosing())
		return 0;

	// we collect all masterhashes which we find in the known2.met and store them in a list
	CArray<CAICHHash> aKnown2Hashes;
	CArray<ULONGLONG> aKnown2HashesFilePos;
	const CString &fullpath(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + KNOWN2_MET_FILENAME);

	CSafeFile file;
	uint32 nLastVerifiedPos = 0;

	// we need to keep a lock on this file while the thread is running
	CSingleLock lockKnown2Met(&CAICHRecoveryHashSet::m_mutKnown2File, TRUE);
	bool bJustCreated = ConvertToKnown2ToKnown264(&file);

	CFileException fexp;
	if (!bJustCreated && !file.Open(fullpath, CFile::modeCreate | CFile::modeReadWrite | CFile::modeNoTruncate | CFile::osSequentialScan | CFile::typeBinary | CFile::shareDenyNone, &fexp)) {
		if (fexp.m_cause != CFileException::fileNotFound) {
			CString strError(_T("Failed to load ") KNOWN2_MET_FILENAME _T(" file"));
			TCHAR szError[MAX_CFEXP_ERRORMSG];
			if (GetExceptionMessage(fexp, szError, _countof(szError)))
				strError.AppendFormat(_T(" - %s"), szError);
			LogError(LOG_STATUSBAR, _T("%s"), (LPCTSTR)strError);
		}
		return 0;
	}
	try {
		if (file.GetLength() >= 1) {
			uint8 header = file.ReadUInt8();
			if (header != KNOWN2_MET_VERSION)
				AfxThrowFileException(CFileException::endOfFile, 0, file.GetFileName());

			//setvbuf(file.m_pStream, NULL, _IOFBF, 16384);
			ULONGLONG nExistingSize = file.GetLength();
			while (file.GetPosition() < nExistingSize) {
				aKnown2HashesFilePos.Add(file.GetPosition());
				aKnown2Hashes.Add(CAICHHash(&file));
				uint32 nHashCount = file.ReadUInt32();
				if (file.GetPosition() + nHashCount * (ULONGLONG)CAICHHash::GetHashSize() > nExistingSize)
					AfxThrowFileException(CFileException::endOfFile, 0, file.GetFileName());

				// skip the rest of this hashset
				file.Seek(nHashCount * (LONGLONG)CAICHHash::GetHashSize(), CFile::current);
				nLastVerifiedPos = (uint32)file.GetPosition();
			}
		} else
			file.WriteUInt8(KNOWN2_MET_VERSION);
	} catch (CFileException *error) {
		if (error->m_cause == CFileException::endOfFile) {
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_MET_BAD), KNOWN2_MET_FILENAME);
			// truncate the file to the size to the last verified valid pos
			try {
				file.SetLength(nLastVerifiedPos);
				if (file.GetLength() == 0) {
					file.SeekToBegin();
					file.WriteUInt8(KNOWN2_MET_VERSION);
				}
			} catch (CFileException *error2) {
				error2->Delete();
			}
		} else {
			TCHAR buffer[MAX_CFEXP_ERRORMSG];
			GetExceptionMessage(*error, buffer, _countof(buffer));
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_SERVERMET_UNKNOWN), buffer);
		}
		error->Delete();
		return 0;
	}

	// now we check that all files which are in the shared file list have a corresponding hash in out list
	// those who don't are added to the hashing list
	CList<CAICHHash> liUsedHashes;
	CSingleLock sharelock(&theApp.sharedfiles->m_mutWriteList, TRUE);

	bool bDbgMsgCreatingPartHashes = true;
	for (POSITION pos = BEFORE_START_POSITION; pos != NULL;) {
		if (theApp.IsClosing()) // in case of shutdown while still hashing
			return 0;
		CKnownFile *pFile = theApp.sharedfiles->GetFileNext(pos);
		if (pFile != NULL && !pFile->IsPartFile()) {
			CFileIdentifier &fileid = pFile->GetFileIdentifier();
			if (fileid.HasAICHHash()) {
				bool bAICHfound = false;
				for (INT_PTR i = aKnown2Hashes.GetCount(); --i >= 0;) {
					if (aKnown2Hashes[i] == fileid.GetAICHHash()) {
						bAICHfound = true;
						liUsedHashes.AddTail(CAICHHash(aKnown2Hashes[i]));
						pFile->SetAICHRecoverHashSetAvailable(true);
						// Has the file the proper AICH Part hashset? If not, probably upgrading, create it
						if (!fileid.HasExpectedAICHHashCount()) {
							if (bDbgMsgCreatingPartHashes) {
								bDbgMsgCreatingPartHashes = false;
								DebugLogWarning(_T("Missing AICH Part Hashsets for known files - maybe upgrading from earlier version. Creating them out of full AICH recovery Hashsets, shouldn't take too long"));
							}
							CAICHRecoveryHashSet tempHashSet(pFile, pFile->GetFileSize());
							tempHashSet.SetMasterHash(fileid.GetAICHHash(), AICH_HASHSETCOMPLETE);
							if (!tempHashSet.LoadHashSet()) {
								ASSERT(0);
								DebugLogError(_T("Failed to load full AICH recovery Hashset - known2.met might be corrupt. Unable to create AICH Part Hashset - %s"), (LPCTSTR)pFile->GetFileName());
							} else {
								if (!fileid.SetAICHHashSet(tempHashSet)) {
									DebugLogError(_T("Failed to create AICH Part Hashset out of full AICH recovery Hashset - %s"), (LPCTSTR)pFile->GetFileName());
									ASSERT(0);
								}
								ASSERT(fileid.HasExpectedAICHHashCount());
							}
						}
						//theApp.QueueDebugLogLine(false, _T("%s - %s"), current_hash.GetString(), pFile->GetFileName());
						break;
					}
				}
				if (bAICHfound)
					continue;
			}
			pFile->SetAICHRecoverHashSetAvailable(false);
			m_liToHash.AddTail(pFile);
		}
	}
	sharelock.Unlock();

	// remove all unused AICH hashsets from known2.met
	if (liUsedHashes.GetCount() != aKnown2Hashes.GetCount()
		&& (!thePrefs.IsRememberingDownloadedFiles() || thePrefs.DoPartiallyPurgeOldKnownFiles()))
	{
		file.SeekToBegin();
		try {
			uint8 header = file.ReadUInt8();
			if (header != KNOWN2_MET_VERSION)
				AfxThrowFileException(CFileException::endOfFile, 0, file.GetFileName());

			ULONGLONG nExistingSize = file.GetLength();
			ULONGLONG posWritePos = file.GetPosition();
			ULONGLONG posReadPos = posWritePos;
			uint32 nPurgeCount = 0;
			uint32 nPurgeBecauseOld = 0;
			uint32 nPurgeDups = 0;
			static const CAICHHash empty; //zero AICH hash
			while (file.GetPosition() < nExistingSize) {
				ULONGLONG nCurrentHashsetPos = file.GetPosition();
				ULONGLONG posTmp = 0; //position of an old duplicate hash
				CAICHHash aichHash(&file);
				uint32 nHashCount = file.ReadUInt32();
				if (file.GetPosition() + nHashCount * (ULONGLONG)CAICHHash::GetHashSize() > nExistingSize)
					AfxThrowFileException(CFileException::endOfFile, 0, file.GetFileName());

				if (aichHash == empty || (!thePrefs.IsRememberingDownloadedFiles() && liUsedHashes.Find(aichHash) == NULL)) {
					// unused hashset skip the rest of this hashset
					file.Seek(nHashCount * (LONGLONG)CAICHHash::GetHashSize(), CFile::current);
					++nPurgeCount;
				} else if (thePrefs.IsRememberingDownloadedFiles() && theApp.knownfiles->ShouldPurgeAICHHashset(aichHash)) {
					ASSERT(thePrefs.DoPartiallyPurgeOldKnownFiles());
					// also unused (purged) hashset skip the rest of this hashset
					file.Seek(nHashCount * (LONGLONG)CAICHHash::GetHashSize(), CFile::current);
					++nPurgeCount;
					++nPurgeBecauseOld;
				} else if (nPurgeCount == 0) {
					// used Hashset, but it does not need to be moved as nothing changed yet
					file.Seek(nHashCount * (LONGLONG)CAICHHash::GetHashSize(), CFile::current);
					posReadPos = posWritePos = file.GetPosition();
					posTmp = CAICHRecoveryHashSet::AddStoredAICHHash(aichHash, nCurrentHashsetPos);
				} else {
					// used Hashset, move position in file
					BYTE *buffer = new BYTE[nHashCount * (size_t)CAICHHash::GetHashSize()];
					file.Read(buffer, nHashCount * CAICHHash::GetHashSize());
					posReadPos = file.GetPosition();
					file.Seek(posWritePos, CFile::begin);
					file.Write(aichHash.GetRawHashC(), CAICHHash::GetHashSize());
					file.WriteUInt32(nHashCount);
					file.Write(buffer, nHashCount * CAICHHash::GetHashSize());
					delete[] buffer;
					posTmp = CAICHRecoveryHashSet::AddStoredAICHHash(aichHash, posWritePos);

					posWritePos = file.GetPosition();
					file.Seek(posReadPos, CFile::begin);
				}
				if (posTmp) {
					file.Seek(posTmp, CFile::begin);
					file.Write(empty.GetRawHashC(), CAICHHash::GetHashSize()); //mark this for purging
					file.Seek(posReadPos, CFile::begin);
					++nPurgeDups;
				}
			}
			posReadPos = file.GetPosition();
			file.SetLength(posWritePos);
			file.Flush();
			file.Close();
			theApp.QueueDebugLogLine(false, _T("Cleaned up known2.met, removed %u hashsets and purged %u hashsets of old known files (%s)")
				, nPurgeCount - nPurgeBecauseOld, nPurgeBecauseOld, (LPCTSTR)CastItoXBytes(posReadPos - posWritePos));
			if (nPurgeDups)
				theApp.QueueDebugLogLine(false, _T("Marked %u duplicate hashsets for purging"), nPurgeDups);
		} catch (CFileException *error) {
			if (error->m_cause == CFileException::endOfFile) {
				// we just parsed this file some ms ago, should never happen here
				ASSERT(0);
			} else {
				TCHAR buffer[MAX_CFEXP_ERRORMSG];
				GetExceptionMessage(*error, buffer, _countof(buffer));
				LogError(LOG_STATUSBAR, GetResString(IDS_ERR_SERVERMET_UNKNOWN), buffer);
			}
			error->Delete();
			return 0;
		}
	} else {
		// remember (/index) all hashes which are stored in the file for faster checking later on
		for (INT_PTR i = 0; !theApp.IsClosing() && i < aKnown2Hashes.GetCount(); ++i)
			CAICHRecoveryHashSet::AddStoredAICHHash(aKnown2Hashes[i], aKnown2HashesFilePos[i]);
	}

#ifdef _DEBUG
	for (POSITION pos = liUsedHashes.GetHeadPosition(); !theApp.IsClosing() && pos != NULL;) {
		CKnownFile *pFile = theApp.sharedfiles->GetFileByAICH(liUsedHashes.GetNext(pos));
		if (pFile == NULL) {
			ASSERT(0);
			continue;
		}
		CAICHRecoveryHashSet *pTempHashSet = new CAICHRecoveryHashSet(pFile);
		pTempHashSet->SetFileSize(pFile->GetFileSize());
		pTempHashSet->SetMasterHash(pFile->GetFileIdentifier().GetAICHHash(), AICH_HASHSETCOMPLETE);
		ASSERT(pTempHashSet->LoadHashSet());
		delete pTempHashSet;
	}
#endif

	lockKnown2Met.Unlock();
	// warn the user if he just upgraded
	if (thePrefs.IsFirstStart() && !m_liToHash.IsEmpty() && !bJustCreated)
		LogWarning(GetResString(IDS_AICH_WARNUSER));

	if (!m_liToHash.IsEmpty()) {
		theApp.QueueLogLine(true, GetResString(IDS_AICH_SYNCTOTAL), m_liToHash.GetCount());
		theApp.emuledlg->sharedfileswnd->sharedfilesctrl.SetAICHHashing(m_liToHash.GetCount());
		// let first all normal hashing be done before starting out sync hashing
		CSingleLock sLock1(&theApp.hashing_mut); // only one file hash at a time
		while (theApp.sharedfiles->GetHashingCount() != 0) {
			if (theApp.IsClosing())
				return 0;
			::Sleep(100);
		}
		sLock1.Lock();
		INT_PTR cDone = 0;
		for (POSITION pos = m_liToHash.GetHeadPosition(); pos != NULL; ++cDone) {
			if (theApp.IsClosing()) // in case of shutdown while still hashing
				return 0;

			theApp.emuledlg->sharedfileswnd->sharedfilesctrl.SetAICHHashing(m_liToHash.GetCount() - cDone);
			if (theApp.emuledlg->sharedfileswnd->sharedfilesctrl.m_hWnd != NULL)
				theApp.emuledlg->sharedfileswnd->sharedfilesctrl.ShowFilesCount();
			CKnownFile *pCurFile = m_liToHash.GetNext(pos);
			// just to be sure that the file hasn't been deleted lately
			if (!(theApp.knownfiles->IsKnownFile(pCurFile) && theApp.sharedfiles->GetFileByID(pCurFile->GetFileHash())))
				continue;
			theApp.QueueLogLine(false, GetResString(IDS_AICH_CALCFILE), (LPCTSTR)pCurFile->GetFileName());
			if (!pCurFile->CreateAICHHashSetOnly())
				theApp.QueueDebugLogLine(false, _T("Failed to create AICH Hashset while sync. for file %s"), (LPCTSTR)pCurFile->GetFileName());
		}

		theApp.emuledlg->sharedfileswnd->sharedfilesctrl.SetAICHHashing(0);
		if (theApp.emuledlg->sharedfileswnd->sharedfilesctrl.m_hWnd != NULL)
			theApp.emuledlg->sharedfileswnd->sharedfilesctrl.ShowFilesCount();
		sLock1.Unlock();
	}

	theApp.QueueDebugLogLine(false, _T("AICHSyncThread finished"));
	return 0;
}

bool CAICHSyncThread::ConvertToKnown2ToKnown264(CSafeFile *pTargetFile)
{
	// converting known2.met to known2_64.met to support large files
	// changing hashcount from uint16 to uint32

	// there still exists a lock on known2_64.met and it should be not opened at this point
	CString oldfullpath(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + OLD_KNOWN2_MET_FILENAME);
	CString newfullpath(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + KNOWN2_MET_FILENAME);

	if (PathFileExists(newfullpath) || !PathFileExists(oldfullpath))
		// only continue if the old file does and the new file does not exists
		return false;

	CSafeFile oldfile;
	CFileException fexp;
	if (!oldfile.Open(oldfullpath, CFile::modeRead | CFile::osSequentialScan | CFile::typeBinary | CFile::shareDenyNone, &fexp)) {
		if (fexp.m_cause != CFileException::fileNotFound) {
			CString strError(_T("Failed to load ") OLD_KNOWN2_MET_FILENAME _T(" file"));
			TCHAR szError[MAX_CFEXP_ERRORMSG];
			if (GetExceptionMessage(fexp, szError, _countof(szError)))
				strError.AppendFormat(_T(" - %s"), szError);
			LogError(LOG_STATUSBAR, _T("%s"), (LPCTSTR)strError);
		}
		// else -> known2.met also doesn't exists, so nothing to convert
		return false;
	}

	if (!pTargetFile->Open(newfullpath, CFile::modeCreate | CFile::modeReadWrite | CFile::osSequentialScan | CFile::typeBinary | CFile::shareDenyNone, &fexp)) {
		if (fexp.m_cause != CFileException::fileNotFound) {
			CString strError(_T("Failed to load ") KNOWN2_MET_FILENAME _T(" file"));
			TCHAR szError[MAX_CFEXP_ERRORMSG];
			if (GetExceptionMessage(fexp, szError, _countof(szError)))
				strError.AppendFormat(_T(" - %s"), szError);
			LogError(LOG_STATUSBAR, _T("%s"), (LPCTSTR)strError);
		}
		return false;
	}

	theApp.QueueLogLine(false, GetResString(IDS_CONVERTINGKNOWN2MET), OLD_KNOWN2_MET_FILENAME, KNOWN2_MET_FILENAME);

	try {
		pTargetFile->WriteUInt8(KNOWN2_MET_VERSION);
		while (oldfile.GetPosition() < oldfile.GetLength()) {
			CAICHHash aichHash(&oldfile);
			uint32 nHashCount = oldfile.ReadUInt16();
			if (oldfile.GetPosition() + nHashCount * (ULONGLONG)CAICHHash::GetHashSize() > oldfile.GetLength())
				AfxThrowFileException(CFileException::endOfFile, 0, oldfile.GetFileName());

			BYTE *buffer = new BYTE[nHashCount * (size_t)CAICHHash::GetHashSize()];
			oldfile.Read(buffer, nHashCount * CAICHHash::GetHashSize());
			pTargetFile->Write(aichHash.GetRawHash(), CAICHHash::GetHashSize());
			pTargetFile->WriteUInt32(nHashCount);
			pTargetFile->Write(buffer, nHashCount * CAICHHash::GetHashSize());
			delete[] buffer;
		}
		pTargetFile->Flush();
		oldfile.Close();
	} catch (CFileException *error) {
		if (error->m_cause == CFileException::endOfFile) {
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_MET_BAD), OLD_KNOWN2_MET_FILENAME);
			ASSERT(0);
		} else {
			TCHAR buffer[MAX_CFEXP_ERRORMSG];
			GetExceptionMessage(*error, buffer, _countof(buffer));
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_SERVERMET_UNKNOWN), buffer);
		}
		error->Delete();
		theApp.QueueLogLine(false, GetResString(IDS_CONVERTINGKNOWN2FAILED));
		pTargetFile->Close();
		return false;
	}
	theApp.QueueLogLine(false, GetResString(IDS_CONVERTINGKNOWN2DONE));

	// FIXME LARGE FILES (uncomment)
	//::DeleteFile(oldfullpath);
	pTargetFile->SeekToBegin();
	return true;
}