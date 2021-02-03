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
#pragma once

#define CS_FATALERROR	-5
#define CS_DISCONNECTED	-4
#define CS_SERVERDEAD	-3
#define	CS_ERROR		-2
#define CS_SERVERFULL	-1
#define	CS_NOTCONNECTED	0
#define	CS_CONNECTING	1
#define	CS_CONNECTED	2
#define	CS_WAITFORLOGIN	3
#define CS_WAITFORPROXYLISTENING 4 // deadlake PROXYSUPPORT

#define CS_RETRYCONNECTTIME  30 // seconds

class CServerList;
class CUDPSocket;
class CServerSocket;
class CServer;
class Packet;

class CServerConnect
{
public:
	CServerConnect();
	~CServerConnect();
	CServerConnect(const CServerConnect&) = delete;
	CServerConnect& operator=(const CServerConnect&) = delete;

	void	ConnectionFailed(CServerSocket *sender);
	void	ConnectionEstablished(CServerSocket *sender);

	void	ConnectToAnyServer()			{ ConnectToAnyServer(0, true, true); }
	void	ConnectToAnyServer(UINT startAt, bool prioSort = false, bool isAuto = true, bool bNoCrypt = false);
	void	ConnectToServer(CServer *server, bool multiconnect = false, bool bNoCrypt = false);
	void	StopConnectionTry();
	static  VOID CALLBACK RetryConnectTimer(HWND hWnd, UINT nMsg, UINT_PTR nId, DWORD dwTime) noexcept;

	void	CheckForTimeout();
	void	DestroySocket(CServerSocket *pSck);	// safe socket closure and destruction
	bool	SendPacket(Packet *packet, CServerSocket *to = NULL);
	bool	IsUDPSocketAvailable() const	{ return udpsocket != NULL; }
	bool	SendUDPPacket(Packet *packet, CServer *host, bool bDelPacket/* = false*/, uint16 nSpecialPort = 0, BYTE *pRawPacket = NULL, uint32 nLen = 0);
	void	KeepConnectionAlive();
	bool	Disconnect();
	bool	IsConnecting() const			{ return connecting; }
	bool	IsConnected() const				{ return connected; }
	uint32	GetClientID() const				{ return m_clientid; }
	CServer* GetCurrentServer();

	bool	IsLowID() const;
	void	SetClientID(uint32 newid);
	bool	IsLocalServer(uint32 dwIP, uint16 nPort) const;
	void	TryAnotherConnectionRequest();
	bool	IsSingleConnect() const			{ return singleconnecting; }
	void	InitLocalIP();
	uint32	GetLocalIP() const				{ return m_nLocalIP; }

	bool	AwaitingTestFromIP(uint32 dwIP) const;
	bool	IsConnectedObfuscated() const;

	uint32	m_clientid;
	uint32	m_curuser;
	uint8	pendingConnects;

private:
	CMap<ULONG, ULONG, CServerSocket*, CServerSocket*> connectionattemps;
	CPtrList m_lstOpenSockets;	// list of currently opened sockets
	CServerSocket *connectedsocket;
	CUDPSocket *udpsocket;
	UINT_PTR m_idRetryTimer;
	uint32	m_nLocalIP;
	UINT	m_uStartAutoConnectPos;
	uint8	max_simcons;
	bool	connecting;
	bool	singleconnecting;
	bool	connected;
	bool	m_bTryObfuscated;
};