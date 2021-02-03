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
#include <sys/stat.h>
#include "emule.h"
#include "SharedFileList.h"
#include "KnownFileList.h"
#include "Packets.h"
#include "Kademlia/Kademlia/Kademlia.h"
#include "kademlia/kademlia/search.h"
#include "kademlia/kademlia/SearchManager.h"
#include "kademlia/kademlia/prefs.h"
#include "kademlia/kademlia/Tag.h"
#include "DownloadQueue.h"
#include "Statistics.h"
#include "Preferences.h"
#include "OtherFunctions.h"
#include "KnownFile.h"
#include "ServerConnect.h"
#include "SafeFile.h"
#include "Server.h"
#include "UpDownClient.h"
#include "PartFile.h"
#include "emuledlg.h"
#include "SharedFilesWnd.h"
#include "StringConversion.h"
#include "ClientList.h"
#include "Log.h"
#include "Collection.h"
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "md5sum.h"
#include "ImportParts.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


typedef CSimpleArray<CKnownFile*> CSimpleKnownFileArray;
#define	SHAREDFILES_FILE	_T("sharedfiles.dat")


///////////////////////////////////////////////////////////////////////////////
// CPublishKeyword

class CPublishKeyword
{
public:
	explicit CPublishKeyword(const Kademlia::CKadTagValueString &rstrKeyword)
		: m_strKeyword(rstrKeyword)
	{
		// min. keyword char is allowed to be < 3 in some cases (see also 'CSearchManager::GetWords')
		//ASSERT( rstrKeyword.GetLength() >= 3 );
		ASSERT(!rstrKeyword.IsEmpty());
		KadGetKeywordHash(rstrKeyword, &m_nKadID);
		SetNextPublishTime(0);
		SetPublishedCount(0);
	}

	const Kademlia::CUInt128 &GetKadID() const			{ return m_nKadID; }
	const Kademlia::CKadTagValueString &GetKeyword() const { return m_strKeyword; }
	int GetRefCount() const								{ return m_aFiles.GetSize(); }
	const CSimpleKnownFileArray &GetReferences() const	{ return m_aFiles; }

	time_t GetNextPublishTime() const					{ return m_tNextPublishTime; }
	void SetNextPublishTime(time_t tNextPublishTime)	{ m_tNextPublishTime = tNextPublishTime; }

	UINT GetPublishedCount() const						{ return m_uPublishedCount; }
	void SetPublishedCount(UINT uPublishedCount)		{ m_uPublishedCount = uPublishedCount; }
	void IncPublishedCount()							{ ++m_uPublishedCount; }

	BOOL AddRef(CKnownFile *pFile)
	{
		if (m_aFiles.Find(pFile) >= 0) {
			ASSERT(0);
			return FALSE;
		}
		return m_aFiles.Add(pFile);
	}

	int RemoveRef(CKnownFile *pFile)
	{
		m_aFiles.Remove(pFile);
		return m_aFiles.GetSize();
	}

	void RemoveAllReferences()
	{
		m_aFiles.RemoveAll();
	}

	void RotateReferences(int iRotateSize)
	{
		if (m_aFiles.GetSize() > iRotateSize) {
			int i = m_aFiles.GetSize() - iRotateSize;
			CKnownFile **ppRotated = reinterpret_cast<CKnownFile**>(malloc(m_aFiles.m_nAllocSize * sizeof(*m_aFiles.GetData())));
			if (ppRotated != NULL) {
				memcpy(ppRotated, m_aFiles.GetData() + iRotateSize, i * sizeof(*m_aFiles.GetData()));
				memcpy(ppRotated + i, m_aFiles.GetData(), iRotateSize * sizeof(*m_aFiles.GetData()));
				free(m_aFiles.GetData());
				m_aFiles.m_aT = ppRotated;
			}
		}
	}

protected:
	CSimpleKnownFileArray m_aFiles;
	Kademlia::CKadTagValueString m_strKeyword;
	Kademlia::CUInt128 m_nKadID;
	time_t m_tNextPublishTime;
	UINT m_uPublishedCount;
};


///////////////////////////////////////////////////////////////////////////////
// CPublishKeywordList

class CPublishKeywordList
{
public:
	CPublishKeywordList();
	~CPublishKeywordList();

	void AddKeywords(CKnownFile *pFile);
	void RemoveKeywords(CKnownFile *pFile);
	void RemoveAllKeywords();

	void RemoveAllKeywordReferences();
	void PurgeUnreferencedKeywords();

	INT_PTR GetCount() const								{ return m_lstKeywords.GetCount(); }

	CPublishKeyword *GetNextKeyword();
	void ResetNextKeyword();

	time_t GetNextPublishTime() const						{ return m_tNextPublishKeywordTime; }
	void SetNextPublishTime(time_t tNextPublishKeywordTime)	{ m_tNextPublishKeywordTime = tNextPublishKeywordTime; }

#ifdef _DEBUG
	void Dump();
#endif

protected:
	// can't use a CMap - too many disadvantages in processing the 'list'
	//CTypedPtrMap<CMapStringToPtr, CString, CPublishKeyword*> m_lstKeywords;
	CTypedPtrList<CPtrList, CPublishKeyword*> m_lstKeywords;
	POSITION m_posNextKeyword;
	time_t m_tNextPublishKeywordTime;

	CPublishKeyword *FindKeyword(const CStringW &rstrKeyword, POSITION *ppos = NULL) const;
};

CPublishKeywordList::CPublishKeywordList()
{
	ResetNextKeyword();
	SetNextPublishTime(0);
}

CPublishKeywordList::~CPublishKeywordList()
{
	RemoveAllKeywords();
}

CPublishKeyword *CPublishKeywordList::GetNextKeyword()
{
	if (m_posNextKeyword == NULL) {
		m_posNextKeyword = m_lstKeywords.GetHeadPosition();
		if (m_posNextKeyword == NULL)
			return NULL;
	}
	return m_lstKeywords.GetNext(m_posNextKeyword);
}

void CPublishKeywordList::ResetNextKeyword()
{
	m_posNextKeyword = m_lstKeywords.GetHeadPosition();
}

CPublishKeyword *CPublishKeywordList::FindKeyword(const CStringW &rstrKeyword, POSITION *ppos) const
{
	for (POSITION pos = m_lstKeywords.GetHeadPosition(); pos != NULL;) {
		POSITION posLast = pos;
		CPublishKeyword *pPubKw = m_lstKeywords.GetNext(pos);
		if (pPubKw->GetKeyword() == rstrKeyword) {
			if (ppos)
				*ppos = posLast;
			return pPubKw;
		}
	}
	return NULL;
}

void CPublishKeywordList::AddKeywords(CKnownFile *pFile)
{
	const Kademlia::WordList &wordlist = pFile->GetKadKeywords();
	//ASSERT( !wordlist.empty() );
	for (Kademlia::WordList::const_iterator it = wordlist.begin(); it != wordlist.end(); ++it) {
		const CStringW &strKeyword = *it;
		CPublishKeyword *pPubKw = FindKeyword(strKeyword);
		if (pPubKw == NULL) {
			pPubKw = new CPublishKeyword(Kademlia::CKadTagValueString(strKeyword));
			m_lstKeywords.AddTail(pPubKw);
			SetNextPublishTime(0);
		}
		if (pPubKw->AddRef(pFile) && pPubKw->GetNextPublishTime() > MIN2S(30)) {
			// User may be adding and removing files, so if this is a keyword that
			// has already been published, we reduce the time, but still give the user
			// enough time to finish what they are doing.
			// If this is a hot node, the Load list will prevent from republishing.
			pPubKw->SetNextPublishTime(MIN2S(30));
		}
	}
}

void CPublishKeywordList::RemoveKeywords(CKnownFile *pFile)
{
	const Kademlia::WordList &wordlist = pFile->GetKadKeywords();
	//ASSERT( !wordlist.empty() );
	for (Kademlia::WordList::const_iterator it = wordlist.begin(); it != wordlist.end(); ++it) {
		const CStringW &strKeyword = *it;
		POSITION pos;
		CPublishKeyword *pPubKw = FindKeyword(strKeyword, &pos);
		if (pPubKw != NULL && pPubKw->RemoveRef(pFile) == 0) {
			if (pos == m_posNextKeyword)
				(void)m_lstKeywords.GetNext(m_posNextKeyword);
			m_lstKeywords.RemoveAt(pos);
			delete pPubKw;
			SetNextPublishTime(0);
		}
	}
}

void CPublishKeywordList::RemoveAllKeywords()
{
	while (!m_lstKeywords.IsEmpty())
		delete m_lstKeywords.RemoveHead();
	ResetNextKeyword();
	SetNextPublishTime(0);
}

void CPublishKeywordList::RemoveAllKeywordReferences()
{
	for (POSITION pos = m_lstKeywords.GetHeadPosition(); pos != NULL;)
		m_lstKeywords.GetNext(pos)->RemoveAllReferences();
}

void CPublishKeywordList::PurgeUnreferencedKeywords()
{
	for (POSITION pos = m_lstKeywords.GetHeadPosition(); pos != NULL;) {
		POSITION posLast = pos;
		const CPublishKeyword *pPubKw = m_lstKeywords.GetNext(pos);
		if (pPubKw->GetRefCount() == 0) {
			if (posLast == m_posNextKeyword)
				m_posNextKeyword = pos;
			m_lstKeywords.RemoveAt(posLast);
			delete pPubKw;
			SetNextPublishTime(0);
		}
	}
}

#ifdef _DEBUG
void CPublishKeywordList::Dump()
{
	unsigned i = 0;
	for (POSITION pos = m_lstKeywords.GetHeadPosition(); pos != NULL;) {
		CPublishKeyword *pPubKw = m_lstKeywords.GetNext(pos);
		TRACE(_T("%3u: %-10ls  ref=%u  %s\n"), i, (LPCTSTR)pPubKw->GetKeyword(), pPubKw->GetRefCount(), (LPCTSTR)CastSecondsToHM(pPubKw->GetNextPublishTime()));
		++i;
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
// CAddFileThread

IMPLEMENT_DYNCREATE(CAddFileThread, CWinThread)

CAddFileThread::CAddFileThread()
	: m_pOwner()
	, m_partfile()
{
}

void CAddFileThread::SetValues(CSharedFileList *pOwner, LPCTSTR directory, LPCTSTR filename, LPCTSTR strSharedDir, CPartFile *partfile)
{
	m_pOwner = pOwner;
	m_strDirectory = directory;
	m_strFilename = filename;
	m_partfile = partfile;
	m_strSharedDir = strSharedDir;
}

// Special case for SR13-ImportParts
uint16 CAddFileThread::SetPartToImport(LPCTSTR import)
{
	if (m_partfile->GetFilePath() == import)
		return 0;

	m_strImport = import;

	for (uint16 i = 0; i < m_partfile->GetPartCount(); ++i)
		if (!m_partfile->IsComplete(i, false))
			m_PartsToImport.Add(i);

	return (uint16)m_PartsToImport.GetSize();
}

bool CAddFileThread::ImportParts()
{
	CFile f;
	if (!f.Open(m_strImport, CFile::modeRead | CFile::shareDenyNone)) {
		LogError(LOG_STATUSBAR, GetResString(IDS_IMPORTPARTS_ERR_CANTOPENFILE), (LPCTSTR)m_strImport);
		return false;
	}

	CString strFilePath;
	_tmakepath(strFilePath.GetBuffer(MAX_PATH), NULL, m_strDirectory, m_strFilename, NULL);
	strFilePath.ReleaseBuffer();

	Log(LOG_STATUSBAR, GetResString(IDS_IMPORTPARTS_IMPORTSTART), m_PartsToImport.GetSize(), (LPCTSTR)strFilePath);

	uint64 fileSize = f.GetLength();
	CKnownFile *kfimport = new CKnownFile;
	BYTE *partData = NULL;
	unsigned partsuccess = 0;
	for (INT_PTR i = 0; i < m_PartsToImport.GetSize(); ++i) {
		const uint16 partnumber = m_PartsToImport[i];
		const uint64 uStart = PARTSIZE * partnumber;
		if (uStart > fileSize)
			break;

		try {
			uint32 partSize;
			try {
				if (partData == NULL)
					partData = new BYTE[PARTSIZE];
				*(uint64*)partData = 0; //quick check for zero
				CSingleLock sLock1(&theApp.hashing_mut, TRUE);	//SafeHash - wait for the current hashing process end before reading a chunk
				f.Seek((LONGLONG)uStart, CFile::begin);
				partSize = f.Read(partData, PARTSIZE);
				if (*(uint64*)partData == 0 && (partSize <= sizeof(uint64) || !memcmp(partData, partData + sizeof(uint64), partSize - sizeof(uint64))))
					continue;
			} catch (...) {
				LogWarning(LOG_STATUSBAR, _T("Part %i: Not accessible (You may have a bad cluster on your hard disk)."), (int)partnumber);
				continue;
			}
			uchar hash[MDX_DIGEST_SIZE];
			kfimport->CreateHash(partData, partSize, hash);
			ImportPart_Struct *importpart = new ImportPart_Struct;
			importpart->start = uStart;
			importpart->end = importpart->start + partSize - 1;
			importpart->data = partData;
			if (!theApp.emuledlg->PostMessage(TM_IMPORTPART, (WPARAM)importpart, (LPARAM)m_partfile))
				break;
			partData = NULL; //Will be deleted in async write thread
			//Log(LOG_STATUSBAR, GetResString(IDS_IMPORTPARTS_PARTIMPORTEDGOOD), partnumber);
			++partsuccess;

			if (theApp.IsRunning()) {
				WPARAM uProgress = (WPARAM)(i * 100 / m_PartsToImport.GetSize());
				VERIFY(theApp.emuledlg->PostMessage(TM_FILEOPPROGRESS, uProgress, (LPARAM)m_partfile));
				::Sleep(100); // sleep very shortly to give time to write (or else mem grows!)
			}

			if (!theApp.IsRunning() || partSize != PARTSIZE || m_partfile->GetFileOp() != PFOP_IMPORTPARTS)
				break;
		} catch (...) {
		}
	}
	f.Close();
	delete[] partData;
	delete kfimport;

	try {
		bool importaborted = !theApp.IsRunning() || m_partfile->GetFileOp() == PFOP_NONE;
		if (m_partfile->GetFileOp() == PFOP_IMPORTPARTS)
			m_partfile->SetFileOp(PFOP_NONE);
		Log(LOG_STATUSBAR, _T("Import %s. %u parts imported to %s.")
			, importaborted ? _T("aborted") : _T("completed")
			, partsuccess
			, (LPCTSTR)m_strFilename);
	} catch (...) {
		//This could happen if we deleted the part file instance
	}
	return true;
}

BOOL CAddFileThread::InitInstance()
{
	InitThreadLocale();
	return TRUE;
}

int CAddFileThread::Run()
{
	DbgSetThreadName(m_partfile && m_partfile->GetFileOp() == PFOP_IMPORTPARTS ? "ImportingParts %s" : "Hashing %s", (LPCTSTR)m_strFilename);
	if (theApp.IsClosing() || !(m_pOwner || m_partfile) || m_strFilename.IsEmpty())
		return 0;

	(void)CoInitialize(NULL);

	if (m_partfile && m_partfile->GetFileOp() == PFOP_IMPORTPARTS) {
		ImportParts();
		CoUninitialize();
		return 0;
	}

	// Locking that hashing thread is needed because we may create a couple of those threads
	// at startup when rehashing potentially corrupted downloading part files.
	// If all those hash threads would run concurrently, the io-system would be under
	// very heavy load and slowly progressing
	CSingleLock sLock1(&theApp.hashing_mut, TRUE); // hash only one file at a time

	TCHAR strFilePath[MAX_PATH];
	_tmakepathlimit(strFilePath, NULL, m_strDirectory, m_strFilename, NULL);
	if (m_partfile)
		Log(_T("%s \"%s\" \"%s\""), (LPCTSTR)GetResString(IDS_HASHINGFILE), (LPCTSTR)m_partfile->GetFileName(), strFilePath);
	else
		Log(_T("%s \"%s\""), (LPCTSTR)GetResString(IDS_HASHINGFILE), strFilePath);

	CKnownFile *newrecord = new CKnownFile();
	if (!theApp.IsClosing() && newrecord->CreateFromFile(m_strDirectory, m_strFilename, m_partfile)) { // SLUGFILLER: SafeHash - in case of shutdown while still hashing
		newrecord->SetSharedDirectory(m_strSharedDir);
		if (m_partfile && m_partfile->GetFileOp() == PFOP_HASHING)
			m_partfile->SetFileOp(PFOP_NONE);
		if (!theApp.emuledlg->PostMessage(TM_FINISHEDHASHING, (m_pOwner ? 0 : (WPARAM)m_partfile), (LPARAM)newrecord))
			delete newrecord;
	} else {
		if (!theApp.IsClosing()) {
			if (m_partfile && m_partfile->GetFileOp() == PFOP_HASHING)
				m_partfile->SetFileOp(PFOP_NONE);

			// SLUGFILLER: SafeHash - inform main program of hash failure
			if (m_pOwner) {
				UnknownFile_Struct *hashed = new UnknownFile_Struct;
				hashed->strDirectory = m_strDirectory;
				hashed->strName = m_strFilename;
				if (!theApp.emuledlg->PostMessage(TM_HASHFAILED, 0, (LPARAM)hashed))
					delete hashed;
			}
		}
		// SLUGFILLER: SafeHash
		delete newrecord;
	}

	sLock1.Unlock();
	CoUninitialize();
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// CSharedFileList

void CSharedFileList::AddDirectory(const CString &strDir, CStringList &dirlist)
{
	CString tmpDir(strDir);
	slosh(tmpDir);
	CString slDir(tmpDir);
	slDir.MakeLower();

	if (dirlist.Find(slDir) == NULL) {
		dirlist.AddHead(slDir);
		AddFilesFromDirectory(tmpDir);
	}
}

CSharedFileList::CSharedFileList(CServerConnect *in_server)
	: server(in_server)
	, output()
	, m_currFileSrc()
	, m_currFileNotes()
	//, m_currFileKey()
	, m_lastPublishKadSrc()
	, m_lastPublishKadNotes()
	, m_lastPublishED2K()
	, m_lastPublishED2KFlag(true)
	, bHaveSingleSharedFiles()
{
	m_Files_map.InitHashTable(1031);
	m_keywords = new CPublishKeywordList;

	LoadSingleSharedFilesList();
	FindSharedFiles();
}

CSharedFileList::~CSharedFileList()
{
	while (!waitingforhash_list.IsEmpty())
		delete waitingforhash_list.RemoveHead();
	// SLUGFILLER: SafeHash
	while (!currentlyhashing_list.IsEmpty())
		delete currentlyhashing_list.RemoveHead();
	// SLUGFILLER: SafeHash
	delete m_keywords;

#if defined(_BETA) || defined(_DEVBUILD)
	// On Beta builds we created a testfile, delete it when closing eMule
	CString tempDir = thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR);
	slosh(tempDir);
	CString strBetaFileName;
	strBetaFileName.Format(_T("eMule%u.%u%c.%u Beta Testfile "), CemuleApp::m_nVersionMjr,
		CemuleApp::m_nVersionMin, _T('a') + CemuleApp::m_nVersionUpd, CemuleApp::m_nVersionBld);
	const MD5Sum md5(strBetaFileName + CemuleApp::m_sPlatform);
	strBetaFileName += md5.GetHashString().Left(6) + _T(".txt");
	::DeleteFile(tempDir + strBetaFileName);
#endif
}

void CSharedFileList::CopySharedFileMap(CMap<CCKey, const CCKey &, CKnownFile*, CKnownFile*> & Files_Map)
{
	CCKey key;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
		CKnownFile *cur_file;
		m_Files_map.GetNextAssoc(pos, key, cur_file);
		Files_Map.SetAt(key, cur_file);
	}
}

void CSharedFileList::FindSharedFiles()
{
	if (!m_Files_map.IsEmpty() && theApp.downloadqueue) {
		CSingleLock listlock(&m_mutWriteList);

		CCKey key;
		for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
			CKnownFile *cur_file;
			m_Files_map.GetNextAssoc(pos, key, cur_file);
			if (!cur_file->IsKindOf(RUNTIME_CLASS(CPartFile))
				|| theApp.downloadqueue->IsPartFile(cur_file)
				|| theApp.knownfiles->IsFilePtrInList(cur_file)
				|| _taccess(cur_file->GetFilePath(), 0) != 0)
			{
				m_UnsharedFiles_map.SetAt(CSKey(cur_file->GetFileHash()), true);
				listlock.Lock();
				m_Files_map.RemoveKey(key);
				listlock.Unlock();
			}
		}
		theApp.downloadqueue->AddPartFilesToShare(); // read partfiles
	}

	// khaos::kmod+ Fix: Shared files loaded multiple times.
	CStringList l_sAdded;
	CString tempDir(thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR));
	slosh(tempDir);

#if defined(_BETA) || defined(_DEVBUILD)
	// In Beta version we create a test file which is published in order to make testing easier
	// by allowing to easily find files which are published and shared by "new" nodes
	CStdioFile f;
	CString strBetaFileName;
	strBetaFileName.Format(_T("eMule%u.%u%c.%u Beta Testfile "), CemuleApp::m_nVersionMjr,
		CemuleApp::m_nVersionMin, _T('a') + CemuleApp::m_nVersionUpd, CemuleApp::m_nVersionBld);
	const MD5Sum md5(strBetaFileName + CemuleApp::m_sPlatform);
	strBetaFileName.AppendFormat(_T("%s.txt"), (LPCTSTR)md5.GetHashString().Left(6));
	if (!f.Open(tempDir + strBetaFileName, CFile::modeCreate | CFile::modeWrite | CFile::shareDenyWrite))
		ASSERT(0);
	else {
		try {
			// do not translate the content!
			f.WriteString(strBetaFileName + '\n'); // guarantees a different hash on different versions
			f.WriteString(_T("This file is automatically created by eMule Beta versions to help the developers testing and debugging new the new features. eMule will delete this file when exiting, otherwise you can remove this file at any time.\nThanks for beta testing eMule :)"));
			f.Close();
		} catch (CFileException *ex) {
			ASSERT(0);
			ex->Delete();
		}
	}
#endif

	AddDirectory(tempDir, l_sAdded);

	for (int i = 1; i < thePrefs.GetCatCount(); ++i)
		AddDirectory(thePrefs.GetCatPath(i), l_sAdded);

	for (POSITION pos = thePrefs.shareddir_list.GetHeadPosition(); pos != NULL;)
		AddDirectory(thePrefs.shareddir_list.GetNext(pos), l_sAdded);

	// add all single shared files
	for (POSITION pos = m_liSingleSharedFiles.GetHeadPosition(); pos != NULL;)
		CheckAndAddSingleFile(m_liSingleSharedFiles.GetNext(pos));

	// khaos::kmod-
	if (waitingforhash_list.IsEmpty())
		AddLogLine(false, GetResString(IDS_SHAREDFOUND), (unsigned)m_Files_map.GetCount());
	else
		AddLogLine(false, GetResString(IDS_SHAREDFOUNDHASHING), (unsigned)m_Files_map.GetCount(), (unsigned)waitingforhash_list.GetCount());

	HashNextFile();
}

void CSharedFileList::AddFilesFromDirectory(const CString &rstrDirectory)
{
	CString strSearchPath(rstrDirectory);
	PathAddBackslash(strSearchPath.GetBuffer(strSearchPath.GetLength() + 1));
	strSearchPath.ReleaseBuffer();
	strSearchPath += _T('*');

	CFileFind ff;
	bool end = !ff.FindFile(strSearchPath, 0);
	if (end) {
		DWORD dwError = ::GetLastError();
		if (dwError != ERROR_FILE_NOT_FOUND)
			LogWarning(GetResString(IDS_ERR_SHARED_DIR), (LPCTSTR)rstrDirectory, (LPCTSTR)GetErrorMessage(dwError));
		return;
	}

	while (!end) {
		end = !ff.FindNextFile();
		CheckAndAddSingleFile(ff);
	}
	ff.Close();
}

bool CSharedFileList::AddSingleSharedFile(const CString &rstrFilePath, bool bNoUpdate)
{
	bool bExclude = false;
	// first check if we are explicitly excluding this file
	for (POSITION pos = m_liSingleExcludedFiles.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		if (rstrFilePath.CompareNoCase(m_liSingleExcludedFiles.GetNext(pos)) == 0) {
			bExclude = true;
			m_liSingleExcludedFiles.RemoveAt(pos2);
			break;
		}
	}

	// check if we share this file in general
	bool bShared = ShouldBeShared(rstrFilePath.Left(rstrFilePath.ReverseFind(_T('\\')) + 1), rstrFilePath, false);

	if (bShared && !bExclude)
		// we should share this file already
		return false;
	if (!bShared) {
		// the directory is not shared, so we need a special entry
		m_liSingleSharedFiles.AddTail(rstrFilePath);
	}
	return bNoUpdate || CheckAndAddSingleFile(rstrFilePath);
}

bool CSharedFileList::CheckAndAddSingleFile(const CString &rstrFilePath)
{
	CFileFind ff;
	bool end = !ff.FindFile(rstrFilePath, 0);
	if (end) {
		DWORD dwError = ::GetLastError();
		if (dwError != ERROR_FILE_NOT_FOUND)
			LogWarning(GetResString(IDS_ERR_SHARED_DIR), (LPCTSTR)rstrFilePath, (LPCTSTR)GetErrorMessage(dwError));
		return false;
	}
	ff.FindNextFile();
	CheckAndAddSingleFile(ff);
	ff.Close();
	HashNextFile();
	bHaveSingleSharedFiles = true;
	// GUI updating needs to be done by caller
	return true;
}

bool CSharedFileList::SafeAddKFile(CKnownFile *toadd, bool bOnlyAdd)
{
	RemoveFromHashing(toadd);	// SLUGFILLER: SafeHash - hashed OK, remove from list if it was in
	bool bAdded = AddFile(toadd);
	if (!bOnlyAdd) {
		if (bAdded && output) {
			output->AddFile(toadd);
			output->ShowFilesCount();
		}
		m_lastPublishED2KFlag = true;
	}
	return bAdded;
}

void CSharedFileList::RepublishFile(CKnownFile *pFile)
{
	CServer *pCurServer = server->GetCurrentServer();
	if (pCurServer && (pCurServer->GetTCPFlags() & SRV_TCPFLG_COMPRESSION)) {
		m_lastPublishED2KFlag = true;
		pFile->SetPublishedED2K(false); // FIXME: this creates a wrong 'No' for the ed2k shared info in the listview until the file is shared again.
	}
}

bool CSharedFileList::AddFile(CKnownFile *pFile)
{
	ASSERT(pFile->GetFileIdentifier().HasExpectedMD4HashCount());
	ASSERT(!pFile->IsKindOf(RUNTIME_CLASS(CPartFile)) || !static_cast<CPartFile*>(pFile)->m_bMD4HashsetNeeded);
	ASSERT(!pFile->IsShellLinked() || ShouldBeShared(pFile->GetSharedDirectory(), _T(""), false));
	CCKey key(pFile->GetFileHash());
	CKnownFile *pFileInMap;
	if (m_Files_map.Lookup(key, pFileInMap)) {
		TRACE(_T("%hs: File already in shared file list: %s \"%s\" \"%s\"\n"), __FUNCTION__, (LPCTSTR)md4str(pFileInMap->GetFileHash()), (LPCTSTR)pFileInMap->GetFileName(), (LPCTSTR)pFileInMap->GetFilePath());
		TRACE(_T("%hs: File to add:                      %s \"%s\" \"%s\"\n"), __FUNCTION__, (LPCTSTR)md4str(pFile->GetFileHash()), (LPCTSTR)pFile->GetFileName(), (LPCTSTR)pFile->GetFilePath());
		if (!pFileInMap->IsKindOf(RUNTIME_CLASS(CPartFile)) || theApp.downloadqueue->IsPartFile(pFileInMap))
			LogWarning(GetResString(IDS_ERR_DUPL_FILES), (LPCTSTR)pFileInMap->GetFilePath(), (LPCTSTR)pFile->GetFilePath());
		return false;
	}
	m_UnsharedFiles_map.RemoveKey(CSKey(pFile->GetFileHash()));

	CSingleLock listlock(&m_mutWriteList, TRUE);
	m_Files_map.SetAt(key, pFile);
	listlock.Unlock();

	bool bKeywordsNeedUpdated = true;

	if (!pFile->IsPartFile() && !pFile->m_pCollection && CCollection::HasCollectionExtention(pFile->GetFileName())) {
		pFile->m_pCollection = new CCollection();
		if (!pFile->m_pCollection->InitCollectionFromFile(pFile->GetFilePath(), pFile->GetFileName())) {
			delete pFile->m_pCollection;
			pFile->m_pCollection = NULL;
		} else if (!pFile->m_pCollection->GetCollectionAuthorKeyString().IsEmpty()) {
			//If the collection has a key, resetting the file name will
			//cause the key to be added into the wordlist to be stored
			//into Kad.
			pFile->SetFileName(pFile->GetFileName());
			//During the initial startup, shared files are not accessible
			//to SetFileName which will then not call AddKeywords.
			//But when it is accessible, we don't allow it to re-add them.
			if (theApp.sharedfiles)
				bKeywordsNeedUpdated = false;
		}
	}

	if (bKeywordsNeedUpdated)
		m_keywords->AddKeywords(pFile);

	pFile->SetLastSeen();

	theApp.knownfiles->m_nRequestedTotal += pFile->statistic.GetAllTimeRequests();
	theApp.knownfiles->m_nAcceptedTotal += pFile->statistic.GetAllTimeAccepts();
	theApp.knownfiles->m_nTransferredTotal += pFile->statistic.GetAllTimeTransferred();

	return true;
}

void CSharedFileList::FileHashingFinished(CKnownFile *file)
{
	// File hashing finished for a shared file (none partfile)
	//	- reading shared directories at startup and hashing files which were not found in known.met
	//	- reading shared directories during runtime (user hit Reload button, added a shared directory, ...)

	ASSERT(!IsFilePtrInList(file));
	ASSERT(!theApp.knownfiles->IsFilePtrInList(file));

	CKnownFile *found_file = GetFileByID(file->GetFileHash());
	if (found_file == NULL) {
		// check if we still want to actually share this file, the user might have unshared it while hashing
		if (!ShouldBeShared(file->GetSharedDirectory(), file->GetFilePath(), false)) {
			RemoveFromHashing(file);
			if (!IsFilePtrInList(file) && !theApp.knownfiles->IsFilePtrInList(file))
				delete file;
			else
				ASSERT(0);
		} else {
			SafeAddKFile(file);
			theApp.knownfiles->SafeAddKFile(file);
		}
	} else {
		TRACE(_T("%hs: File already in shared file list: %s \"%s\"\n"), __FUNCTION__, (LPCTSTR)md4str(found_file->GetFileHash()), (LPCTSTR)found_file->GetFilePath());
		TRACE(_T("%hs: File to add:                      %s \"%s\"\n"), __FUNCTION__, (LPCTSTR)md4str(file->GetFileHash()), (LPCTSTR)file->GetFilePath());
		LogWarning(GetResString(IDS_ERR_DUPL_FILES), (LPCTSTR)found_file->GetFilePath(), (LPCTSTR)file->GetFilePath());

		RemoveFromHashing(file);
		if (!IsFilePtrInList(file) && !theApp.knownfiles->IsFilePtrInList(file))
			delete file;
		else
			ASSERT(0);
	}
}

bool CSharedFileList::RemoveFile(CKnownFile *pFile, bool bDeleted)
{
	CSingleLock listlock(&m_mutWriteList, TRUE);
	bool bResult = (m_Files_map.RemoveKey(CCKey(pFile->GetFileHash())) != FALSE);
	listlock.Unlock();

	output->RemoveFile(pFile, bDeleted);
	m_keywords->RemoveKeywords(pFile);
	if (bResult) {
		m_UnsharedFiles_map.SetAt(CSKey(pFile->GetFileHash()), true);
		theApp.knownfiles->m_nRequestedTotal -= pFile->statistic.GetAllTimeRequests();
		theApp.knownfiles->m_nAcceptedTotal -= pFile->statistic.GetAllTimeAccepts();
		theApp.knownfiles->m_nTransferredTotal -= pFile->statistic.GetAllTimeTransferred();
	}
	return bResult;
}

void CSharedFileList::Reload()
{
	ClearVolumeInfoCache();
	m_mapPseudoDirNames.RemoveAll();
	m_keywords->RemoveAllKeywordReferences();
	while (!waitingforhash_list.IsEmpty()) // delete all files which are waiting to get hashed, will be re-added if still shared below
		delete waitingforhash_list.RemoveHead();
	bHaveSingleSharedFiles = false;
	FindSharedFiles();
	m_keywords->PurgeUnreferencedKeywords();
	if (output)
		output->ReloadFileList();
}

void CSharedFileList::SetOutputCtrl(CSharedFilesCtrl *in_ctrl)
{
	output = in_ctrl;
	output->ReloadFileList();
	HashNextFile();		// SLUGFILLER: SafeHash - if hashing not yet started, start it now
}

void CSharedFileList::SendListToServer()
{
	if (m_Files_map.IsEmpty() || !server->IsConnected())
		return;

	CServer *pCurServer = server->GetCurrentServer();
	CSafeMemFile files(1024);
	CTypedPtrList<CPtrList, CKnownFile*> sortedList;

	CCKey bufKey;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != 0;) {
		CKnownFile *cur_file;
		m_Files_map.GetNextAssoc(pos, bufKey, cur_file);
		//insert sorted into sortedList
		if (!cur_file->GetPublishedED2K() && (!cur_file->IsLargeFile() || (pCurServer != NULL && pCurServer->SupportsLargeFilesTCP()))) {
			bool added = false;
			for (POSITION pos1 = sortedList.GetHeadPosition(); pos1 != 0 && !added;) {
				POSITION pos2 = pos1;
				if (GetRealPrio(sortedList.GetNext(pos1)->GetUpPriority()) <= GetRealPrio(cur_file->GetUpPriority())) {
					sortedList.InsertBefore(pos2, cur_file);
					added = true;
				}
			}
			if (!added)
				sortedList.AddTail(cur_file);
		}
	}


	// add to packet
	uint32 limit = pCurServer ? pCurServer->GetSoftFiles() : 0;
	if (limit == 0 || limit > 200)
		limit = 200;

	if ((uint32)sortedList.GetCount() < limit) {
		limit = (uint32)sortedList.GetCount();
		if (limit == 0) {
			m_lastPublishED2KFlag = false;
			return;
		}
	}
	files.WriteUInt32(limit);
	uint32 count = limit;
	for (POSITION pos = sortedList.GetHeadPosition(); pos != 0 && count-- > 0;) {
		CKnownFile *file = sortedList.GetNext(pos);
		CreateOfferedFilePacket(file, &files, pCurServer);
		file->SetPublishedED2K(true);
	}
	sortedList.RemoveAll();
	Packet *packet = new Packet(&files);
	packet->opcode = OP_OFFERFILES;
	// compress packet
	//   - this kind of data is highly compressible (N * (1 MD4 and at least 3 string meta data tags and 1 integer meta data tag))
	//   - the min. amount of data needed for one published file is ~100 bytes
	//   - this function is called once when connecting to a server and when a file becomes shareable - so, it's called rarely.
	//   - if the compressed size is still >= the original size, we send the uncompressed packet
	// therefore we always try to compress the packet
	if (pCurServer && pCurServer->GetTCPFlags() & SRV_TCPFLG_COMPRESSION) {
		UINT uUncomprSize = packet->size;
		packet->PackPacket();
		if (thePrefs.GetDebugServerTCPLevel() > 0)
			Debug(_T(">>> Sending OP_OfferFiles(compressed); uncompr size=%u  compr size=%u  files=%u\n"), uUncomprSize, packet->size, limit);
	} else if (thePrefs.GetDebugServerTCPLevel() > 0)
		Debug(_T(">>> Sending OP_OfferFiles; size=%u  files=%u\n"), packet->size, limit);

	theStats.AddUpDataOverheadServer(packet->size);
	if (thePrefs.GetVerbose())
		AddDebugLogLine(false, _T("Server, Sendlist: Packet size:%u"), packet->size);
	server->SendPacket(packet);
}

void CSharedFileList::ClearED2KPublishInfo()
{
	m_lastPublishED2KFlag = true;
	CCKey bufKey;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
		CKnownFile *cur_file;
		m_Files_map.GetNextAssoc(pos, bufKey, cur_file);
		cur_file->SetPublishedED2K(false);
	}
}

void CSharedFileList::ClearKadSourcePublishInfo()
{
	CCKey bufKey;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
		CKnownFile *cur_file;
		m_Files_map.GetNextAssoc(pos, bufKey, cur_file);
		cur_file->SetLastPublishTimeKadSrc(0, 0);
	}
}

void CSharedFileList::CreateOfferedFilePacket(CKnownFile *cur_file, CSafeMemFile *files
	, CServer *pServer, CUpDownClient *pClient)
{
	UINT uEmuleVer = (pClient && pClient->IsEmuleClient()) ? pClient->GetVersion() : 0;

	// NOTE: This function is used for creating the offered file packet for Servers _and_ for Clients.
	files->WriteHash16(cur_file->GetFileHash());

	// *) This function is used for offering files to the local server and for sending
	//    shared files to some other client. In each case we send our IP+Port only, if
	//    we have a HighID.
	// *) Newer eservers also support 2 special IP+port values which are used to hold basic file status info.
	uint32 nClientID = 0;
	uint16 nClientPort = 0;
	if (pServer) {
		// we use the 'TCP-compression' server feature flag as indicator for a 'newer' server.
		if (pServer->GetTCPFlags() & SRV_TCPFLG_COMPRESSION) {
			if (cur_file->IsPartFile()) {
				// publishing an incomplete file
				nClientID = 0xFCFCFCFC;
				nClientPort = 0xFCFC;
			} else {
				// publishing a complete file
				nClientID = 0xFBFBFBFB;
				nClientPort = 0xFBFB;
			}
		} else {
			// check eD2K ID state
			if (theApp.serverconnect->IsConnected() && !theApp.serverconnect->IsLowID()) {
				nClientID = theApp.GetID();
				nClientPort = thePrefs.GetPort();
			}
		}
	} else {
		if (theApp.IsConnected() && !theApp.IsFirewalled()) {
			nClientID = theApp.GetID();
			nClientPort = thePrefs.GetPort();
		}
	}
	files->WriteUInt32(nClientID);
	files->WriteUInt16(nClientPort);
	//TRACE(_T("Publishing file: Hash=%s  ClientIP=%s  ClientPort=%u\n"), (LPCTSTR)md4str(cur_file->GetFileHash()), (LPCTSTR)ipstr(nClientID), nClientPort);

	CSimpleArray<CTag*> tags;

	tags.Add(new CTag(FT_FILENAME, cur_file->GetFileName()));

	if (!cur_file->IsLargeFile())
		tags.Add(new CTag(FT_FILESIZE, (uint32)(uint64)cur_file->GetFileSize()));
	else {
		// we send 2*32 bit tags to servers, but a real 64 bit tag to other clients.
		if (pServer != NULL) {
			if (!pServer->SupportsLargeFilesTCP()) {
				ASSERT(0);
				tags.Add(new CTag(FT_FILESIZE, 0, false));
			} else {
				tags.Add(new CTag(FT_FILESIZE, (uint32)(uint64)cur_file->GetFileSize()));
				tags.Add(new CTag(FT_FILESIZE_HI, (uint32)((uint64)cur_file->GetFileSize() >> 32)));
			}
		} else {
			if (pClient)
				if (!pClient->SupportsLargeFiles()) {
					ASSERT(0);
					tags.Add(new CTag(FT_FILESIZE, 0, false));
				} else
					tags.Add(new CTag(FT_FILESIZE, cur_file->GetFileSize(), true));
		}
	}

	// eserver 17.6+ supports eMule file rating tag. There is no TCP-capabilities bit available
	// to determine whether the server is really supporting it -- this is by intention (lug).
	// That's why we always send it.
	if (cur_file->GetFileRating()) {
		uint32 uRatingVal = cur_file->GetFileRating();
		if (pClient) {
			// eserver is sending the rating which it received in a different format (see
			// 'CSearchFile::CSearchFile'). If we are creating the packet for other client
			// we must use eserver's format.
			uRatingVal *= (255 / 5/*RatingExcellent*/);
		}
		tags.Add(new CTag(FT_FILERATING, uRatingVal));
	}

	// NOTE: Archives and CD-Images are published+searched with file type "Pro"
	bool bAddedFileType = false;
	if (pServer && (pServer->GetTCPFlags() & SRV_TCPFLG_TYPETAGINTEGER)) {
		// Send integer file type tags to newer servers
		EED2KFileType eFileType = GetED2KFileTypeSearchID(GetED2KFileTypeID(cur_file->GetFileName()));
		if (eFileType >= ED2KFT_AUDIO && eFileType <= ED2KFT_CDIMAGE) {
			tags.Add(new CTag(FT_FILETYPE, (UINT)eFileType));
			bAddedFileType = true;
		}
	}
	if (!bAddedFileType) {
		// Send string file type tags to:
		//	- newer servers, in case there is no integer type available for the file type (e.g. emulecollection)
		//	- older servers
		//	- all clients
		const CString &strED2KFileType(GetED2KFileTypeSearchTerm(GetED2KFileTypeID(cur_file->GetFileName())));
		if (!strED2KFileType.IsEmpty())
			tags.Add(new CTag(FT_FILETYPE, strED2KFileType));
	}

	// eserver 16.4+ does not need the FT_FILEFORMAT tag at all nor does any eMule client. This tag
	// was used for older (very old) eDonkey servers only. -> We send it only to non-eMule clients.
	if (pServer == NULL && uEmuleVer == 0) {
		int iExt = cur_file->GetFileName().ReverseFind(_T('.'));
		if (iExt >= 0) {
			CString strExt = cur_file->GetFileName().Mid(iExt);
			if (!strExt.IsEmpty()) {
				strExt.Delete(0, 1);
				if (!strExt.IsEmpty())
					tags.Add(new CTag(FT_FILEFORMAT, strExt.MakeLower())); // file extension without a "."
			}
		}
	}

	// only send verified meta data to servers/clients
	if (cur_file->GetMetaDataVer() > 0) {
		static const struct
		{
			bool	bSendToServer;
			uint8	nName;
			uint8	nED2KType;
			LPCSTR	pszED2KName;
		} _aMetaTags[] =
		{
			// Artist, Album and Title are disabled because they should be already part of the filename
			// and would therefore be redundant information sent to the servers. and the servers count the
			// amount of sent data!
			{ false, FT_MEDIA_ARTIST,	TAGTYPE_STRING, FT_ED2K_MEDIA_ARTIST },
			{ false, FT_MEDIA_ALBUM,	TAGTYPE_STRING, FT_ED2K_MEDIA_ALBUM },
			{ false, FT_MEDIA_TITLE,	TAGTYPE_STRING, FT_ED2K_MEDIA_TITLE },
			{ true,  FT_MEDIA_LENGTH,	TAGTYPE_STRING, FT_ED2K_MEDIA_LENGTH },
			{ true,  FT_MEDIA_BITRATE,	TAGTYPE_UINT32, FT_ED2K_MEDIA_BITRATE },
			{ true,  FT_MEDIA_CODEC,	TAGTYPE_STRING, FT_ED2K_MEDIA_CODEC }
		};
		for (unsigned i = 0; i < _countof(_aMetaTags); ++i) {
			if (pServer != NULL && !_aMetaTags[i].bSendToServer)
				continue;
			CTag *pTag = cur_file->GetTag(_aMetaTags[i].nName);
			if (pTag != NULL) {
				// skip string tags with empty string values
				if (pTag->IsStr() && pTag->GetStr().IsEmpty())
					continue;

				// skip integer tags with '0' values
				if (pTag->IsInt() && pTag->GetInt() == 0)
					continue;

				if (_aMetaTags[i].nED2KType == TAGTYPE_STRING && pTag->IsStr()) {
					if (pServer && (pServer->GetTCPFlags() & SRV_TCPFLG_NEWTAGS))
						tags.Add(new CTag(_aMetaTags[i].nName, pTag->GetStr()));
					else
						tags.Add(new CTag(_aMetaTags[i].pszED2KName, pTag->GetStr()));
				} else if (_aMetaTags[i].nED2KType == TAGTYPE_UINT32 && pTag->IsInt()) {
					if (pServer && (pServer->GetTCPFlags() & SRV_TCPFLG_NEWTAGS))
						tags.Add(new CTag(_aMetaTags[i].nName, pTag->GetInt()));
					else
						tags.Add(new CTag(_aMetaTags[i].pszED2KName, pTag->GetInt()));
				} else if (_aMetaTags[i].nName == FT_MEDIA_LENGTH && pTag->IsInt()) {
					ASSERT(_aMetaTags[i].nED2KType == TAGTYPE_STRING);
					// All 'eserver' versions and eMule versions >= 0.42.4 support the media length tag with type 'integer'
					if ((pServer != NULL && (pServer->GetTCPFlags() & SRV_TCPFLG_COMPRESSION))
						|| uEmuleVer >= MAKE_CLIENT_VERSION(0, 42, 4))
					{
						if (pServer && (pServer->GetTCPFlags() & SRV_TCPFLG_NEWTAGS))
							tags.Add(new CTag(_aMetaTags[i].nName, pTag->GetInt()));
						else
							tags.Add(new CTag(_aMetaTags[i].pszED2KName, pTag->GetInt()));
					} else {
						CString strValue;
						SecToTimeLength(pTag->GetInt(), strValue);
						tags.Add(new CTag(_aMetaTags[i].pszED2KName, strValue));
					}
				} else
					ASSERT(0);
			}
		}
	}

	EUTF8str eStrEncode;
	if ((pServer && (pServer->GetTCPFlags() & SRV_TCPFLG_UNICODE)) || !pClient || pClient->GetUnicodeSupport())
		eStrEncode = UTF8strRaw;
	else
		eStrEncode = UTF8strNone;

	files->WriteUInt32(tags.GetSize());
	for (int i = 0; i < tags.GetSize(); ++i) {
		const CTag *pTag = tags[i];
		//TRACE(_T("  %s\n"), pTag->GetFullInfo(DbgGetFileMetaTagName));
		if (pServer && (pServer->GetTCPFlags() & SRV_TCPFLG_NEWTAGS) || (uEmuleVer >= MAKE_CLIENT_VERSION(0, 42, 7)))
			pTag->WriteNewEd2kTag(files, eStrEncode);
		else
			pTag->WriteTagToFile(files, eStrEncode);
		delete pTag;
	}
}

// -khaos--+++> New param:  pbytesLargest, pointer to uint64.
//				Various other changes to accommodate our new statistic...
//				Point of this is to find the largest file currently shared.
uint64 CSharedFileList::GetDatasize(uint64 &pbytesLargest) const
{
	pbytesLargest = 0;
	// <-----khaos-
	uint64 fsize;
	fsize = 0;

	CCKey bufKey;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
		CKnownFile *cur_file;
		m_Files_map.GetNextAssoc(pos, bufKey, cur_file);
		fsize += (uint64)cur_file->GetFileSize();
		// -khaos--+++> If this file is bigger than all the others...well duh.
		if (cur_file->GetFileSize() > pbytesLargest)
			pbytesLargest = cur_file->GetFileSize();
		// <-----khaos-
	}
	return fsize;
}

CKnownFile *CSharedFileList::GetFileByID(const uchar *hash) const
{
	if (hash) {
		CCKey key(hash);
		CKnownFile *found_file;
		if (m_Files_map.Lookup(key, found_file))
			return found_file;
	}
	return NULL;
}

CKnownFile *CSharedFileList::GetFileByIdentifier(const CFileIdentifierBase &rFileIdent, bool bStrict) const
{
	CKnownFile *pResult;
	if (m_Files_map.Lookup(CCKey(rFileIdent.GetMD4Hash()), pResult))
		if (bStrict)
			return pResult->GetFileIdentifier().CompareStrict(rFileIdent) ? pResult : NULL;
		else
			return pResult->GetFileIdentifier().CompareRelaxed(rFileIdent) ? pResult : NULL;

	return NULL;
}

CKnownFile *CSharedFileList::GetFileByIndex(INT_PTR index) const // slow
{
	INT_PTR count = 0;
	CCKey bufKey;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
		CKnownFile *cur_file;
		m_Files_map.GetNextAssoc(pos, bufKey, cur_file);
		if (index == count)
			return cur_file;
		++count;
	}
	return NULL;
}

CKnownFile* CSharedFileList::GetFileNext(POSITION &pos) const
{
	CKnownFile *cur_file = NULL;
	if (m_Files_map.IsEmpty()) //XP was crashing without this check
		pos = NULL;
	if (pos != NULL) {
		CCKey bufKey;
		m_Files_map.GetNextAssoc(pos, bufKey, cur_file);
	}
	return cur_file;
}

CKnownFile *CSharedFileList::GetFileByAICH(const CAICHHash &rHash) const // slow
{
	CCKey bufKey;
	CKnownFile *cur_file;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
		m_Files_map.GetNextAssoc(pos, bufKey, cur_file);
		if (cur_file->GetFileIdentifierC().HasAICHHash() && cur_file->GetFileIdentifierC().GetAICHHash() == rHash)
			return cur_file;
	}
	return 0;
}

bool CSharedFileList::IsFilePtrInList(const CKnownFile *file) const
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

void CSharedFileList::HashNextFile()
{
	// SLUGFILLER: SafeHash
	if (!::IsWindow(theApp.emuledlg->m_hWnd))	// wait for the dialog to open
		return;
	if (!theApp.IsClosing())
		theApp.emuledlg->sharedfileswnd->sharedfilesctrl.ShowFilesCount();
	if (!currentlyhashing_list.IsEmpty())	// one hash at a time
		return;
	// SLUGFILLER: SafeHash
	if (waitingforhash_list.IsEmpty())
		return;
	UnknownFile_Struct *nextfile = waitingforhash_list.RemoveHead();
	currentlyhashing_list.AddTail(nextfile);	// SLUGFILLER: SafeHash - keep track
	CAddFileThread *addfilethread = static_cast<CAddFileThread*>(AfxBeginThread(RUNTIME_CLASS(CAddFileThread), THREAD_PRIORITY_BELOW_NORMAL, 0, CREATE_SUSPENDED));
	addfilethread->SetValues(this, nextfile->strDirectory, nextfile->strName, nextfile->strSharedDirectory);
	addfilethread->ResumeThread();
	// SLUGFILLER: SafeHash - nextfile deletion is handled elsewhere
	//delete nextfile;
}

// SLUGFILLER: SafeHash
bool CSharedFileList::IsHashing(const CString &rstrDirectory, const CString &rstrName)
{
	for (POSITION pos = waitingforhash_list.GetHeadPosition(); pos != NULL;) {
		const UnknownFile_Struct *pFile = waitingforhash_list.GetNext(pos);
		if (!pFile->strName.CompareNoCase(rstrName) && !CompareDirectory(pFile->strDirectory, rstrDirectory))
			return true;
	}
	for (POSITION pos = currentlyhashing_list.GetHeadPosition(); pos != NULL;) {
		const UnknownFile_Struct *pFile = currentlyhashing_list.GetNext(pos);
		if (!pFile->strName.CompareNoCase(rstrName) && !CompareDirectory(pFile->strDirectory, rstrDirectory))
			return true;
	}
	return false;
}

void CSharedFileList::RemoveFromHashing(CKnownFile *hashed)
{
	for (POSITION pos = currentlyhashing_list.GetHeadPosition(); pos != NULL;) {
		POSITION posLast = pos;
		const UnknownFile_Struct *pFile = currentlyhashing_list.GetNext(pos);
		if (!pFile->strName.CompareNoCase(hashed->GetFileName()) && !CompareDirectory(pFile->strDirectory, hashed->GetPath())) {
			currentlyhashing_list.RemoveAt(posLast);
			delete pFile;
			HashNextFile();			// start next hash if possible, but only if a previous hash finished
			return;
		}
	}
}

void CSharedFileList::HashFailed(UnknownFile_Struct *hashed)
{
	for (POSITION pos = currentlyhashing_list.GetHeadPosition(); pos != NULL;) {
		POSITION posLast = pos;
		const UnknownFile_Struct *pFile = currentlyhashing_list.GetNext(pos);
		if (!pFile->strName.CompareNoCase(hashed->strName) && !CompareDirectory(pFile->strDirectory, hashed->strDirectory)) {
			currentlyhashing_list.RemoveAt(posLast);
			delete pFile;
			HashNextFile();			// start next hash if possible, but only if a previous hash finished
			break;
		}
	}
	delete hashed;
}

void CSharedFileList::UpdateFile(CKnownFile *toupdate)
{
	output->UpdateFile(toupdate);
}

void CSharedFileList::Process()
{
	Publish();
	if (m_lastPublishED2KFlag && ::GetTickCount() >= m_lastPublishED2K + ED2KREPUBLISHTIME) {
		SendListToServer();
		m_lastPublishED2K = ::GetTickCount();
	}
}

void CSharedFileList::Publish()
{
	if (!Kademlia::CKademlia::IsConnected()
		|| (theApp.IsFirewalled()
			&& theApp.clientlist->GetBuddyStatus() != Connected
			//direct callback
			&& (Kademlia::CUDPFirewallTester::IsFirewalledUDP(true) || !Kademlia::CUDPFirewallTester::IsVerified())
		   )
		|| !GetCount()
		|| !Kademlia::CKademlia::GetPublish())
	{
		return;
	}

	//We are connected to Kad. We are either open or have a buddy. And Kad is ready to start publishing.
	time_t tNow = time(NULL);
	if (Kademlia::CKademlia::GetTotalStoreKey() < KADEMLIATOTALSTOREKEY) {
		//We are not at the max simultaneous keyword publishes
		if (tNow >= m_keywords->GetNextPublishTime()) {
			//Enough time has passed since last keyword publish

			//Get the next keyword which has to be (re-)published
			CPublishKeyword *pPubKw = m_keywords->GetNextKeyword();
			if (pPubKw) {
				//We have the next keyword to check if it can be published

				//Debug check to make sure things are going well.
				ASSERT(pPubKw->GetRefCount() > 0);

				if (tNow >= pPubKw->GetNextPublishTime()) {
					//This keyword can be published.
					Kademlia::CSearch *pSearch = Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::STOREKEYWORD, false, pPubKw->GetKadID());
					if (pSearch) {
						//pSearch was created. Which means no search was already being done with this HashID.
						//This also means that it was checked to see if network load wasn't a factor.

						//This sets the filename into the search object so we can show it in the gui.
						pSearch->SetGUIName(pPubKw->GetKeyword());

						//Add all file IDs which relate to the current keyword to be published
						const CSimpleKnownFileArray &aFiles = pPubKw->GetReferences();
						uint32 count = 0;
						for (int f = 0; f < aFiles.GetSize(); ++f) {
							//Debug check to make sure things are working well.
							ASSERT_VALID(aFiles[f]);
							// JOHNTODO - Why is this happening. I think it may have to do with downloading a file that is already
							// in the known file list.
//							ASSERT( IsFilePtrInList(aFiles[f]) );

							//Only publish complete files as someone else should have the full file to publish these keywords.
							//As a side effect, this may help reduce people finding incomplete files in the network.
							if (!aFiles[f]->IsPartFile() && IsFilePtrInList(aFiles[f])) {
								pSearch->AddFileID(Kademlia::CUInt128(aFiles[f]->GetFileHash()));
								if (++count > 150) {
									//We only publish up to 150 files per keyword publish then rotate the list.
									pPubKw->RotateReferences(f);
									break;
								}
							}
						}

						if (count) {
							//Start our keyword publish
							pPubKw->SetNextPublishTime(tNow + KADEMLIAREPUBLISHTIMEK);
							pPubKw->IncPublishedCount();
							Kademlia::CSearchManager::StartSearch(pSearch);
						} else
							//There were no valid files to publish with this keyword.
							delete pSearch;
					}
				}
			}
			m_keywords->SetNextPublishTime(tNow + KADEMLIAPUBLISHTIME);
		}
	}

	if (Kademlia::CKademlia::GetTotalStoreSrc() < KADEMLIATOTALSTORESRC) {
		if (tNow >= m_lastPublishKadSrc) {
			if (m_currFileSrc > GetCount())
				m_currFileSrc = 0;
			CKnownFile *pCurKnownFile = GetFileByIndex(m_currFileSrc);
			if (pCurKnownFile && pCurKnownFile->PublishSrc())
				if (Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::STOREFILE, true, Kademlia::CUInt128(pCurKnownFile->GetFileHash())) == NULL)
					pCurKnownFile->SetLastPublishTimeKadSrc(0, 0);

			++m_currFileSrc;

			// even if we did not publish a source, reset the timer so that this list is processed
			// only every KADEMLIAPUBLISHTIME seconds.
			m_lastPublishKadSrc = tNow + KADEMLIAPUBLISHTIME;
		}
	}

	if (Kademlia::CKademlia::GetTotalStoreNotes() < KADEMLIATOTALSTORENOTES) {
		if (tNow >= m_lastPublishKadNotes) {
			if (m_currFileNotes > GetCount())
				m_currFileNotes = 0;
			CKnownFile *pCurKnownFile = GetFileByIndex(m_currFileNotes);
			if (pCurKnownFile && pCurKnownFile->PublishNotes())
				if (Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::STORENOTES, true, Kademlia::CUInt128(pCurKnownFile->GetFileHash())) == NULL)
					pCurKnownFile->SetLastPublishTimeKadNotes(0);

			++m_currFileNotes;

			// even if we did not publish a source, reset the timer so that this list is processed
			// only every KADEMLIAPUBLISHTIME seconds.
			m_lastPublishKadNotes = tNow + KADEMLIAPUBLISHTIME;
		}
	}
}

void CSharedFileList::AddKeywords(CKnownFile *pFile)
{
	m_keywords->AddKeywords(pFile);
}

void CSharedFileList::RemoveKeywords(CKnownFile *pFile)
{
	m_keywords->RemoveKeywords(pFile);
}

void CSharedFileList::DeletePartFileInstances() const
{
	// this is only allowed during shut down
	ASSERT(theApp.IsClosing());
	ASSERT(theApp.knownfiles);

	CCKey key;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
		CKnownFile *cur_file;
		m_Files_map.GetNextAssoc(pos, key, cur_file);
		if (cur_file->IsKindOf(RUNTIME_CLASS(CPartFile)))
			if (!theApp.downloadqueue->IsPartFile(cur_file) && !theApp.knownfiles->IsFilePtrInList(cur_file))
				delete cur_file; // this is only allowed during shut down
	}
}

bool CSharedFileList::IsUnsharedFile(const uchar *auFileHash) const
{
	if (auFileHash) {
		bool bFound;
		if (m_UnsharedFiles_map.Lookup(CSKey(auFileHash), bFound))
			return true;
	}
	return false;
}

void CSharedFileList::RebuildMetaData()
{
	CCKey key;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
		CKnownFile *file;
		m_Files_map.GetNextAssoc(pos, key, file);
		if (!file->IsKindOf(RUNTIME_CLASS(CPartFile)))
			file->UpdateMetaDataTags();
	}
}

bool CSharedFileList::ShouldBeShared(const CString &strPath, const CString &strFilePath, bool bMustBeShared) const
{
	// determines if a file should be a shared file based on out shared directories/files preferences

	if (CompareDirectory(strPath, thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR)) == 0)
		return true;

	for (int i = 1; i < thePrefs.GetCatCount(); ++i)
		if (CompareDirectory(strPath, thePrefs.GetCatPath(i)) == 0)
			return true;

	if (bMustBeShared)
		return false;

	// check if this file is explicitly unshared
	if (!strFilePath.IsEmpty()) {
		for (POSITION pos = m_liSingleExcludedFiles.GetHeadPosition(); pos != NULL;)
			if (strFilePath.CompareNoCase(m_liSingleExcludedFiles.GetNext(pos)) == 0)
				return false;

		// check if this file is explicitly shared
		for (POSITION pos = m_liSingleSharedFiles.GetHeadPosition(); pos != NULL;)
			if (strFilePath.CompareNoCase(m_liSingleSharedFiles.GetNext(pos)) == 0)
				return true;
	}

	for (POSITION pos = thePrefs.shareddir_list.GetHeadPosition(); pos != NULL;)
		if (CompareDirectory(strPath, thePrefs.shareddir_list.GetNext(pos)) == 0)
			return true;

	return false;
}

bool CSharedFileList::ContainsSingleSharedFiles(const CString &strDirectory) const
{
	ASSERT(strDirectory.Right(1) == _T("\\"));
	for (POSITION pos = m_liSingleSharedFiles.GetHeadPosition(); pos != NULL;)
		if (strDirectory.CompareNoCase(m_liSingleSharedFiles.GetNext(pos).Left(strDirectory.GetLength())) == 0)
			return true;

	return false;
}

bool CSharedFileList::ExcludeFile(const CString &strFilePath)
{
	bool bShared = false;
	// first check if we are explicitly sharing this file
	for (POSITION pos = m_liSingleSharedFiles.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		if (strFilePath.CompareNoCase(m_liSingleSharedFiles.GetNext(pos)) == 0) {
			bShared = true;
			m_liSingleSharedFiles.RemoveAt(pos2);
			break;
		}
	}

	// check if we implicity share this file
	bShared |= ShouldBeShared(strFilePath.Left(strFilePath.ReverseFind(_T('\\')) + 1), strFilePath, false);

	if (!bShared) {
		// we don't actually share this file, can't be excluded
		return false;
	}
	if (ShouldBeShared(strFilePath.Left(strFilePath.ReverseFind(_T('\\')) + 1), strFilePath, true)) {
		// we cannot unshare this file (incoming directories)
		ASSERT(0); // checks should be done earlier already
		return false;
	}

	// add to exclude list
	m_liSingleExcludedFiles.AddTail(strFilePath);

	// check if the file is in the shared list (doesn't has to for example if it is hashing or not loaded yet) and remove
	CCKey bufKey;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
		CKnownFile *cur_file;
		m_Files_map.GetNextAssoc(pos, bufKey, cur_file);
		if (strFilePath.CompareNoCase(cur_file->GetFilePath()) == 0) {
			RemoveFile(cur_file);
			break;
		}
	}
	// updating the GUI needs to be done by the caller
	return true;
}

void CSharedFileList::CheckAndAddSingleFile(const CFileFind &ff)
{
	if (ff.IsDirectory() || ff.IsDots() || ff.IsSystem() || ff.IsTemporary() || ff.GetLength() == 0 || ff.GetLength() > MAX_EMULE_FILE_SIZE)
		return;

	CString strFoundFileName(ff.GetFileName());
	CString strFoundFilePath(ff.GetFilePath());
	CString strFoundDirectory(strFoundFilePath.Left(ff.GetFilePath().ReverseFind(_T('\\')) + 1));
	CString strShellLinkDir;
	ULONGLONG ullFoundFileSize = ff.GetLength();

	// check if this file is explicit unshared
	for (POSITION pos = m_liSingleExcludedFiles.GetHeadPosition(); pos != NULL;)
		if (strFoundFilePath.CompareNoCase(m_liSingleExcludedFiles.GetNext(pos)) == 0)
			return;

	FILETIME tFoundFileTime;
	ff.GetLastWriteTime(&tFoundFileTime);

	// ignore real(!) LNK files
	if (ExtensionIs(strFoundFileName, _T(".lnk"))) {
		SHFILEINFO info;
		if (SHGetFileInfo(strFoundFilePath, 0, &info, sizeof info, SHGFI_ATTRIBUTES) && (info.dwAttributes & SFGAO_LINK)) {
			if (!thePrefs.GetResolveSharedShellLinks()) {
				TRACE(_T("%hs: Did not share file \"%s\" - not supported file type\n"), __FUNCTION__, (LPCTSTR)strFoundFilePath);
				return;
			}
			// Win98: Would need to implement a different code path which is using 'IShellLinkA' on Win9x.
			CComPtr<IShellLink> pShellLink;
			if (SUCCEEDED(pShellLink.CoCreateInstance(CLSID_ShellLink))) {
				CComQIPtr<IPersistFile> pPersistFile(pShellLink);
				if (pPersistFile) {
					if (SUCCEEDED(pPersistFile->Load(strFoundFilePath, STGM_READ))) {
						TCHAR szResolvedPath[MAX_PATH];
						if (pShellLink->GetPath(szResolvedPath, _countof(szResolvedPath), NULL/*DO NOT USE (read below)*/, 0) == NOERROR) {
							// WIN32_FIND_DATA provided by "IShellLink::GetPath" contains the file stats which where
							// taken when the shortcut was created! Thus the file stats which are returned do *not*
							// reflect the current real file stats. So, do *not* use that data!
							//
							// Need to do an explicit 'FindFile' to get the current WIN32_FIND_DATA file stats.
							//
							CFileFind ffResolved;
							if (!ffResolved.FindFile(szResolvedPath))
								return;
							VERIFY(!ffResolved.FindNextFile());
							if (ffResolved.IsDirectory() || ffResolved.IsDots() || ffResolved.IsSystem() || ffResolved.IsTemporary() || ffResolved.GetLength() == 0 || ffResolved.GetLength() > MAX_EMULE_FILE_SIZE)
								return;
							strShellLinkDir = strFoundDirectory;
							strFoundDirectory = ffResolved.GetRoot();
							strFoundFileName = ffResolved.GetFileName();
							strFoundFilePath = ffResolved.GetFilePath();
							ullFoundFileSize = ffResolved.GetLength();
							ffResolved.GetLastWriteTime(&tFoundFileTime);
							slosh(strFoundDirectory);
						}
					}
				}
			}
		}
	}

	// ignore real(!) thumbs.db files -- seems that lot of ppl have 'thumbs.db' files without the 'System' file attribute
	if (strFoundFileName.CompareNoCase(_T("thumbs.db")) == 0) {
		// if that's a valid 'Storage' file, we declare it as a "thumbs.db" file.
		CComPtr<IStorage> pStorage;
		if (StgOpenStorage(strFoundFilePath, NULL, STGM_READ | STGM_SHARE_DENY_WRITE, NULL, 0, &pStorage) == S_OK) {
			CComPtr<IEnumSTATSTG> pEnumSTATSTG;
			if (SUCCEEDED(pStorage->EnumElements(0, NULL, 0, &pEnumSTATSTG))) {
				STATSTG statstg = {};
				if (pEnumSTATSTG->Next(1, &statstg, 0) == S_OK) {
					CoTaskMemFree(statstg.pwcsName);
					statstg.pwcsName = NULL;
					TRACE(_T("%hs: Did not share file \"%s\" - not supported file type\n"), __FUNCTION__, (LPCTSTR)strFoundFilePath);
					return;
				}
			}
		}
	}

	time_t fdate = (time_t)FileTimeToUnixTime(tFoundFileTime);
	if (fdate == 0)
		fdate = (time_t)-1;
	if (fdate == (time_t)-1) {
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("Failed to get file date of \"%s\""), (LPCTSTR)strFoundFilePath);
	} else
		AdjustNTFSDaylightFileTime(fdate, strFoundFilePath);

	CKnownFile *toadd = theApp.knownfiles->FindKnownFile(strFoundFileName, fdate, ullFoundFileSize);
	if (toadd) {
		CCKey key(toadd->GetFileHash());
		CKnownFile *pFileInMap;
		if (m_Files_map.Lookup(key, pFileInMap)) {
			TRACE(_T("%hs: File already in shared file list: %s \"%s\"\n"), __FUNCTION__, (LPCTSTR)md4str(pFileInMap->GetFileHash()), (LPCTSTR)pFileInMap->GetFilePath());
			TRACE(_T("%hs: File to add:                      %s \"%s\"\n"), __FUNCTION__, (LPCTSTR)md4str(toadd->GetFileHash()), (LPCTSTR)strFoundFilePath);
			if (!pFileInMap->IsKindOf(RUNTIME_CLASS(CPartFile)) || theApp.downloadqueue->IsPartFile(pFileInMap)) {
				if (pFileInMap->GetFilePath().CompareNoCase(toadd->GetFilePath()) != 0) //is it actually really the same file in the same place we already share? if so don't bother too much
					LogWarning(GetResString(IDS_ERR_DUPL_FILES), (LPCTSTR)pFileInMap->GetFilePath(), (LPCTSTR)strFoundFilePath);
				else
					DebugLog(_T("File shared twice, might have been a single shared file before - %s"), (LPCTSTR)pFileInMap->GetFilePath());
			}
		} else {
			if (!strShellLinkDir.IsEmpty())
				DebugLog(_T("Shared link: %s from %s"), (LPCTSTR)strFoundFilePath, (LPCTSTR)strShellLinkDir);
			toadd->SetPath(strFoundDirectory);
			toadd->SetFilePath(strFoundFilePath);
			toadd->SetSharedDirectory(strShellLinkDir);
			AddFile(toadd);
		}
	} else {
		// not in knownfile list - start adding thread to hash file if the hashing of this file isn't already waiting
		// SLUGFILLER: SafeHash - don't double hash, MY way
		if (!IsHashing(strFoundDirectory, strFoundFileName) && !thePrefs.IsTempFile(strFoundDirectory, strFoundFileName)) {
			UnknownFile_Struct *tohash = new UnknownFile_Struct;
			tohash->strDirectory = strFoundDirectory;
			tohash->strName = strFoundFileName;
			tohash->strSharedDirectory = strShellLinkDir;
			waitingforhash_list.AddTail(tohash);
		} else
			TRACE(_T("%hs: Did not share file \"%s\" - already hashing or temp. file\n"), __FUNCTION__, (LPCTSTR)strFoundFilePath);
		// SLUGFILLER: SafeHash
	}
}

void CSharedFileList::Save() const
{
	const CString &strFullPath(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + SHAREDFILES_FILE);
	CStdioFile sdirfile;
	if (sdirfile.Open(strFullPath, CFile::modeCreate | CFile::modeWrite | CFile::shareDenyWrite | CFile::typeBinary)) {
		try {
			// write Unicode byte order mark 0xFEFF
			static const WORD wBOM = 0xFEFFui16;
			sdirfile.Write(&wBOM, sizeof(wBOM));

			for (POSITION pos = m_liSingleSharedFiles.GetHeadPosition(); pos != NULL;) {
				sdirfile.WriteString(m_liSingleSharedFiles.GetNext(pos));
				sdirfile.Write(_T("\r\n"), 2 * sizeof(TCHAR));
			}
			for (POSITION pos = m_liSingleExcludedFiles.GetHeadPosition(); pos != NULL;) {
				sdirfile.WriteString(_T("-") + m_liSingleExcludedFiles.GetNext(pos)); // a '-' prefix means excluded
				sdirfile.Write(_T("\r\n"), 2 * sizeof(TCHAR));
			}
			if ((theApp.IsClosing() && thePrefs.GetCommitFiles() >= 1) || thePrefs.GetCommitFiles() >= 2) {
				sdirfile.Flush(); // flush file stream buffers to disk buffers
				if (_commit(_fileno(sdirfile.m_pStream)) != 0) // commit disk buffers to disk
					AfxThrowFileException(CFileException::hardIO, ::GetLastError(), sdirfile.GetFileName());
			}
			sdirfile.Close();
		} catch (CFileException *error) {
			TCHAR buffer[MAX_CFEXP_ERRORMSG];
			GetExceptionMessage(*error, buffer, _countof(buffer));
			DebugLogError(_T("Failed to save %s - %s"), (LPCTSTR)strFullPath, buffer);
			error->Delete();
		}
	} else
		DebugLogError(_T("Failed to save %s"), (LPCTSTR)strFullPath);
}

void CSharedFileList::LoadSingleSharedFilesList()
{
	const CString &strFullPath(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + SHAREDFILES_FILE);
	CStdioFile *sdirfile = new CStdioFile();
	bool bIsUnicodeFile = IsUnicodeFile(strFullPath); // check for BOM
	if (sdirfile->Open(strFullPath, CFile::modeRead | CFile::shareDenyWrite | (bIsUnicodeFile ? CFile::typeBinary : 0))) {
		try {
			if (bIsUnicodeFile)
				sdirfile->Seek(sizeof(WORD), SEEK_CUR); // skip BOM

			CString toadd;
			while (sdirfile->ReadString(toadd)) {
				toadd.Trim(_T(" \t\r\n")); // need to trim '\r' in binary mode
				if (toadd.IsEmpty())
					continue;

				bool bExclude = (toadd.Left(1) == '-'); // a '-' prefix means excluded
				if (bExclude)
					toadd.Delete(0, 1);

				// Skip non-existing directories from fixed disks only
				int iDrive = PathGetDriveNumber(toadd);
				if (iDrive >= 0 && iDrive <= 25) {
					TCHAR szRootPath[4] = _T("@:\\");
					*szRootPath = (TCHAR)(_T('A') + iDrive);
					if (GetDriveType(szRootPath) == DRIVE_FIXED)
						if (_taccess(toadd, 0) != 0) //check existence
							continue;
				}

				if (bExclude)
					ExcludeFile(toadd);
				else
					AddSingleSharedFile(toadd, true);

			}
			sdirfile->Close();
		} catch (CFileException *error) {
			TCHAR buffer[MAX_CFEXP_ERRORMSG];
			GetExceptionMessage(*error, buffer, _countof(buffer));
			DebugLogError(_T("Failed to load %s - %s"), (LPCTSTR)strFullPath, buffer);
			error->Delete();
		}
	} else
		DebugLogError(_T("Failed to load %s"), (LPCTSTR)strFullPath);
	delete sdirfile;
}

bool CSharedFileList::AddSingleSharedDirectory(const CString &rstrFilePath, bool bNoUpdate)
{
	ASSERT(rstrFilePath.Right(1) == _T("\\"));
	// check if we share this dir already or are not allowed to
	if (ShouldBeShared(rstrFilePath, CString(), false) || !thePrefs.IsShareableDirectory(rstrFilePath))
		return false;
	thePrefs.shareddir_list.AddTail(rstrFilePath); // adds the new directory as shared, GUI updates need to be done by the caller

	if (!bNoUpdate) {
		AddFilesFromDirectory(rstrFilePath);
		HashNextFile();
	}
	return true;
}

CString CSharedFileList::GetPseudoDirName(const CString &strDirectoryName)
{
	// those pseudo names are sent to other clients when requesting shared files instead of the full directory names to avoid
	// giving away too many information about our local file structure, which might be sensitive data in some cases,
	// but we still want to use a descriptive name so the information of files sorted by directories is not lost
	// So, in general we use only the name of the directory, shared subdirs keep the path up to the highest shared dir,
	// this way we never reveal the name of any not directly shared directory. We then make sure its unique.
	if (!ShouldBeShared(strDirectoryName, _T(""), false)) {
		ASSERT(0);
		return CString();
	}
	// does the name already exists?
	CString strTmpPseudo, strTmpPath;
	for (POSITION pos = m_mapPseudoDirNames.GetStartPosition(); pos != NULL;) {
		m_mapPseudoDirNames.GetNextAssoc(pos, strTmpPseudo, strTmpPath);
		if (CompareDirectory(strTmpPath, strDirectoryName) == 0)
			return strTmpPseudo;	// already done here
	}

	// create a new Pseudoname
	CString strDirectoryTmp = strDirectoryName;
	unslosh(strDirectoryTmp);

	CString strPseudoName;
	int iPos;
	while ((iPos = strDirectoryTmp.ReverseFind(_T('\\'))) >= 0) {
		strPseudoName = strDirectoryTmp.Right(strDirectoryTmp.GetLength() - iPos) + strPseudoName;
		strDirectoryTmp.Truncate(iPos);
		if (!ShouldBeShared(strDirectoryTmp, _T(""), false))
			break;
	}
	if (strPseudoName.IsEmpty()) {
		// must be a root Existence only directory
		ASSERT(strDirectoryTmp.GetLength() == 2);
		strPseudoName = strDirectoryTmp;
	} else {
		// remove first backslash
		ASSERT(strPseudoName[0] == _T('\\'));
		strPseudoName.Delete(0, 1);
	}
	// we have the name, make sure it is unique
	if (m_mapPseudoDirNames.Lookup(strPseudoName, strDirectoryTmp)) {
		CString strUnique;
		for (iPos = 2; ; ++iPos) {
			strUnique.Format(_T("%s_%i"), (LPCTSTR)strPseudoName, iPos);
			if (!m_mapPseudoDirNames.Lookup(strUnique, strDirectoryTmp)) {
				DebugLog(_T("Using Pseudoname %s for directory %s"), (LPCTSTR)strUnique, (LPCTSTR)strDirectoryName);
				m_mapPseudoDirNames.SetAt(strUnique, strDirectoryName);
				return strUnique;
			}
			if (iPos > 200) {
				// wth?
				ASSERT(0);
				return CString();
			}
		}
	} else {
		DebugLog(_T("Using Pseudoname %s for directory %s"), (LPCTSTR)strPseudoName, (LPCTSTR)strDirectoryName);
		m_mapPseudoDirNames.SetAt(strPseudoName, strDirectoryName);
		return strPseudoName;
	}
}

CString CSharedFileList::GetDirNameByPseudo(const CString &strPseudoName) const
{
	CString strResult;
	m_mapPseudoDirNames.Lookup(strPseudoName, strResult);
	return strResult;
}

bool CSharedFileList::GetPopularityRank(const CKnownFile *pFile, uint32 &rnOutSession, uint32 &rnOutTotal) const
{
	if (GetFileByIdentifier(pFile->GetFileIdentifierC()) == NULL) {
		rnOutSession = 0;
		rnOutTotal = 0;
		ASSERT(0);
		return false;
	}
	// we start at rank #1, not 0
	rnOutSession = 1;
	rnOutTotal = 1;
	// cycle all files, each file which has more requests than the given file lowers the rank
	CCKey bufKey;
	for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
		CKnownFile *cur_file;
		m_Files_map.GetNextAssoc(pos, bufKey, cur_file);
		if (cur_file != pFile) {
			rnOutTotal += static_cast<uint32>(cur_file->statistic.GetAllTimeRequests() > pFile->statistic.GetAllTimeRequests());
			rnOutSession += static_cast<uint32>(cur_file->statistic.GetRequests() > pFile->statistic.GetRequests());
		}
	}
	return true;
}