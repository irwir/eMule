//this file is part of eMule
//Copyright (C)2020-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
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

struct PartFileBufferedData;

struct ToWrite
{
	CPartFile *pFile;
	PartFileBufferedData *pBuffer;
};

struct OverlappedWrite_Struct
{
	OVERLAPPED				oOverlap; // must be the first member
	CPartFile				*pFile;
	PartFileBufferedData	*pBuffer;
	POSITION				pos; // in m_listPendingIO
};

class CPartFileWriteThread : public CWinThread
{
	DECLARE_DYNCREATE(CPartFileWriteThread)
public:
	CPartFileWriteThread();
	~CPartFileWriteThread();
	CPartFileWriteThread(const CPartFileWriteThread&) = delete;
	CPartFileWriteThread& operator=(const CPartFileWriteThread&) = delete;
	CCriticalSection m_lockWriteList;

	void	EndThread();	//completionkey == 0
	void	WakeUpCall();	//completionkey == -1
	bool	IsRunning() const							{ return m_Run > 0; }
	bool	AddFile(CPartFile *pFile);
	static void	RemFile(CPartFile *pFile);

	CCriticalSection m_lockFlushList;
	CList<ToWrite> m_FlushList;

private:
	static UINT AFX_CDECL RunProc(LPVOID pParam);
	UINT	RunInternal();

	void	WriteBuffers();
	void	WriteCompletionRoutine(DWORD dwBytesWritten, const OverlappedWrite_Struct *pOvWrite);

	CList<ToWrite>	m_listToWrite;
	CTypedPtrList<CPtrList, OverlappedWrite_Struct*>	m_listPendingIO;

	CEvent	m_eventThreadStarted;
	CEvent	m_eventThreadEnded;
	HANDLE	m_hPort;
	volatile char m_Run; //0 - not running; 1 - idle; 2 - processing
	volatile char m_bNewData;
};