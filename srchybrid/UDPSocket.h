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
#include "UploadBandwidthThrottler.h" // ZZ:UploadBandWithThrottler (UDP)
#include "EncryptedDatagramSocket.h"

class CServerConnect;
struct SServerUDPPacket;
struct SServerDNSRequest;
class CUDPSocket;
class Packet;
class CServer;


///////////////////////////////////////////////////////////////////////////////
// CUDPSocketWnd

class CUDPSocketWnd : public CWnd
{
// Construction
public:
	CUDPSocketWnd()
		: m_pOwner(NULL)
	{
	};
	CUDPSocket *m_pOwner;

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg LRESULT OnDNSLookupDone(WPARAM wParam, LPARAM lParam);
};


///////////////////////////////////////////////////////////////////////////////
// CUDPSocket

class CUDPSocket : public CAsyncSocket, public CEncryptedDatagramSocket, public ThrottledControlSocket // ZZ:UploadBandWithThrottler (UDP)
{
	friend class CServerConnect;

public:
	CUDPSocket();
	virtual	~CUDPSocket();
	CUDPSocket(const CUDPSocket&) = delete;
	CUDPSocket& operator=(const CUDPSocket&) = delete;

	bool Create();
	SocketSentBytes SendControlData(uint32 maxNumberOfBytesToSend, uint32 minFragSize); // ZZ:UploadBandWithThrottler (UDP)
	void SendPacket(Packet *packet, CServer *pServer, uint16 nSpecialPort = 0, BYTE *pInRawPacket = NULL, uint32 nRawLen = 0);
	void DnsLookupDone(WPARAM wp, LPARAM lp);

protected:
	virtual void OnSend(int nErrorCode);
	virtual void OnReceive(int nErrorCode);

private:
	void SendBuffer(uint32 nIP, uint16 nPort, BYTE *pPacket, UINT uSize);
	bool ProcessPacket(const BYTE *packet, UINT size, UINT opcode, uint32 nIP, uint16 nUDPPort);
	void ProcessPacketError(UINT size, UINT opcode, uint32 nIP, uint16 nUDPPort, LPCTSTR pszError);
	bool IsBusy() const						{ return m_bWouldBlock; }
	int SendTo(BYTE *lpBuf, int nBufLen, uint32 dwIP, uint16 nPort);

	CUDPSocketWnd m_udpwnd;
	CTypedPtrList<CPtrList, SServerDNSRequest*> m_aDNSReqs;
	CTypedPtrList<CPtrList, SServerUDPPacket*> controlpacket_queue;
	CCriticalSection sendLocker; // ZZ:UploadBandWithThrottler (UDP)
	HWND m_hWndResolveMessage;
	bool m_bWouldBlock;
};