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

#include "StdAfx.h"
#include "uploaddiskiothread.h"
#include "otherfunctions.h"
#include "emule.h"
#include "UploadQueue.h"
#include "updownclient.h"
#include "sharedfilelist.h"
#include "partfile.h"
#include "knownfile.h"
#include "log.h"
#include "preferences.h"
#include "safefile.h"
#include "listensocket.h"
#include "packets.h"
#include "Statistics.h"
#include "PeerCacheSocket.h"
#include "UploadBandwidthThrottler.h"
#include "zlib/zlib.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define SLOT_COMPRESSIONCHECK_DATARATE		(150 * 1024)	// Data rate for a single client from which we start to check if we need to disable compression
#define MAX_FINISHED_REQUESTS_COMPRESSION	15				// Max waiting finished requests before disabling compression (if total upload > SLOT_COMPRESSIONCHECK_DATARATE)
#define BIGBUFFER_MINDATARATE				(75 * 1024)

IMPLEMENT_DYNCREATE(CUploadDiskIOThread, CWinThread)

CUploadDiskIOThread::CUploadDiskIOThread()
	: dbgDataReadPending()
	, m_eventAsyncIOFinished(FALSE, TRUE)
	, m_bSignalThrottler()
	, m_bRun()
{
	ASSERT(theApp.uploadqueue != NULL);
	m_eventThreadEnded = new CEvent(FALSE, TRUE);
	AfxBeginThread(RunProc, (LPVOID)this);
}

CUploadDiskIOThread::~CUploadDiskIOThread()
{
	ASSERT(!m_bRun);
	delete m_eventThreadEnded;
}


UINT AFX_CDECL CUploadDiskIOThread::RunProc(LPVOID pParam)
{
	DbgSetThreadName("UploadDiskIOThread");
	InitThreadLocale();
	return (pParam == NULL) ? 1 : static_cast<CUploadDiskIOThread*>(pParam)->RunInternal();
}


void CUploadDiskIOThread::EndThread()
{
	m_bRun = false;
	m_eventNewBlockRequests.PulseEvent();
	m_eventThreadEnded->Lock();
}

UINT CUploadDiskIOThread::RunInternal()
{
	m_bRun = true;
	bool bCheckForPendingIOOnly = false;
	while (m_bRun) {
		if (!bCheckForPendingIOOnly) {
			m_eventNewBlockRequests.ResetEvent();
			m_eventSocketNeedsData.ResetEvent();
			CCriticalSection *pcsUploadListRead = NULL;
			const CUploadingPtrList &rUploadList = theApp.uploadqueue->GetUploadListTS(&pcsUploadListRead);
			CSingleLock lockUploadListRead(pcsUploadListRead, TRUE);
			ASSERT(lockUploadListRead.IsLocked());

			for (POSITION pos = rUploadList.GetHeadPosition(); pos != NULL;) {
				UploadingToClient_Struct *pCurUploadingClientStruct = rUploadList.GetNext(pos);
				const CUpDownClient *cur_client = pCurUploadingClientStruct->m_pClient;
				if (cur_client->socket != NULL && cur_client->socket->IsConnected()
					&& (!cur_client->IsUploadingToPeerCache() || cur_client->m_pPCUpSocket != NULL))
				{
					StartCreateNextBlockPackage(pCurUploadingClientStruct);
				}
			}
			lockUploadListRead.Unlock();
		}

		// check if any pending requests have finished
		m_eventAsyncIOFinished.SetEvent(); // Windows doesn't like finished Overlapped results with non-signaled events. But we don't want to create an event for each our reads (which would be the suggested solution)
		for (POSITION pos = m_listPendingIO.GetHeadPosition(); pos != NULL;) {
			POSITION pos2 = pos;
			OverlappedEx_Struct *pCurPendingIO = m_listPendingIO.GetNext(pos);
			DWORD dwRead;
			if (GetOverlappedResult(pCurPendingIO->pFileStruct->hFile, &pCurPendingIO->oOverlap, &dwRead, FALSE)) {
				m_listPendingIO.RemoveAt(pos2);
				// we add it to a list instead of processing it directly because we need to know how many finished IOs
				// we have to decide if we can use compression or need to speedup later on
				pCurPendingIO->dwRead = dwRead;
				m_listFinishedIO.AddTail(pCurPendingIO);
			} else {
				DWORD nError = ::GetLastError();
				if (nError != ERROR_IO_PENDING && nError != ERROR_IO_INCOMPLETE) {
					theApp.QueueDebugLogLineEx(LOG_ERROR, _T("IO Error: GetOverlappedResult: %s"), (LPCTSTR)GetErrorMessage(nError));
					m_listPendingIO.RemoveAt(pos2);
					// calling it with an error will cause it to clean up the mess
					ReadCompletionRoutine(1, 0, pCurPendingIO);
				}
			}
		}
		m_eventAsyncIOFinished.ResetEvent();
		// work down all the finished IOs
		while (!m_listFinishedIO.IsEmpty())
			ReadCompletionRoutine(0, 0, m_listFinishedIO.RemoveHead());

		// if we put new data on some socket, tell the throttler
		if (m_bSignalThrottler && theApp.uploadBandwidthThrottler != NULL) {
			theApp.uploadBandwidthThrottler->NewUploadDataAvailable();
			m_bSignalThrottler = false;
		}

		HANDLE aEvents[3] = {m_eventAsyncIOFinished, m_eventNewBlockRequests, m_eventSocketNeedsData};
		DWORD nRes = ::WaitForMultipleObjects(_countof(aEvents), aEvents, FALSE, 500);
		ASSERT(nRes != WAIT_FAILED);
		bCheckForPendingIOOnly = (nRes == WAIT_OBJECT_0);
	}
	// cleanup - signal all open files to cancel all their pending operations
	for (POSITION pos = m_listOpenFiles.GetHeadPosition(); pos != NULL;)
		if (CancelIo(m_listOpenFiles.GetNext(pos)->hFile))
			::WaitForSingleObject(m_eventAsyncIOFinished, SEC2MS(1)); //should be INFINITE, but event is reused
		else {
			theApp.QueueDebugLogLineEx(LOG_ERROR, _T("CUploadDiskIOThread cleanup, CancelIO failed!"));
			ASSERT(0);
		}

	while (!m_listPendingIO.IsEmpty())
		ReadCompletionRoutine(1, 0, m_listPendingIO.RemoveHead());

	if (!m_listOpenFiles.IsEmpty()) {
		theApp.QueueDebugLogLineEx(LOG_ERROR, _T("CUploadDiskIOThread cleanup, failed to cleanup all pending operations (file handles still in use)"));
		ASSERT(0);
	}
	m_eventThreadEnded->SetEvent();
	return 0;
}

// todo: main thread, check for fileid, check for error

void CUploadDiskIOThread::StartCreateNextBlockPackage(UploadingToClient_Struct *pUploadClientStruct)
{
	// when calling this function we already have a lock to the uploadlist (so pUploadClientStruct and its members are safe in terms of not getting deleted/changed)
	// now also get a lock to the Block lists
	CSingleLock lockBlockLists(&pUploadClientStruct->m_csBlockListsLock, TRUE);
	// See if we can do an early return. There may be no new blocks to load from disk and add to buffer, or buffer may be large enough already.

	uint32 nCurQueueSessionPayloadUp = pUploadClientStruct->m_pClient->GetQueueSessionPayloadUp();
	// GetQueueSessionPayloadUp is probably outdated so also add the value reported by the sockets as sent
	CClientReqSocket *pSock = pUploadClientStruct->m_pClient->socket;
	if (pSock != NULL)
		nCurQueueSessionPayloadUp += (uint32)pSock->GetSentPayloadSinceLastCall(false);
	uint32 addedPayloadQueueSession = pUploadClientStruct->m_pClient->GetQueueSessionUploadAdded();

	// buffer at least 1 block (180KB) on normal uploads and 5 blocks (~900KB) on fast uploads
	bool bFastUpload = pUploadClientStruct->m_pClient->GetDatarate() > BIGBUFFER_MINDATARATE;
	const uint32 nBufferLimit = bFastUpload ? ((5 * EMBLOCKSIZE) + 1) : (EMBLOCKSIZE + 1);

	if (pUploadClientStruct->m_BlockRequests_queue.IsEmpty() // There are no new blocks requested
		|| (addedPayloadQueueSession > nCurQueueSessionPayloadUp && addedPayloadQueueSession - nCurQueueSessionPayloadUp >= nBufferLimit))
	{
		return; // the buffered data is large enough already
	}

	try {
		// Buffer new data if current buffer is less than nBufferLimit Bytes
		while (!pUploadClientStruct->m_BlockRequests_queue.IsEmpty()
			&& (addedPayloadQueueSession <= nCurQueueSessionPayloadUp || addedPayloadQueueSession - nCurQueueSessionPayloadUp < nBufferLimit))
		{
			Requested_Block_Struct *currentblock = pUploadClientStruct->m_BlockRequests_queue.GetHead();
			if (!md4equ(currentblock->FileID, pUploadClientStruct->m_pClient->GetUploadFileID())) {
				// the UploadFileID differs. That's normally not a problem, we just switch it, but
				// we don't want to do so in this thread for synchronization issues. So return and wait
				// until the main thread which checks the block request periodically does so.
				// This should happen very rarely anyway so no problem performance wise
				theApp.QueueDebugLogLine(false, _T("CUploadDiskIOThread::StartCreateNextBlockPackage: Switched fileid, waiting for the main thread"));
				return;
			}

			// fetch a lock for the shared files list, makes also sure the file object doesn't gets deleted while we access it
			// other active Locks: UploadList, m_csBlockListsLock
			CSingleLock lockSharedFiles(&theApp.sharedfiles->m_mutWriteList, TRUE);
			CKnownFile *srcfile = theApp.sharedfiles->GetFileByID(currentblock->FileID);
			if (srcfile == NULL)
				throw GetResString(IDS_ERR_REQ_FNF);

			CSyncHelper lockFile;
			CString fullname;
			if (srcfile->IsPartFile() && static_cast<CPartFile*>(srcfile)->GetStatus() != PS_COMPLETE) {
				// Do not access a part file, if it is currently moved into the incoming directory.
				// Because the moving of part file into the incoming directory may take a noticeable
				// amount of time, we can not wait for 'm_FileCompleteMutex' and block the main thread.
				if (!static_cast<CPartFile*>(srcfile)->m_FileCompleteMutex.Lock(0))// just do a quick test of the mutex's state and return if it's locked.
					return;
				lockFile.m_pFile = &static_cast<CPartFile*>(srcfile)->m_FileCompleteMutex;
				fullname = RemoveFileExtension(static_cast<CPartFile*>(srcfile)->GetFullName());
			} else
				fullname.Format(_T("%s\\%s"), (LPCTSTR)srcfile->GetPath(), (LPCTSTR)srcfile->GetFileName());

			// we already have done all important sanity checks for the block request in the main thread when adding it; just redo some quick important ones
			uint64 uTogo = currentblock->EndOffset - currentblock->StartOffset;
			if (currentblock->StartOffset >= currentblock->EndOffset || currentblock->EndOffset > srcfile->GetFileSize())
				throw _T("Invalid Block Offsets");
			if (uTogo > EMBLOCKSIZE * 3)
				throw GetResString(IDS_ERR_LARGEREQBLOCK);
			//DWORD togo = (DWORD)i64uTogo;

			// check if we already have a handle for this shared file and if so use it.
			// Our checks above make sure we don't continue using a file
			// that user might have removed from his share
			OpenOvFile_Struct *pOpenOvFileStruct = NULL;
			for (POSITION pos = m_listOpenFiles.GetHeadPosition(); pos != NULL;) {
				OpenOvFile_Struct *pCurOpenOvFileStruct = m_listOpenFiles.GetNext(pos);
				if (md4equ(pCurOpenOvFileStruct->ucMD4FileHash, currentblock->FileID)) {
					pOpenOvFileStruct = pCurOpenOvFileStruct;
					break;
				}
			}
			if (pOpenOvFileStruct == NULL) {
				DWORD dwShare; // FILE_SHARE_DELETE allows the user to delete files (or complete partfiles) even while we still have our read handle (which stays valid of course)
				if (thePrefs.GetWindowsVersion() >= _WINVER_2K_) // might work on NT4 too, but I can't test and who is using it still anyway ;), so lets go the safe way
					dwShare = FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE;
				else
					dwShare = FILE_SHARE_WRITE | FILE_SHARE_READ;
				// regardless if partfile or sharedfile, we acquire a new file handle to read the data later on
				HANDLE hFile = ::CreateFile(fullname, GENERIC_READ, dwShare, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
				if (hFile == INVALID_HANDLE_VALUE)
					throw GetResString(IDS_ERR_OPEN);

				pOpenOvFileStruct = new OpenOvFile_Struct;
				md4cpy(pOpenOvFileStruct->ucMD4FileHash, currentblock->FileID);
				pOpenOvFileStruct->hFile = hFile;
				pOpenOvFileStruct->nInUse = 0; // we increase it later right before the right call
				pOpenOvFileStruct->bStatsIsPartfile = srcfile->IsPartFile();
				pOpenOvFileStruct->bCompress = ShouldCompressBasedOnFilename(fullname);
				pOpenOvFileStruct->uFileSize = (uint64)srcfile->GetFileSize();
				m_listOpenFiles.AddTail(pOpenOvFileStruct);
			}

			// some cleanup
			// release the partfile if we have locked it, our file handle works as lock against moving instead (or even better we don't need a lock at all on modern OS)
			if (lockFile.m_pFile != NULL) {
				lockFile.m_pFile->Unlock();
				lockFile.m_pFile = NULL;
			}
			// we now have all informations we need, so release the sharedfileslist
			lockSharedFiles.Unlock();
			srcfile = NULL; // not safe any more

			// now for the actual reading
			OverlappedEx_Struct *pstructOverlappedEx = new OverlappedEx_Struct;
			ASSERT((sizeof pstructOverlappedEx->oOverlap) == sizeof(OVERLAPPED));
			pstructOverlappedEx->oOverlap.Internal = 0;
			pstructOverlappedEx->oOverlap.InternalHigh = 0;
			pstructOverlappedEx->oOverlap.Offset = LODWORD(currentblock->StartOffset);
			pstructOverlappedEx->oOverlap.OffsetHigh = HIDWORD(currentblock->StartOffset);
			pstructOverlappedEx->oOverlap.hEvent = m_eventAsyncIOFinished;
			pstructOverlappedEx->pFileStruct = pOpenOvFileStruct;
			pstructOverlappedEx->pUploadClientStruct = pUploadClientStruct;
			pstructOverlappedEx->uStartOffset = currentblock->StartOffset;
			pstructOverlappedEx->uEndOffset = currentblock->EndOffset;
			pstructOverlappedEx->pBuffer = new byte[(size_t)uTogo];
			pstructOverlappedEx->dwRead = 0;
			pstructOverlappedEx->pFileStruct->nInUse++;

			if (!::ReadFile(pstructOverlappedEx->pFileStruct->hFile, pstructOverlappedEx->pBuffer, (DWORD)uTogo, NULL, (LPOVERLAPPED)pstructOverlappedEx)) {
				DWORD dwError = ::GetLastError();
				if (dwError != ERROR_IO_PENDING) {
					ReleaseOvOpenFile(pstructOverlappedEx->pFileStruct);
					delete[] pstructOverlappedEx->pBuffer;
					delete pstructOverlappedEx;

					if (dwError == ERROR_INVALID_USER_BUFFER || dwError == ERROR_NOT_ENOUGH_MEMORY) {
						theApp.QueueDebugLogLineEx(LOG_WARNING, _T("ReadFile failed, possibly too many pending requests, trying again later"));
						return; // make this a recoverable error, as it might just be that we have too many requests in which case we just need to wait
					}
					throw _T("ReadFileEx Error: ") + GetErrorMessage(::GetLastError());
				}
				m_listPendingIO.AddTail(pstructOverlappedEx);
			} else {
				// read succeeded immediately without an async operation
				GetOverlappedResult(pstructOverlappedEx->pFileStruct->hFile, &pstructOverlappedEx->oOverlap, &pstructOverlappedEx->dwRead, FALSE);
				m_listFinishedIO.AddTail(pstructOverlappedEx);
			}
			dbgDataReadPending += (uint32)uTogo;

			addedPayloadQueueSession += (uint32)uTogo;
			pUploadClientStruct->m_pClient->SetQueueSessionUploadAdded(addedPayloadQueueSession);
			pUploadClientStruct->m_DoneBlocks_list.AddHead(pUploadClientStruct->m_BlockRequests_queue.RemoveHead());
		}
	} catch (const CString &error) {
		theApp.QueueDebugLogLineEx(LOG_ERROR, GetResString(IDS_ERR_CLIENTERRORED), pUploadClientStruct->m_pClient->GetUserName(), (LPCTSTR)error);
		pUploadClientStruct->m_bIOError = true; // will let the main thread remove this client from the list
	} catch (CFileException *e) {
		TCHAR szError[MAX_PATH + 256];
		GetExceptionMessage(*e, szError, _countof(szError));
		theApp.QueueDebugLogLineEx(LOG_ERROR, _T("Failed to create upload package for %s - %s"), pUploadClientStruct->m_pClient->GetUserName(), szError);
		e->Delete();
		pUploadClientStruct->m_bIOError = true; // will let the main thread remove this client from the list
	}
}

void CUploadDiskIOThread::ReadCompletionRoutine(DWORD dwErrorCode, DWORD dwBytesRead, OverlappedEx_Struct *pOverlappedExStruct)
{
	if (pOverlappedExStruct == NULL) {
		ASSERT(0);
		return;
	}
	if (m_bRun) {
		if (dwBytesRead == 0)
			dwBytesRead = pOverlappedExStruct->dwRead;
		bool bError = dwErrorCode != 0;
		if (!bError && dwBytesRead != pOverlappedExStruct->uEndOffset - pOverlappedExStruct->uStartOffset) {
			theApp.QueueDebugLogLineEx(LOG_ERROR, _T("ReadCompletionRoutine: Didn't read requested data count - wanted: %u, read: %u")
				, (uint32)(pOverlappedExStruct->uEndOffset - pOverlappedExStruct->uStartOffset), dwBytesRead);
			bError = true;
		}
		dbgDataReadPending -= (uint32)(pOverlappedExStruct->uEndOffset - pOverlappedExStruct->uStartOffset);
		// check if the clientstruct is still in the uploadlist (otherwise it is a deleted pointer)
		CCriticalSection *pcsUploadListRead = NULL;
		const CUploadingPtrList &rUploadList = theApp.uploadqueue->GetUploadListTS(&pcsUploadListRead);
		CSingleLock lockUploadListRead(pcsUploadListRead, TRUE);
		ASSERT(lockUploadListRead.IsLocked());
		bool bFound = (rUploadList.Find(pOverlappedExStruct->pUploadClientStruct) != NULL);
		if (!bFound) {
			theApp.QueueDebugLogLineEx(LOG_WARNING, _T("ReadCompletionRoutine: Client not found in uploadlist any more when reading finished, discarding block"));
			bError = true;
		}

		// all important prechecks done, create the packets
		if (!bError) {
			// keep the uploadlist lock for working with the client object
			// instead of sending the packets directly, we store them and send them after we have released the uploadlist lock
			// just to be sure there isn't any deadlock chance (now or in future version, also it doesn't costs us anything)
			CPacketList packetsList;

			bool bPeerCache = pOverlappedExStruct->pUploadClientStruct->m_pClient->IsUploadingToPeerCache();
			CClientReqSocket *pSocket = bPeerCache ? pOverlappedExStruct->pUploadClientStruct->m_pClient->m_pPCUpSocket : pOverlappedExStruct->pUploadClientStruct->m_pClient->socket;
			if (pSocket == NULL || !(bPeerCache || pSocket->IsConnected()))
				theApp.QueueDebugLogLineEx(LOG_ERROR, _T("ReadCompletionRoutine: Client has no connected socket, %s"), (LPCTSTR)pOverlappedExStruct->pUploadClientStruct->m_pClient->DbgGetClientInfo(true));
			else {
				// figure out if we need to disable compression. If the data rate is high enough, the socket indicates that it needs more data
				// and we have requests pending, its time to disable it
				bool bUseCompression = false;
				// first the static check to see if we could at all
				if (!bPeerCache && pOverlappedExStruct->pFileStruct->bCompress && pOverlappedExStruct->pUploadClientStruct->m_pClient->GetDataCompressionVersion() == 1 && !pOverlappedExStruct->pUploadClientStruct->m_bDisableCompression) {
					// now the dynamic ones to check if its fine for this upload slot
					if (m_listFinishedIO.GetCount() > MAX_FINISHED_REQUESTS_COMPRESSION && theApp.uploadqueue->GetDatarate() > SLOT_COMPRESSIONCHECK_DATARATE) {
						pOverlappedExStruct->pUploadClientStruct->m_bDisableCompression = true;
						if (thePrefs.GetVerbose())
							theApp.QueueDebugLogLine(false, _T("Disabled compression for upload slot because of too many unprocessed finished IO requests (%u), client: %s"), m_listFinishedIO.GetCount(), (LPCTSTR)pOverlappedExStruct->pUploadClientStruct->m_pClient->DbgGetClientInfo(true));
					} else if (pOverlappedExStruct->pUploadClientStruct->m_pClient->GetDatarate() > SLOT_COMPRESSIONCHECK_DATARATE && !pSocket->HasQueues(true) && !pSocket->IsBusyQuickCheck()) {
						pOverlappedExStruct->pUploadClientStruct->m_bDisableCompression = true;
						if (thePrefs.GetVerbose())
							theApp.QueueDebugLogLine(false, _T("Disabled compression for upload slot because socket is starving for data, client: %s"), (LPCTSTR)pOverlappedExStruct->pUploadClientStruct->m_pClient->DbgGetClientInfo(true));
					} else
						bUseCompression = true;
				}

				CString strDbgClientInfo;
				if (thePrefs.GetDebugClientTCPLevel() > 0)
					strDbgClientInfo = pOverlappedExStruct->pUploadClientStruct->m_pClient->DbgGetClientInfo(true);
				if (bPeerCache) {
					CreatePeerCachePackets(pOverlappedExStruct->pBuffer, pOverlappedExStruct->uStartOffset
						, pOverlappedExStruct->uEndOffset, pOverlappedExStruct->pFileStruct->uFileSize
						, pOverlappedExStruct->pFileStruct->bStatsIsPartfile, packetsList, pOverlappedExStruct->pFileStruct->ucMD4FileHash
						, pOverlappedExStruct->pUploadClientStruct->m_pClient);
				} else if (bUseCompression) {
					CreatePackedPackets(pOverlappedExStruct->pBuffer, pOverlappedExStruct->uStartOffset
						, pOverlappedExStruct->uEndOffset, pOverlappedExStruct->pFileStruct->bStatsIsPartfile, packetsList
						, pOverlappedExStruct->pFileStruct->ucMD4FileHash, strDbgClientInfo);
				} else {
					CreateStandardPackets(pOverlappedExStruct->pBuffer, pOverlappedExStruct->uStartOffset
						, pOverlappedExStruct->uEndOffset, pOverlappedExStruct->pFileStruct->bStatsIsPartfile, packetsList
						, pOverlappedExStruct->pFileStruct->ucMD4FileHash, strDbgClientInfo);
				}
				m_bSignalThrottler = true;
			}

			lockUploadListRead.Unlock();

			// now send out all packets we made. By default, our socket object is not safe to use neither now, because we have no lock on itself (to make sure it is not deleted)
			// however the way we handle sockets, they cannot get deleted directly (takes 10s), so this isn't a problem in our case
			while (!packetsList.IsEmpty() && pSocket != NULL) {
				Packet *packet = packetsList.RemoveHead();
				pSocket->SendPacket(packet, false, packet->uStatsPayLoad);
			}
		} else { // bError
			if (bFound)
				pOverlappedExStruct->pUploadClientStruct->m_bIOError = true;
			lockUploadListRead.Unlock();
		}
	}

	// cleanup
	ReleaseOvOpenFile(pOverlappedExStruct->pFileStruct);
	delete[] pOverlappedExStruct->pBuffer;
	delete pOverlappedExStruct;
}

bool CUploadDiskIOThread::ReleaseOvOpenFile(OpenOvFile_Struct *pOpenOvFileStruct)
{
	POSITION pos = m_listOpenFiles.Find(pOpenOvFileStruct);
	if (pos) {
		pOpenOvFileStruct->nInUse--;
		if (pOpenOvFileStruct->nInUse == 0) {
			VERIFY(::CloseHandle(pOpenOvFileStruct->hFile));
			m_listOpenFiles.RemoveAt(pos);
			delete pOpenOvFileStruct;
		}
		return true;
	}
	ASSERT(0);
	return false;
}

bool CUploadDiskIOThread::ShouldCompressBasedOnFilename(const CString &strFileName)
{
	int pos = strFileName.ReverseFind(_T('.'));
	if (pos < 0)
		return true;
	const CString &strExt(strFileName.Mid(pos + 1).MakeLower());
	if (strExt == _T("avi"))
		return !thePrefs.GetDontCompressAvi();
	return strExt != _T("zip") && strExt != _T("7z") && strExt != _T("rar") && strExt != _T("cbz") && strExt != _T("cbr") && strExt != _T("ace") && strExt != _T("ogm");
}

void CUploadDiskIOThread::CreateStandardPackets(byte *pbyData, uint64 uStartOffset, uint64 uEndOffset, bool bFromPF, CPacketList &rOutPacketList, const uchar *pucMD4FileHash, const CString &strDbgClientInfo)
{
	uint32 togo = (uint32)(uEndOffset - uStartOffset);
	CMemFile memfile((BYTE*)pbyData, togo);
	uint32 nPacketSize = (togo > 10240u) ? togo / (togo / 10240u) : togo;

	while (togo) {
		if (togo < nPacketSize * 2)
			nPacketSize = togo;
		ASSERT(nPacketSize);
		togo -= nPacketSize;

		uint64 endpos = (uEndOffset - togo);
		uint64 statpos = endpos - nPacketSize;

		Packet *packet;
		if (statpos > _UI32_MAX || endpos > _UI32_MAX) {
			packet = new Packet(OP_SENDINGPART_I64, nPacketSize + 32, OP_EMULEPROT, bFromPF);
			md4cpy(&packet->pBuffer[0], pucMD4FileHash);
			PokeUInt64(&packet->pBuffer[16], statpos);
			PokeUInt64(&packet->pBuffer[24], endpos);
			memfile.Read(&packet->pBuffer[32], nPacketSize);
			theStats.AddUpDataOverheadFileRequest(32);
		} else {
			packet = new Packet(OP_SENDINGPART, nPacketSize + 24, OP_EDONKEYPROT, bFromPF);
			md4cpy(&packet->pBuffer[0], pucMD4FileHash);
			PokeUInt32(&packet->pBuffer[16], (uint32)statpos);
			PokeUInt32(&packet->pBuffer[20], (uint32)endpos);
			memfile.Read(&packet->pBuffer[24], nPacketSize);
			theStats.AddUpDataOverheadFileRequest(24);
		}
		if (thePrefs.GetDebugClientTCPLevel() > 0) {
			Debug(_T(">>> %-20hs to   %s; %s\n"), _T("OP_SendingPart"), (LPCTSTR)strDbgClientInfo, (LPCTSTR)md4str(pucMD4FileHash));
			Debug(_T("  Start=%I64u  End=%I64u  Size=%u\n"), statpos, endpos, nPacketSize);
		}
		packet->uStatsPayLoad = nPacketSize;
		rOutPacketList.AddTail(packet);
	}
}

void CUploadDiskIOThread::CreatePackedPackets(byte *pbyData, uint64 uStartOffset, uint64 uEndOffset, bool bFromPF, CPacketList &rOutPacketList, const uchar *pucMD4FileHash, const CString &strDbgClientInfo)
{
	uint32 togo = (uint32)(uEndOffset - uStartOffset);
	uLongf newsize = togo + 300;
	BYTE *output = new BYTE[newsize];
	int result = compress2(output, &newsize, pbyData, togo, 9);
	if (result != Z_OK || togo <= newsize) {
		delete[] output;
		CreateStandardPackets(pbyData, uStartOffset, uEndOffset, bFromPF, rOutPacketList, pucMD4FileHash, strDbgClientInfo);
		return;
	}
	CMemFile memfile(output, newsize);
	uint32 oldSize = togo;
	togo = newsize;
	uint32 nPacketSize = (togo > 10240u) ? togo / (togo / 10240u) : togo;

	uint32 totalPayloadSize = 0;

	while (togo) {
		if (togo < nPacketSize * 2)
			nPacketSize = togo;
		ASSERT(nPacketSize);
		togo -= nPacketSize;
		uint64 statpos = uStartOffset;
		Packet *packet;
		if (uStartOffset > UINT32_MAX || uEndOffset > UINT32_MAX) {
			packet = new Packet(OP_COMPRESSEDPART_I64, nPacketSize + 28, OP_EMULEPROT, bFromPF);
			md4cpy(&packet->pBuffer[0], pucMD4FileHash);
			PokeUInt64(&packet->pBuffer[16], statpos);
			PokeUInt32(&packet->pBuffer[24], newsize);
			memfile.Read(&packet->pBuffer[28], nPacketSize);
		} else {
			packet = new Packet(OP_COMPRESSEDPART, nPacketSize + 24, OP_EMULEPROT, bFromPF);
			md4cpy(&packet->pBuffer[0], pucMD4FileHash);
			PokeUInt32(&packet->pBuffer[16], (uint32)statpos);
			PokeUInt32(&packet->pBuffer[20], newsize);
			memfile.Read(&packet->pBuffer[24], nPacketSize);
		}

		if (thePrefs.GetDebugClientTCPLevel() > 0) {
			Debug(_T(">>> %-20hs to   %s; %s\n"), _T("OP_CompressedPart"), (LPCTSTR)strDbgClientInfo, (LPCTSTR)md4str(pucMD4FileHash));
			Debug(_T("  Start=%I64u  BlockSize=%u  Size=%u\n"), statpos, newsize, nPacketSize);
		}
		// approximate payload size
		uint32 payloadSize = nPacketSize * oldSize / newsize;

		if (togo == 0 && totalPayloadSize + payloadSize < oldSize)
			payloadSize = oldSize - totalPayloadSize;

		totalPayloadSize += payloadSize;

		theStats.AddUpDataOverheadFileRequest(24);
		packet->uStatsPayLoad = payloadSize;
		rOutPacketList.AddTail(packet);
	}
	delete[] output;
}

// not tested for the new threaded upload, if we ever use the PeerCache code again, take a look before
void CUploadDiskIOThread::CreatePeerCachePackets(byte *pbyData, uint64 uStartOffset, uint64 uEndOffset, uint64 uFilesize, bool bFromPF, CPacketList &rOutPacketList, const uchar* /*pucMD4FileHash*/, CUpDownClient *pClient)
{
	uint32 togo = (uint32)(uEndOffset - uStartOffset);
	uint32 nPacketSize = (togo > 10240) ? togo / (togo / 10240u) : togo;

	while (togo) {
		if (togo < nPacketSize * 2)
			nPacketSize = togo;
		ASSERT(nPacketSize);
		togo -= nPacketSize;

		ASSERT(uFilesize != 0);
		CSafeMemFile dataHttp(10240u);
		if (pClient->GetHttpSendState() == 0) {
			CStringA str;
			str.Format("HTTP/1.0 206\r\n"
				"Content-Range: bytes %I64u-%I64u/%I64u\r\n"
				"Content-Type: application/octet-stream\r\n"
				"Content-Length: %u\r\n"
				"Server: eMule/%S\r\n"
				"\r\n"
				, uStartOffset, uEndOffset - 1, uFilesize
				, (unsigned)(uEndOffset - uStartOffset)
				, (LPCTSTR)theApp.m_strCurVersionLong);

			dataHttp.Write((LPCSTR)str, str.GetLength());
			theStats.AddUpDataOverheadFileRequest((uint32)dataHttp.GetLength());

			pClient->SetHttpSendState(1);
			/*if (thePrefs.GetDebugClientTCPLevel() > 0){
			DebugSend("PeerCache-HTTP", this, pucMD4FileHash);
			Debug(_T("  %hs\n"), str);
			}*/
		}
		dataHttp.Write(pbyData, nPacketSize);
		pbyData += nPacketSize;

		/*if (thePrefs.GetDebugClientTCPLevel() > 1){
		DebugSend("PeerCache-HTTP data", this, GetUploadFileID());
		Debug(_T("  Start=%I64u  End=%I64u  Size=%u\n"), statpos, endpos, nPacketSize);
		}*/

		UINT uRawPacketSize = (UINT)dataHttp.GetLength();
		LPBYTE pRawPacketData = dataHttp.Detach();
		CRawPacket *packet = new CRawPacket((char*)pRawPacketData, uRawPacketSize, bFromPF);
		packet->uStatsPayLoad = nPacketSize;
		rOutPacketList.AddTail(packet);
		free(pRawPacketData);
	}
}

void CUploadDiskIOThread::NewBlockRequestsAvailable()
{
	if (m_bRun)
		m_eventNewBlockRequests.SetEvent();
}

void CUploadDiskIOThread::SocketNeedsMoreData()
{
	if (m_bRun)
		m_eventSocketNeedsData.SetEvent();
}