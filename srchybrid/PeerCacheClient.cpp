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
#include <wininet.h>
#include "emule.h"
#include "UrlClient.h"
#include "PartFile.h"
#include "SafeFile.h"
#include "Statistics.h"
#include "Packets.h"
#include "ListenSocket.h"
#include "Preferences.h"
#include "OtherFunctions.h"
#include "SharedFileList.h"
#include "PeerCacheSocket.h"
#include "UploadBandwidthThrottler.h"
#include "PeerCacheFinder.h"
#include "UploadQueue.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define HTTP_STATUS_INV_RANGE	416

DWORD GetPeerCacheSocketUploadTimeout()
{
	return DOWNLOADTIMEOUT + SEC2MS(20 + 30);
}

DWORD GetPeerCacheSocketDownloadTimeout()
{
	return DOWNLOADTIMEOUT + SEC2MS(20);	// must be lower than Upload timeout
}


///////////////////////////////////////////////////////////////////////////////
// CPeerCacheSocket

IMPLEMENT_DYNCREATE(CPeerCacheSocket, CHttpClientReqSocket)

CPeerCacheSocket::CPeerCacheSocket(CUpDownClient *pClient)
{
	ASSERT(client == NULL);
	client = NULL;
	m_client = pClient;
}

CPeerCacheSocket::~CPeerCacheSocket()
{
	CPeerCacheSocket::DetachFromClient();
}

void CPeerCacheSocket::DetachFromClient()
{
	if (GetClient()) {
		// fail safe, should never be needed
		if (GetClient()->m_pPCDownSocket == this) {
			ASSERT(0);
			GetClient()->m_pPCDownSocket = NULL;
		}
		if (GetClient()->m_pPCUpSocket == this) {
			ASSERT(0);
			GetClient()->m_pPCUpSocket = NULL;
		}
	}
}

void CPeerCacheSocket::Safe_Delete()
{
	DetachFromClient();
	CClientReqSocket::Safe_Delete();
	m_client = NULL;
	ASSERT(GetClient() == NULL);
	ASSERT(client == NULL);
}

void CPeerCacheSocket::OnSend(int nErrorCode)
{
//	Debug("%08x %hs\n", this, __FUNCTION__);
	// PC-TODO: We have to keep the ed2k connection of a client as long active as we are using
	// the associated PeerCache connection -> Update the timeout of the ed2k socket.
	if (nErrorCode == 0 && GetClient() && GetClient()->socket)
		GetClient()->socket->ResetTimeOutTimer();
	CHttpClientReqSocket::OnSend(nErrorCode);
}

void CPeerCacheSocket::OnReceive(int nErrorCode)
{
//	Debug("%08x %hs\n", this, __FUNCTION__);
	// PC-TODO: We have to keep the ed2k connection of a client as long active as we are using
	// the associated PeerCache connection -> Update the timeout of the ed2k socket.
	if (nErrorCode == 0 && GetClient() && GetClient()->socket)
		GetClient()->socket->ResetTimeOutTimer();
	CHttpClientReqSocket::OnReceive(nErrorCode);
}

void CPeerCacheSocket::OnError(int nErrorCode)
{
	DEBUG_ONLY(Debug(_T("%08x %hs\n"), this, __FUNCTION__));
	CHttpClientReqSocket::OnError(nErrorCode);
}

bool CPeerCacheSocket::ProcessHttpResponse()
{
	ASSERT(0);
	return false;
}

bool CPeerCacheSocket::ProcessHttpResponseBody(const BYTE* /*pucData*/, UINT /*size*/)
{
	ASSERT(0);
	return false;
}

bool CPeerCacheSocket::ProcessHttpRequest()
{
	ASSERT(0);
	return false;
}


///////////////////////////////////////////////////////////////////////////////
// CPeerCacheDownSocket

IMPLEMENT_DYNCREATE(CPeerCacheDownSocket, CPeerCacheSocket)

CPeerCacheDownSocket::CPeerCacheDownSocket(CUpDownClient *pClient)
	: CPeerCacheSocket(pClient)
{
	DEBUG_ONLY(Debug(_T("%08x %hs\n"), this, __FUNCTION__));
}

CPeerCacheDownSocket::~CPeerCacheDownSocket()
{
	DEBUG_ONLY(Debug(_T("%08x %hs\n"), this, __FUNCTION__));
	CPeerCacheDownSocket::DetachFromClient();
}

void CPeerCacheDownSocket::DetachFromClient()
{
	if (GetClient()) {
		if (GetClient()->m_pPCDownSocket == this)
			GetClient()->m_pPCDownSocket = NULL;
	}
}

void CPeerCacheDownSocket::OnClose(int nErrorCode)
{
	DEBUG_ONLY(Debug(_T("%08x %hs\n"), this, __FUNCTION__));

	DisableDownloadLimit(); // receive pending data
	CUpDownClient *pCurClient = GetClient();
	if (pCurClient && pCurClient->m_pPCDownSocket != this)
		pCurClient = NULL;

	CPeerCacheSocket::OnClose(nErrorCode);

	if (pCurClient) {
		ASSERT(pCurClient->m_pPCDownSocket == NULL);

		// this callback is only invoked if that closed socket was(!) currently attached to the client
		pCurClient->OnPeerCacheDownSocketClosed(nErrorCode);
	}
}

bool CPeerCacheDownSocket::ProcessHttpResponse()
{
	if (GetClient() == NULL)
		throw CString(__FUNCTION__ " - No client attached to HTTP socket");

	if (!GetClient()->ProcessPeerCacheDownHttpResponse(m_astrHttpHeaders))
		return false;

	return true;
}

bool CPeerCacheDownSocket::ProcessHttpResponseBody(const BYTE *pucData, UINT uSize)
{
	if (GetClient() == NULL)
		throw CString(__FUNCTION__ " - No client attached to HTTP socket");

	GetClient()->ProcessPeerCacheDownHttpResponseBody(pucData, uSize);

	return true;
}

bool CPeerCacheDownSocket::ProcessHttpRequest()
{
	throw CString(_T("Unexpected HTTP request received"));
}


///////////////////////////////////////////////////////////////////////////////
// CPeerCacheUpSocket

IMPLEMENT_DYNCREATE(CPeerCacheUpSocket, CPeerCacheSocket)

CPeerCacheUpSocket::CPeerCacheUpSocket(CUpDownClient *pClient)
	: CPeerCacheSocket(pClient)
{
	DEBUG_ONLY(Debug(_T("%08x %hs\n"), this, __FUNCTION__));
}

CPeerCacheUpSocket::~CPeerCacheUpSocket()
{
	DEBUG_ONLY(Debug(_T("%08x %hs\n"), this, __FUNCTION__));
	theApp.uploadBandwidthThrottler->RemoveFromAllQueues(this);
	CPeerCacheUpSocket::DetachFromClient();
}

void CPeerCacheUpSocket::DetachFromClient()
{
	if (GetClient()) {
		if (GetClient()->m_pPCUpSocket == this) {
			GetClient()->m_pPCUpSocket = NULL;
			theApp.uploadBandwidthThrottler->RemoveFromStandardList(this);
		}
	}
}

void CPeerCacheUpSocket::OnSend(int nErrorCode)
{
	DEBUG_ONLY(Debug(_T("%08x %hs\n"), this, __FUNCTION__));
	CPeerCacheSocket::OnSend(nErrorCode);
}

void CPeerCacheUpSocket::OnClose(int nErrorCode)
{
	DEBUG_ONLY(Debug(_T("%08x %hs\n"), this, __FUNCTION__));
	CPeerCacheSocket::OnClose(nErrorCode);
	if (GetClient() && GetClient()->m_pPCUpSocket == this) {
		DetachFromClient();

		// this callback is only invoked if that closed socket was(!) currently attached to the client
		//GetClient()->OnPeerCacheUpSocketClosed(nErrorCode);
	}
}

#pragma warning(push)
#pragma warning(disable: 4702) //unreachable code
bool CPeerCacheUpSocket::ProcessHttpResponse()
{
	if (GetClient() == NULL)
		throw CString(__FUNCTION__ " - No client attached to HTTP socket");

	GetClient()->ProcessPeerCacheUpHttpResponse(m_astrHttpHeaders);

	return true;
}
#pragma warning(pop)

bool CPeerCacheUpSocket::ProcessHttpResponseBody(const BYTE* /*pucData*/, UINT /*uSize*/)
{
	throw CString(_T("Unexpected HTTP body in response received"));
}

bool CPeerCacheUpSocket::ProcessHttpRequest()
{
	if (GetClient() == NULL)
		throw CString(__FUNCTION__ " - No client attached to HTTP socket");

	UINT uHttpRes = GetClient()->ProcessPeerCacheUpHttpRequest(m_astrHttpHeaders);
	if (uHttpRes != HTTP_STATUS_OK) {
		CStringA strResponse;
		strResponse.Format("HTTP/1.0 %u\r\n"
			"Content-Length: 0\r\n"
			"\r\n"
			, uHttpRes);

		if (thePrefs.GetDebugClientTCPLevel() > 0)
			Debug(_T("Sending PeerCache HTTP response:\n%hs"), (LPCSTR)strResponse);
		CRawPacket *pHttpPacket = new CRawPacket(strResponse);
		theStats.AddUpDataOverheadFileRequest(pHttpPacket->size);
		SendPacket(pHttpPacket);
		SetHttpState(HttpStateUnknown);

		// PC-TODO: Problem, the packet which was queued for sending will not be sent, if we immediately
		// close that socket. Currently I just let it timeout.
		//return false;
		SetTimeOut(SEC2MS(30));
		return true;
	}
	GetClient()->SetHttpSendState(0);

	SetHttpState(HttpStateRecvExpected);
	GetClient()->SetUploadState(US_UPLOADING);

	return true;
}


///////////////////////////////////////////////////////////////////////////////
// PeerCache client

bool CUpDownClient::ProcessPeerCacheDownHttpResponse(const CStringAArray &astrHeaders)
{
	ASSERT(GetDownloadState() == DS_DOWNLOADING);
	ASSERT(m_ePeerCacheDownState == PCDS_WAIT_CACHE_REPLY);

	if (m_reqfile == NULL)
		throw CString(_T("Failed to process HTTP response - No 'reqfile' attached"));
	if (GetDownloadState() != DS_DOWNLOADING)
		throw CString(_T("Failed to process HTTP response - Invalid client download state"));
	if (astrHeaders.IsEmpty())
		throw CString(_T("Unexpected HTTP response - No headers available"));

	const CStringA &rstrHdr = astrHeaders[0];
	UINT uHttpMajVer, uHttpMinVer, uHttpStatusCode;
	if (sscanf(rstrHdr, "HTTP/%u.%u %u", &uHttpMajVer, &uHttpMinVer, &uHttpStatusCode) != 3) {
		CString strError;
		strError.Format(_T("Unexpected HTTP response: \"%hs\""), (LPCSTR)rstrHdr);
		throw strError;
	}
	if (uHttpMajVer != 1 || (uHttpMinVer != 0 && uHttpMinVer != 1)) {
		CString strError;
		strError.Format(_T("Unexpected HTTP version: \"%hs\""), (LPCSTR)rstrHdr);
		throw strError;
	}
	if (uHttpStatusCode != HTTP_STATUS_OK && uHttpStatusCode != HTTP_STATUS_PARTIAL_CONTENT) {
		CString strError;
		strError.Format(_T("Unexpected HTTP status code \"%u\""), uHttpStatusCode);
		throw strError;
	}

	uint64 uContentLength = 0;
	bool bCacheHit = false;
	bool bValidContentRange = false;
	for (int i = 1; i < astrHeaders.GetCount(); ++i) {
		const CStringA &rstrHdr1 = astrHeaders[i];
		if (_strnicmp(rstrHdr1, "Content-Length:", 15) == 0) {
			uContentLength = _atoi64((LPCSTR)rstrHdr1 + 15);
			if (uContentLength > m_uReqEnd - m_uReqStart + 1) {
				CString strError;
				strError.Format(_T("Unexpected HTTP header field \"%hs\""), (LPCSTR)rstrHdr1);
				throw strError;
			}
		} else if (_strnicmp(rstrHdr1, "Content-Range:", 14) == 0) {
			uint64 ui64Start = 0, ui64End = 0, ui64Len = 0;
			if (sscanf((LPCSTR)rstrHdr1 + 14, " bytes %I64u - %I64u / %I64u", &ui64Start, &ui64End, &ui64Len) != 3) {
				CString strError;
				strError.Format(_T("Unexpected HTTP header field \"%hs\""), (LPCSTR)rstrHdr1);
				throw strError;
			}

			if (ui64Start > ui64End
				|| ui64Len != m_reqfile->GetFileSize()
				|| ui64Start < m_uReqStart || ui64Start > m_uReqEnd
				|| ui64End < m_uReqStart || ui64End > m_uReqEnd)
			{
				CString strError;
				strError.Format(_T("Unexpected HTTP header field \"%hs\""), (LPCSTR)rstrHdr1);
				throw strError;
			}
			bValidContentRange = true;
			m_nUrlStartPos = ui64Start;
		} else if (_strnicmp(rstrHdr1, "Server:", 7) == 0) {
			if (m_strClientSoftware.IsEmpty())
				m_strClientSoftware = rstrHdr1.Mid(7).Trim();
		} else if (_strnicmp(rstrHdr1, "X-Cache: MISS", 13) == 0)
			bCacheHit = false;
		else if (_strnicmp(rstrHdr1, "X-Cache: HIT", 12) == 0)
			bCacheHit = true;
	}

	// we either get a 'Content-Range' or (for very small files) just a 'Content-Length' for the entire file
	if (!bValidContentRange && uContentLength == m_reqfile->GetFileSize()) {
		bValidContentRange = true;
		m_nUrlStartPos = 0;
	}

	if (!bValidContentRange) {
		if (thePrefs.GetDebugClientTCPLevel() <= 0)
			DebugHttpHeaders(astrHeaders);
		CString strError;
		strError.Format(_T("Unexpected HTTP response - No valid HTTP content range found"));
		throw strError;
	}

	if (m_pPCDownSocket) {
		CSafeMemFile dataAck(128);
		dataAck.WriteUInt8(static_cast<uint8>(bCacheHit));
		if (thePrefs.GetDebugClientTCPLevel() > 0) {
			DebugSend("OP_PeerCacheAck", this, m_reqfile->GetFileHash());
			Debug(_T("  %s\n"), bCacheHit ? _T("CacheHit") : _T("CacheMiss"));
		}

		Packet *pEd2kPacket = new Packet(&dataAck, OP_EMULEPROT, OP_PEERCACHE_ACK);
		theStats.AddUpDataOverheadFileRequest(pEd2kPacket->size);
		socket->SendPacket(pEd2kPacket);
	}

//	SetDownloadState(DS_DOWNLOADING);

	//PC-TODO: Where does this flag need to be cleared again?
	// When client is allowed to send more block requests?
	// Also, we have to support both type of downloads within in the same connection.
	SetPeerCacheDownState(PCDS_DOWNLOADING);
	m_bPeerCacheDownHit = bCacheHit;

	return true;
}

bool CUpDownClient::ProcessPeerCacheDownHttpResponseBody(const BYTE *pucData, UINT uSize)
{
	ProcessHttpBlockPacket(pucData, uSize);
	return true;
}

UINT CUpDownClient::ProcessPeerCacheUpHttpRequest(const CStringAArray &astrHeaders)
{
	ASSERT(m_ePeerCacheUpState == PCUS_WAIT_CACHE_REPLY);

	if (astrHeaders.IsEmpty())
		return HTTP_STATUS_BAD_REQUEST;

	const CStringA &rstrHdr = astrHeaders[0];
	char szUrl[1024];
	UINT uHttpMajVer, uHttpMinVer;
	if (sscanf(rstrHdr, "GET %1023s HTTP/%u.%u", szUrl, &uHttpMajVer, &uHttpMinVer) != 3) {
		DebugHttpHeaders(astrHeaders);
		return HTTP_STATUS_BAD_REQUEST;
	}
	if (uHttpMajVer != 1 || (uHttpMinVer != 0 && uHttpMinVer != 1)) {
		DebugHttpHeaders(astrHeaders);
		return HTTP_STATUS_BAD_REQUEST;
	}

	char szFileHash[33];
	if (sscanf(szUrl, "/.ed2khash=%32s", szFileHash) != 1) {
		DebugHttpHeaders(astrHeaders);
		return HTTP_STATUS_BAD_REQUEST;
	}
	uchar aucUploadFileID[MDX_DIGEST_SIZE];
	if (!strmd4(szFileHash, aucUploadFileID)) {
		DebugHttpHeaders(astrHeaders);
		return HTTP_STATUS_BAD_REQUEST;
	}

	CKnownFile *pUploadFile = theApp.sharedfiles->GetFileByID(aucUploadFileID);
	if (pUploadFile == NULL) {
		DebugHttpHeaders(astrHeaders);
		return HTTP_STATUS_NOT_FOUND;
	}

	bool bValidRange = false;
	uint64 ui64RangeStart = 0;
	uint64 ui64RangeEnd = 0;
	DWORD dwPushID = 0;
	for (int i = 1; i < astrHeaders.GetCount(); ++i) {
		const CStringA &rstrHdr2 = astrHeaders[i];
		if (_strnicmp(rstrHdr2, "Range:", 6) == 0) {
			int iParams;
			if ((iParams = sscanf((LPCSTR)rstrHdr2 + 6, " bytes = %I64u - %I64u", &ui64RangeStart, &ui64RangeEnd)) != 2
				&& (iParams = sscanf((LPCSTR)rstrHdr2 + 6, " bytes = %I64u -", &ui64RangeStart)) != 1)
			{
				DebugHttpHeaders(astrHeaders);
				TRACE("*** Unexpected HTTP %hs\n", (LPCSTR)rstrHdr2);
				return HTTP_STATUS_BAD_REQUEST;
			}
			if (iParams == 1)
				ui64RangeEnd = pUploadFile->GetFileSize() - (uint64)1;

			if (ui64RangeEnd < ui64RangeStart) {
				DebugHttpHeaders(astrHeaders);
				TRACE("*** Unexpected HTTP %hs\n", (LPCSTR)rstrHdr2);
				return HTTP_STATUS_INV_RANGE;
			}

			bValidRange = true;
		} else if (_strnicmp(rstrHdr2, "X-ED2K-PushId:", 14) == 0)
			if (sscanf((LPCSTR)rstrHdr2 + 14, "%lu", &dwPushID) != 1) {
				DebugHttpHeaders(astrHeaders);
				TRACE("*** Unexpected HTTP %hs\n", (LPCSTR)rstrHdr2);
				return HTTP_STATUS_BAD_REQUEST;
			}

	}

	if (!bValidRange) {
		DebugHttpHeaders(astrHeaders);
		return HTTP_STATUS_LENGTH_REQUIRED;
	}

	m_uPeerCacheUploadPushId = dwPushID;

	//PC-TODO: Where does this flag need to be cleared again?
	// When client is removed from uploading list?
	// When client is allowed to send more block requests?

	// everything is setup for uploading with PeerCache.
	SetPeerCacheUpState(PCUS_UPLOADING);

	Requested_Block_Struct *reqblock = new Requested_Block_Struct;
	reqblock->StartOffset = ui64RangeStart;
	reqblock->EndOffset = ui64RangeEnd + 1;
	md4cpy(reqblock->FileID, aucUploadFileID);
	reqblock->transferred = 0;
	AddReqBlock(reqblock, true);

	return HTTP_STATUS_OK;
}

void CUpDownClient::ProcessPeerCacheUpHttpResponse(const CStringAArray &astrHeaders)
{
	ASSERT(m_ePeerCacheUpState == PCUS_WAIT_CACHE_REPLY);

	if (astrHeaders.IsEmpty())
		throw CString(_T("Unexpected HTTP response - No headers available"));

	const CStringA &rstrHdr = astrHeaders[0];
	UINT uHttpMajVer, uHttpMinVer, uHttpStatusCode;
	if (sscanf(rstrHdr, "HTTP/%u.%u %u", &uHttpMajVer, &uHttpMinVer, &uHttpStatusCode) != 3) {
		CString strError;
		strError.Format(_T("Unexpected HTTP response: \"%hs\""), (LPCSTR)rstrHdr);
		throw strError;
	}
	if (uHttpMajVer != 1 || (uHttpMinVer != 0 && uHttpMinVer != 1)) {
		CString strError;
		strError.Format(_T("Unexpected HTTP version: \"%hs\""), (LPCSTR)rstrHdr);
		throw strError;
	}

	CString strError;
	strError.Format(_T("Unexpected HTTP status code \"%u\""), uHttpStatusCode);
	throw strError;
}

bool CUpDownClient::SendHttpBlockRequests()
{
	ASSERT(GetDownloadState() == DS_DOWNLOADING);
	ASSERT(m_ePeerCacheDownState == PCDS_WAIT_CLIENT_REPLY || m_ePeerCacheDownState == PCDS_DOWNLOADING);

	m_bPeerCacheDownHit = false;
	m_dwLastBlockReceived = ::GetTickCount();
	if (m_reqfile == NULL)
		throw CString(_T("Failed to send block requests - No 'reqfile' attached"));

	CreateBlockRequests(1, 1);
	if (m_PendingBlocks_list.IsEmpty()) {
		if (m_pPCDownSocket != NULL) {
			m_pPCDownSocket->Safe_Delete();
			ASSERT(m_pPCDownSocket == NULL);
			SetPeerCacheDownState(PCDS_NONE);
		}
		SetDownloadState(DS_NONEEDEDPARTS);
		SwapToAnotherFile(_T("A4AF for NNP file. CUpDownClient::SendHttpBlockRequests()"), true, false, false, NULL, true, true);
		return false;
	}

	// PeerCache does not support persistent HTTP connections
	if (m_pPCDownSocket != NULL) {
		m_pPCDownSocket->Safe_Delete();
		ASSERT(m_pPCDownSocket == NULL);
		SetPeerCacheDownState(PCDS_NONE);
		return SendPeerCacheFileRequest();
	}

	ASSERT(m_pPCDownSocket == NULL);
	m_pPCDownSocket = new CPeerCacheDownSocket(this);
	m_pPCDownSocket->SetTimeOut(GetPeerCacheSocketDownloadTimeout());
	if (!m_pPCDownSocket->Create()) {
		m_pPCDownSocket->Safe_Delete();
		ASSERT(m_pPCDownSocket == NULL);
		return false;
	}

	ASSERT(!m_pPCDownSocket->IsConnected());
	SOCKADDR_IN sockAddr = {};
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(theApp.m_pPeerCache->GetCachePort());
	sockAddr.sin_addr.s_addr = theApp.m_pPeerCache->GetCacheIP();
	//Try to always tell the socket to WaitForOnConnect before you call Connect.
	m_pPCDownSocket->WaitForOnConnect();
	m_pPCDownSocket->Connect((LPSOCKADDR)&sockAddr, sizeof sockAddr);

	Pending_Block_Struct *pending = m_PendingBlocks_list.GetHead();
	ASSERT(pending->block->StartOffset <= pending->block->EndOffset);

	m_uReqStart = pending->block->StartOffset;
	m_uReqEnd = pending->block->EndOffset;
	m_nUrlStartPos = _UI64_MAX;

	CStringA strPCRequest;
	strPCRequest.Format("GET http://%s/.ed2khash=%S HTTP/1.0\r\n"
		"X-ED2K-PushId: %u\r\n"
		"Range: bytes=%I64u-%I64u\r\n"
		"User-Agent: eMule/%S\r\n"
		"X-Network: eDonkey,Kademlia\r\n"
		"\r\n"
		, (LPCSTR)ipstrA(m_uPeerCacheRemoteIP), (LPCTSTR)md4str(m_reqfile->GetFileHash())
		, m_uPeerCacheDownloadPushId
		, m_uReqStart, m_uReqEnd
		, (LPCTSTR)theApp.m_strCurVersionLong);

	if (thePrefs.GetDebugClientTCPLevel() > 0) {
		DebugSend("PeerCache-GET", this, m_reqfile->GetFileHash());
		Debug(_T("  %hs\n"), (LPCSTR)strPCRequest);
	}
	CRawPacket *pHttpPacket = new CRawPacket(strPCRequest);
	theStats.AddUpDataOverheadFileRequest(pHttpPacket->size);
	m_pPCDownSocket->SendPacket(pHttpPacket);
	m_pPCDownSocket->SetHttpState(HttpStateRecvExpected);
	SetPeerCacheDownState(PCDS_WAIT_CACHE_REPLY);
	return true;
}

bool CUpDownClient::SendPeerCacheFileRequest()
{
	if (GetDownloadState() == DS_ONQUEUE) {
		ASSERT(m_ePeerCacheDownState == PCDS_NONE);
		ASSERT(m_pPCDownSocket == NULL);
	} else if (GetDownloadState() == DS_DOWNLOADING) {
		ASSERT(m_ePeerCacheDownState == PCDS_NONE);
		ASSERT(m_pPCDownSocket == NULL);
	} else
		ASSERT(0);

	if (!SupportPeerCache() || socket == NULL) {
		ASSERT(0);
		return false;
	}

	m_uPeerCacheDownloadPushId = GetRandomUInt32();

	CSafeMemFile data(128);
	data.WriteUInt8(PCPCK_VERSION);
	data.WriteUInt8(PCOP_REQ);
	data.WriteUInt8(5);
	CTag tagCacheIP(PCTAG_CACHEIP, theApp.m_pPeerCache->GetCacheIP());
	tagCacheIP.WriteNewEd2kTag(&data);
	CTag tagPushId(PCTAG_PUSHID, m_uPeerCacheDownloadPushId);
	tagPushId.WriteNewEd2kTag(&data);
	CTag tagFileId(PCTAG_FILEID, (uchar*)m_reqfile->GetFileHash());
	tagFileId.WriteNewEd2kTag(&data);
	CTag tagPublicIP(PCTAG_PUBLICIP, theApp.GetPublicIP());
	tagPublicIP.WriteNewEd2kTag(&data);
	CTag tagCachePort(PCTAG_CACHEPORT, theApp.m_pPeerCache->GetCachePort());
	tagCachePort.WriteNewEd2kTag(&data);

	if (thePrefs.GetDebugClientTCPLevel() > 0) {
		DebugSend("OP_PeerCacheQuery", this, m_reqfile->GetFileHash());
		Debug(_T("  CacheIP=%s  PushId=%u  PublicIP=%s  FileId=%s\n"), (LPCTSTR)ipstr(tagCacheIP.GetInt()), tagPushId.GetInt(), (LPCTSTR)ipstr(tagPublicIP.GetInt()), (LPCTSTR)md4str(tagFileId.GetHash()));
	}

	Packet *pEd2kPacket = new Packet(&data, OP_EMULEPROT, OP_PEERCACHE_QUERY);
	theStats.AddUpDataOverheadFileRequest(pEd2kPacket->size);
	socket->SendPacket(pEd2kPacket);
	SetDownloadState(DS_DOWNLOADING);
	m_dwLastBlockReceived = ::GetTickCount();
	SetPeerCacheDownState(PCDS_WAIT_CLIENT_REPLY);
	return true;
}

bool CUpDownClient::ProcessPeerCacheQuery(const uchar *packet, UINT size)
{
	const bool bDebug = (thePrefs.GetDebugClientTCPLevel() > 0);
	if (bDebug)
		DebugRecv("OP_PeerCacheQuery", this);

	if (socket == NULL) {
		ASSERT(0);
		return false;
	}

	CSafeMemFile dataRecv(packet, size);
	uint8 uPCVersion = dataRecv.ReadUInt8();
	if (uPCVersion != PCPCK_VERSION) {
		if (bDebug)
			Debug(_T("   ***Invalid packet version: 0x%02x\n"), uPCVersion);
		ASSERT(0);
		return false;
	}
	uint8 uPCOpcode = dataRecv.ReadUInt8();
	if (uPCOpcode != PCOP_REQ) {
		if (bDebug)
			Debug(_T("   ***Invalid packet opcode: 0x%02x\n"), uPCOpcode);
		ASSERT(0);
		return false;
	}

	uchar aucFileHash[16] = {};
	uint32 uPushId = 0;
	uint32 uCacheIP = 0;
	uint16 uCachePort = 0;

	CString strInfo;
	for (UINT uTags = dataRecv.ReadUInt8(); uTags > 0; --uTags) {
		CTag tag(&dataRecv, GetUnicodeSupport() != UTF8strNone);
		if (tag.GetNameID() == PCTAG_CACHEIP && tag.IsInt()) {
			uCacheIP = tag.GetInt();
			if (bDebug)
				strInfo.AppendFormat(_T("  CacheIP=%s"), (LPCTSTR)ipstr(uCacheIP));
		} else if (tag.GetNameID() == PCTAG_CACHEPORT && tag.IsInt()) {
			uCachePort = (uint16)tag.GetInt();
			if (bDebug)
				strInfo.AppendFormat(_T("  CachePort=%u"), uCachePort);
		} else if (tag.GetNameID() == PCTAG_PUSHID && tag.IsInt()) {
			uPushId = tag.GetInt();
			if (bDebug)
				strInfo.AppendFormat(_T("  PushId=%u"), uPushId);
		} else if (tag.GetNameID() == PCTAG_FILEID && tag.IsHash() && tag.GetHash() != NULL) {
			md4cpy(aucFileHash, tag.GetHash());
			if (bDebug)
				strInfo.AppendFormat(_T("  FileId=%s"), (LPCTSTR)md4str(aucFileHash));
		} else if (tag.GetNameID() == PCTAG_PUBLICIP && tag.IsInt()) {
			uint32 uPublicIP = tag.GetInt();
			if (bDebug)
				strInfo.AppendFormat(_T("  PublicIP=%s"), (LPCTSTR)ipstr(uPublicIP));
		} else {
			if (bDebug)
				strInfo.AppendFormat(_T("  ***UnkTag: %s"), (LPCTSTR)tag.GetFullInfo());
			ASSERT(0);
		}
	}

	if (bDebug) {
		if (dataRecv.GetPosition() < dataRecv.GetLength())
			strInfo.AppendFormat(_T("  ***AddData: %u bytes"), (UINT)(dataRecv.GetLength() - dataRecv.GetPosition()));
		Debug(_T("%s\n"), (LPCTSTR)strInfo);
	}

	if (uCacheIP == 0 || uCachePort == 0 || uPushId == 0 || isnulmd4(aucFileHash)) {
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("Invalid PeerCacheQuery; %s"), (LPCTSTR)DbgGetClientInfo());
		return false;
	}

	CKnownFile *pUploadFile = theApp.sharedfiles->GetFileByID(aucFileHash);
	if (pUploadFile == NULL) {
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("PeerCacheQuery reqfile does not match ed2k reqfile; %s"), (LPCTSTR)DbgGetClientInfo());
		return false;
	}

	if (m_pPCUpSocket != NULL) {
		SetPeerCacheUpState(PCUS_NONE);
		m_pPCUpSocket->Safe_Delete();
		ASSERT(m_pPCUpSocket == NULL);
	}
	m_pPCUpSocket = new CPeerCacheUpSocket(this);
	m_pPCUpSocket->SetTimeOut(GetPeerCacheSocketUploadTimeout());
	m_pPCUpSocket->Create();

	SOCKADDR_IN sockAddr = {};
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(uCachePort);
	sockAddr.sin_addr.s_addr = uCacheIP;
	//Try to always tell the socket to WaitForOnConnect before you call Connect.
	m_pPCUpSocket->WaitForOnConnect();
	m_pPCUpSocket->Connect((LPSOCKADDR)&sockAddr, sizeof sockAddr);

	CStringA strPCRequest;
	strPCRequest.Format("GIVE %u\r\n", uPushId);

	if (thePrefs.GetDebugClientTCPLevel() > 0) {
		DebugSend("PeerCache-GIVE", this, pUploadFile->GetFileHash());
		Debug(_T("  %hs\n"), (LPCSTR)strPCRequest);
	}

	CRawPacket *pHttpPacket = new CRawPacket(strPCRequest);
	theStats.AddUpDataOverheadFileRequest(pHttpPacket->size);
	m_pPCUpSocket->SendPacket(pHttpPacket);
	m_pPCUpSocket->SetHttpState(HttpStateRecvExpected);
	m_bPeerCacheUpHit = false;
	SetPeerCacheUpState(PCUS_WAIT_CACHE_REPLY);
	//theApp.uploadBandwidthThrottler->AddToStandardList(0, m_pPCUpSocket);

	CSafeMemFile dataSend(128);
	dataSend.WriteUInt8(PCPCK_VERSION);
	dataSend.WriteUInt8(PCOP_RES);
	dataSend.WriteUInt8(3);
	CTag tagPushId(PCTAG_PUSHID, uPushId);
	tagPushId.WriteNewEd2kTag(&dataSend);
	CTag tagPublicIP(PCTAG_PUBLICIP, theApp.GetPublicIP());
	tagPublicIP.WriteNewEd2kTag(&dataSend);
	CTag tagFileId(PCTAG_FILEID, (BYTE*)aucFileHash);
	tagFileId.WriteNewEd2kTag(&dataSend);

	if (thePrefs.GetDebugClientTCPLevel() > 0) {
		DebugSend("OP_PeerCacheAnswer", this, aucFileHash);
		Debug(_T("  PushId=%u  PublicIP=%s  FileId=%s\n"), tagPushId.GetInt(), (LPCTSTR)ipstr(tagPublicIP.GetInt()), (LPCTSTR)md4str(tagFileId.GetHash()));
	}

	Packet *pEd2kPacket = new Packet(&dataSend, OP_EMULEPROT, OP_PEERCACHE_ANSWER);
	theStats.AddUpDataOverheadFileRequest(pEd2kPacket->size);
	socket->SendPacket(pEd2kPacket);
	return true;
}

bool CUpDownClient::ProcessPeerCacheAnswer(const uchar *packet, UINT size)
{
	const bool bDebug = (thePrefs.GetDebugClientTCPLevel() > 0);
	ASSERT(GetDownloadState() == DS_DOWNLOADING);
	ASSERT(m_ePeerCacheDownState == PCDS_WAIT_CLIENT_REPLY);

	if (bDebug)
		DebugRecv("OP_PeerCacheAnswer", this);

	if (socket == NULL || m_reqfile == NULL) {
		ASSERT(0);
		return false;
	}

	CSafeMemFile dataRecv(packet, size);
	uint8 uPCVersion = dataRecv.ReadUInt8();
	if (uPCVersion != PCPCK_VERSION) {
		if (bDebug)
			Debug(_T("  ***Invalid packet version: 0x%02x\n"), uPCVersion);
		ASSERT(0);
		return false;
	}
	uint8 uPCOpcode = dataRecv.ReadUInt8();
	if (uPCOpcode == PCOP_NONE) {
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("Client does not support PeerCache; %s"), (LPCTSTR)DbgGetClientInfo());
		return false;
	}
	if (uPCOpcode != PCOP_RES) {
		if (bDebug)
			Debug(_T("  ***Invalid packet opcode: 0x%02x\n"), uPCOpcode);
		ASSERT(0);
		return false;
	}

	uint32 uPushId = 0;
	uint32 uRemoteIP = 0;
	uchar aucFileHash[16] = {};

	CString strInfo;
	for (UINT uTags = dataRecv.ReadUInt8(); uTags > 0; --uTags) {
		CTag tag(&dataRecv, GetUnicodeSupport() != UTF8strNone);
		if (tag.GetNameID() == PCTAG_PUSHID && tag.IsInt()) {
			uPushId = tag.GetInt();
			if (bDebug)
				strInfo.AppendFormat(_T("  PushId=%u"), uPushId);
		} else if (tag.GetNameID() == PCTAG_PUBLICIP && tag.IsInt()) {
			uRemoteIP = tag.GetInt();
			if (bDebug)
				strInfo.AppendFormat(_T("  RemoteIP=%s"), (LPCTSTR)ipstr(uRemoteIP));
		} else if (tag.GetNameID() == PCTAG_FILEID && tag.IsHash() && tag.GetHash() != NULL) {
			md4cpy(aucFileHash, tag.GetHash());
			if (bDebug)
				strInfo.AppendFormat(_T("  FileId=%s"), (LPCTSTR)md4str(aucFileHash));
		} else {
			if (bDebug)
				strInfo.AppendFormat(_T("  ***UnkTag: %s"), (LPCTSTR)tag.GetFullInfo());
			ASSERT(0);
		}
	}

	if (bDebug) {
		if (dataRecv.GetPosition() < dataRecv.GetLength())
			strInfo.AppendFormat(_T("  ***AddData: %u bytes"), (UINT)(dataRecv.GetLength() - dataRecv.GetPosition()));
		Debug(_T("%s\n"), (LPCTSTR)strInfo);
	}

	if (uPushId == 0 || uRemoteIP == 0 || isnulmd4(aucFileHash)) {
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("Invalid PeerCacheAnswer; %s"), (LPCTSTR)DbgGetClientInfo());
		return false;
	}

	if (!md4equ(aucFileHash, m_reqfile->GetFileHash())) {
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("PeerCacheAnswer reqfile does not match ed2k reqfile; %s"), (LPCTSTR)DbgGetClientInfo());
		return false;
	}

	m_uPeerCacheDownloadPushId = uPushId;
	m_uPeerCacheRemoteIP = uRemoteIP;

	if (!SendHttpBlockRequests())
		return false;

	theApp.m_pPeerCache->DownloadAttemptStarted();
	ASSERT(m_ePeerCacheDownState == PCDS_WAIT_CACHE_REPLY);
	return true;
}

bool CUpDownClient::ProcessPeerCacheAcknowledge(const uchar *packet, UINT size)
{
	const bool bDebug = (thePrefs.GetDebugClientTCPLevel() > 0);
	if (bDebug)
		DebugRecv("OP_PeerCacheAck", this);

	if (socket == NULL) {
		ASSERT(0);
		return false;
	}

	m_bPeerCacheUpHit = false;
	CSafeMemFile data(packet, size);
	UINT uAck = data.ReadUInt8();
	if (uAck == 1) {
		// Cache hit
		if (bDebug)
			Debug(_T("  Cache hit\n"));

		// PC-TODO: If this socket is closed, PeerCache also closes the socket which it had opened to the
		// remote client! So, to give the remote client a chance to receive all the data from the PeerCache,
		// we have to keep this socket open, although it's not needed nor could it be reused!
		if (m_pPCUpSocket == NULL) {
			if (thePrefs.GetVerbose())
				DebugLogError(_T("PeerCacheAck received - missing socket; %s"), (LPCTSTR)DbgGetClientInfo());
			ASSERT(0);
			return false;
		}
//		m_pPCUpSocket->Safe_Delete();
//		m_pPCUpSocket = NULL;
		m_pPCUpSocket->SetTimeOut(MIN2MS(60)); // set socket timeout to 1 hour ??
		m_bPeerCacheUpHit = true;
	} else if (uAck == 0) {
		// Cache miss, keep uploading
		if (bDebug)
			Debug(_T("  Cache miss\n"));
	} else {
		ASSERT(0);
		return false;
	}

	// PC-TODO: Since we can not close the PC socket, what exactly do we need this ACK-packet for?
	;

	UpdateDisplayedInfo();

	return true;
}

bool CUpDownClient::IsUploadingToPeerCache() const
{
	// this function should not check any socket ptrs, as the according sockets may already be closed/deleted
	return m_ePeerCacheUpState == PCUS_UPLOADING;
}

bool CUpDownClient::IsDownloadingFromPeerCache() const
{
	// this function should not check any socket ptrs, as the according sockets may already be closed/deleted
	return m_ePeerCacheDownState == PCDS_DOWNLOADING;
}

void CUpDownClient::OnPeerCacheDownSocketClosed(int nErrorCode)
{
	if (nErrorCode)
		return;

	// restart PC download if cache just closed the connection without obvious reason
	if (GetDownloadState() == DS_DOWNLOADING
		&& m_ePeerCacheDownState == PCDS_DOWNLOADING
		&& !m_PendingBlocks_list.IsEmpty())
	{
		if (thePrefs.GetVerbose())
			AddDebugLogLine(DLP_HIGH, false, _T("PeerCache: Socket closed unexpectedly, trying to reestablish connection"));
		theApp.m_pPeerCache->DownloadAttemptFailed();
		TRACE("+++ Restarting PeerCache download - socket closed\n");
		ASSERT(m_pPCDownSocket == NULL);
		SetPeerCacheDownState(PCDS_NONE);
		ClearDownloadBlockRequests();
		SendPeerCacheFileRequest();
	}
}

bool CUpDownClient::OnPeerCacheDownSocketTimeout()
{
	// restart PC download if cache just stalls
	if (GetDownloadState() != DS_DOWNLOADING || m_ePeerCacheDownState != PCDS_DOWNLOADING
		|| m_PendingBlocks_list.IsEmpty())
		return false;

	if (thePrefs.GetVerbose())
		AddDebugLogLine(DLP_HIGH, false, _T("PeerCache Error: Socket TimeOut, trying to reestablish connection"));
	theApp.m_pPeerCache->DownloadAttemptFailed();
	TRACE("+++ Restarting PeerCache download - socket timeout\n");
	if (m_pPCDownSocket) {
		m_pPCDownSocket->Safe_Delete();
		ASSERT(m_pPCDownSocket == NULL);
	}
	SetPeerCacheDownState(PCDS_NONE);
	ClearDownloadBlockRequests();
	return SendPeerCacheFileRequest();
}

void CUpDownClient::SetPeerCacheDownState(EPeerCacheDownState eState)
{
	if (m_ePeerCacheDownState != eState) {
		m_ePeerCacheDownState = eState;
		if (m_ePeerCacheDownState == PCDS_NONE)
			m_bPeerCacheDownHit = false;
		UpdateDisplayedInfo();
	}
}

void CUpDownClient::SetPeerCacheUpState(EPeerCacheUpState eState)
{
	if (m_ePeerCacheUpState != eState) {
		//if (thePrefs.GetVerbose())
			//AddDebugLogLine(false, _T(" %s changed PeercacheState to %i"), (LPCTSTR)DbgGetClientInfo(), eState);

		m_ePeerCacheUpState = eState;
		if (m_ePeerCacheUpState == PCUS_NONE)
			m_bPeerCacheUpHit = false;

		theApp.uploadqueue->ReSortUploadSlots(true);

		UpdateDisplayedInfo();
	}
}