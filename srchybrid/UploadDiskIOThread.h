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
#pragma once

class Packet;
class CUpDownClient;
typedef CTypedPtrList<CPtrList, Packet*> CPacketList;

struct UploadingToClient_Struct;

struct OverlappedRead_Struct
{
	OVERLAPPED				oOverlap; // must be the first member
	CKnownFile				*pFile;
	UploadingToClient_Struct *pUploadClientStruct;
	uint64					uStartOffset;
	uint64					uEndOffset;
	BYTE					*pBuffer;
	POSITION				pos;
};

class CUploadDiskIOThread : public CWinThread
{
	DECLARE_DYNCREATE(CUploadDiskIOThread)
public:
	CUploadDiskIOThread();
	~CUploadDiskIOThread();
	CUploadDiskIOThread(const CUploadDiskIOThread&) = delete;
	CUploadDiskIOThread& operator=(const CUploadDiskIOThread&) = delete;

	void		EndThread();	//completionkey == 0
	void		WakeUpCall();	//completionkey == WAKEUP
	static void	DissociateFile(CKnownFile *pFile);

private:
	static UINT AFX_CDECL RunProc(LPVOID pParam);
	UINT		RunInternal();

	bool		AssociateFile(CKnownFile *pFile);
	static bool ShouldCompressBasedOnFilename(const CString &strFileName);
	void		StartCreateNextBlockPackage(UploadingToClient_Struct *pUploadClientStruct);
	void		ReadCompletionRoutine(DWORD dwRead, const OverlappedRead_Struct *pOvRead);

	static void CreatePackedPackets(const OverlappedRead_Struct &OverlappedRead, CPacketList &rOutPacketList);
	static void CreateStandardPackets(const OverlappedRead_Struct &OverlappedRead, CPacketList &rOutPacketList);

	CEvent		m_eventThreadStarted;
	CEvent		m_eventThreadEnded;
	CTypedPtrList<CPtrList, OverlappedRead_Struct*>	m_listPendingIO;

	HANDLE		m_hPort;
#ifdef _DEBUG
	uint64		dbgDataReadPending;
#endif
	volatile char m_Run; //0 - not running; 1 - idle; 2 - processing
	volatile char m_bNewData;
	bool		m_bSignalThrottler;
};