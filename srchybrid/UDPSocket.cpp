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
#include "UDPSocket.h"
#include "SearchList.h"
#include "DownloadQueue.h"
#include "Statistics.h"
#include "Server.h"
#include "Preferences.h"
#include "OtherFunctions.h"
#include "ServerList.h"
#include "Opcodes.h"
#include "SafeFile.h"
#include "PartFile.h"
#include "Packets.h"
#include "IPFilter.h"
#include "emuledlg.h"
#include "ServerWnd.h"
#include "SearchDlg.h"
#include "Log.h"
#include "ServerConnect.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#pragma pack(push, 1)
struct SServerUDPPacket
{
	BYTE	*packet;
	int		size;
	uint32	dwIP;
	uint16	nPort;
};
#pragma pack(pop)

struct SRawServerPacket
{
	SRawServerPacket(BYTE *pPacket, UINT uSize, uint16 nPort)
		: m_pPacket(pPacket)
		, m_uSize(uSize)
		, m_nPort(nPort)
	{
	}
	~SRawServerPacket()
	{
		delete[] m_pPacket;
	}
	BYTE	*m_pPacket;
	UINT	m_uSize;
	uint16	m_nPort;
};

struct SServerDNSRequest
{
	SServerDNSRequest(HANDLE hDNSTask, CServer *pServer)
		: m_dwCreated(::GetTickCount())
		, m_hDNSTask(hDNSTask)
		, m_DnsHostBuffer()
		, m_pServer(pServer)
	{
	}
	~SServerDNSRequest()
	{
		if (m_hDNSTask)
			WSACancelAsyncRequest(m_hDNSTask);
		delete m_pServer;
		while (!m_aPackets.IsEmpty())
			delete m_aPackets.RemoveHead();
	}
	DWORD m_dwCreated;
	HANDLE m_hDNSTask;
	char m_DnsHostBuffer[MAXGETHOSTSTRUCT];
	CServer *m_pServer;
	CTypedPtrList<CPtrList, SRawServerPacket*> m_aPackets;
};

#define WM_DNSLOOKUPDONE	(WM_USER+0x101)		// does not need to be placed in "UserMsgs.h"

BEGIN_MESSAGE_MAP(CUDPSocketWnd, CWnd)
	ON_MESSAGE(WM_DNSLOOKUPDONE, OnDNSLookupDone)
END_MESSAGE_MAP()

LRESULT CUDPSocketWnd::OnDNSLookupDone(WPARAM wParam, LPARAM lParam)
{
	m_pOwner->DnsLookupDone(wParam, lParam);
	return true;
}

CUDPSocket::CUDPSocket()
	: m_hWndResolveMessage()
	, m_bWouldBlock()
{
}

CUDPSocket::~CUDPSocket()
{
	theApp.uploadBandwidthThrottler->RemoveFromAllQueuesLocked(this); // ZZ:UploadBandWithThrottler (UDP)

	while (!controlpacket_queue.IsEmpty()) {
		const SServerUDPPacket *p = controlpacket_queue.RemoveHead();
		delete[] p->packet;
		delete p;
	}
	m_udpwnd.DestroyWindow();

	while (!m_aDNSReqs.IsEmpty())
		delete m_aDNSReqs.RemoveHead();
}

bool CUDPSocket::Create()
{
	if (thePrefs.GetServerUDPPort()) {
		VERIFY(m_udpwnd.CreateEx(0, AfxRegisterWndClass(0), _T("eMule Async DNS Resolve Socket Wnd #1"), WS_OVERLAPPED, 0, 0, 0, 0, HWND_MESSAGE, NULL));
		m_hWndResolveMessage = m_udpwnd.m_hWnd;
		m_udpwnd.m_pOwner = this;
		if (CAsyncSocket::Create(thePrefs.GetServerUDPPort() == _UI16_MAX ? 0 : thePrefs.GetServerUDPPort(), SOCK_DGRAM, FD_READ | FD_WRITE, thePrefs.GetBindAddrW()))
			return true;
		LogError(LOG_STATUSBAR, _T("Error: Server UDP socket: Failed to create server UDP socket - %s"), (LPCTSTR)GetErrorMessage(CAsyncSocket::GetLastError()));
	}
	return false;
}

void CUDPSocket::OnReceive(int nErrorCode)
{
	if (nErrorCode) {
		if (thePrefs.GetDebugServerUDPLevel() > 0)
			Debug(_T("Error: Server UDP socket: Receive failed - %s\n"), (LPCTSTR)GetErrorMessage(nErrorCode, 1));
		if (thePrefs.GetVerbose())
			DebugLogError(_T("Error: Server UDP socket: Receive failed - %s"), (LPCTSTR)GetErrorMessage(nErrorCode, 1));
	}

	BYTE buffer[5000];
	BYTE *pBuffer = buffer;
	SOCKADDR_IN sockAddr = {};
	int iSockAddrLen = sizeof sockAddr;
	int length = ReceiveFrom(buffer, sizeof buffer, (LPSOCKADDR)&sockAddr, &iSockAddrLen);
	if (length != SOCKET_ERROR) {
		int nPayLoadLen = length;
		CServer *pServer = theApp.serverlist->GetServerByIPUDP(sockAddr.sin_addr.s_addr, ntohs(sockAddr.sin_port), true);
		if (pServer != NULL && thePrefs.IsServerCryptLayerUDPEnabled() &&
			((pServer->GetServerKeyUDP() != 0 && pServer->SupportsObfuscationUDP()) || (pServer->GetCryptPingReplyPending() && pServer->GetChallenge() != 0)))
		{
			// TODO
			uint32 dwKey;
			if (pServer->GetCryptPingReplyPending() && pServer->GetChallenge() != 0 /* && pServer->GetPort() == ntohs(sockAddr.sin_port) - 12 */)
				dwKey = pServer->GetChallenge();
			else
				dwKey = pServer->GetServerKeyUDP();

			ASSERT(dwKey != 0);
			nPayLoadLen = DecryptReceivedServer(buffer, length, &pBuffer, dwKey, sockAddr.sin_addr.s_addr);
			if (nPayLoadLen == length)
				DebugLogWarning(_T("Expected encrypted packet, but received unencrypted from server %s, UDPKey %u, Challenge: %u"), (LPCTSTR)pServer->GetListName(), pServer->GetServerKeyUDP(), pServer->GetChallenge());
			else if (thePrefs.GetDebugServerUDPLevel() > 0)
				DEBUG_ONLY(DebugLog(_T("Received encrypted packet from server %s, UDPKey %u, Challenge: %u"), (LPCTSTR)pServer->GetListName(), pServer->GetServerKeyUDP(), pServer->GetChallenge()));
		}

		if (pBuffer[0] == OP_EDONKEYPROT)
			ProcessPacket(pBuffer + 2, nPayLoadLen - 2, pBuffer[1], sockAddr.sin_addr.s_addr, ntohs(sockAddr.sin_port));
		else if (thePrefs.GetDebugServerUDPLevel() > 0)
			Debug(_T("***NOTE: ServerUDPMessage from %s:%u - Unknown protocol 0x%02x, Encrypted: %s\n"), (LPCTSTR)ipstr(sockAddr.sin_addr), ntohs(sockAddr.sin_port) - 4, pBuffer[0], (nPayLoadLen == length) ? _T("Yes") : _T("No"));
	} else {
		DWORD dwError = WSAGetLastError();
		if (thePrefs.GetDebugServerUDPLevel() > 0) {
			CString strServerInfo;
			if (iSockAddrLen > 0 && sockAddr.sin_addr.s_addr != 0 && sockAddr.sin_addr.s_addr != INADDR_NONE)
				strServerInfo.Format(_T(" from %s:%d"), (LPCTSTR)ipstr(sockAddr.sin_addr), ntohs(sockAddr.sin_port) - 4);
			Debug(_T("Error: Server UDP socket: Failed to receive data%s: %s\n"), (LPCTSTR)strServerInfo, (LPCTSTR)GetErrorMessage(dwError, 1));
		}
		if (dwError == WSAECONNRESET) {
			// Depending on local and remote OS and depending on used local (remote?) router we may receive
			// WSAECONNRESET errors. According some KB articles, this is a special way of winsock to report
			// that a sent UDP packet was not received by the remote host because it was not listening on
			// the specified port -> no server running there.

			// If we are not currently pinging this server, increase the failure counter
			CServer *pServer = theApp.serverlist->GetServerByIPUDP(sockAddr.sin_addr.s_addr, ntohs(sockAddr.sin_port), true);
			if (pServer)
				if (!pServer->GetCryptPingReplyPending() && ::GetTickCount() >= pServer->GetLastPinged() + SEC2MS(30)) {
					pServer->IncFailedCount();
					theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
				} else if (pServer->GetCryptPingReplyPending())
					DEBUG_ONLY(DebugLog(_T("CryptPing failed (WSACONNRESET) for server %s"), (LPCTSTR)pServer->GetListName()));

		} else if (thePrefs.GetVerbose()) {
			CString strServerInfo;
			if (iSockAddrLen > 0 && sockAddr.sin_addr.s_addr != 0 && sockAddr.sin_addr.s_addr != INADDR_NONE)
				strServerInfo.Format(_T(" from %s:%d"), (LPCTSTR)ipstr(sockAddr.sin_addr), ntohs(sockAddr.sin_port) - 4);
			DebugLogError(_T("Error: Server UDP socket: Failed to receive data%s: %s"), (LPCTSTR)strServerInfo, (LPCTSTR)GetErrorMessage(dwError, 1));
		}
	}
}

bool CUDPSocket::ProcessPacket(const BYTE *packet, UINT size, UINT opcode, uint32 nIP, uint16 nUDPPort)
{
	try {
		theStats.AddDownDataOverheadServer(size);
		CServer *pServer = theApp.serverlist->GetServerByIPUDP(nIP, nUDPPort, true);
		if (pServer) {
			pServer->ResetFailedCount();
			theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
		}

		switch (opcode) {
		case OP_GLOBSEARCHRES:	// process all search result packets
			{
				CSafeMemFile data(packet, size);
				int iLeft;
				int iDbgPacket = 1;
				do {
					if (thePrefs.GetDebugServerUDPLevel() > 0) {
						if (data.GetLength() - data.GetPosition() >= 16 + 4 + 2) {
							const BYTE *pDbgPacket = data.GetBuffer() + data.GetPosition();
							Debug(_T("ServerUDPMessage from %-21s - OP_GlobSearchResult(%u); %s\n"), (LPCTSTR)ipstr(nIP, nUDPPort - 4), iDbgPacket++, (LPCTSTR)DbgGetFileInfo(pDbgPacket), (LPCTSTR)DbgGetClientID(PeekUInt32(pDbgPacket + 16)), PeekUInt16(pDbgPacket + 20));
						}
					}
					UINT uResultCount = theApp.searchlist->ProcessUDPSearchAnswer(data, true/*pServer->GetUnicodeSupport()*/, nIP, nUDPPort - 4);
					theApp.emuledlg->searchwnd->AddEd2kSearchResults(uResultCount);

					// check if there is another source packet
					iLeft = (int)(data.GetLength() - data.GetPosition());
					if (iLeft >= 2) {
						uint8 protocol = data.ReadUInt8();
						--iLeft;
						if (protocol != OP_EDONKEYPROT) {
							data.Seek(-1, SEEK_CUR);
							++iLeft;
							break;
						}

						uint8 opcode1 = data.ReadUInt8();
						--iLeft;
						if (opcode1 != OP_GLOBSEARCHRES) {
							data.Seek(-2, SEEK_CUR);
							iLeft += 2;
							break;
						}
					}
				} while (iLeft > 0);

				if (iLeft > 0 && thePrefs.GetDebugServerUDPLevel() > 0) {
					Debug(_T("***NOTE: OP_GlobSearchResult contains %d additional bytes\n"), iLeft);
					if (thePrefs.GetDebugServerUDPLevel() > 1)
						DebugHexDump(data);
				}
			}
			break;
		case OP_GLOBFOUNDSOURCES:	// process all source packets
			{
				CSafeMemFile data(packet, size);
				int iLeft;
				int iDbgPacket = 1;
				do {
					uchar fileid[16];
					data.ReadHash16(fileid);
					if (thePrefs.GetDebugServerUDPLevel() > 0)
						Debug(_T("ServerUDPMessage from %-21s - OP_GlobFoundSources(%u); %s\n"), (LPCTSTR)ipstr(nIP, nUDPPort - 4), iDbgPacket++, (LPCTSTR)DbgGetFileInfo(fileid));
					CPartFile *file = theApp.downloadqueue->GetFileByID(fileid);
					if (file)
						file->AddSources(&data, nIP, nUDPPort - 4, false);
					else {
						// skip sources for that file
						UINT count = data.ReadUInt8();
						data.Seek(count * (4 + 2ull), SEEK_CUR);
					}

					// check if there is another source packet
					iLeft = (int)(data.GetLength() - data.GetPosition());
					if (iLeft >= 2) {
						uint8 protocol = data.ReadUInt8();
						--iLeft;
						if (protocol != OP_EDONKEYPROT) {
							data.Seek(-1, SEEK_CUR);
							++iLeft;
							break;
						}

						uint8 opcode2 = data.ReadUInt8();
						--iLeft;
						if (opcode2 != OP_GLOBFOUNDSOURCES) {
							data.Seek(-2, SEEK_CUR);
							iLeft += 2;
							break;
						}
					}
				} while (iLeft > 0);

				if (iLeft > 0 && thePrefs.GetDebugServerUDPLevel() > 0) {
					Debug(_T("***NOTE: OP_GlobFoundSources contains %d additional bytes\n"), iLeft);
					if (thePrefs.GetDebugServerUDPLevel() > 1)
						DebugHexDump(data);
				}
			}
			break;
		case OP_GLOBSERVSTATRES:
			{
				if (thePrefs.GetDebugServerUDPLevel() > 0)
					Debug(_T("ServerUDPMessage from %-21s - OP_GlobServStatRes\n"), (LPCTSTR)ipstr(nIP, nUDPPort - 4));
				if (size < 12 || pServer == NULL)
					return true;
				uint32 challenge = PeekUInt32(packet);
				if (challenge != pServer->GetChallenge()) {
					if (thePrefs.GetDebugServerUDPLevel() > 0)
						Debug(_T("***NOTE: Received unexpected challenge %08x (waiting on packet with challenge %08x)\n"), challenge, pServer->GetChallenge());
					return true;
				}

				pServer->SetChallenge(0);
				pServer->SetCryptPingReplyPending(false);
				// if we used Obfuscated ping, we still need to reset the time properly
				pServer->SetLastPingedTime(time(NULL) - (rand() % HR2S(1)));

				uint32 cur_user = PeekUInt32(packet + 4);
				uint32 cur_files = PeekUInt32(packet + 8);
				uint32 cur_maxusers = (size >= 16) ? PeekUInt32(packet + 12) : 0;

				uint32 cur_softfiles, cur_hardfiles;
				if (size >= 24) {
					cur_softfiles = PeekUInt32(packet + 16);
					cur_hardfiles = PeekUInt32(packet + 20);
				} else
					cur_softfiles = cur_hardfiles = 0;

				uint32 uUDPFlags;
				if (size >= 28) {
					uUDPFlags = PeekUInt32(packet + 24);
					if (thePrefs.GetDebugServerUDPLevel() > 0) {
						static const DWORD dwKnownBits = SRV_UDPFLG_EXT_GETSOURCES | SRV_UDPFLG_EXT_GETFILES | SRV_UDPFLG_NEWTAGS | SRV_UDPFLG_UNICODE | SRV_UDPFLG_EXT_GETSOURCES2 | SRV_UDPFLG_LARGEFILES | SRV_UDPFLG_UDPOBFUSCATION | SRV_UDPFLG_TCPOBFUSCATION;
						CString strInfo;
						if (uUDPFlags & ~dwKnownBits)
							strInfo.Format(_T("  ***UnkUDPFlags=0x%08lx"), uUDPFlags & ~dwKnownBits);
						if (uUDPFlags & SRV_UDPFLG_EXT_GETSOURCES)
							strInfo += _T("  ExtGetSources=1");
						if (uUDPFlags & SRV_UDPFLG_EXT_GETSOURCES2)
							strInfo += _T("  ExtGetSources2=1");
						if (uUDPFlags & SRV_UDPFLG_EXT_GETFILES)
							strInfo += _T("  ExtGetFiles=1");
						if (uUDPFlags & SRV_UDPFLG_NEWTAGS)
							strInfo += _T("  NewTags=1");
						if (uUDPFlags & SRV_UDPFLG_UNICODE)
							strInfo += _T("  Unicode=1");
						if (uUDPFlags & SRV_UDPFLG_LARGEFILES)
							strInfo += _T("  LargeFiles=1");
						if (uUDPFlags & SRV_UDPFLG_UDPOBFUSCATION)
							strInfo += _T("  UDP_Obfuscation=1");
						if (uUDPFlags & SRV_UDPFLG_TCPOBFUSCATION)
							strInfo += _T("  TCP_Obfuscation=1");
						Debug(_T("%s\n"), (LPCTSTR)strInfo);
					}
				} else
					uUDPFlags = 0;

				uint32 uLowIDUsers = (size >= 32) ? PeekUInt32(packet + 28) : 0;

				uint16 nUDPObfuscationPort, nTCPObfuscationPort;
				uint32 dwServerUDPKey;
				if (size >= 40) {
					// TODO debug check if this packet was encrypted if it has a key
					nUDPObfuscationPort = PeekUInt16(packet + 32);
					nTCPObfuscationPort = PeekUInt16(packet + 34);
					dwServerUDPKey = PeekUInt32(packet + 36);
					DEBUG_ONLY(DebugLog(_T("New UDP key for server %s: UDPKey %u - Old Key %u"), (LPCTSTR)pServer->GetListName(), dwServerUDPKey, pServer->GetServerKeyUDP()));

					if (size > 40 && thePrefs.GetDebugServerUDPLevel() > 0) {
						Debug(_T("***NOTE: OP_GlobServStatRes contains %d additional bytes\n"), size - 40);
						if (thePrefs.GetDebugServerUDPLevel() > 1)
							DbgGetHexDump(packet + 32, size - 32);
					}
				} else {
					nUDPObfuscationPort = nTCPObfuscationPort = 0;
					dwServerUDPKey = 0;
				}

				pServer->SetPing(::GetTickCount() - pServer->GetLastPinged());
				pServer->SetUserCount(cur_user);
				pServer->SetFileCount(cur_files);
				pServer->SetMaxUsers(cur_maxusers);
				pServer->SetSoftFiles(cur_softfiles);
				pServer->SetHardFiles(cur_hardfiles);
				pServer->SetServerKeyUDP(dwServerUDPKey);
				pServer->SetObfuscationPortTCP(nTCPObfuscationPort);
				pServer->SetObfuscationPortUDP(nUDPObfuscationPort);
				// if the received UDP flags do not match any already stored UDP flags,
				// reset the server version string because the version (which was determined by
				// last connecting to that server) is most likely not accurate any longer.
				// this may also give 'false' results because we don't know the UDP flags
				// when connecting to a server with TCP.
				//if (pServer->GetUDPFlags() != uUDPFlags)
				//	pServer->SetVersion(_T(""));
				pServer->SetUDPFlags(uUDPFlags);
				pServer->SetLowIDUsers(uLowIDUsers);
				theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);

				pServer->SetLastDescPingedCount(false);
				if (pServer->GetLastDescPingedCount() < 2) {
					// eserver 16.45+ supports a new OP_SERVER_DESC_RES answer, if the OP_SERVER_DESC_REQ
					// contains a uint32 challenge, the server returns additional info with OP_SERVER_DESC_RES.
					// To properly distinguish the old and new OP_SERVER_DESC_RES answer, the challenge
					// has to be selected carefully. The first 2 bytes of the challenge (in network byte order)
					// MUST NOT be a valid string-len-int16!
					Packet *packet1 = new Packet(OP_SERVER_DESC_REQ, 4);
					uint32 uDescReqChallenge = ((uint32)GetRandomUInt16() << 16) + INV_SERV_DESC_LEN; // 0xF0FF = an 'invalid' string length.
					pServer->SetDescReqChallenge(uDescReqChallenge);
					PokeUInt32(packet1->pBuffer, uDescReqChallenge);
					theStats.AddUpDataOverheadServer(packet1->size);
					if (thePrefs.GetDebugServerUDPLevel() > 0)
						Debug(_T(">>> Sending OP_ServDescReq     to server %s:%u, challenge %08x\n"), pServer->GetAddress(), pServer->GetPort(), uDescReqChallenge);
					theApp.serverconnect->SendUDPPacket(packet1, pServer, true);
				} else
					pServer->SetLastDescPingedCount(true);
			}
			break;
		case OP_SERVER_DESC_RES:
			{
				if (thePrefs.GetDebugServerUDPLevel() > 0)
					Debug(_T("ServerUDPMessage from %-21s - OP_ServerDescRes\n"), (LPCTSTR)ipstr(nIP, nUDPPort - 4));
				if (pServer == NULL)
					return true;

				// old packet: <name_len 2><name name_len><desc_len 2 desc_en>
				// new packet: <challenge 4><taglist>
				//
				// NOTE: To properly distinguish between the two packets which are both using the same opcode...
				// the first two bytes of <challenge> (in network byte order) have to be an invalid <name_len> at least.

				CSafeMemFile srvinfo(packet, size);
				if (size >= 8 && PeekUInt16(packet) == INV_SERV_DESC_LEN) {
					if (pServer->GetDescReqChallenge() != 0 && PeekUInt32(packet) == pServer->GetDescReqChallenge()) {
						pServer->SetDescReqChallenge(0);
						srvinfo.Seek(sizeof(uint32), SEEK_SET); // skip challenge

						for (uint32 uTags = srvinfo.ReadUInt32(); uTags > 0; --uTags) {
							CTag tag(&srvinfo, true/*pServer->GetUnicodeSupport()*/);
							if (tag.GetNameID() == ST_SERVERNAME && tag.IsStr())
								pServer->SetListName(tag.GetStr());
							else if (tag.GetNameID() == ST_DESCRIPTION && tag.IsStr())
								pServer->SetDescription(tag.GetStr());
							else if (tag.GetNameID() == ST_DYNIP && tag.IsStr()) {
								// Verify that we really received a DN.
								if (inet_addr((CStringA)tag.GetStr()) == INADDR_NONE) {
									const CString &strOldDynIP = pServer->GetDynIP();
									pServer->SetDynIP(tag.GetStr());
									// If a dynIP-server changed its address or, if this is the
									// first time we get the dynIP-address for a server which we
									// already have as non-dynIP in our list, we need to remove
									// an already available server with the same 'dynIP:port'.
									if (strOldDynIP.CompareNoCase(pServer->GetDynIP()) != 0)
										theApp.serverlist->RemoveDuplicatesByAddress(pServer);
								}
							} else if (tag.GetNameID() == ST_VERSION && tag.IsStr())
								pServer->SetVersion(tag.GetStr());
							else if (tag.GetNameID() == ST_VERSION && tag.IsInt()) {
								CString strVersion;
								strVersion.Format(_T("%u.%02u"), tag.GetInt() >> 16, tag.GetInt() & 0xffff);
								pServer->SetVersion(strVersion);
							} else if (tag.GetNameID() == ST_AUXPORTSLIST && tag.IsStr())
								// currently not implemented.
								; // <string> = <port> [, <port>...]
							else if (thePrefs.GetDebugServerUDPLevel() > 0)
								Debug(_T("***NOTE: Unknown tag in OP_ServerDescRes: %s\n"), (LPCTSTR)tag.GetFullInfo());
						}
					} else {
						// A server sent us a new server description packet (including a challenge) although we did not
						// ask for it. This may happen, if there are multiple servers running on the same machine with
						// multiple IPs. If such a server is asked for a description, the server will answer 2 times,
						// but with the same IP.

						if (thePrefs.GetDebugServerUDPLevel() > 0)
							Debug(_T("***NOTE: Received unexpected new format OP_ServerDescRes from %s with challenge %08x (waiting on packet with challenge %08x)\n"), (LPCTSTR)ipstr(nIP, nUDPPort - 4), PeekUInt32(packet), pServer->GetDescReqChallenge());
						; // ignore this packet
					}
				} else {
					const CString &strName = srvinfo.ReadString(true/*pServer->GetUnicodeSupport()*/);
					const CString &strDesc = srvinfo.ReadString(true/*pServer->GetUnicodeSupport()*/);
					pServer->SetDescription(strDesc);
					pServer->SetListName(strName);
				}

				if (thePrefs.GetDebugServerUDPLevel() > 0) {
					int iAddData = (int)(srvinfo.GetLength() - srvinfo.GetPosition());
					if (iAddData > 0) {
						Debug(_T("***NOTE: OP_ServerDescRes contains %d additional bytes\n"), iAddData);
						if (thePrefs.GetDebugServerUDPLevel() > 1)
							DebugHexDump(srvinfo);
					}
				}
				theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
			}
			break;
		default:
			if (thePrefs.GetDebugServerUDPLevel() > 0)
				Debug(_T("***NOTE: ServerUDPMessage from %s - Unknown packet: opcode=0x%02X  %s\n"), (LPCTSTR)ipstr(nIP, nUDPPort - 4), opcode, (LPCTSTR)DbgGetHexDump(packet, size));
			return false;
		}
		return true;
	} catch (CFileException *error) {
		TCHAR szError[MAX_CFEXP_ERRORMSG];
		error->m_strFileName = _T("server UDP packet");
		if (!GetExceptionMessage(*error, szError, _countof(szError)))
			szError[0] = _T('\0');
		ProcessPacketError(size, opcode, nIP, nUDPPort, szError);
		error->Delete();
		//ASSERT(0);
		if (opcode == OP_GLOBSEARCHRES || opcode == OP_GLOBFOUNDSOURCES)
			return true;
	} catch (CMemoryException *error) {
		TCHAR szError[MAX_CFEXP_ERRORMSG];
		if (!GetExceptionMessage(*error, szError, _countof(szError)))
			szError[0] = _T('\0');
		ProcessPacketError(size, opcode, nIP, nUDPPort, szError);
		error->Delete();
		//ASSERT(0);
		if (opcode == OP_GLOBSEARCHRES || opcode == OP_GLOBFOUNDSOURCES)
			return true;
	} catch (const CString &error) {
		ProcessPacketError(size, opcode, nIP, nUDPPort, error);
		//ASSERT(0);
#ifndef _DEBUG
	} catch (...) {
		ProcessPacketError(size, opcode, nIP, nUDPPort, _T("Unknown exception"));
		ASSERT(0);
#endif
	}
	return false;
}

void CUDPSocket::ProcessPacketError(UINT size, UINT opcode, uint32 nIP, uint16 nUDPPort, LPCTSTR pszError)
{
	if (thePrefs.GetVerbose()) {
		CString strName;
		CServer *pServer = theApp.serverlist->GetServerByIPUDP(nIP, nUDPPort);
		if (pServer)
			strName.Format(_T(" (%s)"), (LPCTSTR)pServer->GetListName());
		DebugLogWarning(LOG_DEFAULT, _T("Error: Failed to process server UDP packet from %s:%u%s opcode=0x%02x size=%u - %s"), (LPCTSTR)ipstr(nIP), nUDPPort, (LPCTSTR)strName, opcode, size, pszError);
	}
}

void CUDPSocket::DnsLookupDone(WPARAM wp, LPARAM lp)
{
	// A Winsock DNS task has completed. Search the according application data for that
	// task handle.
	SServerDNSRequest *pDNSReq = NULL;
	HANDLE hDNSTask = (HANDLE)wp;
	for (POSITION pos = m_aDNSReqs.GetHeadPosition(); pos != NULL;) {
		POSITION posPrev = pos;
		SServerDNSRequest *pCurDNSReq = m_aDNSReqs.GetNext(pos);
		if (pCurDNSReq->m_hDNSTask == hDNSTask) {
			// Remove this DNS task from our list
			m_aDNSReqs.RemoveAt(posPrev);
			pDNSReq = pCurDNSReq;
			break;
		}
	}
	if (pDNSReq == NULL) {
		if (thePrefs.GetVerbose())
			DebugLogError(_T("Error: Server UDP socket: Unknown DNS task completed"));
		return;
	}

	// DNS task did complete successfully?
	if (WSAGETASYNCERROR(lp) != 0) {
		if (thePrefs.GetVerbose())
			DebugLogWarning(_T("Error: Server UDP socket: Failed to resolve address for server '%s' (%s) - %s"), (LPCTSTR)pDNSReq->m_pServer->GetListName(), pDNSReq->m_pServer->GetAddress(), (LPCTSTR)GetErrorMessage(WSAGETASYNCERROR(lp), 1));
		delete pDNSReq;
		return;
	}

	// Get the IP value
	uint32 nIP = INADDR_NONE;
	WORD iBufLen = WSAGETASYNCBUFLEN(lp);
	if (iBufLen >= sizeof(HOSTENT)) {
		LPHOSTENT pHost = (LPHOSTENT)pDNSReq->m_DnsHostBuffer;
		if (pHost->h_length == 4 && pHost->h_addr_list && pHost->h_addr_list[0])
			nIP = ((LPIN_ADDR)(pHost->h_addr_list[0]))->s_addr;
	}

	if (nIP != INADDR_NONE) {
		DEBUG_ONLY(DebugLog(_T("Resolved DN for server '%s': IP=%s"), pDNSReq->m_pServer->GetAddress(), (LPCTSTR)ipstr(nIP)));

		bool bRemoveServer = false;
		if (!IsGoodIP(nIP)) {
			// However, if we are currently connected to a "not-good-ip", that IP can't
			// be that bad -- may only happen when debugging in a LAN.
			CServer *pConnectedServer = theApp.serverconnect->GetCurrentServer();
			if (!pConnectedServer || pConnectedServer->GetIP() != nIP) {
				if (thePrefs.GetLogFilteredIPs())
					AddDebugLogLine(false, _T("IPFilter(UDP/DNSResolve): Filtered server \"%s\" (IP=%s) - Invalid IP or LAN address."), pDNSReq->m_pServer->GetAddress(), (LPCTSTR)ipstr(nIP));
				bRemoveServer = true;
			}
		}
		if (!bRemoveServer && thePrefs.GetFilterServerByIP() && theApp.ipfilter->IsFiltered(nIP)) {
			if (thePrefs.GetLogFilteredIPs())
				AddDebugLogLine(false, _T("IPFilter(UDP/DNSResolve): Filtered server \"%s\" (IP=%s) - IP filter (%s)"), pDNSReq->m_pServer->GetAddress(), (LPCTSTR)ipstr(nIP), (LPCTSTR)theApp.ipfilter->GetLastHit());
			bRemoveServer = true;
		}

		CServer *pServer = theApp.serverlist->GetServerByAddress(pDNSReq->m_pServer->GetAddress(), pDNSReq->m_pServer->GetPort());
		if (pServer) {
			pServer->SetIP(nIP);
			// If we already have entries in the server list (dynIP-servers without a DN)
			// with the same IP as this dynIP-server, remove the duplicates.
			theApp.serverlist->RemoveDuplicatesByIP(pServer);
		}

		if (bRemoveServer) {
			if (pServer)
				theApp.emuledlg->serverwnd->serverlistctrl.RemoveServer(pServer);
			delete pDNSReq;
			return;
		}

		// Send all of the queued packets for this server.
		for (POSITION posPacket = pDNSReq->m_aPackets.GetHeadPosition(); posPacket;) {
			SRawServerPacket *pServerPacket = pDNSReq->m_aPackets.GetNext(posPacket);
			SendBuffer(nIP, pServerPacket->m_nPort, pServerPacket->m_pPacket, pServerPacket->m_uSize);
			// Detach packet data
			pServerPacket->m_pPacket = NULL;
			pServerPacket->m_uSize = 0;
		}
	} else {
		// still no valid IP for this server
		if (thePrefs.GetVerbose())
			DebugLogWarning(_T("Error: Server UDP socket: Failed to resolve address for server '%s' (%s)"), (LPCTSTR)pDNSReq->m_pServer->GetListName(), pDNSReq->m_pServer->GetAddress());
	}
	delete pDNSReq;
}

void CUDPSocket::OnSend(int nErrorCode)
{
	if (nErrorCode) {
		if (thePrefs.GetVerbose())
			DebugLogError(_T("Error: Server UDP socket: Failed to send packet - %s"), (LPCTSTR)GetErrorMessage(nErrorCode, 1));
		return;
	}
	m_bWouldBlock = false;

// ZZ:UploadBandWithThrottler (UDP) -->
	sendLocker.Lock();
	if (!controlpacket_queue.IsEmpty())
		theApp.uploadBandwidthThrottler->QueueForSendingControlPacket(this);

	sendLocker.Unlock();
// <-- ZZ:UploadBandWithThrottler (UDP)
}

SocketSentBytes CUDPSocket::SendControlData(uint32 maxNumberOfBytesToSend, uint32 /*minFragSize*/) // ZZ:UploadBandWithThrottler (UDP)
{
// ZZ:UploadBandWithThrottler (UDP) -->
	// NOTE: *** This function is invoked from a *different* thread!
	uint32 sentBytes = 0;

	sendLocker.Lock();

// <-- ZZ:UploadBandWithThrottler (UDP)
	while (!controlpacket_queue.IsEmpty() && !IsBusy() && sentBytes < maxNumberOfBytesToSend) { // ZZ:UploadBandWithThrottler (UDP)
		SServerUDPPacket *packet = controlpacket_queue.RemoveHead();
		int iLen = SendTo(packet->packet, packet->size, packet->dwIP, packet->nPort);
		if (iLen >= 0) {
			sentBytes += iLen; // ZZ:UploadBandWithThrottler (UDP)
			delete[] packet->packet;
			delete packet;
		} else {
			controlpacket_queue.AddHead(packet); //try to resend
			::Sleep(20);
		}
	}

// ZZ:UploadBandWithThrottler (UDP) -->
	if (!IsBusy() && !controlpacket_queue.IsEmpty())
		theApp.uploadBandwidthThrottler->QueueForSendingControlPacket(this);

	sendLocker.Unlock();

	return SocketSentBytes{0, sentBytes, true};
// <-- ZZ:UploadBandWithThrottler (UDP)
}

int CUDPSocket::SendTo(BYTE *lpBuf, int nBufLen, uint32 dwIP, uint16 nPort)
{
	// NOTE: *** This function is invoked from a *different* thread!
	//Currently called only locally; sendLocker must be locked by the caller
	int result = CAsyncSocket::SendTo(lpBuf, nBufLen, nPort, ipstr(dwIP));
	if (result == SOCKET_ERROR) {
		DWORD dwError = (DWORD)CAsyncSocket::GetLastError();
		if (dwError == WSAEWOULDBLOCK) {
			m_bWouldBlock = true;
			return -1; //blocked
		}
		if (thePrefs.GetVerbose())
			theApp.QueueDebugLogLine(false, _T("Error: Server UDP socket: Failed to send packet to %s:%u - %s"), (LPCTSTR)ipstr(dwIP), nPort, (LPCTSTR)GetErrorMessage(dwError, 1));
		return 0; //error
	}
	return result; //success
}

void CUDPSocket::SendBuffer(uint32 nIP, uint16 nPort, BYTE *pPacket, UINT uSize)
{
// ZZ:UploadBandWithThrottler (UDP) -->
	SServerUDPPacket *newpending = new SServerUDPPacket;
	newpending->dwIP = nIP;
	newpending->nPort = nPort;
	newpending->packet = pPacket;
	newpending->size = uSize;
	sendLocker.Lock();
	controlpacket_queue.AddTail(newpending);
	sendLocker.Unlock();
	theApp.uploadBandwidthThrottler->QueueForSendingControlPacket(this);
// <-- ZZ:UploadBandWithThrottler (UDP)
}

void CUDPSocket::SendPacket(Packet *packet, CServer *pServer, uint16 nSpecialPort, BYTE *pInRawPacket, uint32 nRawLen)
{
	// Just for safety. Ensure that there are no stalled DNS queries and/or packets
	// hanging endlessly in the queue.
	DWORD dwNow = ::GetTickCount();
	for (POSITION pos = m_aDNSReqs.GetHeadPosition(); pos != NULL;) {
		POSITION posPrev = pos;
		const SServerDNSRequest *pDNSReq = m_aDNSReqs.GetNext(pos);
		if (dwNow >= pDNSReq->m_dwCreated + MIN2MS(2)) {
			delete pDNSReq;
			m_aDNSReqs.RemoveAt(posPrev);
		}
	}

	// Create raw UDP packet
	BYTE *pRawPacket;
	uint32 uRawPacketSize;
	uint16 nPort = nSpecialPort;
	if (packet != NULL) {
		uRawPacketSize = packet->size + 2;
		size_t iLen = thePrefs.IsServerCryptLayerUDPEnabled() && pServer->GetServerKeyUDP() && pServer->SupportsObfuscationUDP()
			? EncryptOverheadSize(false) : 0;
		pRawPacket = new BYTE[uRawPacketSize + iLen];
		memcpy(pRawPacket + iLen, packet->GetUDPHeader(), 2);
		memcpy(pRawPacket + iLen + 2, packet->pBuffer, packet->size);
		if (iLen) {
			uRawPacketSize = EncryptSendServer(pRawPacket, uRawPacketSize, pServer->GetServerKeyUDP());
			if (thePrefs.GetDebugServerUDPLevel() > 0)
				DEBUG_ONLY(DebugLog(_T("Sending encrypted packet to server %s, UDPKey %u"), (LPCTSTR)pServer->GetListName(), pServer->GetServerKeyUDP()));
			if (!nPort)
				nPort = pServer->GetObfuscationPortUDP();
		}
	} else if (pInRawPacket != 0) {
		// we don't encrypt raw packets (!)
		pRawPacket = new BYTE[nRawLen];
		memcpy(pRawPacket, pInRawPacket, nRawLen);
		uRawPacketSize = nRawLen;
	} else {
		ASSERT(0);
		return;
	}
	if (!nPort)
		nPort = pServer->GetPort() + 4;
	ASSERT(nPort != 0);

	// Do we need to resolve the DN of this server?
	CStringA pszHostAddressA(pServer->GetAddress());
	uint32 nIP = inet_addr(pszHostAddressA);
	if (nIP == INADDR_NONE) {
		// If there is already a DNS query ongoing or queued for this server, append the
		// current packet to this DNS query. The packet(s) will be sent later after the DNS
		// query has completed.
		for (POSITION reqpos = m_aDNSReqs.GetHeadPosition(); reqpos != NULL;) {
			SServerDNSRequest *pDNSReq = m_aDNSReqs.GetNext(reqpos);
			if (_tcsicmp(pDNSReq->m_pServer->GetAddress(), pServer->GetAddress()) == 0) {
				SRawServerPacket *pServerPacket = new SRawServerPacket(pRawPacket, uRawPacketSize, nPort);
				pDNSReq->m_aPackets.AddTail(pServerPacket);
				return;
			}
		}

		// Create a new DNS query for this server
		SServerDNSRequest *pDNSReq = new SServerDNSRequest(0, new CServer(pServer));
		pDNSReq->m_hDNSTask = WSAAsyncGetHostByName(m_hWndResolveMessage, WM_DNSLOOKUPDONE,
			pszHostAddressA, pDNSReq->m_DnsHostBuffer, sizeof pDNSReq->m_DnsHostBuffer);
		if (pDNSReq->m_hDNSTask == NULL) {
			if (thePrefs.GetVerbose())
				DebugLogWarning(_T("Error: Server UDP socket: Failed to resolve address for '%s' - %s"), (LPCSTR)pServer->GetAddress(), (LPCTSTR)GetErrorMessage(CAsyncSocket::GetLastError(), 1));
			delete pDNSReq;
			delete[] pRawPacket;
			return;
		}
		SRawServerPacket *pServerPacket = new SRawServerPacket(pRawPacket, uRawPacketSize, nPort);
		pDNSReq->m_aPackets.AddTail(pServerPacket);
		m_aDNSReqs.AddTail(pDNSReq);
	} else {
		// No DNS query needed for this server. Just send the packet.
		SendBuffer(nIP, nPort, pRawPacket, uRawPacketSize);
	}
}