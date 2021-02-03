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

class CServer;

class CServerSocket : public CEMSocket
{
	friend class CServerConnect;

public:
	CServerSocket(CServerConnect *in_serverconnect, bool bManualSingleConnect);
	~CServerSocket();

	void ConnectTo(CServer *server, bool bNoCrypt = false);
	int GetConnectionState() const							{ return connectionstate; }
	DWORD GetLastTransmission() const						{ return m_dwLastTransmission; }
	virtual void SendPacket(Packet *packet, bool controlpacket = true, uint32 actualPayloadSize = 0, bool bForceImmediateSend = false);
	virtual void OnClose(int nErrorCode);
	virtual void OnConnect(int nErrorCode);
	virtual bool OnHostNameResolved(const SOCKADDR_IN *pSockAddr);
	virtual void OnReceive(int nErrorCode);

protected:
	virtual void OnError(int nErrorCode);
	bool PacketReceived(Packet *packet);
	void ProcessPacketError(UINT size, UINT opcode, LPCTSTR pszError);

private:
	bool ProcessPacket(const BYTE *packet, uint32 size, uint8 opcode);
	void SetConnectionState(int newstate);

	CServerConnect *serverconnect;
	CServer *cur_server; // holds a copy of a CServer from the CServerList
	DWORD m_dwLastTransmission;
	int connectionstate;
	bool m_bIsDeleting;	// true: socket is already in deletion phase, don't destroy it in ::StopConnectionTry
	bool m_bStartNewMessageLog;
	bool m_bManualSingleConnect;
};