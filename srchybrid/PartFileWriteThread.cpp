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
#include "StdAfx.h"
#include "updownclient.h"
#include "PartFileWriteThread.h"
#include "emule.h"
#include "partfile.h"
#include "log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define RUN_STOP	0
#define RUN_IDLE	1
#define RUN_WORK	2
#define WAKEUP		((ULONG_PTR)(~0))

IMPLEMENT_DYNCREATE(CPartFileWriteThread, CWinThread)

CPartFileWriteThread::CPartFileWriteThread()
	: m_eventThreadStarted(FALSE, TRUE)
	, m_eventThreadEnded(FALSE, TRUE)
	, m_hPort()
	, m_Run(RUN_STOP)
	, m_bNewData()
{
	if (AfxBeginThread(RunProc, (LPVOID)this, THREAD_PRIORITY_BELOW_NORMAL) == NULL) {
		m_eventThreadStarted.SetEvent();
		m_eventThreadEnded.SetEvent();
		DebugLogError(_T("Failed to create PartFileWriteThread (Win32 error %lu)"), ::GetLastError());
	}
}

CPartFileWriteThread::~CPartFileWriteThread()
{
	ASSERT(!m_hPort && !m_Run);
}

UINT AFX_CDECL CPartFileWriteThread::RunProc(LPVOID pParam)
{
	DbgSetThreadName("PartWriteThread");
	InitThreadLocale();
	return pParam ? static_cast<CPartFileWriteThread*>(pParam)->RunInternal() : 1;
}

void CPartFileWriteThread::EndThread()
{
	m_eventThreadStarted.Lock();
	m_Run = RUN_STOP;
	if (m_hPort != NULL)
		PostQueuedCompletionStatus(m_hPort, 0, 0, NULL);
	m_eventThreadEnded.Lock();
}

UINT CPartFileWriteThread::RunInternal()
{
	m_hPort = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);
	if (!m_hPort) {
		const DWORD dwError = ::GetLastError();
		m_eventThreadStarted.SetEvent();
		m_eventThreadEnded.SetEvent();
		return dwError;
	}

	DWORD dwWrite = 0;
	ULONG_PTR completionKey = 0;
	OverlappedWrite_Struct *pCurIO = NULL;
	m_Run = RUN_IDLE;
	m_eventThreadStarted.SetEvent();
	while (m_Run
		&& ::GetQueuedCompletionStatus(m_hPort, &dwWrite, &completionKey, (LPOVERLAPPED*)&pCurIO, INFINITE)
		&& completionKey)
	{
		m_Run = RUN_WORK;
		//move buffer lists into the local storage
		if (!m_FlushList.IsEmpty()) {
			m_lockFlushList.Lock();
			while (!m_FlushList.IsEmpty())
				m_listToWrite.AddTail(m_FlushList.RemoveHead());
			InterlockedExchange8(&m_bNewData, 0);
			m_lockFlushList.Unlock();
		}
		//start new I/O
		WriteBuffers();
		//completed I/O
		do {
			if (!completionKey)
				break;
			if (completionKey != WAKEUP) //ignore wakeups
				WriteCompletionRoutine(dwWrite, pCurIO);
		} while (::GetQueuedCompletionStatus(m_hPort, &dwWrite, &completionKey, (LPOVERLAPPED*)&pCurIO, 0));

		if (!completionKey) //thread termination
			break;
		m_Run = RUN_IDLE;
		if (InterlockedExchange8(&m_bNewData, 0) && m_listPendingIO.IsEmpty())
			PostQueuedCompletionStatus(m_hPort, 0, WAKEUP, NULL);
	}
	m_Run = RUN_STOP;

	//Improper termination of asynchronous I/O follows...
	//close file handles to release I/O completion port
	while (!m_listPendingIO.IsEmpty())
		WriteCompletionRoutine(0, m_listPendingIO.RemoveHead());

	::CloseHandle(m_hPort);
	m_hPort = 0;

	m_eventThreadEnded.SetEvent();
	return 0;
}

void CPartFileWriteThread::WriteBuffers()
{
	//process internal list
	while (!m_listToWrite.IsEmpty() && m_Run) {
		const ToWrite &item = m_listToWrite.RemoveHead();
		PartFileBufferedData *pBuffer = item.pBuffer;
		ASSERT(pBuffer->end >= pBuffer->start && (pBuffer->data || pBuffer->end == pBuffer->start)); //verifies allocation requests too

		CPartFile *pFile = item.pFile;
		if (AddFile(pFile)) {
			//initiate write
			OverlappedWrite_Struct *pOvWrite = new OverlappedWrite_Struct;
			pOvWrite->oOverlap.Internal = 0;
			pOvWrite->oOverlap.InternalHigh = 0;
			//pOvWrite->oOverlap.Offset = LODWORD(currentblock->StartOffset);
			//pOvWrite->oOverlap.OffsetHigh = HIDWORD(currentblock->StartOffset);
			*(uint64*)&pOvWrite->oOverlap.Offset = pBuffer->start;
			pOvWrite->oOverlap.hEvent = 0;
			pOvWrite->pFile = pFile;
			pOvWrite->pBuffer = pBuffer;

			static const BYTE zero = 0;
			if (!::WriteFile(pFile->m_hWrite, pBuffer->data ? pBuffer->data : &zero, (DWORD)(pBuffer->end - pBuffer->start + 1), NULL, (LPOVERLAPPED)pOvWrite)) {
				DWORD dwError = ::GetLastError();
				if (dwError != ERROR_IO_PENDING) {
					delete pOvWrite;
					if (item.pBuffer->data) { //check for an allocation request
						item.pBuffer->dwError = dwError;
						item.pBuffer->flushed = PB_ERROR;
						theApp.QueueDebugLogLineEx(LOG_WARNING, _T("WriteBuffers error: %lu"), dwError);
					}
					RemFile(pFile);
					return;
				}
			}
			pOvWrite->pos = m_listPendingIO.AddTail(pOvWrite);
			++pFile->m_iWrites;
		} else
			theApp.QueueDebugLogLineEx(LOG_ERROR, _T("WriteBuffers error: CPartFile cannot be written"));
	}
}

void CPartFileWriteThread::WriteCompletionRoutine(DWORD dwBytesWritten, const OverlappedWrite_Struct *pOvWrite)
{
	if (pOvWrite == NULL) {
		ASSERT(0);
		return;
	}
	CPartFile *pFile = pOvWrite->pFile;
	if (m_Run) {
		PartFileBufferedData *pBuffer = pOvWrite->pBuffer;
		const DWORD dwWrite = (DWORD)(pBuffer->end - pBuffer->start + 1);

		ASSERT(pOvWrite->pos);
		m_listPendingIO.RemoveAt(pOvWrite->pos);
		if (dwBytesWritten && dwWrite == dwBytesWritten) {
			if (pFile) {
				--pFile->m_iWrites;
				if (pBuffer->data) { //write data
					ASSERT(pBuffer->flushed = PB_PENDING && pFile->m_iWrites >= 0);
					pBuffer->flushed = PB_WRITTEN;
				} else { //full file allocation
					ASSERT(dwBytesWritten == 1);
					::FlushFileBuffers(pFile->m_hWrite);
					pFile->m_hpartfile.SetLength(pBuffer->start); //truncate the extra byte
					delete pBuffer;
				}
			}
		} else {
			pBuffer->flushed = PB_ERROR; //error code is unknown
			Debug(_T("  Completed write size: expected %lu, written %lu\n"), dwWrite, dwBytesWritten);
		}
	} else if (pFile)
		RemFile(pFile);

	delete pOvWrite;
}

bool CPartFileWriteThread::AddFile(CPartFile *pFile)
{
	ASSERT(m_hPort && m_Run);
	if (pFile && pFile->m_hWrite == INVALID_HANDLE_VALUE) {
		const CString sPartFile(RemoveFileExtension(pFile->GetFullName()));
		pFile->m_hWrite = ::CreateFile(sPartFile, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if (pFile->m_hWrite == INVALID_HANDLE_VALUE) {
			theApp.QueueDebugLogLineEx(LOG_ERROR, _T("Failed to open \"%s\" for overlapped write: %s"), (LPCTSTR)sPartFile, (LPCTSTR)GetErrorMessage(::GetLastError(), 1));
			pFile->SetStatus(PS_ERROR);
			return false;
		}
		if (m_hPort != ::CreateIoCompletionPort(pFile->m_hWrite, m_hPort, (ULONG_PTR)pFile, 0)) {
			theApp.QueueDebugLogLineEx(LOG_ERROR, _T("Failed to associate \"%s\" with IOCP: %s"), (LPCTSTR)sPartFile, (LPCTSTR)GetErrorMessage(::GetLastError(), 1));
			RemFile(pFile);
			pFile->SetStatus(PS_ERROR);
			return false;
		}
	}
	return true;
}

void CPartFileWriteThread::RemFile(CPartFile *pFile)
{
	ASSERT(pFile);
	if (pFile->m_hWrite != INVALID_HANDLE_VALUE) {
		VERIFY(::CloseHandle(pFile->m_hWrite));
		pFile->m_hWrite = INVALID_HANDLE_VALUE;
	}
}

void CPartFileWriteThread::WakeUpCall()
{
	//pending I/O makes posting unnecessary
	if (m_Run == RUN_IDLE && m_listPendingIO.IsEmpty())
		PostQueuedCompletionStatus(m_hPort, 0, WAKEUP, NULL);
	else
		InterlockedExchange8(&m_bNewData, 1);
}