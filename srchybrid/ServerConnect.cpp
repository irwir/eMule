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
#include "ServerConnect.h"
#include "Opcodes.h"
#include "UDPSocket.h"
#include "Exceptions.h"
#include "OtherFunctions.h"
#include "Statistics.h"
#include "ServerSocket.h"
#include "ServerList.h"
#include "Server.h"
#include "ListenSocket.h"
#include "SafeFile.h"
#include "Packets.h"
#include "SharedFileList.h"
#include "PeerCacheFinder.h"
#include "emuleDlg.h"
#include "SearchDlg.h"
#include "ServerWnd.h"
#include "TaskbarNotifier.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void CServerConnect::TryAnotherConnectionRequest()
{
	if (connectionattemps.GetCount() < (thePrefs.IsSafeServerConnectEnabled() ? 1 : 2)) {
		CServer *next_server = theApp.serverlist->GetNextServer(m_bTryObfuscated);
		if (next_server == NULL) {
			if (connectionattemps.IsEmpty()) {
				if (m_bTryObfuscated && !thePrefs.IsClientCryptLayerRequired()) {
					// try all servers on the non-obfuscated port next
					m_bTryObfuscated = false;
					ConnectToAnyServer(0, true, true, true);
				} else if (m_idRetryTimer == 0) {
					// 05-Nov-2003: If we have a very short server list, we could put serious load
					// on those few servers if we start the next connection tries without waiting.
					LogWarning(LOG_STATUSBAR, GetResString(IDS_OUTOFSERVERS));
					AddLogLine(false, GetResString(IDS_RECONNECT), CS_RETRYCONNECTTIME);
					m_uStartAutoConnectPos = 0; // default: start at 0
					VERIFY((m_idRetryTimer = ::SetTimer(NULL, 0, SEC2MS(CS_RETRYCONNECTTIME), RetryConnectTimer)) != 0);
					if (thePrefs.GetVerbose() && !m_idRetryTimer)
						DebugLogError(_T("Failed to create 'server connect retry' timer - %s"), (LPCTSTR)GetErrorMessage(::GetLastError()));
				}
			}
		} else
			// Barry - Only auto-connect to static server option
			if (!thePrefs.GetAutoConnectToStaticServersOnly() || next_server->IsStaticMember())
				ConnectToServer(next_server, true, !m_bTryObfuscated);
	}
}

void CServerConnect::ConnectToAnyServer(UINT startAt, bool prioSort, bool isAuto, bool bNoCrypt)
{
	StopConnectionTry();
	Disconnect();
	connecting = true;
	singleconnecting = false;
	theApp.emuledlg->ShowConnectionState();
	m_bTryObfuscated = thePrefs.IsServerCryptLayerTCPRequested() && !bNoCrypt;

	// Barry - Only auto-connect to static server option
	if (thePrefs.GetAutoConnectToStaticServersOnly() && isAuto) {
		bool anystatic = false;
		CServer *next_server;
		theApp.serverlist->SetServerPosition(startAt);
		while ((next_server = theApp.serverlist->GetNextServer(false)) != NULL)
			if (next_server->IsStaticMember()) {
				anystatic = true;
				break;
			}

		if (!anystatic) {
			connecting = false;
			theApp.emuledlg->ShowConnectionState();
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_NOVALIDSERVERSFOUND));
			return;
		}
	}

	theApp.serverlist->SetServerPosition(startAt);
	if (thePrefs.GetUseUserSortedServerList() && startAt == 0 && prioSort)
		theApp.serverlist->GetUserSortedServers();
	if (thePrefs.GetUseServerPriorities() && prioSort)
		theApp.serverlist->Sort();

	if (theApp.serverlist->GetServerCount() == 0) {
		connecting = false;
		theApp.emuledlg->ShowConnectionState();
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_NOVALIDSERVERSFOUND));
	} else {
		theApp.listensocket->Process();
		TryAnotherConnectionRequest();
	}
}

void CServerConnect::ConnectToServer(CServer *server, bool multiconnect, bool bNoCrypt)
{
	if (!multiconnect) {
		StopConnectionTry();
		Disconnect();
	}
	connecting = true;
	singleconnecting = !multiconnect;
	theApp.emuledlg->ShowConnectionState();

	CServerSocket *newsocket = new CServerSocket(this, !multiconnect);
	m_lstOpenSockets.AddTail((void*)newsocket);
	newsocket->Create(0, SOCK_STREAM, FD_READ | FD_WRITE | FD_CLOSE | FD_CONNECT, thePrefs.GetBindAddr());
	newsocket->ConnectTo(server, bNoCrypt);
	connectionattemps.SetAt(::GetTickCount(), newsocket);
}

void CServerConnect::StopConnectionTry()
{
	connectionattemps.RemoveAll();
	connecting = false;
	singleconnecting = false;
	theApp.emuledlg->ShowConnectionState();

	if (m_idRetryTimer) {
		::KillTimer(NULL, m_idRetryTimer);
		m_idRetryTimer = 0;
	}

	// close all currently opened sockets except those that are:
	//- connected to our current server
	//- going to destroy itself later on
	for (POSITION pos = m_lstOpenSockets.GetHeadPosition(); pos != NULL; ) {
		CServerSocket *pSck = static_cast<CServerSocket*>(m_lstOpenSockets.GetNext(pos));
		if (pSck != connectedsocket && !pSck->m_bIsDeleting)
			DestroySocket(pSck);
	}
}

void CServerConnect::ConnectionEstablished(CServerSocket *sender)
{
	if (!connecting) {
		// we are already connected to another server
		DestroySocket(sender);
		return;
	}

	InitLocalIP();
	if (sender->GetConnectionState() == CS_WAITFORLOGIN) {
		const CServer *cserver = sender->cur_server;
		AddLogLine(false, GetResString(IDS_CONNECTEDTOREQ), (LPCTSTR)cserver->GetListName(), cserver->GetAddress(), sender->IsServerCryptEnabledConnection() ? cserver->GetObfuscationPortTCP() : cserver->GetPort());

		CServer *pServer = theApp.serverlist->GetServerByAddress(cserver->GetAddress(), cserver->GetPort());
		if (pServer) {
			pServer->ResetFailedCount();
			theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
		}

		// Send login packet
		CSafeMemFile data(256);
		data.WriteHash16(thePrefs.GetUserHash());
		data.WriteUInt32(GetClientID());
		data.WriteUInt16(thePrefs.GetPort());

		UINT tagcount = 4;
		data.WriteUInt32(tagcount);

		CTag tagName(CT_NAME, thePrefs.GetUserNick());
		tagName.WriteTagToFile(&data);

		CTag tagVersion(CT_VERSION, EDONKEYVERSION);
		tagVersion.WriteTagToFile(&data);

		uint32 dwCryptFlags = 0;
		if (thePrefs.IsClientCryptLayerSupported())
			dwCryptFlags |= SRVCAP_SUPPORTCRYPT;
		if (thePrefs.IsClientCryptLayerRequested())
			dwCryptFlags |= SRVCAP_REQUESTCRYPT;
		if (thePrefs.IsClientCryptLayerRequired())
			dwCryptFlags |= SRVCAP_REQUIRECRYPT;

		CTag tagFlags(CT_SERVER_FLAGS, SRVCAP_ZLIB | SRVCAP_NEWTAGS | SRVCAP_LARGEFILES | SRVCAP_UNICODE | dwCryptFlags);
		tagFlags.WriteTagToFile(&data);

		// eMule Version (14-Mar-2004: requested by lugdunummaster (needed for LowID clients
		// that have no chance to send a Hello packet to the server during the callback test))
		CTag tagMuleVersion(CT_EMULE_VERSION,
			//(uCompatibleClientID		<< 24) |
			(CemuleApp::m_nVersionMjr << 17) |
			(CemuleApp::m_nVersionMin << 10) |
			(CemuleApp::m_nVersionUpd << 7));
		tagMuleVersion.WriteTagToFile(&data);

		Packet *packet = new Packet(&data);
		packet->opcode = OP_LOGINREQUEST;
		if (thePrefs.GetDebugServerTCPLevel() > 0)
			Debug(_T(">>> Sending OP_LoginRequest\n"));
		theStats.AddUpDataOverheadServer(packet->size);
		SendPacket(packet, sender);
	} else if (sender->GetConnectionState() == CS_CONNECTED) {
		theStats.reconnects++;
		theStats.serverConnectTime = ::GetTickCount();
		connected = true;
		const CServer *cserver = sender->cur_server;
		CString strMsg;
		strMsg.Format(GetResString(sender->IsObfusicating() ? IDS_CONNECTEDTOOBFUSCATED : IDS_CONNECTEDTO) + _T(" (%s:%u)")
			, (LPCTSTR)cserver->GetListName()
			, cserver->GetAddress()
			, sender->IsObfusicating() ? cserver->GetObfuscationPortTCP() : cserver->GetPort());

		Log(LOG_SUCCESS | LOG_STATUSBAR, strMsg);
		theApp.emuledlg->ShowConnectionState();
		connectedsocket = sender;
		StopConnectionTry();
		theApp.sharedfiles->ClearED2KPublishInfo();
		theApp.sharedfiles->SendListToServer();
		theApp.emuledlg->serverwnd->serverlistctrl.RemoveAllDeadServers();

		// tecxx 1609 2002 - serverlist update
		if (thePrefs.GetAddServersFromServer()) {
			Packet *packet = new Packet(OP_GETSERVERLIST, 0);
			if (thePrefs.GetDebugServerTCPLevel() > 0)
				Debug(_T(">>> Sending OP_GetServerList\n"));
			theStats.AddUpDataOverheadServer(packet->size);
			SendPacket(packet);
		}

		CServer *pServer = theApp.serverlist->GetServerByAddress(cserver->GetAddress(), cserver->GetPort());
		if (pServer)
			theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
	}
	theApp.emuledlg->ShowConnectionState();
}

bool CServerConnect::SendPacket(Packet *packet, CServerSocket *to)
{
	if (!to) {
		if (!connected) {
			delete packet;
			return false;
		}
		connectedsocket->SendPacket(packet, true);
	} else
		to->SendPacket(packet, true);

	return true;
}

bool CServerConnect::SendUDPPacket(Packet *packet, CServer *host, bool bDelPacket, uint16 nSpecialPort, BYTE *pRawPacket, uint32 nLen)
{
	if (theApp.IsConnected() && udpsocket != NULL)
		udpsocket->SendPacket(packet, host, nSpecialPort, pRawPacket, nLen);

	if (bDelPacket) {
		delete packet;
		delete[] pRawPacket;
	}
	return true;
}

void CServerConnect::ConnectionFailed(CServerSocket *sender)
{
	if (!connecting && sender != connectedsocket)
		// just return, cleanup is done by the socket itself
		return;
	const CServer *cserver = sender->cur_server;
	CServer *pServer = theApp.serverlist->GetServerByAddress(cserver->GetAddress(), cserver->GetPort());
	switch (sender->GetConnectionState()) {
	case CS_FATALERROR:
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_FATAL));
		break;
	case CS_DISCONNECTED:
		theApp.sharedfiles->ClearED2KPublishInfo();
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_LOSTC), (LPCTSTR)cserver->GetListName(), cserver->GetAddress(), cserver->GetPort());
		break;
	case CS_SERVERDEAD:
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_DEAD), (LPCTSTR)cserver->GetListName(), cserver->GetAddress(), cserver->GetPort());
		if (pServer) {
			pServer->IncFailedCount();
			theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
		}
		break;
	case CS_ERROR:
		break;
	case CS_SERVERFULL:
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_FULL), (LPCTSTR)cserver->GetListName(), cserver->GetAddress(), cserver->GetPort());
	}

	// IMPORTANT: mark this socket not to be deleted in StopConnectionTry(),
	// because it will delete itself after this function!
	sender->m_bIsDeleting = true;

	switch (sender->GetConnectionState()) {
	case CS_FATALERROR:
		{
			bool autoretry = !singleconnecting;
			StopConnectionTry();
			if (thePrefs.Reconnect() && autoretry && !m_idRetryTimer) {
				LogWarning(GetResString(IDS_RECONNECT), CS_RETRYCONNECTTIME);

				// There are situations where we may get Winsock error codes which indicate
				// that the network is down, although it is not. Those error codes may get
				// thrown only for particular IPs. If the first server in our list has such
				// an IP and will therefore throw such an error we would never connect to
				// any server at all. To circumvent that, start the next auto-connection
				// attempt with a different server (use the next server in the list).
				m_uStartAutoConnectPos = 0; // default: start at 0
				if (pServer) {
					// If possible, use the "next" server.
					int iPosInList = theApp.serverlist->GetPositionOfServer(pServer);
					if (iPosInList >= 0)
						m_uStartAutoConnectPos = (iPosInList + 1) % theApp.serverlist->GetServerCount();
				}
				VERIFY((m_idRetryTimer = ::SetTimer(NULL, 0, SEC2MS(CS_RETRYCONNECTTIME), RetryConnectTimer)) != 0);
				if (thePrefs.GetVerbose() && !m_idRetryTimer)
					DebugLogError(_T("Failed to create 'server connect retry' timer - %s"), (LPCTSTR)GetErrorMessage(::GetLastError()));
			}
		}
		break;
	case CS_DISCONNECTED:
		theApp.sharedfiles->ClearED2KPublishInfo();
		connected = false;
		if (connectedsocket) {
			connectedsocket->Close();
			connectedsocket = NULL;
		}
		theApp.emuledlg->searchwnd->CancelEd2kSearch();
		theStats.serverConnectTime = 0;
		theStats.Add2TotalServerDuration();
		if (thePrefs.Reconnect() && !connecting)
			ConnectToAnyServer();
		if (thePrefs.GetNotifierOnImportantError())
			theApp.emuledlg->ShowNotifier(GetResString(IDS_CONNECTIONLOST), TBN_IMPORTANTEVENT);
		break;
	case CS_ERROR:
	case CS_NOTCONNECTED:
		//if (!connecting)
		//	break;
	case CS_SERVERDEAD:
	case CS_SERVERFULL:
		if (!connecting)
			break;
		if (singleconnecting) {
			if (pServer != NULL && sender->IsServerCryptEnabledConnection() && !thePrefs.IsClientCryptLayerRequired()) {
				// try reconnecting without obfuscation
				ConnectToServer(pServer, false, true);
				break;
			}
			StopConnectionTry();
			break;
		}

		for (POSITION pos = connectionattemps.GetStartPosition(); pos != NULL;) {
			DWORD tmpkey;
			CServerSocket *tmpsock;
			connectionattemps.GetNextAssoc(pos, tmpkey, tmpsock);
			if (tmpsock == sender) {
				connectionattemps.RemoveKey(tmpkey);
				break;
			}
		}
		TryAnotherConnectionRequest();
	}
	theApp.emuledlg->ShowConnectionState();
}

VOID CALLBACK CServerConnect::RetryConnectTimer(HWND /*hWnd*/, UINT /*nMsg*/, UINT_PTR /*nId*/, DWORD /*dwTime*/) noexcept
{
	// NOTE: Always handle all type of MFC exceptions in TimerProcs - otherwise we'll get mem leaks
	try {
		CServerConnect *_this = theApp.serverconnect;
		ASSERT(_this);
		if (_this) {
			_this->StopConnectionTry();
			if (_this->IsConnected())
				return;
			if (_this->m_uStartAutoConnectPos >= (UINT)theApp.serverlist->GetServerCount())
				_this->m_uStartAutoConnectPos = 0;
			_this->ConnectToAnyServer(_this->m_uStartAutoConnectPos, true, true);
		}
	}
	CATCH_DFLT_EXCEPTIONS(_T("CServerConnect::RetryConnectTimer"))
}

void CServerConnect::CheckForTimeout()
{
	DWORD dwServerConnectTimeout = CONSERVTIMEOUT;
	// If we are using a proxy, increase server connection timeout to default connection timeout
	if (thePrefs.GetProxySettings().bUseProxy)
		dwServerConnectTimeout = max(dwServerConnectTimeout, CONNECTION_TIMEOUT);

	DWORD dwCurTick = ::GetTickCount();
	for (POSITION pos = connectionattemps.GetStartPosition(); pos != NULL;) {
		DWORD tmpkey;
		CServerSocket *tmpsock;
		connectionattemps.GetNextAssoc(pos, tmpkey, tmpsock);
		if (!tmpsock) {
			if (thePrefs.GetVerbose())
				DebugLogError(_T("Error: Socket invalid at timeout check"));
			connectionattemps.RemoveKey(tmpkey);
			return;
		}

		if (dwCurTick >= tmpkey + dwServerConnectTimeout) {
			LogWarning(GetResString(IDS_ERR_CONTIMEOUT), (LPCTSTR)tmpsock->cur_server->GetListName(), tmpsock->cur_server->GetAddress(), tmpsock->cur_server->GetPort());
			connectionattemps.RemoveKey(tmpkey);
			DestroySocket(tmpsock);
			if (singleconnecting)
				StopConnectionTry();
			else
				TryAnotherConnectionRequest();

		}
	}
}

bool CServerConnect::Disconnect()
{
	if (connected && connectedsocket) {
		theApp.sharedfiles->ClearED2KPublishInfo();
		connected = false;
		CServer *pServer = theApp.serverlist->GetServerByAddress(connectedsocket->cur_server->GetAddress(), connectedsocket->cur_server->GetPort());
		if (pServer)
			theApp.emuledlg->serverwnd->serverlistctrl.RefreshServer(pServer);
		theApp.SetPublicIP(0);
		DestroySocket(connectedsocket);
		connectedsocket = NULL;
		theApp.emuledlg->ShowConnectionState();
		theStats.serverConnectTime = 0;
		theStats.Add2TotalServerDuration();
		return true;
	}
	return false;
}

CServerConnect::CServerConnect()
	: m_clientid()
	, m_curuser()
	, pendingConnects()
	, connectedsocket()
	, m_idRetryTimer()
	, m_uStartAutoConnectPos()
	, max_simcons(thePrefs.IsSafeServerConnectEnabled() ? 1 : 2)
	, connecting()
	, singleconnecting()
	, connected()
	, m_bTryObfuscated()
{
	if (thePrefs.GetServerUDPPort() != 0) {
		udpsocket = new CUDPSocket(); // initialize socket for udp packets
		if (!udpsocket->Create()) {
			delete udpsocket;
			udpsocket = NULL;
		}
	} else
		udpsocket = NULL;
	InitLocalIP();
}

CServerConnect::~CServerConnect()
{
	// stop all connections
	StopConnectionTry();
	// close connected socket, if any
	DestroySocket(connectedsocket);
	connectedsocket = NULL;
	// close udp socket
	if (udpsocket) {
		udpsocket->Close();
		delete udpsocket;
	}
}

CServer* CServerConnect::GetCurrentServer()
{
	return (IsConnected() && connectedsocket) ? connectedsocket->cur_server : NULL;
}

void CServerConnect::SetClientID(uint32 newid)
{
	m_clientid = newid;

	if (!::IsLowID(newid))
		theApp.SetPublicIP(newid);

	theApp.emuledlg->ShowConnectionState();
}

void CServerConnect::DestroySocket(CServerSocket *pSck)
{
	if (pSck == NULL)
		return;
	// remove socket from list of opened sockets
	POSITION pos = m_lstOpenSockets.Find(pSck);
	if (pos != NULL)
		m_lstOpenSockets.RemoveAt(pos);
	if (pSck->m_SocketData.hSocket != INVALID_SOCKET) { // deadlake PROXYSUPPORT - changed to AsyncSocketEx
		pSck->AsyncSelect(FD_CLOSE);
		pSck->ShutDown(CAsyncSocket::both);
		pSck->Close();
	}
	delete pSck;
}

bool CServerConnect::IsLocalServer(uint32 dwIP, uint16 nPort) const
{
	return IsConnected() && connectedsocket->cur_server->GetIP() == dwIP && connectedsocket->cur_server->GetPort() == nPort;
}

void CServerConnect::InitLocalIP()
{
	m_nLocalIP = 0;

	// Using 'gethostname/gethostbyname' does not solve the problem when we have more than
	// one IP address. Using 'gethostname/gethostbyname' even seems to return the last IP
	// address which we got. e.g. if we already got an IP from our ISP,
	// 'gethostname/gethostbyname' will returned that (primary) IP, but if we add another
	// IP by opening a VPN connection, 'gethostname' will still return the same hostname,
	// but 'gethostbyname' will return the 2nd IP.
	// To weaken that problem at least for users which are binding eMule to a certain IP,
	// we use the explicitly specified bind address as our local IP address.
	if (thePrefs.GetBindAddrA() != NULL) {
		unsigned long ulBindAddr = inet_addr(thePrefs.GetBindAddrA());
		if (ulBindAddr != INADDR_ANY && ulBindAddr != INADDR_NONE) {
			m_nLocalIP = ulBindAddr;
			return;
		}
	}

	// Don't use 'gethostbyname(NULL)'. The winsock DLL may be replaced by a DLL from a third party
	// which is not fully compatible to the original winsock DLL. ppl reported crash with SCORSOCK.DLL
	// when using 'gethostbyname(NULL)'.
	__try {
		char szHost[256];
		if (gethostname(szHost, sizeof szHost) == 0) {
			hostent *pHostEnt = gethostbyname(szHost);
			if (pHostEnt != NULL && pHostEnt->h_length == 4 && pHostEnt->h_addr_list[0] != NULL)
				m_nLocalIP = *((uint32*)pHostEnt->h_addr_list[0]);
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		// at least two ppl reported crashes when using 'gethostbyname' with third party winsock DLLs
		if (thePrefs.GetVerbose())
			DebugLogError(_T("Unknown exception in CServerConnect::InitLocalIP"));
		ASSERT(0);
	}
}

void CServerConnect::KeepConnectionAlive()
{
	DWORD dwServerKeepAliveTimeout = thePrefs.GetServerKeepAliveTimeout();
	if (dwServerKeepAliveTimeout && connected && connectedsocket && connectedsocket->connectionstate == CS_CONNECTED
		&& ::GetTickCount() >= connectedsocket->GetLastTransmission() + dwServerKeepAliveTimeout)
	{
		// "Ping" the server if the TCP connection was not used for the specified interval with
		// an empty publish files packet -> recommended by lugdunummaster himself!
		CSafeMemFile files(4);
		files.WriteUInt32(0); // nr. of files
		Packet *packet = new Packet(&files);
		packet->opcode = OP_OFFERFILES;
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("Refreshing server connection"));
		if (thePrefs.GetDebugServerTCPLevel() > 0)
			Debug(_T(">>> Sending OP_OfferFiles(KeepAlive) to server\n"));
		theStats.AddUpDataOverheadServer(packet->size);
		connectedsocket->SendPacket(packet);
	}
}

bool CServerConnect::IsLowID() const
{
	return ::IsLowID(m_clientid);
}

// true if the IP is one of a server which we currently try to connect to
bool CServerConnect::AwaitingTestFromIP(uint32 dwIP) const
{
	if (connectionattemps.IsEmpty())
		return false;
	for (POSITION pos = connectionattemps.GetStartPosition(); pos != NULL;) {
		DWORD tmpkey;
		CServerSocket *tmpsock;
		connectionattemps.GetNextAssoc(pos, tmpkey, tmpsock);
		if (tmpsock != NULL && tmpsock->cur_server != NULL && tmpsock->cur_server->GetIP() == dwIP && tmpsock->GetConnectionState() == CS_WAITFORLOGIN)
			return true;
	}
	return false;
}

bool CServerConnect::IsConnectedObfuscated() const
{
	return connectedsocket != NULL && connectedsocket->IsObfusicating();
}