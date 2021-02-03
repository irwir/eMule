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
#include "emule.h"
#include "UpDownClient.h"
#include "UrlClient.h"
#include "Opcodes.h"
#include "Packets.h"
#include "UploadQueue.h"
#include "Statistics.h"
#include "ClientList.h"
#include "ClientUDPSocket.h"
#include "SharedFileList.h"
#include "KnownFileList.h"
#include "PartFile.h"
#include "ClientCredits.h"
#include "ListenSocket.h"
#include "PeerCacheSocket.h"
#include "ServerConnect.h"
#include "OtherFunctions.h"
#include "SafeFile.h"
#include "DownloadQueue.h"
#include "emuledlg.h"
#include "TransferDlg.h"
#include "Log.h"
#include "Collection.h"
#include "UploadDiskIOThread.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//	members of CUpDownClient
//	which are mainly used for uploading functions

CBarShader CUpDownClient::s_UpStatusBar(16);

void CUpDownClient::DrawUpStatusBar(CDC *dc, const CRect &rect, bool onlygreyrect, bool  bFlat) const
{
	COLORREF crNeither;
	COLORREF crNextSending;
	COLORREF crBoth;
	COLORREF crSending;

	if (GetSlotNumber() <= (UINT)theApp.uploadqueue->GetActiveUploadsCount()
		|| (GetUploadState() != US_UPLOADING && GetUploadState() != US_CONNECTING))
	{
		crNeither = RGB(224, 224, 224);
		crNextSending = RGB(255, 208, 0);
		crBoth = bFlat ? RGB(0, 0, 0) : RGB(104, 104, 104);
		crSending = RGB(0, 150, 0);
	} else {
		// grayed out
		crNeither = RGB(248, 248, 248);
		crNextSending = RGB(255, 244, 191);
		crBoth = /*bFlat ? RGB(191, 191, 191) :*/ RGB(191, 191, 191);
		crSending = RGB(191, 229, 191);
	}

	// wistily: UpStatusFix
	CKnownFile *currequpfile = theApp.sharedfiles->GetFileByID(requpfileid);
	EMFileSize filesize = currequpfile ? currequpfile->GetFileSize() : PARTSIZE * m_nUpPartCount;
	// wistily: UpStatusFix

	if (filesize > 0ull) {
		s_UpStatusBar.SetFileSize(filesize);
		s_UpStatusBar.SetHeight(rect.Height());
		s_UpStatusBar.SetWidth(rect.Width());
		s_UpStatusBar.Fill(crNeither);
		if (!onlygreyrect && m_abyUpPartStatus)
			for (UINT i = 0; i < m_nUpPartCount; ++i)
				if (m_abyUpPartStatus[i])
					s_UpStatusBar.FillRange(i * PARTSIZE, i * PARTSIZE + PARTSIZE - 1, crBoth);

		UploadingToClient_Struct *pUpClientStruct = theApp.uploadqueue->GetUploadingClientStructByClient(this);
//		ASSERT( pUpClientStruct != NULL || theApp.uploadqueue->IsOnUploadQueue((CUpDownClient*)this) != NULL);
		if (pUpClientStruct != NULL) {
			CSingleLock lockBlockLists(&pUpClientStruct->m_csBlockListsLock, TRUE);
			ASSERT(lockBlockLists.IsLocked());
			const Requested_Block_Struct *block;
			if (!pUpClientStruct->m_BlockRequests_queue.IsEmpty()) {
				block = pUpClientStruct->m_BlockRequests_queue.GetHead();
				if (block) {
					uint32 start = (uint32)(block->StartOffset / PARTSIZE);
					s_UpStatusBar.FillRange(start * PARTSIZE, start * PARTSIZE + PARTSIZE - 1, crNextSending);
				}
			}
			if (!pUpClientStruct->m_DoneBlocks_list.IsEmpty()) {
				block = pUpClientStruct->m_DoneBlocks_list.GetHead();
				if (block) {
					uint32 start = (uint32)(block->StartOffset / PARTSIZE);
					s_UpStatusBar.FillRange(start * PARTSIZE, start * PARTSIZE + PARTSIZE - 1, crNextSending);
				}
				for (POSITION pos = pUpClientStruct->m_DoneBlocks_list.GetHeadPosition();pos != 0;) {
					block = pUpClientStruct->m_DoneBlocks_list.GetNext(pos);
					s_UpStatusBar.FillRange(block->StartOffset, block->EndOffset + 1, crSending);
				}
			}
			lockBlockLists.Unlock();
		}
		s_UpStatusBar.Draw(dc, rect.left, rect.top, bFlat);
	}
}

void CUpDownClient::SetUploadState(EUploadState eNewState)
{
	if (eNewState != m_eUploadState) {
		if (m_eUploadState == US_UPLOADING) {
			// Reset upload data rate computation
			m_nUpDatarate = 0;
			m_nSumForAvgUpDataRate = 0;
			m_AverageUDR_list.RemoveAll();
		}
		if (eNewState == US_UPLOADING)
			m_fSentOutOfPartReqs = 0;

		// don't add any final cleanups for US_NONE here
		m_eUploadState = eNewState;
		theApp.emuledlg->transferwnd->GetClientList()->RefreshClient(this);
	}
}

/**
 * Gets the queue score multiplier for this client, taking into consideration client's credits
 * and the requested file's priority.
 */
float CUpDownClient::GetCombinedFilePrioAndCredit()
{
	if (credits == 0) {
		ASSERT(IsKindOf(RUNTIME_CLASS(CUrlClient)));
		return 0.0F;
	}

	return 10.0f * credits->GetScoreRatio(GetIP()) * GetFilePrioAsNumber();
}

/**
 * Gets the file multiplier for the file this client has requested.
 */
int CUpDownClient::GetFilePrioAsNumber() const
{
	const CKnownFile *currequpfile = theApp.sharedfiles->GetFileByID(requpfileid);
	if (!currequpfile)
		return 0;

	// TODO coded by tecxx & herbert, one yet unsolved problem here:
	// sometimes a client asks for 2 files and there is no way to decide, which file the
	// client finally gets. so it could happen that he is queued first because of a
	// high prio file, but then asks for something completely different.
	int filepriority = 10; // standard
	switch (currequpfile->GetUpPriority()) {
	case PR_VERYHIGH:
		filepriority = 18;
		break;
	case PR_HIGH:
		filepriority = 9;
		break;
	case PR_LOW:
		filepriority = 6;
		break;
	case PR_VERYLOW:
		filepriority = 2;
		break;
	case PR_NORMAL:
	default:
		filepriority = 7;
		break;
	}
	return filepriority;
}

/**
 * Gets the current waiting score for this client, taking into consideration waiting
 * time, priority of requested file, and the client's credits.
 */
uint32 CUpDownClient::GetScore(bool sysvalue, bool isdownloading, bool onlybasevalue) const
{
	if (!m_pszUsername)
		return 0;

	if (!credits) {
		ASSERT(IsKindOf(RUNTIME_CLASS(CUrlClient)));
		return 0;
	}

	if (!theApp.sharedfiles->GetFileByID(requpfileid)) //is any file requested?
		return 0;

	// bad clients (see note in function)
	if (credits->GetCurrentIdentState(GetIP()) == IS_IDBADGUY)
		return 0;
	// friend slot
	if (IsFriend() && GetFriendSlot() && !HasLowID())
		return 0x0FFFFFFFu;

	if (IsBanned() || m_bGPLEvildoer)
		return 0;

	if (sysvalue && HasLowID() && !(socket && socket->IsConnected()))
		return 0;

	int filepriority = GetFilePrioAsNumber();

	// calculate score, based on waitingtime and other factors
	float fBaseValue;
	if (onlybasevalue)
		fBaseValue = 100;
	else if (!isdownloading)
		fBaseValue = (::GetTickCount() - GetWaitStartTime()) / SEC2MS(1.0f);
	else {
		// we don't want one client to download forever
		// the first 15 min download time counts as 15 min waiting time and you get a 15 min bonus
		// while you are in the first 15 min :)
		// (to avoid 20 sec downloads) after this the score won't raise any more
		fBaseValue = (float)(m_dwUploadTime - GetWaitStartTime());
		//ASSERT ( m_dwUploadTime-GetWaitStartTime() >= 0 ); //oct 28, 02: changed this from "> 0" to ">= 0" -> // 02-Okt-2006 []: ">=0" is always true!
		fBaseValue += MIN2MS(::GetTickCount() >= m_dwUploadTime + MIN2MS(15) ? 15.0f : 30.0f);
		fBaseValue /= SEC2MS(1);
	}
	if (thePrefs.UseCreditSystem()) {
		float modif = credits->GetScoreRatio(GetIP());
		fBaseValue *= modif;
	}
	if (!onlybasevalue)
		fBaseValue *= filepriority / 10.0f;

	if ((IsEmuleClient() || GetClientSoft() < 10) && m_byEmuleVersion <= 0x19)
		fBaseValue *= 0.5f;
	return (uint32)fBaseValue;
}

bool CUpDownClient::ProcessExtendedInfo(CSafeMemFile *data, CKnownFile *tempreqfile)
{
	delete[] m_abyUpPartStatus;
	m_abyUpPartStatus = NULL;
	m_nUpPartCount = 0;
	m_nUpCompleteSourcesCount = 0;
	if (GetExtendedRequestsVersion() == 0)
		return true;

	uint16 nED2KUpPartCount = data->ReadUInt16();
	if (!nED2KUpPartCount) {
		m_nUpPartCount = tempreqfile->GetPartCount();
		m_abyUpPartStatus = new uint8[m_nUpPartCount]();
	} else {
		if (tempreqfile->GetED2KPartCount() != nED2KUpPartCount) {
			//We already checked if we are talking about the same file. So if we get here, something really strange happened!
			m_nUpPartCount = 0;
			return false;
		}
		m_nUpPartCount = tempreqfile->GetPartCount();
		m_abyUpPartStatus = new uint8[m_nUpPartCount];
		for (uint16 done = 0; done != m_nUpPartCount;) {
			uint8 toread = data->ReadUInt8();
			for (UINT i = 0; i != 8; ++i) {
				m_abyUpPartStatus[done] = (toread >> i) & 1;
//				We may want to use this for another feature.
//				if (m_abyUpPartStatus[done] && !tempreqfile->IsComplete(done * PARTSIZE, done * PARTSIZE + PARTSIZE - 1))
//					bPartsNeeded = true;
				if (++done == m_nUpPartCount)
					break;
			}
		}
	}
	if (GetExtendedRequestsVersion() > 1) {
		uint16 nCompleteCountLast = GetUpCompleteSourcesCount();
		uint16 nCompleteCountNew = data->ReadUInt16();
		SetUpCompleteSourcesCount(nCompleteCountNew);
		if (nCompleteCountLast != nCompleteCountNew)
			tempreqfile->UpdatePartsInfo();
	}
	theApp.emuledlg->transferwnd->GetQueueList()->RefreshClient(this);
	return true;
}

void CUpDownClient::SetUploadFileID(CKnownFile *newreqfile)
{
	CKnownFile *oldreqfile = theApp.downloadqueue->GetFileByID(requpfileid);
	//We use the knownfile list because we may have unshared the file.
	//But we always check the download list first because that person may have decided to re-download that file.
	//Which will replace the object in the knownfile list if completed.
	if (oldreqfile == NULL)
		oldreqfile = theApp.knownfiles->FindKnownFileByID(requpfileid);
	else {
		// In some _very_ rare cases it is possible that we have different files with the same hash in the downloads list
		// as well as in the shared list (re-downloading an unshared file, then re-sharing it before the first part has been downloaded)
		// to make sure that in no case a deleted client object is left on the list, we need to double-check
		// TODO: Fix the whole issue properly
		CKnownFile *pCheck = theApp.sharedfiles->GetFileByID(requpfileid);
		if (pCheck != NULL && pCheck != oldreqfile) {
			ASSERT(0);
			pCheck->RemoveUploadingClient(this);
		}
	}

	if (newreqfile == oldreqfile)
		return;

	// clear old status
	delete[] m_abyUpPartStatus;
	m_abyUpPartStatus = NULL;
	m_nUpPartCount = 0;
	m_nUpCompleteSourcesCount = 0;

	if (newreqfile) {
		newreqfile->AddUploadingClient(this);
		md4cpy(requpfileid, newreqfile->GetFileHash());
	} else
		md4clr(requpfileid);

	if (oldreqfile)
		oldreqfile->RemoveUploadingClient(this);
}

static INT_PTR dbgLastQueueCount = 0;
void CUpDownClient::AddReqBlock(Requested_Block_Struct *reqblock, bool bSignalIOThread)
{
	// do _all_ sanity checks on the requested block here, than put it on the block list for the client
	// UploadDiskIPThread will handle those later on

	if (reqblock != NULL) {
		if (GetUploadState() != US_UPLOADING) {
			if (thePrefs.GetLogUlDlEvents())
				AddDebugLogLine(DLP_LOW, false, _T("UploadClient: Client tried to add req block when not in upload slot! Prevented req blocks from being added. %s"), (LPCTSTR)DbgGetClientInfo());
			delete reqblock;
			return;
		}

		if (HasCollectionUploadSlot()) {
			CKnownFile *pDownloadingFile = theApp.sharedfiles->GetFileByID(reqblock->FileID);
			if (pDownloadingFile != NULL) {
				if (!(CCollection::HasCollectionExtention(pDownloadingFile->GetFileName()) && pDownloadingFile->GetFileSize() < (uint64)MAXPRIORITYCOLL_SIZE)) {
					AddDebugLogLine(DLP_HIGH, false, _T("UploadClient: Client tried to add req block for non collection while having a collection slot! Prevented req blocks from being added. %s"), (LPCTSTR)DbgGetClientInfo());
					delete reqblock;
					return;
				}
			} else
				ASSERT(0);
		}

		CKnownFile *srcfile = theApp.sharedfiles->GetFileByID(reqblock->FileID);
		if (srcfile == NULL) {
			DebugLogWarning(GetResString(IDS_ERR_REQ_FNF));
			delete reqblock;
			return;
		}

		UploadingToClient_Struct *pUploadingClientStruct = theApp.uploadqueue->GetUploadingClientStructByClient(this);
		if (pUploadingClientStruct == NULL) {
			DebugLogError(_T("AddReqBlock: Uploading client not found in Uploadlist, %s, %s"), (LPCTSTR)DbgGetClientInfo(), (LPCTSTR)srcfile->GetFileName());
			delete reqblock;
			return;
		}

		if (pUploadingClientStruct->m_bIOError) {
			DebugLogWarning(_T("AddReqBlock: Uploading client has pending IO Error, %s, %s"), (LPCTSTR)DbgGetClientInfo(), (LPCTSTR)srcfile->GetFileName());
			delete reqblock;
			return;
		}

		if (srcfile->IsPartFile() && !static_cast<CPartFile*>(srcfile)->IsComplete(reqblock->StartOffset, reqblock->EndOffset - 1, true)) {
			DebugLogWarning(_T("AddReqBlock: %s, %s"), (LPCTSTR)GetResString(IDS_ERR_INCOMPLETEBLOCK), (LPCTSTR)DbgGetClientInfo(), (LPCTSTR)srcfile->GetFileName());
			delete reqblock;
			return;
		}

		if (reqblock->StartOffset >= reqblock->EndOffset || reqblock->EndOffset > srcfile->GetFileSize()) {
			DebugLogError(_T("AddReqBlock: Invalid Blockrequests (negative or bytes to read, read after EOF), %s, %s"), (LPCTSTR)DbgGetClientInfo(), (LPCTSTR)srcfile->GetFileName());
			delete reqblock;
			return;
		}

		if (reqblock->EndOffset - reqblock->StartOffset > EMBLOCKSIZE * 3) {
			DebugLogWarning(_T("AddReqBlock: %s, %s"), (LPCTSTR)GetResString(IDS_ERR_LARGEREQBLOCK), (LPCTSTR)DbgGetClientInfo(), (LPCTSTR)srcfile->GetFileName());
			delete reqblock;
			return;
		}

		CSingleLock lockBlockLists(&pUploadingClientStruct->m_csBlockListsLock, TRUE);
		if (!lockBlockLists.IsLocked()) {
			ASSERT(0);
			delete reqblock;
			return;
		}

		for (POSITION pos = pUploadingClientStruct->m_DoneBlocks_list.GetHeadPosition(); pos != NULL;) {
			const Requested_Block_Struct *cur_reqblock = pUploadingClientStruct->m_DoneBlocks_list.GetNext(pos);
			if (reqblock->StartOffset == cur_reqblock->StartOffset
				&& reqblock->EndOffset == cur_reqblock->EndOffset
				&& md4equ(reqblock->FileID, cur_reqblock->FileID))
			{
				delete reqblock;
				return;
			}
		}
		for (POSITION pos = pUploadingClientStruct->m_BlockRequests_queue.GetHeadPosition(); pos != NULL;) {
			const Requested_Block_Struct *cur_reqblock = pUploadingClientStruct->m_BlockRequests_queue.GetNext(pos);
			if (reqblock->StartOffset == cur_reqblock->StartOffset
				&& reqblock->EndOffset == cur_reqblock->EndOffset
				&& md4equ(reqblock->FileID, cur_reqblock->FileID))
			{
				delete reqblock;
				return;
			}
		}
		pUploadingClientStruct->m_BlockRequests_queue.AddTail(reqblock);
		dbgLastQueueCount = pUploadingClientStruct->m_BlockRequests_queue.GetCount();
		lockBlockLists.Unlock(); // not needed, just to make it visible
	}
	if (bSignalIOThread && theApp.m_pUploadDiskIOThread != NULL) {
		/*DebugLog(_T("BlockRequest Packet received, we have currently %u waiting requests and %s data in buffer (%u in ready packets, %s in pending IO Disk read), socket busy: %s"), dbgLastQueueCount
			, (LPCTSTR)CastItoXBytes(GetQueueSessionUploadAdded() - (GetQueueSessionPayloadUp() + socket->GetSentPayloadSinceLastCall(false)), false, false, 2)
			, socket->DbgGetStdQueueCount(), (LPCTSTR)CastItoXBytes((uint32)theApp.m_pUploadDiskIOThread->dbgDataReadPending, false, false, 2)
			,_T('?')); */
		theApp.m_pUploadDiskIOThread->NewBlockRequestsAvailable();
	}
}

uint32 CUpDownClient::UpdateUploadingStatisticsData()
{
	DWORD curTick = ::GetTickCount();

	uint64 sentBytesCompleteFile = 0;
	uint64 sentBytesPartFile = 0;

	if (GetFileUploadSocket() && (m_ePeerCacheUpState != PCUS_WAIT_CACHE_REPLY)) {
		CEMSocket *s = GetFileUploadSocket();
		UINT uUpStatsPort;
		if (m_pPCUpSocket && IsUploadingToPeerCache()) {
			uUpStatsPort = UINT_MAX;

			// Check if filedata has been sent via the normal socket since the last call.
			uint64 sentBytesCompleteFileNormalSocket = socket->GetSentBytesCompleteFileSinceLastCallAndReset();
			uint64 sentBytesPartFileNormalSocket = socket->GetSentBytesPartFileSinceLastCallAndReset();

			if (thePrefs.GetVerbose() && (sentBytesCompleteFileNormalSocket + sentBytesPartFileNormalSocket > 0)) {
				AddDebugLogLine(false, _T("Sent file data via normal socket when in PC mode. Bytes: %I64i."), sentBytesCompleteFileNormalSocket + sentBytesPartFileNormalSocket);
			}
		} else
			uUpStatsPort = GetUserPort();

		// Extended statistics information based on which client software and which port we sent this data to...
		// This also updates the grand total for sent bytes, etc.  And where this data came from.
		sentBytesCompleteFile = s->GetSentBytesCompleteFileSinceLastCallAndReset();
		sentBytesPartFile = s->GetSentBytesPartFileSinceLastCallAndReset();
		thePrefs.Add2SessionTransferData(GetClientSoft(), uUpStatsPort, false, true, (UINT)sentBytesCompleteFile, (IsFriend() && GetFriendSlot()));
		thePrefs.Add2SessionTransferData(GetClientSoft(), uUpStatsPort, true, true, (UINT)sentBytesPartFile, (IsFriend() && GetFriendSlot()));

		m_nTransferredUp = (UINT)(m_nTransferredUp + sentBytesCompleteFile + sentBytesPartFile);
		credits->AddUploaded((uint32)(sentBytesCompleteFile + sentBytesPartFile), GetIP());

		uint64 sentBytesPayload = s->GetSentPayloadSinceLastCall(true);
		m_nCurQueueSessionPayloadUp = (UINT)(m_nCurQueueSessionPayloadUp + sentBytesPayload);

		// on some rare cases (namely switching upload files while still data is in the send queue),
		// we count some bytes for the wrong file, but fixing it (and not counting data only based on
		// what was put into the queue and not sent yet) isn't really worth it
		CKnownFile *pCurrentUploadFile = theApp.sharedfiles->GetFileByID(GetUploadFileID());
		if (pCurrentUploadFile != NULL)
			pCurrentUploadFile->statistic.AddTransferred(sentBytesPayload);
//		else
//			ASSERT( false ); //fired after deleting shared files which had uploads in the current eMule run. Closing messagebox did not cause any problems.

		// increase the sockets buffer on fast uploads. Even though this check should rather be
		// in the throttler thread, its better to do it here because we can access the client's
		// download rate and the throttler can't
		if (GetDatarate() > 100 * 1024)
			s->UseBigSendBuffer();
	}

	if (sentBytesCompleteFile + sentBytesPartFile > 0 ||
		m_AverageUDR_list.IsEmpty() || curTick >= m_AverageUDR_list.GetTail().timestamp + SEC2MS(1)) {
		// Store how much data we've transferred this round,
		// to be able to calculate average speed later
		// keep sum of all values in list up to date
		TransferredData newitem = {(UINT)(sentBytesCompleteFile + sentBytesPartFile), curTick};
		m_AverageUDR_list.AddTail(newitem);
		m_nSumForAvgUpDataRate = (UINT)(m_nSumForAvgUpDataRate + sentBytesCompleteFile + sentBytesPartFile);
	}

	// remove to old values in list
	while (!m_AverageUDR_list.IsEmpty() && curTick >= m_AverageUDR_list.GetHead().timestamp + SEC2MS(10)) {
		// keep sum of all values in list up to date
		m_nSumForAvgUpDataRate -= m_AverageUDR_list.RemoveHead().datalen;
	}

	// Calculate average speed for this slot
	if (!m_AverageUDR_list.IsEmpty() && curTick > m_AverageUDR_list.GetHead().timestamp && GetUpStartTimeDelay() > SEC2MS(2))
		m_nUpDatarate = (UINT)(SEC2MS((uint64)m_nSumForAvgUpDataRate) / (curTick - m_AverageUDR_list.GetHead().timestamp));
	else
		// not enough values to calculate trustworthy speed
		m_nUpDatarate = 0;

	// Check if it's time to update the display.
	if (curTick >= m_lastRefreshedULDisplay + MINWAIT_BEFORE_ULDISPLAY_WINDOWUPDATE + (uint32)(rand() * 800 / RAND_MAX)) {
		// Update display
		theApp.emuledlg->transferwnd->GetUploadList()->RefreshClient(this);
		theApp.emuledlg->transferwnd->GetClientList()->RefreshClient(this);
		m_lastRefreshedULDisplay = curTick;
	}

	return (uint32)(sentBytesCompleteFile + sentBytesPartFile);
}

void CUpDownClient::SendOutOfPartReqsAndAddToWaitingQueue()
{
	//OP_OUTOFPARTREQS will tell the downloading client to go back to OnQueue.
	//The main reason for this is that if we put the client back on queue and it goes
	//back to the upload before the socket times out... We get a situation where the
	//downloader thinks it already sent the requested blocks and the uploader thinks
	//the downloader didn't send any block requests. Then the connection times out.
	//I did some tests with eDonkey also and it seems to work well with them also.
	if (thePrefs.GetDebugClientTCPLevel() > 0)
		DebugSend("OP_OutOfPartReqs", this);
	Packet *pPacket = new Packet(OP_OUTOFPARTREQS, 0);
	theStats.AddUpDataOverheadFileRequest(pPacket->size);
	SendPacket(pPacket);
	m_fSentOutOfPartReqs = 1;
	theApp.uploadqueue->AddClientToQueue(this, true);
}

/**
 * See description for CEMSocket::TruncateQueues().
 */
void CUpDownClient::FlushSendBlocks() // call this when you stop upload, or the socket might be not able to send
{
	if (socket) //socket may be NULL...
		socket->TruncateQueues();
}

void CUpDownClient::SendHashsetPacket(const uchar *pData, uint32 nSize, bool bFileIdentifiers)
{
	Packet *packet;
	CSafeMemFile fileResponse(1024);
	if (bFileIdentifiers) {
		CSafeMemFile data(pData, nSize);
		CFileIdentifierSA fileIdent;
		if (!fileIdent.ReadIdentifier(&data))
			throw _T("Bad FileIdentifier (OP_HASHSETREQUEST2)");
		CKnownFile *file = theApp.sharedfiles->GetFileByIdentifier(fileIdent, false);
		if (file == NULL) {
			CheckFailedFileIdReqs(fileIdent.GetMD4Hash());
			throw GetResString(IDS_ERR_REQ_FNF) + _T(" (SendHashsetPacket2)");
		}
		uint8 byOptions = data.ReadUInt8();
		bool bMD4 = (byOptions & 0x01) > 0;
		bool bAICH = (byOptions & 0x02) > 0;
		if (!bMD4 && !bAICH) {
			DebugLogWarning(_T("Client sent HashSet request with none or unknown HashSet type requested (%u) - file: %s, client %s")
				, byOptions, (LPCTSTR)file->GetFileName(), (LPCTSTR)DbgGetClientInfo());
			return;
		}
		const CFileIdentifier &fileid = file->GetFileIdentifier();
		fileid.WriteIdentifier(&fileResponse);
		// even if we don't happen to have an AICH hashset yet for some reason we send a proper (possibly empty) response
		fileid.WriteHashSetsToPacket(&fileResponse, bMD4, bAICH);
		if (thePrefs.GetDebugClientTCPLevel() > 0)
			DebugSend("OP_HashSetAnswer", this, fileid.GetMD4Hash());
		packet = new Packet(&fileResponse, OP_EMULEPROT, OP_HASHSETANSWER2);
	} else {
		if (nSize != 16) {
			ASSERT(0);
			return;
		}
		CKnownFile *file = theApp.sharedfiles->GetFileByID(pData);
		if (!file) {
			CheckFailedFileIdReqs(pData);
			throw GetResString(IDS_ERR_REQ_FNF) + _T(" (SendHashsetPacket)");
		}
		file->GetFileIdentifier().WriteMD4HashsetToFile(&fileResponse);
		if (thePrefs.GetDebugClientTCPLevel() > 0)
			DebugSend("OP_HashSetAnswer", this, pData);
		packet = new Packet(&fileResponse, OP_EDONKEYPROT, OP_HASHSETANSWER);
	}
	theStats.AddUpDataOverheadFileRequest(packet->size);
	SendPacket(packet);
}

void CUpDownClient::SendRankingInfo()
{
	if (!ExtProtocolAvailable())
		return;
	UINT nRank = theApp.uploadqueue->GetWaitingPosition(this);
	if (!nRank)
		return;
	Packet *packet = new Packet(OP_QUEUERANKING, 12, OP_EMULEPROT);
	PokeUInt16(packet->pBuffer, (uint16)nRank);
	memset(packet->pBuffer + 2, 0, 10);
	if (thePrefs.GetDebugClientTCPLevel() > 0)
		DebugSend("OP_QueueRank", this);
	theStats.AddUpDataOverheadFileRequest(packet->size);
	SendPacket(packet);
}

void CUpDownClient::SendCommentInfo(/*const */CKnownFile *file)
{
	if (!m_bCommentDirty || file == NULL || !ExtProtocolAvailable() || m_byAcceptCommentVer < 1)
		return;
	m_bCommentDirty = false;

	UINT rating = file->GetFileRating();
	const CString &desc = file->GetFileComment();
	if (rating == 0 && desc.IsEmpty())
		return;

	CSafeMemFile data(256);
	data.WriteUInt8((uint8)rating);
	data.WriteLongString(desc, GetUnicodeSupport());
	if (thePrefs.GetDebugClientTCPLevel() > 0)
		DebugSend("OP_FileDesc", this, file->GetFileHash());
	Packet *packet = new Packet(&data, OP_EMULEPROT);
	packet->opcode = OP_FILEDESC;
	theStats.AddUpDataOverheadFileRequest(packet->size);
	SendPacket(packet);
}

void CUpDownClient::AddRequestCount(const uchar *fileid)
{
	for (POSITION pos = m_RequestedFiles_list.GetHeadPosition(); pos != NULL;) {
		Requested_File_Struct *cur_struct = m_RequestedFiles_list.GetNext(pos);
		if (md4equ(cur_struct->fileid, fileid)) {
			if (::GetTickCount() < cur_struct->lastasked + MIN_REQUESTTIME && !GetFriendSlot()) {
				if (GetDownloadState() != DS_DOWNLOADING)
					cur_struct->badrequests++;
				if (cur_struct->badrequests == BADCLIENTBAN)
					Ban();
			} else {
				if (cur_struct->badrequests)
					cur_struct->badrequests--;
			}
			cur_struct->lastasked = ::GetTickCount();
			return;
		}
	}
	Requested_File_Struct *new_struct = new Requested_File_Struct;
	md4cpy(new_struct->fileid, fileid);
	new_struct->lastasked = ::GetTickCount();
	new_struct->badrequests = 0;
	m_RequestedFiles_list.AddHead(new_struct);
}

void  CUpDownClient::UnBan()
{
	theApp.clientlist->AddTrackClient(this);
	theApp.clientlist->RemoveBannedClient(GetIP());
	SetUploadState(US_NONE);
	ClearWaitStartTime();
	theApp.emuledlg->transferwnd->ShowQueueCount(theApp.uploadqueue->GetWaitingUserCount());
	for (POSITION pos = m_RequestedFiles_list.GetHeadPosition(); pos != NULL;) {
		Requested_File_Struct *cur_struct = m_RequestedFiles_list.GetNext(pos);
		cur_struct->badrequests = 0;
		cur_struct->lastasked = 0;
	}
}

void CUpDownClient::Ban(LPCTSTR pszReason)
{
	SetChatState(MS_NONE);
	theApp.clientlist->AddTrackClient(this);
	if (!IsBanned()) {
		if (thePrefs.GetLogBannedClients())
			AddDebugLogLine(false, _T("Banned: %s; %s"), pszReason == NULL ? _T("Aggressive behaviour") : pszReason, (LPCTSTR)DbgGetClientInfo());
	}
#ifdef _DEBUG
	else {
		if (thePrefs.GetLogBannedClients())
			AddDebugLogLine(false, _T("Banned: (refreshed): %s; %s"), pszReason == NULL ? _T("Aggressive behaviour") : pszReason, (LPCTSTR)DbgGetClientInfo());
	}
#endif
	theApp.clientlist->AddBannedClient(GetIP());
	SetUploadState(US_BANNED);
	theApp.emuledlg->transferwnd->ShowQueueCount(theApp.uploadqueue->GetWaitingUserCount());
	theApp.emuledlg->transferwnd->GetQueueList()->RefreshClient(this);
	if (socket != NULL && socket->IsConnected())
		socket->ShutDown(CAsyncSocket::receives); // let the socket timeout, since we don't want to risk to delete the client right now. This isn't actually perfect, could be changed later
}

uint32 CUpDownClient::GetWaitStartTime() const
{
	if (credits == NULL) {
		ASSERT(0);
		return 0;
	}
	uint32 dwResult = credits->GetSecureWaitStartTime(GetIP());
	if (dwResult > m_dwUploadTime && IsDownloading()) {
		//this happens only if two clients with invalid securehash are in the queue - if at all
		dwResult = m_dwUploadTime - 1;

		if (thePrefs.GetVerbose())
			DEBUG_ONLY(AddDebugLogLine(false, _T("Warning: CUpDownClient::GetWaitStartTime() waittime Collision (%s)"), GetUserName()));
	}
	return dwResult;
}

void CUpDownClient::SetWaitStartTime()
{
	if (credits != NULL)
		credits->SetSecWaitStartTime(GetIP());
}

void CUpDownClient::ClearWaitStartTime()
{
	if (credits != NULL)
		credits->ClearWaitStartTime();
}

bool CUpDownClient::GetFriendSlot() const
{
	if (credits && theApp.clientcredits->CryptoAvailable())
		switch (credits->GetCurrentIdentState(GetIP())) {
		case IS_IDFAILED:
		case IS_IDNEEDED:
		case IS_IDBADGUY:
			return false;
		}

	return m_bFriendSlot;
}

CEMSocket* CUpDownClient::GetFileUploadSocket(bool bLog)
{
	if (m_pPCUpSocket && (IsUploadingToPeerCache() || m_ePeerCacheUpState == PCUS_WAIT_CACHE_REPLY)) {
		if (bLog && thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("%s got peercache socket."), (LPCTSTR)DbgGetClientInfo());
		return m_pPCUpSocket;
	}
	if (bLog && thePrefs.GetVerbose())
		AddDebugLogLine(false, _T("%s got normal socket."), (LPCTSTR)DbgGetClientInfo());
	return socket;
}

void CUpDownClient::SetCollectionUploadSlot(bool bValue)
{
	ASSERT(!IsDownloading() || bValue == m_bCollectionUploadSlot);
	m_bCollectionUploadSlot = bValue;
}