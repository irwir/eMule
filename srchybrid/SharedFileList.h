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
#include "MapKey.h"
#include "FileIdentifier.h"

class CKnownFileList;
class CServerConnect;
class CPartFile;
class CKnownFile;
class CPublishKeywordList;
class CSafeMemFile;
class CServer;
class CCollection;

struct UnknownFile_Struct
{
	CString strName;
	CString strDirectory;
	CString strSharedDirectory;
};

class CSharedFileList
{
	friend class CSharedFilesCtrl;
	friend class CClientReqSocket;

	void	AddDirectory(const CString &strDir, CStringList &dirlist);
public:
	explicit CSharedFileList(CServerConnect *in_server);
	~CSharedFileList();
	CSharedFileList(const CSharedFileList&) = delete;
	CSharedFileList& operator=(const CSharedFileList&) = delete;

	void	SendListToServer();
	void	Reload();
	void	Save() const;
	void	Process();
	void	Publish();
	void	RebuildMetaData();
	void	DeletePartFileInstances() const;
	void	PublishNextTurn()						{ m_lastPublishED2KFlag = true; }
	void	ClearED2KPublishInfo();
	void	ClearKadSourcePublishInfo();

	static void	CreateOfferedFilePacket(CKnownFile *cur_file, CSafeMemFile *files, CServer *pServer, CUpDownClient *pClient = NULL);

	bool	SafeAddKFile(CKnownFile *toadd, bool bOnlyAdd = false);
	void	RepublishFile(CKnownFile *pFile);
	void	SetOutputCtrl(CSharedFilesCtrl *in_ctrl);
	bool	RemoveFile(CKnownFile *pFile, bool bDeleted = false);	// removes a specific shared file from the list
	void	UpdateFile(CKnownFile *toupdate);
	void	AddFileFromNewlyCreatedCollection(const CString &rstrFilePath)	{ CheckAndAddSingleFile(rstrFilePath); }

	// GUI is not initially updated
	bool	AddSingleSharedFile(const CString &rstrFilePath, bool bNoUpdate = false); // includes updating sharing preferences, calls CheckAndAddSingleSharedFile afterwards
	bool	AddSingleSharedDirectory(const CString &rstrFilePath, bool bNoUpdate = false);
	bool	ExcludeFile(const CString &strFilePath);	// excludes a specific file from being shared and removes it from the list if it exists

	void	AddKeywords(CKnownFile *pFile);
	void	RemoveKeywords(CKnownFile *pFile);

	void	CopySharedFileMap(CMap<CCKey, const CCKey&, CKnownFile*, CKnownFile*> &Files_Map);

	CKnownFile*	GetFileByID(const uchar *hash) const;
	CKnownFile*	GetFileByIdentifier(const CFileIdentifierBase &rFileIdent, bool bStrict = false) const;
	CKnownFile*	GetFileByIndex(INT_PTR index) const; // slow
	CKnownFile*	GetFileNext(POSITION &pos) const;
	CKnownFile*	GetFileByAICH(const CAICHHash &rHash) const; // slow

	bool	IsFilePtrInList(const CKnownFile *file) const; // slow
	bool	IsUnsharedFile(const uchar *auFileHash) const;
	bool	ShouldBeShared(const CString &strPath, const CString &strFilePath, bool bMustBeShared) const;
	bool	ContainsSingleSharedFiles(const CString &strDirectory) const; // includes subdirs
	CString	GetPseudoDirName(const CString &strDirectoryName);
	CString	GetDirNameByPseudo(const CString &strPseudoName) const;

	uint64	GetDatasize(uint64 &pbytesLargest) const;
	INT_PTR	GetCount()								{ return m_Files_map.GetCount(); }
	INT_PTR	GetHashingCount()						{ return waitingforhash_list.GetCount() + currentlyhashing_list.GetCount(); }
	bool	ProbablyHaveSingleSharedFiles() const	{ return bHaveSingleSharedFiles && !m_liSingleSharedFiles.IsEmpty(); } // might not be always up-to-date, could give false "true"s, not a problem currently

	void	HashFailed(UnknownFile_Struct *hashed);	// SLUGFILLER: SafeHash
	void	FileHashingFinished(CKnownFile *file);

	bool	GetPopularityRank(const CKnownFile *pFile, uint32 &rnOutSession, uint32 &rnOutTotal) const;

	CCriticalSection m_mutWriteList; // don't acquire other locks while having this one in the main thread or make sure deadlocks are impossible
	static uint8 GetRealPrio(uint8 in)				{ return (in < 4) ? in + 1 : 0; };
protected:
	bool	AddFile(CKnownFile *pFile);
	void	AddFilesFromDirectory(const CString &rstrDirectory);
	void	FindSharedFiles();

	void	HashNextFile();
	bool	IsHashing(const CString &rstrDirectory, const CString &rstrName);
	void	RemoveFromHashing(CKnownFile *hashed);
	void	LoadSingleSharedFilesList();

	void	CheckAndAddSingleFile(const CFileFind &ff);
	bool	CheckAndAddSingleFile(const CString &rstrFilePath); // add specific files without editing sharing preferences

private:
	CMap<CCKey, const CCKey&, CKnownFile*, CKnownFile*> m_Files_map;
	CMap<CSKey, const CSKey&, bool, bool>			 m_UnsharedFiles_map;
	CMapStringToString m_mapPseudoDirNames;
	CPublishKeywordList *m_keywords;
	CTypedPtrList<CPtrList, UnknownFile_Struct*> waitingforhash_list;
	CTypedPtrList<CPtrList, UnknownFile_Struct*> currentlyhashing_list;	// SLUGFILLER: SafeHash
	CServerConnect	 *server;
	CSharedFilesCtrl *output;
	CStringList		 m_liSingleSharedFiles;
	CStringList		 m_liSingleExcludedFiles;

	INT_PTR	m_currFileSrc;
	INT_PTR	m_currFileNotes;
	//INT_PTR	m_currFileKey; - not used
	time_t	m_lastPublishKadSrc;
	time_t	m_lastPublishKadNotes;
	DWORD	m_lastPublishED2K;
	bool	m_lastPublishED2KFlag;
	bool	bHaveSingleSharedFiles;
};

class CAddFileThread : public CWinThread
{
	DECLARE_DYNCREATE(CAddFileThread)
protected:
	CAddFileThread();
public:
	virtual BOOL InitInstance();
	virtual int	Run();
	void	SetValues(CSharedFileList *pOwner, LPCTSTR directory, LPCTSTR filename, LPCTSTR strSharedDir, CPartFile *partfile = NULL);
	bool ImportParts();
	uint16 SetPartToImport(LPCTSTR import);
private:
	CSharedFileList	*m_pOwner;
	CPartFile		*m_partfile;
	CString			m_strDirectory;
	CString			m_strFilename;
	CString			m_strSharedDir;
	CString			m_strImport;
	CArray<uint16, uint16>	m_PartsToImport;
};