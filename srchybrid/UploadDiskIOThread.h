//this file is part of eMule
//Copyright (C)2002-2010 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
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

struct OpenOvFile_Struct
{
	uchar		ucMD4FileHash[16];
	uint64		uFileSize;
	HANDLE		hFile;
	uint32		nInUse;
	bool		bStatsIsPartfile;
	bool		bCompress;
};

struct UploadingToClient_Struct;
class CUploadDiskIOThread;
struct OverlappedEx_Struct
{
	OVERLAPPED				oOverlap; // must be the first member
	OpenOvFile_Struct		*pFileStruct;
	UploadingToClient_Struct *pUploadClientStruct;
	uint64					uStartOffset;
	uint64					uEndOffset;
	BYTE					*pBuffer;
	DWORD					dwRead;
};

class Packet;
typedef CTypedPtrList<CPtrList, Packet*> CPacketList;

class CUpDownClient;

class CUploadDiskIOThread : public CWinThread
{
	DECLARE_DYNCREATE(CUploadDiskIOThread)
public:
	CUploadDiskIOThread();
	~CUploadDiskIOThread();
	CUploadDiskIOThread(const CUploadDiskIOThread&) = delete;
	CUploadDiskIOThread& operator=(const CUploadDiskIOThread&) = delete;

	void		NewBlockRequestsAvailable();
	void		SocketNeedsMoreData();
	void		EndThread();
	static bool ShouldCompressBasedOnFilename(const CString &strFileName);

	uint32		dbgDataReadPending;
protected:


private:
	static UINT AFX_CDECL RunProc(LPVOID pParam);
	UINT		RunInternal();

	void		StartCreateNextBlockPackage(UploadingToClient_Struct *pUploadClientStruct);
	bool		ReleaseOvOpenFile(OpenOvFile_Struct *pOpenOvFileStruct);
	void		ReadCompletionRoutine(DWORD dwErrorCode, DWORD dwBytesRead, OverlappedEx_Struct *pOverlappedExStruct);

	static void CreateStandardPackets(byte *pbyData, uint64 uStartOffset, uint64 uEndOffset, bool bFromPF, CPacketList &rOutPacketList, const uchar *pucMD4FileHash, const CString &strDbgClientInfo);
	static void CreatePackedPackets(byte *pbyData, uint64 uStartOffset, uint64 uEndOffset, bool bFromPF, CPacketList &rOutPacketList, const uchar *pucMD4FileHash, const CString &strDbgClientInfo);
	static void CreatePeerCachePackets(byte *pbyData, uint64 uStartOffset, uint64 uEndOffset, uint64 uFilesize, bool bFromPF, CPacketList &rOutPacketList, const uchar *pucMD4FileHash, CUpDownClient *pClient);

	CEvent		*m_eventThreadEnded;
	CEvent		m_eventNewBlockRequests;
	CEvent		m_eventAsyncIOFinished;
	CEvent		m_eventSocketNeedsData;
	CTypedPtrList<CPtrList, OpenOvFile_Struct*>		m_listOpenFiles;
	CTypedPtrList<CPtrList, OverlappedEx_Struct*>	m_listPendingIO;
	CTypedPtrList<CPtrList, OverlappedEx_Struct*>	m_listFinishedIO;
	bool		m_bSignalThrottler;
	volatile bool m_bRun;
};

class CSyncHelper
{
public:
	CSyncHelper()
		: m_pFile()
	{
	}

	~CSyncHelper()
	{
		if (m_pFile)
			m_pFile->Unlock();
	}

	CSyncObject *m_pFile;
};