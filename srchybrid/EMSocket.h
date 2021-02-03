//this file is part of eMule
//Copyright (C)2002-2010 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#pragma once
#include "packets.h"
#include "EncryptedStreamSocket.h"
#include "ThrottledSocket.h" // ZZ:UploadBandWithThrottler (UDP)

class CAsyncProxySocketLayer;

#define ES_DISCONNECTED	0xFF
#define ES_NOTCONNECTED	0x00
#define ES_CONNECTED	0x01

struct StandardPacketQueueEntry
{
	Packet *packet;
	uint32 actualPayloadSize;
};

class CEMSocket : public CEncryptedStreamSocket, public ThrottledFileSocket // ZZ:UploadBandWithThrottler
{
	DECLARE_DYNAMIC(CEMSocket)
public:
	CEMSocket();
	~CEMSocket();

	virtual void SendPacket(Packet *packet, bool controlpacket = true, uint32 actualPayloadSize = 0, bool bForceImmediateSend = false);
	bool	IsConnected() const				{ return byConnected == ES_CONNECTED; }
	uint8	GetConState() const				{ return byConnected; }
	void	SetConState(uint8 val)			{ sendLocker.Lock(); byConnected = val; sendLocker.Unlock(); }
	virtual bool IsRawDataMode() const		{ return false; }
	void	SetDownloadLimit(uint32 limit);
	void	DisableDownloadLimit();
	BOOL	AsyncSelect(long lEvent);
	virtual bool IsBusyExtensiveCheck();
	virtual bool IsBusyQuickCheck() const;
	virtual bool HasQueues(bool bOnlyStandardPackets = false) const;
	virtual bool IsEnoughFileDataQueued(uint32 nMinFilePayloadBytes) const;
	virtual bool UseBigSendBuffer();
	INT_PTR	DbgGetStdQueueCount() const		{ return standardpacket_queue.GetCount(); }

	virtual DWORD GetTimeOut() const;
	virtual void SetTimeOut(DWORD uTimeOut);

	virtual bool Connect(const CString &sHostAddress, UINT nHostPort);
	virtual BOOL Connect(const LPSOCKADDR pSockAddr, int iSockAddrLen);
	virtual int Receive(void *lpBuf, int nBufLen, int nFlags = 0);

	virtual void	OnClose(int nErrorCode);
	virtual void	OnSend(int nErrorCode);
	virtual void	OnReceive(int nErrorCode);

	void InitProxySupport();
	virtual void RemoveAllLayers();
	const CString GetLastProxyError() const	{ return m_strLastProxyError; }
	bool GetProxyConnectFailed() const		{ return m_bProxyConnectFailed; }

	CString GetFullErrorMessage(DWORD dwError);

	DWORD GetLastCalledSend() const			{ return lastCalledSend; }
	uint64 GetSentBytesCompleteFileSinceLastCallAndReset();
	uint64 GetSentBytesPartFileSinceLastCallAndReset();
	uint64 GetSentBytesControlPacketSinceLastCallAndReset();
	uint64 GetSentPayloadSinceLastCall(bool bReset);
	void TruncateQueues();

	virtual SocketSentBytes SendControlData(uint32 maxNumberOfBytesToSend, uint32 minFragSize)			{ return Send(maxNumberOfBytesToSend, minFragSize, true); };
	virtual SocketSentBytes SendFileAndControlData(uint32 maxNumberOfBytesToSend, uint32 minFragSize)	{ return Send(maxNumberOfBytesToSend, minFragSize, false); };

	uint32	GetNeededBytes();
#ifdef _DEBUG
	// Diagnostic Support
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext &dc) const;
#endif

protected:
	virtual int	OnLayerCallback(std::list<t_callbackMsg> &callbacks);

	virtual void	DataReceived(const BYTE *pcData, UINT uSize);
	virtual bool	PacketReceived(Packet *packet) = 0;
	virtual void	OnError(int nErrorCode) = 0;
	uint8	byConnected;
	UINT	m_uTimeOut;
	bool	m_bProxyConnectFailed;
	CAsyncProxySocketLayer *m_pProxyLayer;
	CString m_strLastProxyError;

private:
	virtual SocketSentBytes Send(uint32 maxNumberOfBytesToSend, uint32 minFragSize, bool onlyAllowedToSendControlPacket);
	SocketSentBytes SendStd(uint32 maxNumberOfBytesToSend, uint32 minFragSize, bool onlyAllowedToSendControlPacket);
	SocketSentBytes SendOv(uint32 maxNumberOfBytesToSend, uint32 minFragSize, bool onlyAllowedToSendControlPacket);
	void	ClearQueues();
	void	CleanUpOverlappedSendOperation(bool bCancel);

	static uint32 GetNextFragSize(uint32 current, uint32 minFragSize);

	// Download (pseudo) rate control
	uint32	downloadLimit;
	bool	downloadLimitEnable;
	bool	pendingOnReceive;

	// Download partial header
	char	pendingHeader[PACKET_HEADER_SIZE];	// actually, this holds only 'PACKET_HEADER_SIZE-1' bytes.
	size_t	pendingHeaderSize;

	// Download partial packet
	Packet	*pendingPacket;
	uint32	pendingPacketSize;

	// Upload control
	char	*sendbuffer;
	uint32	sendblen; //packet length in sendbuffer
	uint32	sent;
	WSAOVERLAPPED m_PendingSendOperation;
	CArray<WSABUF> m_aBufferSend;

	CTypedPtrList<CPtrList, Packet*> controlpacket_queue;
	CList<StandardPacketQueueEntry> standardpacket_queue;
	CCriticalSection sendLocker;
	uint64	m_numberOfSentBytesCompleteFile;
	uint64	m_numberOfSentBytesPartFile;
	uint64	m_numberOfSentBytesControlPacket;
	DWORD	lastCalledSend;
	DWORD	lastSent;
	DWORD	lastFinishedStandard;
	uint32	m_actualPayloadSize;			// Payloadsize of the data currently in sendbuffer
	uint32	m_actualPayloadSizeSent;
	bool	m_currentPacket_is_controlpacket;
	bool	m_currentPackageIsFromPartFile;
	bool	m_bAccelerateUpload;
	bool	m_bBusy;
	bool	m_hasSent;
	bool	m_bUseBigSendBuffers;
	bool	m_bUseOverlappedSend;
	bool	m_bPendingSendOv;
};