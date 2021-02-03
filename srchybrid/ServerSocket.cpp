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
#include "ServerSocket.h"
#include "SearchList.h"
#include "DownloadQueue.h"
#include "Statistics.h"
#include "ClientList.h"
#include "Server.h"
#include "ServerList.h"
#include "ServerConnect.h"
#include "OtherFunctions.h"
#include "Opcodes.h"
#include "Preferences.h"
#include "SafeFile.h"
#include "PartFile.h"
#include "Packets.h"
#include "UpDownClient.h"
#include "emuleDlg.h"
#include "ServerWnd.h"
#include "SearchDlg.h"
#include "IPFilter.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#pragma pack(push, 1)
struct LoginAnswer_Struct
{
	uint32	clientid;
};
#pragma pack(pop)


CServerSocket::CServerSocket(CServerConnect *in_serverconnect, bool bManualSingleConnect)
	: serverconnect(in_serverconnect)
	, cur_server()
	, m_dwLastTransmission()
	, connectionstate(CS_NOTCONNECTED)
	, m_bIsDeleting()
	, m_bStartNewMessageLog(true)
	, m_bManualSingleConnect(bManualSingleConnect)
{
}

CServerSocket::~CServerSocket()
{
	delete cur_server;
}

bool CServerSocket::OnHostNameResolved(const SOCKADDR_IN *pSockAddr)
{
	// If we are connecting to a dynIP-server by DN, we will get this callback after the
	// DNS query finished.
	//
	if (cur_server->HasDynIP()) {
		// Update the IP of this dynIP-server
		//
		cur_server->SetIP(pSockAddr->sin_addr.s_addr);
		CServer *pServer = theApp.serverlist->GetServerByAddress(cur_server->GetAddress(), cur_server->GetPort());
		if (pServer) {
			pServer->SetIP(pSockAddr->sin_addr.s_addr);
			// If we already have entries in the server list (dynIP-servers without a DN)
			// with the same IP as this dynIP-server, remove the duplicates.
			theApp.serverlist->RemoveDuplicatesByIP(pServer);
		}
		DEBUG_ONLY(DebugLog(_T("Resolved DN for server '%s': IP=%s"), cur_server->GetAddress(), (LPCTSTR)ipstr(cur_server->GetIP())));

		// As this is a dynIP-server, we need to check the IP against the IP-filter
		// and eventually disconnect and delete that server.
		//
		if (thePrefs.GetFilterServerByIP() && theApp.ipfilter->IsFiltered(cur_server->GetIP())) {
			m_bIsDeleting = true;
			SetConnectionState(CS_ERROR);
			if (thePrefs.GetLogFilteredIPs())
				AddDebugLogLine(false, _T("IPFilter(TCP/DNSResolve): Filtered server \"%s\" (IP=%s) - IP filter (%s)"), pServer ? pServer->GetAddress() : cur_server->GetAddress(), (LPCTSTR)ipstr(cur_server->GetIP()), (LPCTSTR)theApp.ipfilter->GetLastHit());
			if (pServer)
				theApp.emuledlg->serverwnd->serverlistctrl.RemoveServer(pServer);
			serverconnect->DestroySocket(this);
			return false;	// Do *NOT* connect to this server
		}
	}
	return true; // Connect to this server
}

void CServerSocket::OnConnect(int nErrorCode)
{
	CAsyncSocketEx::OnConnect(nErrorCode);
	int state;
	switch (nErrorCode) {
	case 0:
		SetConnectionState(CS_WAITFORLOGIN);
		return;

	case WSAEADDRNOTAVAIL:
	case WSAECONNREFUSED:
	//case WSAENETUNREACH:	// let this error default to 'fatal error' as it does not increase the server's failed count
	case WSAETIMEDOUT:
	case WSAEADDRINUSE:
		state = CS_SERVERDEAD;
		break;
	case WSAECONNABORTED:
		if (m_bProxyConnectFailed) {
			m_bProxyConnectFailed = false;
			state = CS_SERVERDEAD;
			break;
		}
		/* fall through */
	default:
		state = CS_FATALERROR;
	}
	m_bIsDeleting = true;
	SetConnectionState(state);
	if (thePrefs.GetVerbose())
		DebugLogError(_T("Failed to connect to server %s; %s"), cur_server->GetAddress(), (LPCTSTR)GetFullErrorMessage(nErrorCode));
	serverconnect->DestroySocket(this);
}

void CServerSocket::OnReceive(int nErrorCode)
{
	if (connectionstate != CS_CONNECTED && !serverconnect->IsConnecting())
		serverconnect->DestroySocket(this);
	else {
		CEMSocket::OnReceive(nErrorCode);
		m_dwLastTransmission = ::GetTickCount();
	}
}

bool CServerSocket::ProcessPacket(const BYTE *packet, uint32 size, uint8 opcode)
{
	try {
		switch (opcode) {
		case OP_SERVERMESSAGE:
			{
				if (thePrefs.GetDebugServerTCPLevel() > 0)
					Debug(_T("ServerMsg - OP_ServerMessage\n"));

				CServer *pServer = cur_server ? theApp.serverlist->GetServerByAddress(cur_server->GetAddress(), cur_server->GetPort()) : NULL;
				CSafeMemFile data(packet, size);
				const CString &strMessages(data.ReadString(pServer && pServer->GetUnicodeSupport()));

				if (thePrefs.GetDebugServerTCPLevel() > 0) {
					UINT uAddData = (UINT)(data.GetLength() - data.GetPosition());
					if (uAddData > 0) {
						Debug(_T("*** NOTE: OP_ServerMessage: ***AddData: %u bytes\n"), uAddData);
						DebugHexDump(packet + data.GetPosition(), uAddData);
					}
				}

				// 16.40 servers do not send separate OP_SERVERMESSAGE packets for each line;
				// instead, they are sending all text lines in one OP_SERVERMESSAGE packet.
				for (int iPos = 0; iPos >= 0;) {
					const CString &message(strMessages.Tokenize(_T("\r\n"), iPos));
					if (message.IsEmpty())
						break;
					bool bOutputMessage = true;
					if (_tcsnicmp(message, _T("server version"), 14) == 0) {
						if (pServer) {
							// truncate string to avoid misuse by servers in showing ads
							CString strVer(message.Mid(14).Trim().Left(64));
							UINT nVerMaj, nVerMin;
							if (_stscanf(strVer, _T("%u.%u"), &nVerMaj, &nVerMin) == 2)
								strVer.Format(_T("%u.%02u"), nVerMaj, nVerMin);
							pServer->SetVersion(strVer);
							theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
							theApp.emuledlg->serverwnd->UpdateMyInfo();
						}
						if (thePrefs.GetDebugServerTCPLevel() > 0)
							Debug(_T("%s\n"), (LPCTSTR)message);
					} else if (_tcsncmp(message, _T("ERROR"), 5) == 0) {
						LogError(LOG_STATUSBAR, _T("%s %s (%s:%u) - %s")
							, (LPCTSTR)GetResString(IDS_ERROR)
							, pServer ? (LPCTSTR)pServer->GetListName() : (LPCTSTR)GetResString(IDS_PW_SERVER)
							, cur_server ? cur_server->GetAddress() : _T("")
							, cur_server ? cur_server->GetPort() : 0, (LPCTSTR)message.Mid(5).Trim(_T(" :")));
						bOutputMessage = false;
					} else if (_tcsncmp(message, _T("WARNING"), 7) == 0) {
						LogWarning(LOG_STATUSBAR, _T("%s %s (%s:%u) - %s")
							, (LPCTSTR)GetResString(IDS_WARNING)
							, pServer ? (LPCTSTR)pServer->GetListName() : (LPCTSTR)GetResString(IDS_PW_SERVER)
							, cur_server ? cur_server->GetAddress() : _T("")
							, cur_server ? cur_server->GetPort() : 0, (LPCTSTR)message.Mid(7).Trim(_T(" :")));
						bOutputMessage = false;
					}

					static const TCHAR sDynIP[] = _T("[emDynIP: ");
					int iDynIP = message.Find(sDynIP);
					if (iDynIP >= 0) {
						iDynIP += _countof(sDynIP) - 1;
						int iBracket = message.Find(_T(']'), iDynIP);
						if (iBracket > 0) {
							const CString dynip(message.Mid(iDynIP, iBracket - iDynIP).Trim());
							if (!dynip.IsEmpty() && dynip.GetLength() < 51) {
								// Verify that we really received a DN.
								if (pServer && inet_addr((CStringA)dynip) == INADDR_NONE) {
									// Update the dynIP of this server, but do not reset it's IP
									// which we just determined during connecting.
									const CString &strOldDynIP(pServer->GetDynIP());
									pServer->SetDynIP(dynip);
									// If a dynIP-server changed its address, or if this is the
									// first time we get the dynIP-address for a server which we
									// already have as non-dynIP in our list, we need to remove
									// an already available server with the same 'dynIP:port'.
									if (strOldDynIP.CompareNoCase(pServer->GetDynIP()) != 0)
										theApp.serverlist->RemoveDuplicatesByAddress(pServer);
									if (cur_server)
										cur_server->SetDynIP(dynip);
									theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
									theApp.emuledlg->serverwnd->UpdateMyInfo();
								}
							}
						}
					}

					if (bOutputMessage) {
						if (m_bStartNewMessageLog) {
							m_bStartNewMessageLog = false;
							theApp.emuledlg->AddServerMessageLine(LOG_INFO, _T(""));
							if (cur_server) {
								CString strMsg;
								strMsg.Format(_T("%s: ") + GetResString(IsObfusicating() ? IDS_CONNECTEDTOOBFUSCATED : IDS_CONNECTEDTO) + _T(" (%s:%u)")
									, (LPCTSTR)CTime::GetCurrentTime().Format(thePrefs.GetDateTimeFormat4Log())
									, (LPCTSTR)cur_server->GetListName()
									, cur_server->GetAddress()
									, IsObfusicating() ? cur_server->GetObfuscationPortTCP() : cur_server->GetPort());
								theApp.emuledlg->AddServerMessageLine(LOG_SUCCESS, strMsg);
							}
						}
						theApp.emuledlg->AddServerMessageLine(LOG_INFO, message);
					}
				}
			}
			break;
		case OP_IDCHANGE:
			{
				if (thePrefs.GetDebugServerTCPLevel() > 0)
					Debug(_T("ServerMsg - OP_IDChange\n"));
				if (size < sizeof(LoginAnswer_Struct))
					throw GetResString(IDS_ERR_BADSERVERREPLY);

				const LoginAnswer_Struct *la = reinterpret_cast<const LoginAnswer_Struct*>(packet);

				// save TCP flags in 'cur_server'
				CServer *pServer;
				if (cur_server) {
					if (size >= sizeof(LoginAnswer_Struct) + 4) {
						uint32 dwFlags = PeekUInt32(packet + sizeof(LoginAnswer_Struct));
						if (thePrefs.GetDebugServerTCPLevel() > 0) {
							CString strInfo;
							strInfo.Format(_T("  TCP Flags=0x%08x"), dwFlags);
							const uint32 dwKnownBits = SRV_TCPFLG_COMPRESSION | SRV_TCPFLG_NEWTAGS | SRV_TCPFLG_UNICODE | SRV_TCPFLG_RELATEDSEARCH | SRV_TCPFLG_TYPETAGINTEGER | SRV_TCPFLG_LARGEFILES | SRV_TCPFLG_TCPOBFUSCATION;
							if (dwFlags & ~dwKnownBits)
								strInfo.AppendFormat(_T("  ***UnkBits=0x%08x"), dwFlags & ~dwKnownBits);
							if (dwFlags & SRV_TCPFLG_COMPRESSION)
								strInfo.AppendFormat(_T("  Compression=1"));
							if (dwFlags & SRV_TCPFLG_NEWTAGS)
								strInfo.AppendFormat(_T("  NewTags=1"));
							if (dwFlags & SRV_TCPFLG_UNICODE)
								strInfo.AppendFormat(_T("  Unicode=1"));
							if (dwFlags & SRV_TCPFLG_RELATEDSEARCH)
								strInfo.AppendFormat(_T("  RelatedSearch=1"));
							if (dwFlags & SRV_TCPFLG_TYPETAGINTEGER)
								strInfo.AppendFormat(_T("  IntTypeTags=1"));
							if (dwFlags & SRV_TCPFLG_LARGEFILES)
								strInfo.AppendFormat(_T("  LargeFiles=1"));
							if (dwFlags & SRV_TCPFLG_TCPOBFUSCATION)
								strInfo.AppendFormat(_T("  TCP_Obfscation=1"));
							Debug(_T("%s\n"), (LPCTSTR)strInfo);
						}
						cur_server->SetTCPFlags(dwFlags);
					} else
						cur_server->SetTCPFlags(0);

					// copy TCP flags into the server in the server list
					pServer = theApp.serverlist->GetServerByAddress(cur_server->GetAddress(), cur_server->GetPort());
					if (pServer) {
						if (IsObfusicating())
							pServer->SetTriedCrypt(false);
						pServer->SetTCPFlags(cur_server->GetTCPFlags());
					}
				} else {
					pServer = NULL;
					ASSERT(0);
				}

				uint32 dwServerReportedIP;
				if (size >= 16) {
					dwServerReportedIP = PeekUInt32(packet + 12);
					if (::IsLowID(dwServerReportedIP)) {
						ASSERT(0);
						dwServerReportedIP = 0;
					}
					ASSERT(dwServerReportedIP == la->clientid || ::IsLowID(la->clientid));
				} else
					dwServerReportedIP = 0;

				if (la->clientid == 0) {
					uint8 state = thePrefs.GetSmartIdState();
					if (state > 0) {
						if (state++ == 1)
							theApp.emuledlg->RefreshUPnP(false); // refresh the UPnP mappings once
						thePrefs.SetSmartIdState(state > 2 ? 0 : state);
					}
					break;
				}
				if (thePrefs.GetSmartIdCheck())
					if (!::IsLowID(la->clientid))
						thePrefs.SetSmartIdState(1);
					else {
						uint8 state = thePrefs.GetSmartIdState();
						if (state > 0) {
							if (state++ == 1)
								theApp.emuledlg->RefreshUPnP(false); // refresh the UPnP mappings once
							thePrefs.SetSmartIdState(state > 2 ? 0 : state);
							// if this is a connect to any/multiple server connection try, disconnect and try another one
							if (!m_bManualSingleConnect)
								break;
						}
					}

				// we need to know our client's HighID when sending our shared files (done indirectly in SetConnectionState)
				serverconnect->m_clientid = la->clientid;

				if (connectionstate != CS_CONNECTED) {
					SetConnectionState(CS_CONNECTED);
					theApp.OnlineSig();       // Added By Bouc7
				}
				serverconnect->SetClientID(la->clientid);
				if (::IsLowID(la->clientid) && dwServerReportedIP != 0)
					theApp.SetPublicIP(dwServerReportedIP);
				AddLogLine(false, GetResString(IDS_NEWCLIENTID), la->clientid);

				theApp.downloadqueue->ResetLocalServerRequests();
			}
			break;
		case OP_SEARCHRESULT:
			{
				if (thePrefs.GetDebugServerTCPLevel() > 0)
					Debug(_T("ServerMsg - OP_SearchResult\n"));
				CServer *cur_srv = (serverconnect) ? serverconnect->GetCurrentServer() : NULL;
				//CServer *pServer = cur_srv ? theApp.serverlist->GetServerByAddress(cur_srv->GetAddress(), cur_srv->GetPort()) : NULL;
				//(void)pServer;
				bool bMoreResultsAvailable;
				UINT uSearchResults = theApp.searchlist->ProcessSearchAnswer(packet, size, true/*pServer ? pServer->GetUnicodeSupport() : false*/, cur_srv ? cur_srv->GetIP() : 0, cur_srv ? cur_srv->GetPort() : (uint16)0, &bMoreResultsAvailable);
				theApp.emuledlg->searchwnd->LocalEd2kSearchEnd(uSearchResults, bMoreResultsAvailable);
				break;
			}
		case OP_FOUNDSOURCES_OBFU:
		case OP_FOUNDSOURCES:
			if (thePrefs.GetDebugServerTCPLevel() > 0)
				Debug(_T("ServerMsg - OP_FoundSources%s; Sources=%u  %s\n"), (opcode == OP_FOUNDSOURCES_OBFU) ? _T("_OBFU") : _T(""), (UINT)packet[16], (LPCTSTR)DbgGetFileInfo(packet));

			ASSERT(cur_server);
			if (cur_server) {
				CSafeMemFile sources(packet, size);
				uchar fileid[MDX_DIGEST_SIZE];
				sources.ReadHash16(fileid);
				CPartFile *file = theApp.downloadqueue->GetFileByID(fileid);
				if (file)
					file->AddSources(&sources, cur_server->GetIP(), cur_server->GetPort(), (opcode == OP_FOUNDSOURCES_OBFU));
			}
			break;
		case OP_SERVERSTATUS:
			{
				if (thePrefs.GetDebugServerTCPLevel() > 0)
					Debug(_T("ServerMsg - OP_ServerStatus\n"));
				// FIXME some status packets have a different size -> why? structure?
				if (size < 8)
					break; //throw "Invalid status packet";
				uint32 cur_user = PeekUInt32(packet);
				uint32 cur_files = PeekUInt32(packet + 4);
				CServer *pServer = cur_server ? theApp.serverlist->GetServerByAddress(cur_server->GetAddress(), cur_server->GetPort()) : NULL;
				if (pServer) {
					pServer->SetUserCount(cur_user);
					pServer->SetFileCount(cur_files);
					theApp.emuledlg->ShowUserCount();
					theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
					theApp.emuledlg->serverwnd->UpdateMyInfo();
				}
				if (thePrefs.GetDebugServerTCPLevel() > 0) {
					if (size > 8) {
						Debug(_T("*** NOTE: OP_ServerStatus: ***AddData: %u bytes\n"), size - 8);
						DebugHexDump(packet + 8, size - 8);
					}
				}
			}
			break;
		case OP_SERVERIDENT:
			{
				// OP_SERVERIDENT - this is sent by the server only if we send an OP_GETSERVERLIST
				if (thePrefs.GetDebugServerTCPLevel() > 0)
					Debug(_T("ServerMsg - OP_ServerIdent\n"));
				if (size < 16 + 4 + 2 + 4) {
					if (thePrefs.GetVerbose())
						DebugLogError((LPCTSTR)GetResString(IDS_ERR_KNOWNSERVERINFOREC));
					break;// throw "Invalid server info received";
				}

				CServer *pServer = cur_server ? theApp.serverlist->GetServerByAddress(cur_server->GetAddress(), cur_server->GetPort()) : NULL;
				CString strInfo;
				CSafeMemFile data(packet, size);

				uint8 aucHash[16];
				data.ReadHash16(aucHash);
				if (thePrefs.GetDebugServerTCPLevel() > 0)
					strInfo.AppendFormat(_T("Hash=%s (%s)"), (LPCTSTR)md4str(aucHash), DbgGetHashTypeString(aucHash));
				uint32 nServerIP = data.ReadUInt32();
				uint16 nServerPort = data.ReadUInt16();
				if (thePrefs.GetDebugServerTCPLevel() > 0)
					strInfo.AppendFormat(_T("  IP=%s:%u"), (LPCTSTR)ipstr(nServerIP), nServerPort);
				uint32 nTags = data.ReadUInt32();
				if (thePrefs.GetDebugServerTCPLevel() > 0)
					strInfo.AppendFormat(_T("  Tags=%u"), nTags);

				CString strName;
				CString strDescription;
				for (uint32 i = 0; i < nTags; ++i) {
					CTag tag(&data, pServer ? pServer->GetUnicodeSupport() : false);
					if (tag.GetNameID() == ST_SERVERNAME) {
						if (tag.IsStr()) {
							strName = tag.GetStr();
							if (thePrefs.GetDebugServerTCPLevel() > 0)
								strInfo.AppendFormat(_T("  Name=%s"), (LPCTSTR)strName);
						}
					} else if (tag.GetNameID() == ST_DESCRIPTION) {
						if (tag.IsStr()) {
							strDescription = tag.GetStr();
							if (thePrefs.GetDebugServerTCPLevel() > 0)
								strInfo.AppendFormat(_T("  Desc=%s"), (LPCTSTR)strDescription);
						}
					} else if (thePrefs.GetDebugServerTCPLevel() > 0)
						strInfo.AppendFormat(_T("  ***UnkTag: 0x%02x=%u"), tag.GetNameID(), tag.GetInt());
				}
				if (thePrefs.GetDebugServerTCPLevel() > 0) {
					Debug(_T("%s\n"), (LPCTSTR)strInfo);

					UINT uAddData = (UINT)(data.GetLength() - data.GetPosition());
					if (uAddData > 0) {
						Debug(_T("*** NOTE: OP_ServerIdent: ***AddData: %u bytes\n"), uAddData);
						DebugHexDump(packet + data.GetPosition(), uAddData);
					}
				}

				if (pServer) {
					pServer->SetListName(strName);
					pServer->SetDescription(strDescription);
					if (*(uint32*)aucHash == 0x2A2A2A2Au)
						pServer->SetVersion(_T("eFarm ") + pServer->GetVersion());
					theApp.emuledlg->ShowConnectionState();
					theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
				}
			}
			break;
		// tecxx 1609 2002 - add server's serverlist to own serverlist
		case OP_SERVERLIST:
			if (!thePrefs.GetAddServersFromServer())
				break;
			if (thePrefs.GetDebugServerTCPLevel() > 0)
				Debug(_T("ServerMsg - OP_ServerList\n"));
			try {
				CSafeMemFile servers(packet, size);
				UINT count = servers.ReadUInt8();
				// check if packet is valid
				if (1 + count * (4 + 2) <= size) {
					int addcount = 0;
					for (; count; --count) {
						uint32 ip = servers.ReadUInt32();
						uint16 port = servers.ReadUInt16();
						CServer *srv = new CServer(port, ipstr(ip));
						srv->SetListName(srv->GetFullIP());
						srv->SetPreference(SRV_PR_LOW);
						if (!theApp.emuledlg->serverwnd->serverlistctrl.AddServer(srv, true))
							delete srv;
						else
							++addcount;
					}
					if (addcount)
						AddLogLine(false, GetResString(IDS_NEWSERVERS), addcount);
				}
				if (thePrefs.GetDebugServerTCPLevel() > 0) {
					UINT uAddData = (UINT)(servers.GetLength() - servers.GetPosition());
					if (uAddData > 0) {
						Debug(_T("*** NOTE: OP_ServerList: ***AddData: %u bytes\n"), uAddData);
						DebugHexDump(packet + servers.GetPosition(), uAddData);
					}
				}
			} catch (CFileException *error) {
				if (thePrefs.GetVerbose())
					DebugLogError((LPCTSTR)GetResString(IDS_ERR_BADSERVERLISTRECEIVED));
				error->Delete();
			}
			break;
		case OP_CALLBACKREQUESTED:
			if (thePrefs.GetDebugServerTCPLevel() > 0)
				Debug(_T("ServerMsg - OP_CallbackRequested: %s\n"), (size >= 23) ? _T("With Cryptflag and Userhash") : _T("Without Cryptflag and Userhash"));
			if (size >= 6) {
				uint32 dwIP = PeekUInt32(packet);

				if (theApp.ipfilter->IsFiltered(dwIP)) {
					theStats.filteredclients++;
					if (thePrefs.GetLogFilteredIPs())
						AddDebugLogLine(false, _T("Ignored callback request (IP=%s) - IP filter (%s)"), (LPCTSTR)ipstr(dwIP), (LPCTSTR)theApp.ipfilter->GetLastHit());
					break;
				}

				if (theApp.clientlist->IsBannedClient(dwIP)) {
					if (thePrefs.GetLogBannedClients()) {
						CUpDownClient *pClient = theApp.clientlist->FindClientByIP(dwIP);
						AddDebugLogLine(false, _T("Ignored callback request from banned client %s; %s"), (LPCTSTR)ipstr(dwIP), pClient ? (LPCTSTR)pClient->DbgGetClientInfo() : _T(""));
					}
					break;
				}

				uint16 nPort = PeekUInt16(packet + 4);
				CUpDownClient *client = theApp.clientlist->FindClientByConnIP(dwIP, nPort);
				if (client == NULL) {
					client = new CUpDownClient(0, nPort, dwIP, 0, 0, true);
					theApp.clientlist->AddClient(client);
				}

				if (size >= 23) {
					uint8 byCryptOptions = packet[6];
				    uchar achUserHash[16];
					md4cpy(achUserHash, packet + 7);

					if (client->HasValidHash()) {
						if (md4equ(client->GetUserHash(), achUserHash)) {
							client->SetConnectOptions(byCryptOptions, true, false);
							client->SetDirectUDPCallbackSupport(false);
						} else {
							DebugLogError(_T("Reported Userhash from OP_CALLBACKREQUESTED differs with our stored hash"));
							// disable crypt support since we don't know which hash is true
							client->SetCryptLayerRequest(false);
							client->SetCryptLayerSupport(false);
							client->SetCryptLayerRequires(false);
						}
					} else {
						client->SetUserHash(achUserHash);
						client->SetCryptLayerSupport((byCryptOptions & 0x01) != 0);
						client->SetCryptLayerRequest((byCryptOptions & 0x02) != 0);
						client->SetCryptLayerRequires((byCryptOptions & 0x04) != 0);
						client->SetDirectUDPCallbackSupport(false);
					}
				}

				client->TryToConnect();
			}
			break;
		case OP_CALLBACK_FAIL:
			if (thePrefs.GetDebugServerTCPLevel() > 0)
				Debug(_T("ServerMsg - OP_Callback_Fail %s\n"), (LPCTSTR)DbgGetHexDump(packet, size));
			break;
		case OP_REJECT:
			if (thePrefs.GetDebugServerTCPLevel() > 0)
				Debug(_T("ServerMsg - OP_Reject %s\n"), (LPCTSTR)DbgGetHexDump(packet, size));
			// this could happen if we send a command with the wrong protocol (e.g. sending a compressed packet to
			// a server which does not support that protocol).
			if (thePrefs.GetVerbose())
				DebugLogError(_T("Server rejected last command"));
			break;
		default:
			if (thePrefs.GetDebugServerTCPLevel() > 0)
				Debug(_T("***NOTE: ServerMsg - Unknown message; opcode=0x%02x  %s\n"), opcode, (LPCTSTR)DbgGetHexDump(packet, size));
		}

		return true;
	} catch (CFileException *error) {
		if (thePrefs.GetVerbose()) {
			TCHAR szError[MAX_CFEXP_ERRORMSG];
			error->m_strFileName = _T("server packet");
			GetExceptionMessage(*error, szError, _countof(szError));
			ProcessPacketError(size, opcode, szError);
		}
		ASSERT(0);
		error->Delete();
		if (opcode == OP_SEARCHRESULT || opcode == OP_FOUNDSOURCES)
			return true;
	} catch (CMemoryException *error) {
		ProcessPacketError(size, opcode, _T("CMemoryException"));
		ASSERT(0);
		error->Delete();
		if (opcode == OP_SEARCHRESULT || opcode == OP_FOUNDSOURCES)
			return true;
	} catch (const CString &error) {
		ProcessPacketError(size, opcode, error);
		ASSERT(0);
	}
#ifndef _DEBUG
	catch (...) {
		ProcessPacketError(size, opcode, _T("Unknown exception"));
		ASSERT(0);
	}
#endif

	SetConnectionState(CS_DISCONNECTED);
	return false;
}

void CServerSocket::ProcessPacketError(UINT size, UINT opcode, LPCTSTR pszError)
{
	if (thePrefs.GetVerbose()) {
		CString strServer;
		try {
			if (cur_server)
				strServer.Format(_T("%s:%u"), cur_server->GetAddress(), cur_server->GetPort());
			else
				strServer = _T("Unknown");
		} catch (...) {
		}
		DebugLogWarning(LOG_DEFAULT, _T("Error: Failed to process server TCP packet from %s: opcode=0x%02x size=%u - %s"), (LPCTSTR)strServer, opcode, size, pszError);
	}
}

void CServerSocket::ConnectTo(CServer *server, bool bNoCrypt)
{
	if (cur_server) {
		ASSERT(0);
		delete cur_server;
		cur_server = NULL;
	}

	cur_server = new CServer(server);
	bool bEncrypt = !bNoCrypt && thePrefs.IsServerCryptLayerTCPRequested() && (cur_server->SupportsObfuscationTCP() || !cur_server->TriedCrypt());
	uint16 nPort;
	if (bEncrypt) {
		server->SetTriedCrypt(!server->SupportsObfuscationTCP()); //if no obfuscation support, try only once
		nPort = cur_server->GetObfuscationPortTCP();
		if (!nPort)
			nPort = cur_server->GetPort();
	} else
		nPort = cur_server->GetPort();

	Log(GetResString(bEncrypt ? IDS_CONNECTINGTOOBFUSCATED : IDS_CONNECTINGTO), (LPCTSTR)cur_server->GetListName(), cur_server->GetAddress(), nPort);
	SetConnectionEncryption(bEncrypt, NULL, true);

	// IP-filter: We do not need to IP-filter any servers here, even dynIP-servers are not
	// needed to get filtered here.
	//	1.) Non dynIP-servers were already IP-filtered when they were added to the server
	//		list.
	//	2.) Whenever the IP-filter is updated all servers for which an IP is known (this
	//		includes also dynIP-servers for which we received already an IP) get filtered.
	//	3.)	dynIP-servers get filtered after their DN was resolved. For TCP-connections this
	//		is done in OnConnect. For outgoing UDP packets this is done when explicitly
	//		resolving the DN right before sending the UDP packet.
	//
	SetConnectionState(CS_CONNECTING);
	if (!Connect(server->GetAddress(), nPort)) {
		DWORD dwError = CAsyncSocket::GetLastError();
		if (dwError != WSAEWOULDBLOCK) {
			LogError(GetResString(IDS_ERR_CONNECTIONERROR), (LPCTSTR)cur_server->GetListName(), cur_server->GetAddress(), nPort, (LPCTSTR)GetFullErrorMessage(dwError));
			SetConnectionState(CS_FATALERROR);
		}
	}
}

void CServerSocket::OnError(int nErrorCode)
{
	SetConnectionState(CS_DISCONNECTED);
	if (thePrefs.GetVerbose())
		DebugLogError(GetResString(IDS_ERR_SOCKET), (LPCTSTR)cur_server->GetListName(), cur_server->GetAddress(), cur_server->GetPort(), (LPCTSTR)GetFullErrorMessage(nErrorCode));
}

bool CServerSocket::PacketReceived(Packet *packet)
{
#ifndef _DEBUG
	try {
#endif
		theStats.AddDownDataOverheadServer(packet->size);
		if (packet->prot == OP_PACKEDPROT) {
			uint32 uComprSize = packet->size;
			if (!packet->UnPackPacket(250000)) {
				if (thePrefs.GetVerbose())
					DebugLogError(_T("Failed to decompress server TCP packet: protocol=0x%02x  opcode=0x%02x  size=%u"), packet->prot, packet->opcode, packet->size);
				return true;
			}
			packet->prot = OP_EDONKEYPROT;
			if (thePrefs.GetDebugServerTCPLevel() > 1)
				Debug(_T("Received compressed server TCP packet; opcode=0x%02x  size=%u  uncompr size=%u\n"), packet->opcode, uComprSize, packet->size);
		}

		if (packet->prot == OP_EDONKEYPROT)
			ProcessPacket((BYTE*)packet->pBuffer, packet->size, packet->opcode);
		else if (thePrefs.GetVerbose())
			DebugLogWarning(_T("Received server TCP packet with unknown protocol: protocol=0x%02x  opcode=0x%02x  size=%u"), packet->prot, packet->opcode, packet->size);
#ifndef _DEBUG
	} catch (...) {
		if (thePrefs.GetVerbose())
			DebugLogError(_T("Error: Unhandled exception while processing server TCP packet: protocol=0x%02x  opcode=0x%02x  size=%u")
				, packet ? packet->prot : 0, packet ? packet->opcode : 0, packet ? packet->size : 0);
		ASSERT(0);
		return false;
	}
#endif
	return true;
}

void CServerSocket::OnClose(int /*nErrorCode*/)
{
	CEMSocket::OnClose(0);
	if (connectionstate == CS_WAITFORLOGIN)
		SetConnectionState(CS_SERVERFULL);
	else if (connectionstate == CS_CONNECTED)
		SetConnectionState(CS_DISCONNECTED);
	else
		SetConnectionState(CS_NOTCONNECTED);

	serverconnect->DestroySocket(this);
}

void CServerSocket::SetConnectionState(int newstate)
{
	connectionstate = newstate;
	if (newstate < CS_CONNECTING)
		serverconnect->ConnectionFailed(this);
	else if (newstate == CS_CONNECTED || newstate == CS_WAITFORLOGIN)
		if (serverconnect)
			serverconnect->ConnectionEstablished(this);
}

void CServerSocket::SendPacket(Packet *packet, bool controlpacket, uint32 actualPayloadSize, bool bForceImmediateSend)
{
	m_dwLastTransmission = ::GetTickCount();
	CEMSocket::SendPacket(packet, controlpacket, actualPayloadSize, bForceImmediateSend);
}