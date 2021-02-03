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
#include "SafeFile.h"
#include "Packets.h"
#include "ListenSocket.h"
#include "HttpClientReqSocket.h"
#include "Preferences.h"
#include "Statistics.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


///////////////////////////////////////////////////////////////////////////////
// CHttpClientReqSocket
//

IMPLEMENT_DYNCREATE(CHttpClientReqSocket, CClientReqSocket)

CHttpClientReqSocket::CHttpClientReqSocket(CUpDownClient *pclient)
	: CClientReqSocket(pclient)
{
	SetHttpState(HttpStateUnknown);
	SetConnectionEncryption(false, NULL, false); // just to make sure - disable protocol encryption explicit
}

void CHttpClientReqSocket::SetHttpState(EHttpSocketState eState)
{
	m_eHttpState = eState;
	if (m_eHttpState == HttpStateRecvExpected || m_eHttpState == HttpStateUnknown)
		ClearHttpHeaders();
}

void CHttpClientReqSocket::ClearHttpHeaders()
{
	m_strHttpCurHdrLine.Empty();
	m_astrHttpHeaders.RemoveAll();
	m_iHttpHeadersSize = 0;
}

void CHttpClientReqSocket::SendPacket(Packet *packet, bool controlpacket, uint32 actualPayloadSize, bool bForceImmediateSend)
{
	// just for safety -- never send an ed2k/emule packet via HTTP.
	if (packet->opcode == 0x00 && packet->prot == 0x00)
		CClientReqSocket::SendPacket(packet, controlpacket, actualPayloadSize, bForceImmediateSend);
	else
		ASSERT(0);
}

void CHttpClientReqSocket::OnConnect(int nErrorCode)
{
	CClientReqSocket::OnConnect(nErrorCode);
	if (GetClient())
		GetClient()->OnSocketConnected(nErrorCode);
}

void CHttpClientReqSocket::DataReceived(const BYTE *pucData, UINT uSize)
{
	bool bResult = false;
	CString strError;
	try {
		bResult = ProcessHttpPacket(pucData, uSize);
	} catch (CMemoryException *ex) {
		strError.Format(_T("Error: HTTP socket: Memory exception; %s"), (LPCTSTR)DbgGetClientInfo());
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("%s"), (LPCTSTR)strError);
		ex->Delete();
	} catch (CFileException *ex) {
		TCHAR szError[MAX_CFEXP_ERRORMSG];
		GetExceptionMessage(*ex, szError, _countof(szError));
		strError.Format(_T("Error: HTTP socket: File exception - %s"), szError);
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("%s"), (LPCTSTR)strError);
		ex->Delete();
	} catch (const CString &ex) {
		strError.Format(_T("Error: HTTP socket: %s; %s"), (LPCTSTR)ex, (LPCTSTR)DbgGetClientInfo());
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("%s"), (LPCTSTR)strError);
	}

	if (!bResult && !deletethis) {
		if (thePrefs.GetVerbose() && thePrefs.GetDebugClientTCPLevel() <= 0)
			for (int i = 0; i < m_astrHttpHeaders.GetCount(); ++i)
				AddDebugLogLine(false, _T("<%hs"), (LPCSTR)m_astrHttpHeaders[i]);

		// In case this socket is attached to a CUrlClient, we are dealing with the real CUpDownClient here
		// In case this socket is a PeerCacheUp/Down socket, we are dealing with the attached CUpDownClient here
//		if (GetClient())
//			GetClient()->SetDownloadState(DS_ERROR);
		if (client)	// NOTE: The usage of 'client' and 'GetClient' makes quite a difference here!
			client->SetDownloadState(DS_ERROR);

		if (strError.IsEmpty())
			strError = _T("Error: HTTP socket");

		// In case this socket is attached to a CUrlClient, we are disconnecting the real CUpDownClient here
		// In case this socket is a PeerCacheUp/Down socket, we are not disconnecting the attached CUpDownClient here
		// PC-TODO: This needs to be cleaned up thoroughly because that client dependency is somewhat hidden in the
		// usage of CClientReqSocket::client and CHttpClientReqSocket::GetClient.
		Disconnect(strError);
	}
}

bool CHttpClientReqSocket::ProcessHttpPacket(const BYTE *pucData, UINT uSize)
{
	if (GetHttpState() == HttpStateRecvExpected || GetHttpState() == HttpStateRecvHeaders) {
		// search for EOH
		LPBYTE pBody = NULL;
		int iSizeBody = 0;
		ProcessHttpHeaderPacket((char*)pucData, uSize, pBody, iSizeBody);

		if (pBody) { // EOH found, packet may contain partial body
			if (thePrefs.GetDebugClientTCPLevel() > 0) {
				Debug(_T("Received HTTP\n"));
				DebugHttpHeaders(m_astrHttpHeaders);
			}

			// PC-TODO: Should be done right in 'ProcessHttpHeaderPacket'
			uint32 iSizeHeader = 2;
			for (INT_PTR i = m_astrHttpHeaders.GetCount(); --i >= 0;)
				iSizeHeader += m_astrHttpHeaders[i].GetLength() + 2u;
			theStats.AddDownDataOverheadFileRequest(iSizeHeader);

			if (iSizeBody < 0)
				throw CString(_T("Internal HTTP header/body parsing error"));

			if (m_astrHttpHeaders[0].GetLength() >= 4 && memcmp((LPCSTR)m_astrHttpHeaders[0], "HTTP", 4) == 0) {
				if (!ProcessHttpResponse())
					return false;

				SetHttpState(HttpStateRecvBody);
				if (iSizeBody > 0) {
					// packet contained HTTP headers and (partial) body
					ProcessHttpResponseBody(pBody, iSizeBody);
				} else {
					// packet contained HTTP headers, but no body (packet terminates with EOH)
					// body will be processed because of HTTP state 'HttpStateRecvBody' with next recv
					;
				}
			} else if (m_astrHttpHeaders[0].GetLength() >= 3 && memcmp((LPCSTR)m_astrHttpHeaders[0], "GET", 3) == 0) {
				if (!ProcessHttpRequest())
					return false;
				if (iSizeBody != 0) {
					ASSERT(0); // no body for GET requests allowed yet
					return false;
				}
			} else
				throw CString(_T("Invalid HTTP header received"));
		} else
			TRACE("+++ Received partial HTTP header packet\n");
	} else if (GetHttpState() == HttpStateRecvBody)
		ProcessHttpResponseBody(pucData, uSize);
	else {
		theStats.AddDownDataOverheadFileRequest(uSize);
		throw CString(_T("Invalid HTTP socket state"));
	}

	return true;
}

bool CHttpClientReqSocket::ProcessHttpResponse()
{
	ASSERT(0);
	return false;
}

bool CHttpClientReqSocket::ProcessHttpResponseBody(const BYTE* /*pucData*/, UINT /*uSize*/)
{
	ASSERT(0);
	return false;
}

bool CHttpClientReqSocket::ProcessHttpRequest()
{
	ASSERT(0);
	return false;
}

void SplitHeaders(LPCSTR pszHeaders, CStringArray &astrHeaders)
{
	const char *p = pszHeaders;
	const char *pCrLf;
	while ((pCrLf = strstr(p, "\r\n")) != NULL) {
		int iLineLen = (int)(pCrLf - p);
		const char *pLine = p;
		p = pCrLf + 2;
		ASSERT(iLineLen >= 0);
		if (iLineLen <= 0)
			break;

		CString strHdr(pLine, iLineLen);
		astrHeaders.Add(strHdr);
	}
}

#define	MAX_HTTP_HEADERS_SIZE		2048
#define	MAX_HTTP_HEADER_LINE_SIZE	1024

void CHttpClientReqSocket::ProcessHttpHeaderPacket(const char *packet, UINT size, LPBYTE &pBody, int &iSizeBody)
{
	LPCSTR p = packet;
	int iLeft = size;
	while (iLeft > 0 && pBody == NULL) {
		LPCSTR pszNl = (LPCSTR)memchr(p, '\n', iLeft);
		if (pszNl) {
			// append current (partial) line to any already received partial line
			int iLineLen = (int)(pszNl - p);
			ASSERT(iLineLen >= 0);
			if (iLineLen > 0)
				m_strHttpCurHdrLine += CStringA(p, iLineLen - 1); // do not copy the '\r' character

			// in case the CRLF were split up in different packets, the current line may contain a '\r' character, remove it
			int iCurHdrLineLen = m_strHttpCurHdrLine.GetLength();
			if (iCurHdrLineLen > 0 && m_strHttpCurHdrLine[iCurHdrLineLen - 1] == '\r')
				m_strHttpCurHdrLine.Truncate(--iCurHdrLineLen); //remove the last '\r' character

			p += iLineLen + 1;
			iLeft -= iLineLen + 1;
			ASSERT(iLeft >= 0);

			if (m_strHttpCurHdrLine.IsEmpty()) { // if the current line is empty, we have found 2(!) CRLFs -> start of body
				pBody = (LPBYTE)p;
				iSizeBody = iLeft;
				ASSERT(iSizeBody >= 0);
			} else {
				// add the current line to headers
				m_astrHttpHeaders.Add(m_strHttpCurHdrLine);
				m_iHttpHeadersSize += iCurHdrLineLen;
				m_strHttpCurHdrLine.Empty();

				// safety check
				if (m_iHttpHeadersSize > MAX_HTTP_HEADERS_SIZE)
					throw CString(_T("Received HTTP headers exceed limit"));
			}
		} else {
			// partial line, add to according buffer
			m_strHttpCurHdrLine += CStringA(p, iLeft);
			iLeft = 0;

			// safety check
			if (m_strHttpCurHdrLine.GetLength() > MAX_HTTP_HEADER_LINE_SIZE)
				throw CString(_T("Received HTTP header line exceeds limit"));
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// CHttpClientDownSocket
//

IMPLEMENT_DYNCREATE(CHttpClientDownSocket, CHttpClientReqSocket)

CHttpClientDownSocket::CHttpClientDownSocket(CUpDownClient *pclient)
	: CHttpClientReqSocket(pclient)
{
}

bool CHttpClientDownSocket::ProcessHttpResponse()
{
	if (GetClient() == NULL)
		throw CString(__FUNCTION__ " - No client attached to HTTP socket");

	return GetClient()->ProcessHttpDownResponse(m_astrHttpHeaders);
}

bool CHttpClientDownSocket::ProcessHttpResponseBody(const BYTE *pucData, UINT size)
{
	if (GetClient() == NULL)
		throw CString(__FUNCTION__ " - No client attached to HTTP socket");

	GetClient()->ProcessHttpDownResponseBody(pucData, size);

	return true;
}

bool CHttpClientDownSocket::ProcessHttpRequest()
{
	throw CString(_T("Unexpected HTTP request received"));
}