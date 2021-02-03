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
#include "EMSocket.h"

class CUpDownClient;
class CPacket;
class CTimerWnd;
enum EDebugLogPriority : int;

enum SocketState
{
	SS_Other,	//These are sockets we created that may or may not be used. Or incoming connections.
	SS_Half,	//These are sockets that we called ->connect(...) and waiting for some kind of response.
	SS_Complete	//These are sockets that have responded with either a connection or error.
};

class CClientReqSocket : public CEMSocket
{
	friend class CListenSocket;
	DECLARE_DYNCREATE(CClientReqSocket)

public:
	explicit CClientReqSocket(CUpDownClient *in_client = NULL);

	void	SetClient(CUpDownClient *pClient);
	void	Disconnect(LPCTSTR pszReason);
	void	WaitForOnConnect();
	void	ResetTimeOutTimer();
	bool	CheckTimeOut();
	virtual DWORD GetTimeOut() const;
	virtual void Safe_Delete();
	virtual void Close()				{ CAsyncSocketEx::Close(); }

	bool	Create();
	virtual void SendPacket(Packet *packet, bool controlpacket = true, uint32 actualPayloadSize = 0, bool bForceImmediateSend = false);
	virtual SocketSentBytes SendControlData(uint32 maxNumberOfBytesToSend, uint32 overchargeMaxBytesToSend);
	virtual SocketSentBytes SendFileAndControlData(uint32 maxNumberOfBytesToSend, uint32 overchargeMaxBytesToSend);

	void		 OnClose(int nErrorCode);
	virtual void OnConnect(int nErrorCode);
	void		 OnReceive(int nErrorCode);
	void		 OnSend(int nErrorCode);

	void	DbgAppendClientInfo(CString &str);
	CString DbgGetClientInfo();

	CUpDownClient *client;
protected:
	virtual	~CClientReqSocket();
	void	Delete_Timed();

	void		 OnError(int nErrorCode);

	virtual bool PacketReceived(Packet *packet);

	bool	ProcessPacket(const BYTE *packet, uint32 size, UINT opcode);
	bool	ProcessExtPacket(const BYTE *packet, uint32 size, UINT opcode, UINT uRawSize);
	void	PacketToDebugLogLine(LPCTSTR protocol, const uchar *packet, uint32 size, UINT opcode);
	void	SetConState(SocketState val);

	DWORD	timeout_timer;
	uint32	deltimer;
	uint32	m_nOnConnect;
	bool	deletethis;
	bool	m_bPortTestCon;
};


class CListenSocket : public CAsyncSocketEx
{
	friend class CClientReqSocket;

public:
	CListenSocket();
	virtual	~CListenSocket();

	bool	StartListening();
	void	StopListening();
	virtual void OnAccept(int nErrorCode);
	void	Process();
	void	RemoveSocket(CClientReqSocket *todel);
	void	AddSocket(CClientReqSocket *toadd);
	UINT	GetOpenSockets()			{ return static_cast<UINT>(socket_list.GetCount()); }
	void	KillAllSockets();
	bool	TooManySockets(bool bIgnoreInterval = false);
	uint32	GetMaxConnectionReached()	{ return maxconnectionreached; }
	bool    IsValidSocket(CClientReqSocket *totest);
	void	AddConnection();
	void	RecalculateStats();
	void	ReStartListening();
	void	Debug_ClientDeleted(CUpDownClient *deleted);
	bool	Rebind();
	bool	SendPortTestReply(char result, bool disconnect = false);

	void	UpdateConnectionsStatus();
	float	GetMaxConperFiveModifier();
	uint32	GetPeakConnections()		{ return peakconnections; }
	uint32	GetTotalConnectionChecks()	{ return totalconnectionchecks; }
	float	GetAverageConnections()		{ return averageconnections; }
	uint32	GetActiveConnections()		{ return activeconnections; }
	uint16	GetConnectedPort()			{ return m_port; }
	uint32	GetTotalHalfCon()			{ return m_nHalfOpen; }
	uint32	GetTotalComp()				{ return m_nComp; }

private:
	bool bListening;
	CTypedPtrList<CPtrList, CClientReqSocket*> socket_list;
	uint16	m_OpenSocketsInterval;
	uint32	maxconnectionreached;
	uint16	m_ConnectionStates[3];
	int		m_nPendingConnections;
	uint32	peakconnections;
	uint32	totalconnectionchecks;
	float	averageconnections;
	uint32	activeconnections;
	uint16  m_port;
	uint32	m_nHalfOpen;
	uint32	m_nComp;
};